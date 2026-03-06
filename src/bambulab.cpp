/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "headers/bambulab.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslCipher>
#include <QSslKey>

BambuLab::BambuLab(QObject* parent) : Printer(parent), mqtt() {}

BambuLab::BambuLab(QString name, QString model, QString hostname, QString accessCode, QString username, quint16 port, QObject* parent) : Printer(name, model, "BambuLab", parent) {
    this->hostname = hostname;
    this->accessCode = accessCode;
    this->username = username;
    this->port = port;

    mqtt = new QMqttClient();
    ftps = new FtpsClient();

    qDebug() << "Connecting to BambuLab printer...";
    this->startConnection();
}

void BambuLab::updateState() {
    QJsonParseError err = QJsonParseError();
    QJsonDocument doc = QJsonDocument::fromJson(latestReportBytes, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse JSON: " << err.errorString();
        return;
    }
    if (!doc.isObject()) {
        qWarning() << "Message is not JSON Object";
        return;
    }
    latestReport = doc.object();


    QJsonObject amsInfo = latestReport.value("print").toObject().value("ams").toObject();
    if (amsInfo.value("ams_exist_bits") == "1") {
        this->hasAms = true;
        amsList.clear();
        QJsonArray amsList2 = amsInfo.value("ams").toArray();
        for (QJsonValueRef amsRef : amsList2) {
            QJsonObject amsState = amsRef.toObject();
            BambuAms newAms = BambuAms(amsState.value("id").toInteger());
            QJsonArray amsTray = amsState.value("tray").toArray();
            for (QJsonValueRef filamentRef : amsTray) {
                QJsonObject filament = filamentRef.toObject();
                if (filament.contains("tray_type")) {//filament loaded
                    BambuAmsFilament newFilament = BambuAmsFilament(filament.value("tray_type").toString(), filament.value("tray_color").toString());
                    newAms.insertFilament(filament.value("id").toInteger(), newFilament);
                }
            }
            amsList.append(newAms);
        }
    }
}

void BambuLab::startConnection() {
    //Setup mqtt client
    mqtt->setHostname(hostname);
    mqtt->setPort(port);
    mqtt->setUsername(username);
    mqtt->setPassword(accessCode);
    mqtt->setClientId("PCM 3DP Kiosk Service");


    //TLS Encryption, no cerificate validation
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::AnyProtocol);
    sslConfig.setLocalCertificate(QSslCertificate());
    sslConfig.setPrivateKey(QSslKey());
    sslConfig.setCiphers(QSslConfiguration::supportedCiphers());




    QObject::connect(mqtt, &QMqttClient::connected, this, [this]() {
        qDebug() << "Printer" << name << "connected to MQTT broker";
        emit this->connectionUpdated(true);
        this->connectionStatus = true;

        this->mqtt->subscribe(reportFilter);
    });

    QObject::connect(mqtt, &QMqttClient::disconnected, this, [this]() {
        qDebug() << "Printer" << name << "disconnected from MQTT broker";
        qDebug() << "State:" << mqtt->state();
        qDebug() << "Error:" << mqtt->error();
        emit this->connectionUpdated(false);
        this->connectionStatus = false;

    });

    QObject::connect(mqtt, &QMqttClient::messageReceived, this, [this](const QByteArray &message, const QMqttTopicName &topic) {
        emit messageRecieved(message, topic);
        if (this->reportFilter.match(topic)) {
            latestReportBytes = message;
            if (!requestTopic.isValid()) {
                QStringList topicparts = topic.name().split("/");
                topicparts.pop_back();
                virtualSN = QString(topicparts.back()); //Copy virtual SN of printer
                emit setVSN(virtualSN);
                topicparts.append("request");
                requestTopic = QMqttTopicName(topicparts.join("/"));
                mqtt->subscribe(requestFiler);
                updateState();
                //qDebug() << "LatestReport: "<<latestReport;
            }
        } else if (this->requestFiler.match(topic)) {
            qDebug() << "Response recieved on topic" << topic.name() << ":" << message;
        } else {
            qDebug() << "Message received on topic" << topic.name() << ":" << message;
        }
    });

    mqtt->connectToHostEncrypted(sslConfig);
}

void BambuLab::setHostname(QString hostname) {
    this->hostname = hostname;
}

void BambuLab::setAccessCode(QString accessCode) {
    this->accessCode = accessCode;
}

void BambuLab::startPrint(const QString &filePath) {
    QFileInfo fInfo = QFileInfo(filePath);
    if (fInfo.fileName().endsWith(".gcode.3mf")) {
        startPrintProject(filePath);
    } else if (fInfo.fileName().startsWith(".gcode")) {
        startPrintGCode(filePath);
    } else {
        qCritical() << "Invalid print file for bambu";
    }
}

void BambuLab::startPrintGCode(const QString &fileName) {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QObject::connect(ftps, &FtpsClient::finished, this, [this, &fileName](bool success, const QString &error) {
        if (success) {
            QJsonObject request{
                {"print", QJsonObject {
                    {"sequence_id", QString::number(this->sequenceId)},
                    {"command", "gcode_file"},
                    {"param", storageType + "/" + QFileInfo(fileName).fileName()}
                }}
            };
            this->mqtt->publish(requestTopic, QJsonDocument(request).toJson(QJsonDocument::Compact));
            this->sequenceId++;
            qDebug() <<"Sending print request"<< QJsonDocument(request).toJson(QJsonDocument::Compact);
        } else {
            qCritical() << "FTPS ERROR: " <<error;
        }
    });
    sendGCode(fileName);
}

void BambuLab::startPrintProject(const QString &fileName) {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QObject::connect(ftps, &FtpsClient::finished, this, [this, &fileName](bool success, const QString &error) {
        if (success) {
            BambuPrintOptions opt(QFileInfo(fileName).fileName());
            opt.setAmsMapping(QList<qint8>{3});
            requestPrintProject(opt);
        } else {
            qCritical() << "FTPS ERROR:" << error;
        }
    });
    sendGCode(fileName);
}

void BambuLab::setStorageType(const QString &storage) {
    this->storageType = storage;
}

void BambuLab::requestPrintProject(const BambuPrintOptions &options) {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QJsonObject request{
        {"print", QJsonObject {
                              {"sequence_id", QString::number(this->sequenceId)},
                              {"command", "project_file"},
                              {"param", "Metadata/plate_" + QString::number(options.plateNum) + ".gcode"},
                              {"project_id", "0"},
                              {"profile_id", "0"},
                              {"task_id", "0"},
                              {"subtask_id", "0"},
                              {"subtask_name", ""},
                              {"timelapse", options.timelapse},
                              {"bed_type", "auto"},
                              {"bed_levelling", options.bedLeveling},
                              {"flow_cali", options.flowCali},
                              {"vibration_cali", options.vibroCali},
                              {"layer_inspect", options.layerInspect},
                              {"use_ams", options.useAms},
                              {"ams_mapping", options.amsMapping},
                              {"file", ""},
                              {"url", "file:///mnt/"+options.storageType+"/"+options.fileName},
                              {"md5", ""}
        }}
    };
    this->mqtt->publish(requestTopic, QJsonDocument(request).toJson(QJsonDocument::Compact));
    this->sequenceId++;
    qDebug() <<"Sending print request"<< QJsonDocument(request);
}

void BambuLab::sendGCode(QString filepath) {

    // //Read gcode file
    QFileInfo fileInfo(filepath);
    // QFile file = QFile(filepath);
    // if (!file.open(QIODevice::ReadOnly)) {
    //     qWarning() << "Cannot open file for upload:" << filepath;
    //     return;
    // }
    // QByteArray fileData = file.readAll();
    // file.close();

    ftps->uploadFile(filepath, hostname, username, accessCode, "/"+fileInfo.fileName());
}

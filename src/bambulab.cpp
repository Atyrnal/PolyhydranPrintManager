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
#include "headers/errors.hpp"

BambuLab::BambuLab(QObject* parent) : Printer(parent), mqtt() {}

BambuLab::BambuLab(QString name, QString model, QString hostname, QString accessCode, QString username, quint16 port, QObject* parent) : Printer(name, model, "BambuLab", parent) {
    this->hostname = hostname;
    this->accessCode = accessCode;
    this->username = username;
    this->port = port;

    mqtt = new QMqttClient();
    ftps = new FtpsClient();

    Log::write("BambuLabPrinter", "Connecting to printer " + name + "(" + model + ")@" + hostname);
    loadCertificate(&BambuLab::startConnection);
}

template<typename Func>
void BambuLab::loadCertificate(Func callback) {
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Fetching printer certificate information");
    QSslSocket* socket = new QSslSocket(this);
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    connect(socket, &QSslSocket::encrypted, this, [=](){
        QList<QSslCertificate> chain = socket->peerCertificateChain();
        if (chain.size() >= 2) {
            QSslCertificate cert = chain[0]; // leaf cert, not the CA
            virtualSN = cert.subjectInfo(QSslCertificate::CommonName).at(0);
            QSslCertificate caCert = chain[1];
            certificate = caCert;
            Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Loaded certificates; Serial number: " + virtualSN);
            (this->*callback)();
        } else {
            Error::handle("BambuCertFetchError", "SSL Chain for printer " + name + " too short", El::Warning);
        }
        socket->deleteLater();
    });
    socket->connectToHostEncrypted(hostname, port);
}

void BambuLab::startConnection() {
    //Setup mqtt client
    mqtt->setHostname(hostname);
    mqtt->setPort(port);
    mqtt->setUsername(username);
    mqtt->setPassword(accessCode);
    mqtt->setProtocolVersion(QMqttClient::MQTT_3_1_1);
    mqtt->setCleanSession(true);
    mqtt->setKeepAlive(60);
    mqtt->setClientId("POLYHYDRAN Print Manager");

    //TLS Encryption, cerificate validation
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    sslConfig.setCaCertificates({certificate});

    reportFilter.setFilter(reportFilter.filter().replace("+", virtualSN));
    requestTopic = QMqttTopicName{QString("device/%1/request").arg(virtualSN)};

    QObject::connect(mqtt, &QMqttClient::connected, this, [this]() {
        Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Connected to printer MQTT");
        emit this->connectionUpdated(true);
        this->connectionStatus = true;

        this->mqtt->subscribe(reportFilter);
        isReady = true;
        emit this->ready();
    });

    QObject::connect(mqtt, &QMqttClient::disconnected, this, [this]() {
        Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Disconnected from printer MQTT");
        //qDebug() << "State:" << mqtt->state();
        if (mqtt->error() > 0) Error::handle("BambuLabPrinterMqttError", "Mqtt connection errored for printer " + name + ": " + QString(static_cast<quint16>(mqtt->error())));
        emit this->connectionUpdated(false);
        this->connectionStatus = false;
    });

    QObject::connect(mqtt, &QMqttClient::messageReceived, this, [this](const QByteArray &message, const QMqttTopicName &topic) {
        emit messageRecieved(message, topic);
        if (this->reportFilter.match(topic)) {
            latestReportBytes = message;
        } else {
            Error::handle("BambuLabPrinterMqttError", "Message recieved on unknown topic for printer " + name + ": " + topic.name());
        }
    });

    //qDebug() << reportFilter << "\n" << requestFilter;

    mqtt->connectToHostEncrypted(sslConfig);
    qobject_cast<QSslSocket*>(mqtt->transport())->setPeerVerifyName(virtualSN); //Set CommonName to the serial number to match certificate
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
        Error::handle("BambuLabPrintError", "File has invalid type: " + filePath, El::Warning);
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
            Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Sent gcode print command");
            //qDebug() <<"Sending print request"<< QJsonDocument(request).toJson(QJsonDocument::Compact);
        } else {
            Error::handle("BambuLabFtpsError", error, El::Critical);
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
            opt.setAmsMapping(QList<qint8>{3});//TODO: Make ams mappings dynamic for prints uploaded directly, for slicer prints steal them from the slicer's mqtt request to the emulator
            requestPrintProject(opt);
        } else {
            Error::handle("BambuLabFtpsError", error, El::Critical);
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
            {"sequence_id", QString::number(this->sequenceId++)},
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
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Sent project print command");
    //qDebug() <<"Sending print request"<< QJsonDocument(request);
}

void BambuLab::sendGCode(QString filepath) {
    QFileInfo fileInfo(filepath);
    ftps->uploadFile(filepath, hostname, username, accessCode, "/"+fileInfo.fileName());
}


void BambuLab::updateState() {
    QJsonParseError err = QJsonParseError();
    QJsonDocument doc = QJsonDocument::fromJson(latestReportBytes, &err);
    if (err.error != QJsonParseError::NoError) {
        Error::handle("JsonParseError", err.errorString(), El::Warning);
        return;
    }
    if (!doc.isObject()) {
        Error::handle("JsonParseError", "Bambu state report is not JSON Object", El::Warning);
        return;
    }
    latestReport = doc.object();


    QJsonObject amsInfo = latestReport.value("print").toObject().value("ams").toObject();
    if (amsInfo.value("ams_exist_bits") == "1") {
        this->hasAms = true;
        amsList.clear();
        const QJsonArray amsList2 = amsInfo.value("ams").toArray();
        for (const QJsonValueConstRef &amsRef : amsList2) {
            QJsonObject amsState = amsRef.toObject();
            BambuAms newAms = BambuAms(amsState.value("id").toInteger());
            const QJsonArray amsTray = amsState.value("tray").toArray();
            for (const QJsonValueConstRef &filamentRef : amsTray) {
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

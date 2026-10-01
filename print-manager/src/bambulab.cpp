/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "bambulab.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslCipher>
#include <QSslKey>
#include "errors.hpp"
#include <QTimer>


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

Printer::JobStatus BambuLab::getJobStatus() {
    if (latestReport.isEmpty() || !connectionStatus) return Printer::JobStatus::Error;
    return this->jobStatus;
}

template<typename Func>
void BambuLab::loadCertificate(Func callback) {
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Fetching printer certificate information");
    QSslSocket* socket = new QSslSocket(this);
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    connect(socket, &QSslSocket::encrypted, this, [=, this](){
        QList<QSslCertificate> chain = socket->peerCertificateChain();
        if (chain.size() >= 2) {
            QSslCertificate cert = chain[0]; // leaf cert, not the CA
            virtualSN = cert.subjectInfo(QSslCertificate::CommonName).at(0);
            QSslCertificate caCert = chain[1];
            certificate = caCert;
            Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Loaded certificates; Serial number: " + virtualSN);
            (this->*callback)();
        } else {
            emit this->connectionUpdated(false);
            Error::handle("BambuCertFetchError", "SSL Chain for printer " + name + " too short", El::Warning);
        }
        socket->deleteLater();
    });
    socket->connectToHostEncrypted(hostname, port);
    QTimer::singleShot(10000, this, [this, socket](){
        if (certificate.isNull() && !socket->isEncrypted()) {
            emit this->connectionUpdated(false);
            Error::handle("BambuLabPrinterConnectionError", "Unable to connect to printer " + name, El::Warning);
        }
    });
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
        connectedOnce = true;
        Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Connected to printer MQTT");
        emit this->connectionUpdated(true);
        this->connectionStatus = true;

        this->mqtt->subscribe(reportFilter);
        if (!isReady) {
            isReady = true;
            emit this->ready();
        }
        requestPushall();
    });

    QObject::connect(mqtt, &QMqttClient::disconnected, this, [this]() {
        if (connectedOnce) {
            if (connectionStatus) Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Disconnected from printer MQTT");
            QTimer::singleShot(10000, this, [this](){
                this->reconnect();
            });
        } else {
            Error::handle("BambuLabPrinterConnectionError", "Unable to connect to printer " + name, El::Warning);
        }
        emit this->connectionUpdated(false);
        this->connectionStatus = false;
        if (mqtt->error() > 0) Error::handle("BambuLabPrinterMqttError", "Mqtt connection errored for printer " + name + ": " + QString::number(static_cast<quint16>(mqtt->error())));
    });

    QObject::connect(mqtt, &QMqttClient::messageReceived, this, [this](const QByteArray &message, const QMqttTopicName &topic) {
        emit messageRecieved(message, topic);
        if (this->reportFilter.match(topic)) { //TODO: Pushall vs push_status check //Should be good now with report merging?
            updateState(message);
            //Log::write("BambuLabPrinter("+name+"@"+hostname+")", QJsonDocument(latestReport).toJson());
        } else {
            Error::handle("BambuLabPrinterMqttError", "Message recieved on unknown topic for printer " + name + ": " + topic.name());
        }
    });

    //qDebug() << reportFilter << "\n" << requestFilter;

    mqtt->connectToHostEncrypted(sslConfig);
    qobject_cast<QSslSocket*>(mqtt->transport())->setPeerVerifyName(virtualSN); //Set CommonName to the serial number to match certificate
}

void BambuLab::reconnect() {
    //Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Attempting to reconnect...");
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    sslConfig.setCaCertificates({certificate});
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
        Error::handle("BambuLabPrintError", "Attempted to start project print with no options", El::Warning);
    } else if (fInfo.fileName().startsWith(".gcode")) {
        startPrintGCode(filePath);
    } else {
        Error::handle("BambuLabPrintError", "File has invalid type: " + filePath, El::Warning);
    }
}

void BambuLab::startPrint(const QString &filePath, BambuPrintOptions opt) {
    QFileInfo fInfo = QFileInfo(filePath);
    if (fInfo.fileName().endsWith(".gcode.3mf")) {
        startPrintProject(filePath, opt);
    } else if (fInfo.fileName().startsWith(".gcode")) {
        Error::softHandle("BambuLabPrintError", "Provided print options when printing non-project gcode file", El::Debug);
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

void BambuLab::startPrintProject(const QString &fileName, const BambuPrintOptions &opt) {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QObject::connect(ftps, &FtpsClient::finished, this, [this, fileName, opt](bool success, const QString &error) {
        if (success) {
            // BambuPrintOptions opt(QFileInfo(fileName).fileName());
            //opt.setAmsMapping(QList<qint8>{3});//TODO: Make ams mappings dynamic for prints uploaded directly, for slicer prints steal them from the slicer's mqtt request to the emulator
            requestPrintProject(opt);
        } else {
            Error::handle("BambuLabFtpsError", error, El::Critical);
        }
    }, Qt::SingleShotConnection);
    sendGCode(fileName);
}

void BambuLab::setStorageType(const QString &storage) {
    this->storageType = storage;
}

void BambuLab::requestPushall() {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QJsonObject parameters {
        {"sequence_id", QString::number(this->sequenceId++)},
        {"command", "pushall"},
        {"version", 1},
        {"push_target", 1}
    };
    QJsonObject request{
        {"pushing", parameters}
    };
    this->mqtt->publish(requestTopic, QJsonDocument(request).toJson(QJsonDocument::Compact));
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Sent pushall command");
}

void BambuLab::requestPrintProject(const BambuPrintOptions &options) {
    if (!connectionStatus) return;
    if (!requestTopic.isValid()) return;
    QJsonObject parameters {
        {"sequence_id", QString::number(this->sequenceId++)},
        {"command", "project_file"},
        {"param", "Metadata/plate_" + QString::number(options.plateNum) + ".gcode"},
        {"project_id", "0"},
        {"profile_id", "0"},
        {"task_id", "0"},
        {"subtask_id", "0"},
        {"subtask_name", ""},
        {"timelapse", options.timelapse},
        {"bed_type", options.bedType},
        {"bed_leveling", options.bedLeveling},
        {"flow_cali", options.flowCali},
        {"vibration_cali", options.vibroCali},
        {"layer_inspect", options.layerInspect},
        {"use_ams", options.useAms},
        {"ams_mapping", options.amsMapping},
        {"file", options.fileName},
        {"url", "ftp://"+options.fileName},
        {"md5", ""},
        {"auto_bed_leveling",options.autoLeveling},
        {"extrude_cali_flag", options.extrudeCali},
        {"nozzle_offset_cali", options.nozzleCali}
    };
    if (options.amsMapping2.size() > 0) parameters.insert("ams_mapping2", options.amsMapping2);
    QJsonObject request{
        {"print", parameters}
    };
    this->mqtt->publish(requestTopic, QJsonDocument(request).toJson(QJsonDocument::Compact));
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "Sent project print command");
    //qDebug() <<"Sending print request"<< QJsonDocument(request);
}

void BambuLab::sendGCode(QString filepath) {
    Log::write("BambuLabPrinter("+name+"@"+hostname+")", "SendGCode called");
    QFileInfo fileInfo(filepath);
    ftps->uploadFile(filepath, hostname, username, accessCode, "/"+fileInfo.fileName());
}


QJsonObject mergeObjects(const QJsonObject &base, const QJsonObject &overlay) {
    QJsonObject result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (result.contains(it.key()) && result[it.key()].isObject() && it.value().isObject()) {
            // Recursively merge nested objects
            result[it.key()] = mergeObjects(result[it.key()].toObject(), it.value().toObject());
        } else {
            result[it.key()] = it.value();
        }
    }
    return result;
}

void BambuLab::updateState(QByteArray latestReportBytes) {
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
    latestReport = mergeObjects(latestReport, doc.object()); //Handle P1S partial data

    //Update job status -- Tenative
    QString prev = jobState;
    jobState = latestReport.value("print").toObject({}).value("gcode_state").toString("UNDEFINED");
    if (jobState == "RUNNING" || jobState == "PAUSED" || jobState == "PREPARE") jobStatus = JobStatus::Busy;
    else if (jobState == "IDLE" || jobState == "FAILED" || jobState == "FINISH") jobStatus = JobStatus::Idle;
    else jobStatus = JobStatus::Error;

    if (jobState != prev && prev != "UNDEFINED") { //Switch state
        Log::write("BambuLabPrinter("+name+"@"+hostname+")", QString("jobState updated from %1 to %2").arg(prev, jobState));
        if (jobState == "RUNNING") {
            if (prev == "PAUSED") //update latest print back to ongoing
                emit this->printStatusUpdated("Ongoing");
        } else if (jobState == "PAUSED") {
            if (prev == "RUNNING") //update lastest print to halted
                emit this->printStatusUpdated("Halted");
        } else if (jobState == "IDLE") {
            if (prev == "PAUSED" || prev == "RUNNING" || prev == "PREPARE") //update latest print to failed
                emit this->printStatusUpdated("Failed");
        } else if (jobState == "FAILED") { //update latest print to failed
            emit this->printStatusUpdated("Failed");
        } else if (jobState == "FINISH") { //update latest print to completed
            emit this->printStatusUpdated("Completed");
        }

    }

    //Update amsinfo
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

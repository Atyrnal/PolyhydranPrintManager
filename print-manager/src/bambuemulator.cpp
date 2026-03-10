#include "bambuemulator.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QTcpServer>
#include <QNetworkDatagram>
#include "errors.hpp"
#include <QTimer>
#include <QSslServer>

BambuEmulator::BambuEmulator(QObject* parent) : QObject(parent) {
    startMosquitto();
}

void BambuEmulator::startMosquitto() {
    mosquito = new QProcess(parent());
    mqtt = new QMqttClient();

    #ifdef Q_OS_WIN
    QString mosquitoPath = "C:/Program Files/mosquitto/mosquitto.exe";
    #else
    QString mosquitoPath = "/usr/bin/mosquitto";
    #endif
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath("mosq.conf");

    //Start mosquitto instance
    mosquito->start(mosquitoPath, QStringList() << "-c" << configPath << "-v");
    if (!mosquito->waitForStarted(2000)) {
        Error("BambuEmulatorError", "Failed to start mosquitto", El::Critical).handle();
        return;
    };

    //Console logging
    // QObject::connect(mosquito, &QProcess::readyReadStandardOutput, this, [this](){
    //     qDebug() << "[mosq]" << this->mosquito->readAllStandardOutput();
    // });
    // QObject::connect(mosquito, &QProcess::readyReadStandardError, this, [this](){
    //     qWarning() << "[mosq/err]" << this->mosquito->readAllStandardError();
    // });
    QObject::connect(mosquito, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [](int code, QProcess::ExitStatus){ Error("BambuEmulatorError", "Mosquitto exited " + QString::number(code), El::Critical).handle(); });

    //Mqtt ambassador to moquitto
    mqtt->setHostname("127.0.0.1");
    mqtt->setPort(8883);
    mqtt->setUsername("bblp");
    mqtt->setPassword("00000001");
    mqtt->setClientId("POLYHYDRAN Print Manager");

    //SSL config
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    mqtt->connectToHostEncrypted(sslConfig);


    QObject::connect(mqtt, &QMqttClient::connected, this, [this](){
        mqtt->subscribe(BambuEmulator::requestFilter);
        Log::write("BambuEmulator", "Connected to Mosquitto");
    });

    QObject::connect(mqtt, &QMqttClient::disconnected, this, [this](){
        Log::write("BambuEmulator", "Disconnected to Mosquitto");
    });

    QObject::connect(mqtt, &QMqttClient::messageReceived, this, &BambuEmulator::slicerRequestRecieved);
}

void BambuEmulator::slicerRequestRecieved(const QByteArray &message, const QMqttTopicName &topic) {
    Log::write("BambuEmulator", "Message from slicer on topic " + topic.name());
    if (!BambuEmulator::requestFilter.match(topic)) return;
    QJsonParseError err = QJsonParseError();
    QJsonDocument doc = QJsonDocument::fromJson(message, &err);
    if (err.error != QJsonParseError::NoError) {
        return Error("MqttJsonParseError", err.errorString(), El::Warning).handle();
    }
    if (!doc.isObject()) {
        return Error("MqttJsonParseError", "Message is not Json object", El::Warning).handle();
    }
    QJsonObject msg = doc.object();
    QString vSN = topic.name().split("/").at(1);
    if (printers.contains(vSN)) {
        BambuLab* printer = printers.value(vSN);
        if (msg.contains("print") && msg.value("print").isObject()) { //Handle print request somehow
            QJsonObject print = msg.value("print").toObject();
            if (print.contains("command") && print.value("command").isString()) {
                QString cmd = print.value("command").toString().toLower();
                if (cmd != "gcode_file" && /*cmd != "gcode_line" &&*/ cmd != "project_file") {
                    printer->mqtt->publish(printer->requestTopic, QJsonDocument(msg).toJson(QJsonDocument::Compact));
                } else {
                    Log::write("BambuEmulator", "Print" + cmd + "request recieved from slicer:" + QJsonDocument(msg).toJson(QJsonDocument::Indented));
                }
            } else {
                Error::handle("BambuEmulatorUknownRequestError", QJsonDocument(msg).toJson(QJsonDocument::Indented), El::Warning);
            }
        } else { //Forward request to printer
            printer->mqtt->publish(printer->requestTopic, QJsonDocument(msg).toJson(QJsonDocument::Compact));
        }
    }
}

void BambuEmulator::slicerHandshake(QTcpSocket* socket, BambuLab* printer) {
    QByteArray data = socket->readAll();

    if (data.size() < 6) return;
    if (data[0] != (char)0xa5 || data[1] != (char)0xa5) {
        Error("BambuEmulatorBindingError", "Invalid packet header", El::Warning);
        return;
    }

    // Parse length (little-endian uint16)
    //quint16 length = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + 2));

    // Extract JSON
    QByteArray jsonData = data.mid(4, data.length() - 6); //Strip magic bytes (Fuck you BambuLab eat my ass)
    //qDebug() << "Received:" << jsonData;

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) return Error("BambuEmulatorBindingError", "Device binding packet data is not JSON Object", El::Warning).handle();

    QJsonObject msg = doc.object();
    if (!msg.contains("login") || !msg.value("login").isObject()) return Error("BambuEmulatorBindingError", "Invalid command wrapper").handle();

    QJsonObject login = msg["login"].toObject();
    QString command = login["command"].toString("");
    QString seqId = login["sequence_id"].toString("20000");

    if (command != "detect") return Error("BambuEmulatorBindingError", "Invalid command").handle();
    Log::write("BambuEmulatorBinding", "Recieved binding request for " + printer->name);
    // Respond with printer info
    QJsonObject response;
    response["command"] = "detect";
    response["sequence_id"] = seqId;
    response["id"] = printer->virtualSN;
    response["model"] = printer->modelId; //get from printer
    response["name"] = printer->name;
    response["version"] = printer->firmwareVer;
    response["bind"] = "free";
    response["connect"] = "lan";
    response["dev_cap"] = printer->devCap;

    QJsonObject wrapper;
    wrapper["login"] = response;

    //Send response
    QByteArray jsonDataOut = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    quint16 length = jsonDataOut.size();

    QByteArray packet;
    packet.append((char)0xa5);
    packet.append((char)0xa5);
    uint16_t len = length + 6; //+6 cuz magic bytess
    packet.append(reinterpret_cast<const char*>(&len), 2);
    packet.append(jsonDataOut);
    packet.append((char)0xa7);
    packet.append((char)0xa7);

    socket->write(packet);
    socket->flush();

    //qDebug() << "Sent response:" << jsonDataOut;
    Log::write("BambuEmulatorBinding", "Sent binding response to slicer for " + printer->name);
}

void BambuEmulator::addPrinter(quint32 id, BambuLab* printer) {
    if (printer->isReady) {
        disconnect(printer);
        printers.insert(printer->virtualSN, printer);
        SNs.insert(id, printer->virtualSN);

        Error fperr = fetchPrinterInfo(printer); //Load printer info from port 3000
        if (fperr.isError()) {
            return fperr.handle();
        }

        //Forward messages from printer to slicer
        connect(printer, &BambuLab::messageRecieved, this, [this](const QByteArray &message, const QMqttTopicName &topic) {
            //Forward messages from printer to slicer
            mqtt->publish(topic, message);
        });


        //() << printer->name << id;
        QString virtualIP = QString("127.0.0.%1").arg(id + 2);
        printer->virtualIP = virtualIP;

        QSslServer* tlsServer = new QSslServer(this);
        QTcpServer* tcpServer = new QTcpServer(this);

        //Create servers on ports 3000 and 3002 to emulate device binding behavior
        if (!tlsServer->listen(QHostAddress(virtualIP), printer->bindingPortTLS)) {
            Error("BambuEmulatorServerError", "Failed to listen on" + virtualIP + ":" + QString(printer->bindingPortTLS) ,El::Critical).handle();
            tlsServer->deleteLater();
            tcpServer->deleteLater();
            return;
        }
        if (!tcpServer->listen(QHostAddress(virtualIP), printer->bindingPortTCP)) {
            Error("BambuEmulatorServerError", "Failed to listen on" + virtualIP + ":" + QString(printer->bindingPortTCP) ,El::Critical).handle();
            tlsServer->deleteLater();
            tcpServer->deleteLater();
            return;
        }

        Log::write("BambuEmulatorServer", "Printer " + printer->name + " listening on " + virtualIP);

        connect(tlsServer, &QTcpServer::newConnection, this, [this, tlsServer, printer]() {
            QTcpSocket* socket = tlsServer->nextPendingConnection();
            Log::write("BambuEmulatorBinding", "SSL Connection received for " + printer->name);

            // Wrap in SSL
            QSslSocket* sslSocket = new QSslSocket(this);
            if (!sslSocket->setSocketDescriptor(socket->socketDescriptor())) {
                Error("BambuEmulatorServerError", "Failed to set socket descriptor", El::Critical).handle();
                socket->deleteLater();
                sslSocket->deleteLater();
                return;
            }

            // Configure SSL
            sslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
            sslSocket->setProtocol(QSsl::TlsV1_2OrLater);
            sslSocket->setLocalCertificate("server.crt");
            sslSocket->setPrivateKey("server.key");

            connect(sslSocket, &QSslSocket::encrypted, this, [sslSocket, printer]() {
                Log::write("BambuEmulatorBinding", "SSL handshake complete");
            });

            // connect(sslSocket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            //         [sslSocket](const QList<QSslError>& errors) {
            //             qWarning() << "SSL errors:" << errors;
            //             sslSocket->ignoreSslErrors();
            //         });

            connect(sslSocket, &QSslSocket::readyRead, this, [this, sslSocket, printer]() {
                slicerHandshake(sslSocket, printer);
            });

            connect(sslSocket, &QSslSocket::disconnected, this, [sslSocket, printer]() {
                Log::write("BambuEmulatorBinding", "Binding complete for " + printer->name + " over SSL");
                sslSocket->deleteLater();
            });

            // Start SSL handshake
            sslSocket->startServerEncryption();
        });

        connect(tcpServer, &QTcpServer::newConnection, this, [this, tcpServer, printer]() {
            QTcpSocket* tcpSocket = tcpServer->nextPendingConnection();
            Log::write("BambuEmulatorBinding", "TCP Connection received for " + printer->name);

            connect(tcpSocket, &QTcpSocket::readyRead, this, [this, tcpSocket, printer]() {
                slicerHandshake(tcpSocket, printer);
            });

            connect(tcpSocket, &QSslSocket::disconnected, this, [tcpSocket, printer]() {
                Log::write("BambuEmulatorBinding", "Binding complete for " + printer->name + " over TCP");
                tcpSocket->deleteLater();
            });


        });

        if (printer->model.startsWith("X1") || printer->model.startsWith("H2")) {
            startUDPNotify(printer);
        } else { //P1S
            startSSDPNotify(printer);
        }
    } else { //Wait for VSN set signal, then add
        connect(printer, &BambuLab::ready, this, [this, id, printer]() {
            this->addPrinter(id, printer);
        });
    }
}

void BambuEmulator::startUDPNotify(BambuLab* printer) {
    Log::write("BambuEmulator", "Starting UDP Notifications for " + printer->name);
    QUdpSocket* udp = new QUdpSocket(this);
    if (!udp->bind(QHostAddress(printer->virtualIP), printer->udpBroadcastPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        Error::handle("BambuEmulatorBroadcastError", "UDP Bind error: " + udp->errorString(), El::Warning);
        udp->deleteLater();
        return;
    }
    QString msg2 = \
       "NOTIFY * HTTP/1.1\r\n"
       "Host: 239.255.255.250:1990\r\n"
       "Server: UPnP/1.0\r\n"
       "Location: " + printer->virtualIP + "\r\n"
        "NT: urn:bambulab-com:device:3dprinter:1\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: " + printer->virtualSN + "\r\n"
        "Cache-Control: max-age=1800\r\n"
        "DevModel.bambu.com: " + printer->modelId + "\r\n"
        "DevName.bambu.com: " + printer->name + "\r\n"
        "DevSignal.bambu.com: -48\r\n"
        "DevConnect.bambu.com: lan\r\n"
        "DevBind.bambu.com: free\r\n"
        "Devseclink.bambu.com: secure\r\n"
        "DevInf.bambu.com: wlan0\r\n"
        "DevVersion.bambu.com: " + printer->firmwareVer + "\r\n"
        "DevCap.bambu.com: 1";
    QByteArray notifyMsg = msg2.toUtf8();
    udp->writeDatagram(notifyMsg, QHostAddress("255.255.255.255"), printer->udpBroadcastPort);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() mutable {
        udp->writeDatagram(notifyMsg, QHostAddress("255.255.255.255"), printer->udpBroadcastPort);
    });
    timer->start(5000);
}

void BambuEmulator::startSSDPNotify(BambuLab* printer) {
    Log::write("BambuEmulator", "Starting SSDP Notifications for " + printer->name);
    QUdpSocket* udp = new QUdpSocket(this);
    if (!udp->bind(QHostAddress(printer->virtualIP), printer->ssdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        Error::handle("BambuEmulatorBroadcastError", "UDP Bind error: " + udp->errorString(), El::Warning);
        udp->deleteLater();
        return;
    }
    QString msg2 = \
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "Server: UPnP/1.0\r\n"
        "Location: " + printer->virtualIP + "\r\n"
        "NT: urn:bambulab-com:device:3dprinter:1\r\n"
        "USN: " + printer->virtualSN + "\r\n"
        "Cache-Control: max-age=1800\r\n"
        "DevModel.bambu.com: " + printer->modelId + "\r\n"
        "DevName.bambu.com: " + printer->name + "\r\n"
        "DevSignal.bambu.com: -51\r\n"
        "DevConnect.bambu.com: lan\r\n"
        "DevBind.bambu.com: free\r\n"
        "Devseclink.bambu.com: secure\r\n"
        "DevVersion.bambu.com: " + printer->firmwareVer + "\r\n"
        "DevCap.bambu.com: 1";
    QByteArray notifyMsg = msg2.toUtf8();
    udp->writeDatagram(notifyMsg, QHostAddress("255.255.255.255"), printer->udpBroadcastPort);

    QTimer *timer = new QTimer(this);
    bool useMulticast = false; //Match printer behavior, alternating between true ssdp mutlicast and udp broadcast
    connect(timer, &QTimer::timeout, this, [=]() mutable {
        if (useMulticast) {
            udp->writeDatagram(notifyMsg, QHostAddress("239.255.255.250"), printer->udpMulticastPort);
        } else {
            udp->writeDatagram(notifyMsg, QHostAddress("255.255.255.255"), printer->udpBroadcastPort);
        }
        useMulticast = !useMulticast;
    });
    timer->start(5000);
}

Error BambuEmulator::fetchPrinterInfo(BambuLab* printer) {
    QTcpSocket tcpSocket;

    Log::write("BambuEmulator", "Fetching binding info from " + printer->name + "@" + printer->hostname + ":" + printer->bindingPortTCP);

    tcpSocket.connectToHost(printer->hostname, printer->bindingPortTCP);
    if (!tcpSocket.waitForConnected(5000)) {
        return Error("BambuEmulatorFetchError", "TCP Socket failed to connect", El::Warning);
    }
    // Create detect command
    QJsonObject login;
    login["command"] = "detect";
    login["sequence_id"] = "20000";

    QJsonObject wrapper;
    wrapper["login"] = login;

    QByteArray jsonData = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    quint16 length = jsonData.size();

    QByteArray packet;
    packet.append((char)0xa5);
    packet.append((char)0xa5);
    uint16_t len = length + 6; //+6 cuz magic bytess (printer->model.startsWith("X1") || printer->model.startsWith("H2") ? 6 : 0
    packet.append(reinterpret_cast<const char*>(&len), 2);
    packet.append(jsonData);
    packet.append((char)0xa7);
    packet.append((char)0xa7);

    tcpSocket.write(packet);
    tcpSocket.flush();

    //qDebug() << "Sent detect command";

    // Wait for response
    if (!tcpSocket.waitForReadyRead(5000)) {
        return Error("BambuEmulatorFetchError", "No response from printer " + printer->name, El::Warning);
    }

    QByteArray response = tcpSocket.readAll();
    //qDebug() << "Received" << response.size() << "bytes";

    if (response.size() < 6) {
        return Error("BambuEmulatorFetchError", "Response too short", El::Warning);
    }

    if (response[0] != (char)0xa5 || response[1] != (char)0xa5) {
        return Error("BambuEmulatorFetchError", "Invalid response header", El::Warning);
    }

    //quint16 respLength = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(response.constData()));
    QByteArray jsonResponse = response.mid(4, response.length()-6);

    //qDebug() << "Response JSON:" << jsonResponse;

    QJsonDocument doc = QJsonDocument::fromJson(jsonResponse);
    if (!doc.isObject()) {
        return Error("BambuEmulatorFetchError", "Response is not JSON from " + printer->name, El::Warning);
    }

    QJsonObject respObj = doc.object();
    if (!respObj.contains("login")) {
        return Error("BambuEmulatorFetchError", "Response malformed", El::Warning);
    }

    QJsonObject loginResp = respObj["login"].toObject();

    //info.id = loginResp["id"].toString();
    printer->modelId = loginResp["model"].toString();
    //info.name = loginResp["name"].toString();
    printer->firmwareVer = loginResp["version"].toString();
    //info.bind = loginResp["bind"].toString();
    //info.connect = loginResp["connect"].toString();
    printer->devCap = loginResp["dev_cap"].toInt();
    //info.valid = true;

    tcpSocket.disconnectFromHost();
    tcpSocket.deleteLater();
    Log::write("BambuEmulator", "Retrieved binding info for " + printer->name);
    return Error::None();
}

void BambuEmulator::removePrinter(quint32 id) {
    if (!SNs.contains(id)) return;
    QString sn = SNs.value(id);
    disconnect(printers.value(sn));
    printers.remove(sn);
    SNs.remove(id);
}

void BambuEmulator::closing() {
    mosquito->kill();
}

BambuEmulator::~BambuEmulator() {
    mosquito->kill();
}

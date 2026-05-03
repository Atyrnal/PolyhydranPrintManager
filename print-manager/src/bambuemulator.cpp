#include "bambuemulator.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QSslServer>
#include <QTcpServer>
#include <QNetworkDatagram>
#include "errors.hpp"
#include <QTimer>
#include <QSslKey>
#include <gcodeparser.h>

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
        Error("BambuEmulatorError", "Failed to start mosquitto", El::Warning).handle();
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
    //Log::write("BambuEmulator", "Message from slicer on topic " + topic.name());
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
        //Log::write("BambuEmulator", "Slicer Request: " + QJsonDocument(msg).toJson());
        if (msg.contains("print") && msg.value("print").isObject()) { //Handle print request somehow
            QJsonObject print = msg.value("print").toObject();
            if (print.contains("command") && print.value("command").isString()) {
                QString cmd = print.value("command").toString().toLower();
                if (cmd != "gcode_file" && /*cmd != "gcode_line" &&*/ cmd != "project_file") {
                    printer->mqtt->publish(printer->requestTopic, QJsonDocument(msg).toJson(QJsonDocument::Compact));
                } else {
                    //Log::write("BambuEmulator", "Print" + cmd + "request recieved from slicer:" + QJsonDocument(msg).toJson(QJsonDocument::Indented));
                    if (cmd == "project_file") {
                        BambuPrintOptions opt = BambuPrintOptions::fromMqtt(print);
                        QString filepath = "uploaded/"+opt.fileName;
                        auto propsEo = GCodeParser::parse3mfFile(filepath, opt.plateNum);
                        if (propsEo.isError()) return propsEo.handle();
                        auto props = propsEo.get();
                        props.insert("filename", opt.fileName);
                        printer->loadedOptions = opt;
                        Log::write("BambuEmulator", "Recieved print project request: " + opt.fileName);
                        emit jobLoaded(ids.value(printer->virtualSN), filepath, props);
                    } else {
                        QString filename = print.value("param").toString().replace("\\", "/").split("/").last();
                        QString filepath = "uploaded/"+filename;
                        auto propsEo = GCodeParser::parseFile(filepath);
                        if (propsEo.isError()) return propsEo.handle();
                        auto props = propsEo.get();
                        props.insert("filename", filename);
                        Log::write("BambuEmulator", "Recieved print file request: " + filename);
                        emit jobLoaded(ids.value(printer->virtualSN), filepath, props);
                    }

                }
            } else {
                Error::handle("BambuEmulatorUknownRequestError", QJsonDocument(msg).toJson(QJsonDocument::Indented), El::Warning);
            }
        } else { //Forward request to printer
            printer->mqtt->publish(printer->requestTopic, QJsonDocument(msg).toJson(QJsonDocument::Compact));
        }
    }
}

void BambuEmulator::ftpsController(QTcpSocket* socket, BambuLab* printer) {
    QByteArray data = socket->readAll();
    QString cmd = QString::fromUtf8(data).trimmed();
    //Log::write("BambuEmulatorFTPS", cmd);
    bool quit = false;
    if (cmd.startsWith("USER")) {
        socket->write("331 Password required\r\n");
    } else if (cmd.startsWith("PASS")) {
        socket->write("230 Login successful\r\n");
    } else if (cmd.startsWith("FEAT")) {
        socket->write("211-Features:\r\n UTF8\r\n211 End\r\n");
    } else if (cmd.startsWith("PWD")) {
        socket->write("257 \"/\" is current directory\r\n");
    } else if (cmd.startsWith("TYPE")) {
        socket->write("200 Type set\r\n");
    } else if (cmd.startsWith("PBSZ")) {
        socket->write("200 PBSZ=0\r\n");
    } else if (cmd.startsWith("PROT")) {
        socket->write("200 Protection level set\r\n");
    } else if (cmd.startsWith("EPSV")) {
        recieveFile(socket, printer);
        socket->write(QString("229 Entering Extended Passive Mode (|||%1|)\r\n").arg(printer->ftpsPort).toUtf8());
    } else if (cmd.startsWith("PASV")) {
        quint32 ip = QHostAddress(printer->virtualIP).toIPv4Address();
        socket->write(QString("227 Entering Passive Mode (%1,%2,%3,%4,%5,%6)\r\n")
          .arg((ip >> 24) & 0xFF)
          .arg((ip >> 16) & 0xFF)
          .arg((ip >> 8) & 0xFF)
          .arg(ip & 0xFF)
          .arg(printer->ftpsPort / 256)
          .arg(printer->ftpsPort % 256).toUtf8());
    } else if (cmd.startsWith("STOR")) {
        printer->filename = cmd.mid(5).trimmed();
        socket->write("150 Opening data connection\r\n");
    } else if (cmd.startsWith("QUIT")) {
        socket->write("221 Goodbye\r\n");
    } else {
        socket->write("502 Command not implemented\r\n");
    }
    socket->flush();
    if (quit) {
        socket->disconnectFromHost();
    }
}

void BambuEmulator::recieveFile(QTcpSocket* controlSocket, BambuLab* printer) {
    QSslServer* ftpsServer = new QSslServer(this);

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(QSslCertificate::fromPath("server.crt").at(0));
    QFile keyFile("server.key");
    auto p = keyFile.open(QIODevice::ReadOnly);
    if (!p) {
        ftpsServer->deleteLater();
        return Error("BambuEmulatorFTPS", "Unable to open private key file", El::Critical).handle();
    }
    config.setPrivateKey(QSslKey(&keyFile, QSsl::Rsa));
    keyFile.close();
    ftpsServer->setSslConfiguration(config);

    connect(ftpsServer, &QSslServer::pendingConnectionAvailable, this, [this, ftpsServer, printer, controlSocket]() {
        auto socket = ftpsServer->nextPendingConnection();
        if (!socket) return;
        QByteArray* fileBuffer = new QByteArray();
        //Log::write("BambuEmulatorFTPS", "FTPS fileserver connection received for " + printer->name);

        connect(socket, &QSslSocket::readyRead, this, [this, socket, printer, fileBuffer]() {
            fileBuffer->append(socket->readAll());
            //Log::write("BambuEmulatorFTPS", "Received " + QString::number(fileBuffer->size()) + " bytes total");
        });

        connect(socket, &QSslSocket::disconnected, this, [socket, printer, fileBuffer, controlSocket, ftpsServer]() {
            //Log::write("BambuEmulatorFTPS", "FTPS fileserver disconnected for " + printer->name);
            if (controlSocket == nullptr) Error("BambuEmulatorFTPSError", "Invalid pointer to control socket", El::Critical).handle();
            else controlSocket->write("226 Transfer complete\r\n");
            if (printer->filename.toLower() != "verify_job") {
                QDir uploadDir = QDir("uploaded");
                if (!uploadDir.exists()) {
                    uploadDir.mkpath(".");
                }
                QFile tmpgcode("uploaded/" + printer->filename);
                if(!tmpgcode.open(QIODevice::WriteOnly)) Error("BambuEmulatorFTPSError", "Unable to save recieved gcode file", El::Critical).handle();
                tmpgcode.write(*fileBuffer);
                tmpgcode.close();
                Log::write("BambuEmulatorFTPS", "Saved " + printer->filename + " successfully");
            }
            delete fileBuffer;
            socket->deleteLater();
            ftpsServer->deleteLater();
        });
    });

    if (!ftpsServer->listen(QHostAddress(printer->virtualIP), printer->ftpsPort)) {
        Error("BambuEmulatorServerError", "Failed to listen on " + printer->virtualIP + ":" + QString::number(printer->ftpsControlPort) ,El::Critical).handle();
        ftpsServer->deleteLater();
        return;
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
    //Log::write("BambuEmulatorBinding", "Sent binding response to slicer for " + printer->name);
}

void BambuEmulator::addPrinter(quint32 id, BambuLab* printer) {
    if (printer->isReady) {
        disconnect(printer);
        printers.insert(printer->virtualSN, printer);
        SNs.insert(id, printer->virtualSN);
        ids.insert(printer->virtualSN, id);

        Error fperr = fetchPrinterInfo(printer); //Load printer info from port 3000
        if (fperr.isError()) {
            return fperr.handle();
        }

        QString virtualIP = QString("127.0.0.%1").arg(id + 2);
        printer->virtualIP = virtualIP;

        //Forward messages from printer to slicer
        connect(printer, &BambuLab::messageRecieved, this, [this, printer](QByteArray message, const QMqttTopicName &topic) {
            auto ipToUint32LE = [](const QString& ip) -> quint32 {
                QHostAddress addr(ip);
                return qFromBigEndian(qToLittleEndian(addr.toIPv4Address())); //MORE BAMBULAB TRICKERY!!!!!!!!!!!!
            };

            //FUUUUUUUUCCKKKKK YOU BAMBULAB YOU SNEAKY SONS OF BITCHES YOU THINK YOU CAN OUTSMART ME???????????????? 我操你妈的
            const quint32 realIp = ipToUint32LE(printer->hostname);
            const quint32 fakeIp = ipToUint32LE(printer->virtualIP);
            //Log::write("BambuEmulator", "RealIp:" + QString::number(realIp) + " FakeIp:" + QString::number(fakeIp));
            message.replace(QString::number(realIp).toUtf8(), QString::number(fakeIp).toUtf8());
            message.replace(printer->hostname.toUtf8(), printer->virtualIP.toUtf8()); //Filter out real IP from all requests
            mqtt->publish(topic, message);
        });


        //() << printer->name << id;


        QSslServer* ftpsControlServer = new QSslServer(this);
        QTcpServer* bindingServer = new QTcpServer(this);
        QSslServer* bindingServerSsl = new QSslServer(this);

        QSslConfiguration config = QSslConfiguration::defaultConfiguration();
        config.setLocalCertificate(QSslCertificate::fromPath("server.crt").at(0));
        QFile keyFile("server.key");
        auto p = keyFile.open(QIODevice::ReadOnly);
        if (!p) return Error("BambuEmulatorServerError", "Unable to open private key file", El::Critical).handle();
        config.setPrivateKey(QSslKey(&keyFile, QSsl::Rsa));
        keyFile.close();
        ftpsControlServer->setSslConfiguration(config);
        bindingServerSsl->setSslConfiguration(config);

        //Create server on port 990 to listen for ftps commands
        if (!ftpsControlServer->listen(QHostAddress(virtualIP), printer->ftpsControlPort)) { //THIS NEEDS SUDO "sudo setcap 'cap_net_bind_service=+ep' ./appPolyhydranPrintManager"
            Error("BambuEmulatorServerError", "Failed to listen on " + virtualIP + ":" + QString::number(printer->ftpsControlPort) ,El::Critical).handle();
            ftpsControlServer->deleteLater();
            bindingServer->deleteLater();
            bindingServerSsl->deleteLater();
            return;
        }


        //Create server on ports 3000 to emulate device binding behavior

        if (!bindingServer->listen(QHostAddress(virtualIP), printer->bindingPortTCP)) {
            Error("BambuEmulatorServerError", "Failed to listen on" + virtualIP + ":" + QString::number(printer->bindingPortTCP) ,El::Critical).handle();
            ftpsControlServer->deleteLater();
            bindingServer->deleteLater();
            bindingServerSsl->deleteLater();
            return;
        }

        if (!bindingServerSsl->listen(QHostAddress(virtualIP), printer->bindingPortTLS)) {
            Error("BambuEmulatorServerError", "Failed to listen on" + virtualIP + ":" + QString::number(printer->bindingPortTLS) ,El::Critical).handle();
            ftpsControlServer->deleteLater();
            bindingServer->deleteLater();
            bindingServerSsl->deleteLater();
            return;
        }

        Log::write("BambuEmulatorServer", "Printer " + printer->name + " listening on " + virtualIP);


        connect(bindingServer, &QTcpServer::newConnection, this, [this, bindingServer, printer]() {
            QTcpSocket* tcpSocket = bindingServer->nextPendingConnection();
            //Log::write("BambuEmulatorBinding", "TCP Connection received for " + printer->name);

            connect(tcpSocket, &QTcpSocket::readyRead, this, [this, tcpSocket, printer]() {
                slicerHandshake(tcpSocket, printer);
            });

            connect(tcpSocket, &QSslSocket::disconnected, this, [tcpSocket, printer]() {
                Log::write("BambuEmulatorBinding", "Binding complete for " + printer->name + " over TCP");
                tcpSocket->deleteLater();
            });
        });

        connect(bindingServerSsl, &QSslServer::pendingConnectionAvailable, this, [this, bindingServerSsl, printer]() {
            auto socket = bindingServerSsl->nextPendingConnection();
            if (!socket) return;

            connect(socket, &QSslSocket::readyRead, this, [this, socket, printer]() {
                slicerHandshake(socket, printer);
            });

            connect(socket, &QSslSocket::disconnected, this, [socket, printer](){
                Log::write("BambuEmulatorBinding", "Binding complete for " + printer->name + " over TLS");
                socket->deleteLater();
            });
        });


        connect(ftpsControlServer, &QSslServer::pendingConnectionAvailable, this, [this, ftpsControlServer, printer]() {
            auto socket = ftpsControlServer->nextPendingConnection();
            if (!socket) return;


            //Log::write("BambuEmulatorFTPS", "FTPS Connection received for " + printer->name);
            socket->write("220 BambuLab FTP Service\r\n");
            socket->flush();

            connect(socket, &QSslSocket::readyRead, this, [this, socket, printer]() {
                ftpsController(socket, printer);
            });

            connect(socket, &QSslSocket::disconnected, this, [socket, printer]() {
                //Log::write("BambuEmulatorFTPS", "FTPS Disconnected for " + printer->name);
                socket->deleteLater();
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
    //Log::write("BambuEmulator", "Starting UDP Notifications for " + printer->name);
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
        + ((printer->modelId.startsWith("BL")) ? "DevSignal.bambu.com: -48\r\n" : "") +
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
    //Log::write("BambuEmulator", "Starting SSDP Notifications for " + printer->name);
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

    Log::write("BambuEmulator", "Fetching binding info from " + printer->name + "@" + printer->hostname + ":" + QString::number(printer->bindingPortTCP));

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

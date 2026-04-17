#include "bambulab.h"
#include <QTcpSocket>
#include "errors.hpp"
#include <QTimer>

BambuLab::BambuLab(QString ip, QString accessCode, QObject* parent) : Printer(ip, accessCode, "bblp", parent) {}
BambuLab::BambuLab(QString ip, QString accessCode, QString username, QObject* parent) : Printer(ip, accessCode, username, parent) {}

void BambuLab::testConnection() {
    QTcpSocket* tcpSocket = new QTcpSocket(this);

    tcpSocket->connectToHost(ip, bindingPortTCP);
    connect(tcpSocket, &QTcpSocket::connected, this, [=]() {
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

        tcpSocket->write(packet);
        tcpSocket->flush();

        connect(tcpSocket, &QIODevice::readyRead, this, [=]() {
            QByteArray response = tcpSocket->readAll();
            if (response.size() < 6) {
                Error::handle("BambuLabFetchError", "Response too short", El::Warning);
                return emit connectionTestComplete(false, "Recieved invalid binding response. Ensure your printer is running supported firmware.");
            }
            if (response[0] != (char)0xa5 || response[1] != (char)0xa5) {
                Error::handle("BambuLabFetchError", "Invalid response header", El::Warning);
                return emit connectionTestComplete(false, "Recieved invalid binding response. Ensure your printer is running supported firmware.");
            }
            QByteArray jsonResponse = response.mid(4, response.length()-6);
            QJsonParseError p;
            QJsonDocument doc = QJsonDocument::fromJson(jsonResponse, &p);
            if (p.error) {
                Error::handle("JsonParseError", p.errorString(), El::Warning);
                return emit connectionTestComplete(false, "Recieved invalid binding response. Ensure your printer is running supported firmware.");
            }
            if (!doc.isObject()) {
                Error::handle("BambuLabFetchError", "Response is not JSON", El::Warning);
                return emit connectionTestComplete(false, "Recieved invalid binding response. Ensure your printer is running supported firmware.");
            }

            QJsonObject respObj = doc.object();
            if (!respObj.contains("login") || !respObj.value("login").isObject()) {
                Error::handle("BambuLabFetchError", "Response doesn't match bambu binding schema", El::Warning);
                return emit connectionTestComplete(false, "Recieved invalid binding response. Ensure your printer is running supported firmware.");
            }

            QJsonObject loginResp = respObj["login"].toObject();

            this->serialNumber = loginResp["id"].toString();
            this->modelId = loginResp["model"].toString();
            this->name = loginResp["name"].toString().split("(").at(0).trimmed();
            if (loginResp["connect"].toString() != "lan") emit connectionTestComplete(false, "Failed to connect to printer. Ensure your printer is in LAN mode with developer mode enabled.");

            tcpSocket->disconnectFromHost();
            tcpSocket->deleteLater();
            //now test mqtt

            mqtt = new QMqttClient();

            //Setup mqtt client
            mqtt->setHostname(ip);
            mqtt->setPort(mqttPort);
            mqtt->setUsername(username);
            mqtt->setPassword(password);
            mqtt->setProtocolVersion(QMqttClient::MQTT_3_1_1);
            mqtt->setCleanSession(true);
            mqtt->setKeepAlive(60);
            mqtt->setClientId("POLYHYDRAN Print Manager Config Tool");

            //TLS Encryption, cerificate validation
            QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

            QObject::connect(mqtt, &QMqttClient::connected, this, [this]() {
                return emit connectionTestComplete(true, "Connected to printer.");
                mqtt->disconnect();
                mqtt->deleteLater();
            });

            mqtt->connectToHostEncrypted(sslConfig);
            QTimer::singleShot(3000, this, [this, tcpSocket]() {
                if (!this->testComplete) emit connectionTestComplete(false, "Invalid access code.");
                tcpSocket->disconnectFromHost();
                tcpSocket->deleteLater();
            });

        });
        QTimer::singleShot(3000, this, [this, tcpSocket]() {
            if (!this->testComplete) emit connectionTestComplete(false, "Could not connect to printer.");
            tcpSocket->disconnectFromHost();
            tcpSocket->deleteLater();
        });
    });
    QTimer::singleShot(1000, this, [this, tcpSocket]() {
        if (!this->testComplete) emit connectionTestComplete(false, "Could not connect to network.");
        tcpSocket->deleteLater();
    });
}

QJsonObject BambuLab::generateConfig() {
    QJsonObject output = QJsonObject{
        {"brand", brand},
        {"name", name},
        {"modelId", modelId},
        {"ip", ip},
        {"accessCode", password}
    };
    if (virtualIp != "") output.insert("virtualIp", virtualIp);
    return output;
}

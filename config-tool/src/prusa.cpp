/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "prusa.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include "errors.hpp"
#include <QTimer>


Prusa::Prusa(QString ip, QString password, QObject* parent) : Printer(ip, password, "maker", parent) {}
Prusa::Prusa(QString ip, QString password, QString username, QObject* parent) : Printer(ip, password, username, parent) {}

void Prusa::testConnection() {
    QUrl infoUrl(QString("http://%1/api/info").arg(ip));
    QNetworkRequest infoReq(infoUrl);
    infoReq.setRawHeader("X-Api-Key", password.toUtf8());
    QNetworkReply* infoReply = netman.get(infoReq);
    connect(infoReply, &QNetworkReply::finished, infoReply, [=](){
        enum QNetworkReply::NetworkError err = infoReply->error();
        if (err != QNetworkReply::NoError) {
            Error::softHandle("PrusaPrinterConnectionError", infoReply->errorString(), El::Debug);
            if (err < 200) return emit connectionTestComplete(false, "Unable to connect to printer. Make sure you're using the correct ip address and the printer is on the same network.");
            return emit connectionTestComplete(false, "Access denied. Check that PrusaLink is enabled and your credentials are correct.");
        }
        QJsonParseError p;
        QJsonDocument doc = QJsonDocument::fromJson(infoReply->readAll(), &p);
        if (p.error || doc.isObject()) {
            Error::handle("JsonParseError", p.errorString(), El::Warning);
            return emit connectionTestComplete(false, "Printer response did not match PrusaLink schema.");
        }
        QJsonObject info = doc.object();
        model = (info.contains("name") && info.value("name").isString()) ? info.value("name").toString().replace("Original", "").replace("Prusa", "").trimmed() : "Unknown";
        return emit connectionTestComplete(true, "Connected to printer.");
    });
    QTimer::singleShot(3500, this, [this](){
        if (!this->testComplete) return emit connectionTestComplete(false, "Unable to connect to printer. Make sure you're using the correct ip address and the printer is on the same network.");
    });
}

QJsonObject Prusa::generateConfig() {
    return QJsonObject{
        {"brand", "Prusa"},
        {"model", model},
        {"name", name},
        {"ip", ip},
        {"apiKey", password}
    };
}

/*bool Prusa::testConnection() {




}

*/

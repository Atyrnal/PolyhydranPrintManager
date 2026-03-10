/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#ifndef PRUSA_H
#define PRUSA_H

#include <QObject>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include "printer.h"

class Prusa : public Printer {
    Q_OBJECT
public:
    Prusa(QObject* parent = 0);
    Prusa(QString name, QString model, QString hostname, QString apiKey, QString storageType = "usb", QObject* parent = 0);
    void startPrint(const QString &gcodeFilepath) override;
    void setStorageType(QString storageType);
    void setHostname(QString hostname);
    void setApiKey(QString apiKey);
protected:
    QString hostname;
    QString apiKey;
    QString storageType;
private:
    void sendGCode(QString filepath);
    //bool testConnection();
};


#endif // PRUSA_H

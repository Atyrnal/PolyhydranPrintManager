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
#include <memory>
#include <QTemporaryDir>

class Prusa : public Printer {
    Q_OBJECT
public:
    Prusa(QObject* parent = 0);
    Prusa(QString name, QString model, QString hostname, QString apiKey, QString storageType = "usb", QObject* parent = 0);
    void startPrint(const QString &gcodeFilepath) override;
    void setStorageType(QString storageType);
    void setHostname(QString hostname);
    void setApiKey(QString apiKey);
    Printer::JobStatus getJobStatus() override;
protected:
    QString apiKey;
    QString storageType;
    bool useBinaryGCode = true;
    void testConnection();
private:
    Printer::JobStatus jobStatus;
    QString jobState = "UNDEFINED";
    bool connectedOnce = false;
    void sendGCode(QString filepath);
    QString convertToBgcode(const QString &src, const QString &dst);
    void uploadFile(const QString &localPath, std::shared_ptr<QTemporaryDir> tmp = nullptr);
    qint8 connectFailCount = 0;
    
    //bool testConnection();
};


#endif // PRUSA_H

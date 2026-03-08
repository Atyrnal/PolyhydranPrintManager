/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#ifndef BAMBULAB_H
#define BAMBULAB_H

#include <QObject>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QtMqtt/QMqttClient>
#include <QJsonObject>
#include <QJsonArray>
#include "headers/ftpsclient.h"
#include "printer.h"
#include <QColor>

#define minVal(x,y) ((x < y) ? x : y)

class BambuPrintOptions {
public:
    BambuPrintOptions() {};
    BambuPrintOptions(const QString &fileName) {this->fileName = fileName;};
    QString fileName;
    bool timelapse = false;
    quint16 plateNum = 1;
    QString storageType = "sdcard";
    bool bedLeveling = true;
    bool flowCali = true;
    bool vibroCali = true;
    bool layerInspect = true;
    bool useAms = false;
    QJsonArray amsMapping = QJsonArray{-1, -1, -1, -1, -1};
    void setAmsMapping(QList<qint8> filamentIds) {
        /*if (filamentIds.size() < 1) {
            useAms = false;
            return;
        }
        QJsonArray mapping {-1, -1, -1, -1, -1};
        for (int i = 0; i < min(filamentIds.size(), 5); i++) {
            mapping[4-i] = filamentIds[i];
        }
        amsMapping = mapping;
        useAms = true;*/
        if (filamentIds.size() < 1) {
            useAms = false;
            return;
        }
        QJsonArray mapping {};
        for (int i = 0; i < minVal(filamentIds.size(), 24); i++) {
            mapping.append(filamentIds[i]);
        }
        amsMapping = mapping;
        useAms = true;
    }
};

class BambuAmsFilament {
public:
    BambuAmsFilament();
    BambuAmsFilament(QString material, QString colorString) {
        this->materialType = material;
        this->color = QColor("#"+colorString);
    }
    BambuAmsFilament(QString material, QString colorString, quint32 minTemp, quint32 maxTemp) {
        this->materialType = material;
        this->color = QColor("#"+colorString);
        this->minTemp = minTemp;
        this->maxTemp = maxTemp;
    }
    QColor color;
    QString materialType;
    quint32 maxTemp;
    quint32 minTemp;
};

class BambuAms {
public:
    BambuAms(quint64 id) {tray = QMap<quint32,BambuAmsFilament>(); this->id = id;};
    BambuAms() {tray = QMap<quint32,BambuAmsFilament>();};
    QMap<quint32, BambuAmsFilament> tray;
    quint64 id;
    quint32 capacity = 4;
    void insertFilament(quint32 id, BambuAmsFilament f) {
        tray.insert(id, f);
    }
    void removeFilament(quint32 id) {
        tray.remove(id);
    }
    BambuAmsFilament getFilament(quint32 id) {
        return tray[id];
    }
};


class BambuLab : public Printer {
    Q_OBJECT
public:
    BambuLab(QObject* parent = 0);
    BambuLab(QString name, QString model, QString hostname, QString accessCode, QString username = "bblp", quint16 port = 8883, QObject* parent = 0);
    void startPrint(const QString &filePath) override;
    void startConnection();
    void setHostname(QString hostname);
    void setAccessCode(QString accessCode);
    void setStorageType(const QString &storage);
signals:
    void messageRecieved(const QByteArray &message, const QMqttTopicName &topic);
    //void setVSN(const QString &vSN);
    void ready();
    void certLoaded();
protected:
    QString hostname;
    QString accessCode;
    quint16 port;
    QString username;
    QString modelId = "C12";
    QString firmwareVer = "01.09.00.00";
    int devCap = 1;
    bool hasAms = false;
    QList<BambuAms> amsList;
    void requestPrintProject(const BambuPrintOptions &options);
    void startPrintGCode(const QString &gcodeFilepath);
    void startPrintProject(const QString &projFilepath);
private:
    void updateState();
    bool isReady = false;
    QString virtualIP;
    QString virtualSN /*= "undefined"*/;
    quint32 sequenceId = 0;
    QMqttTopicName requestTopic;
    QByteArray latestReportBytes;
    QJsonObject latestReport;
    quint16 bindingPortTCP = 3000;
    quint16 bindingPortTLS = 3002;
    QMqttClient* mqtt;
    FtpsClient* ftps;
    QString storageType = "sdcard"; //"sdcard", "internal"
    QSslCertificate certificate;
    QMqttTopicFilter reportFilter {"device/+/report"};
    //QMqttTopicFilter requestFilter {"device/+/request"};
    void sendGCode(QString filepath);
    template<typename Func>
    void loadCertificate(Func callback);
    friend class BambuEmulator;
    friend class PrinterManager;
    //bool testConnection();
};

#endif // BAMBULAB_H

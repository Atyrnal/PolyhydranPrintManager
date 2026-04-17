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
#include "ftpsclient.h"
#include "printer.h"
#include <QColor>
#include <QMap>

#define minVal(x,y) ((x < y) ? x : y)

class BambuPrintOptions {
public:
    BambuPrintOptions() {};
    BambuPrintOptions(const QString &fileName) {this->fileName = fileName;};
    static const inline QMap<QString, QString> modelIds = QMap<QString, QString>{
        {"BL-P001", "X1C"},
        {"BL-P002", "X1"},
        {"C13",     "X1E"},
        {"C11",     "P1P"},
        {"C12",     "P1S"},
        {"O1S",     "H2S"},
        {"O1D",     "H2D"},
        {"O1C2",    "H2C"},
        {"O1E",     "H2D Pro"},
        {"N1",      "A1 Mini"},
        {"N2S",     "A1"},
        {"N7",      "P2S"}
    };
    static BambuPrintOptions fromMqtt(const QJsonObject &printCommand) {
        BambuPrintOptions opt = BambuPrintOptions();
        if (printCommand.contains("timelapse") && printCommand.value("timelapse").isBool()) opt.timelapse = printCommand.value("timelapse").toBool();
        if (printCommand.contains("bed_leveling") && printCommand.value("bed_leveling").isBool()) opt.bedLeveling = printCommand.value("bed_levelling").toBool();
        if (printCommand.contains("flow_cali") && printCommand.value("flow_cali").isBool()) opt.flowCali = printCommand.value("flow_cali").toBool();
        if (printCommand.contains("vibration_cali") && printCommand.value("vibration_cali").isBool()) opt.vibroCali = printCommand.value("vibration_cali").toBool();
        if (printCommand.contains("layer_inspect") && printCommand.value("layer_inspect").isBool()) opt.layerInspect = printCommand.value("layer_inspect").toBool();
        if (printCommand.contains("use_ams") && printCommand.value("use_ams").isBool()) opt.useAms = printCommand.value("use_ams").toBool();
        if (printCommand.contains("ams_mapping") && printCommand.value("ams_mapping").isArray()) opt.amsMapping = printCommand.value("ams_mapping").toArray();
        if (printCommand.contains("ams_mapping2") && printCommand.value("ams_mapping2").isArray()) opt.amsMapping2 = printCommand.value("ams_mapping2").toArray();
        if (printCommand.contains("param") && printCommand.value("param").isString() && printCommand.value("param").toString().replace("\\", "/").split("/").last().startsWith("plate_") && printCommand.value("param").toString().endsWith(".gcode")) {
            bool ok;
            int value = printCommand.value("param").toString().replace("\\", "/").split("/").last().replace("plate_", "").replace(".gcode", "").toInt(&ok);
            if (ok && value > 0 && value < 1000) opt.plateNum = value;
        }

        if (printCommand.contains("url") && printCommand.value("url").isString() && printCommand.value("url").toString().endsWith(".gcode.3mf")) opt.fileName = printCommand.value("url").toString().replace("\\", "/").split("/").last();
        else if (printCommand.contains("file") && printCommand.value("file").isString() && printCommand.value("file").toString().endsWith(".gcode.3mf") )opt.fileName = printCommand.value("file").toString().replace("\\", "/").split("/").last();
        else opt.fileName = "";
        if (printCommand.contains("bed_type") && printCommand.value("bed_type").isString()) opt.bedType = printCommand.value("bed_type").toString();
        if (printCommand.contains("nozzle_offset_cali") && printCommand.value("nozzle_offset_cali").isDouble()) opt.nozzleCali = printCommand.value("nozzle_offset_cali").toDouble();
        if (printCommand.contains("extrude_cali_flag") && printCommand.value("extrude_cali_flag").isDouble()) opt.extrudeCali = printCommand.value("extrude_cali_flag").toDouble();
        if (printCommand.contains("auto_bed_leveling") && printCommand.value("auto_bed_leveling").isDouble()) opt.autoLeveling = printCommand.value("auto_bed_leveling").toDouble();
        return opt;
    };
    QString fileName;
    bool timelapse = false;
    quint16 plateNum = 1;
    QString storageType = "sdcard";
    QString bedType = "auto";
    bool bedLeveling = true;
    bool flowCali = true;
    bool vibroCali = true;
    bool layerInspect = true;
    bool useAms = false;
    //H2D
    quint8 nozzleCali = 0;
    quint8 extrudeCali = 0;
    quint8 autoLeveling = 0;
    QJsonArray amsMapping = QJsonArray{-1, -1, -1, -1, -1};
    QJsonArray amsMapping2 = QJsonArray{};
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
    void startPrint(const QString &filePath, BambuPrintOptions opt);
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
    QString modelId;
    QString firmwareVer;
    int devCap = 1;
    bool hasAms = false;
    QList<BambuAms> amsList;
    void requestPrintProject(const BambuPrintOptions &options);
    void startPrintGCode(const QString &gcodeFilepath);
    void startPrintProject(const QString &projFilepath, const BambuPrintOptions &opt);
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
    quint16 udpMulticastPort = 1990;
    quint16 udpBroadcastPort = 2021;
    quint16 ssdpPort = 1900;
    quint16 ftpsControlPort = 990;
    quint16 ftpsPort = 50001;
    QMqttClient* mqtt;
    FtpsClient* ftps;
    QString storageType = "sdcard"; //"sdcard", "internal"
    QString filename = "temp";
    QSslCertificate certificate;
    QMqttTopicFilter reportFilter {"device/+/report"};
    BambuPrintOptions loadedOptions = BambuPrintOptions();
    //QMqttTopicFilter requestFilter {"device/+/request"};
    void sendGCode(QString filepath);
    template<typename Func>
    void loadCertificate(Func callback);
    friend class BambuEmulator;
    friend class PrinterManager;
    //bool testConnection();
};

#endif // BAMBULAB_H

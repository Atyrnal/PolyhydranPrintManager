#ifndef GLOBALSTATE_HPP
#define GLOBALSTATE_HPP


#include <QObject>
#include <QMap>
#include <QVariant>
#include <QDir>
#include <QCoreApplication>
#include <qqmlapplicationengine.h>
#include <qqmlengine.h>
#include "frontendmanager.h"
#include "printermanager.h"
#include "airtable.h"
#include "rfidmanager.h"
#include "usermanager.h"
#include "printmanager.h"
#include "osinterface.h"

//Settings macros
#define stnb(sname) GlobalState::instance().getSetting((sname)).toBool()
#define stnbd(sname, def) GlobalState::instance().getSetting((sname), (def)).toBool()
#define stns(sname) GlobalState::instance().getSetting((sname)).toString()
#define stnsd(sname, def) GlobalState::instance().getSetting((sname), (def)).toString()
#define stnd(sname) GlobalState::instance().getSetting((sname)).toDouble()
#define stndd(sname, def) GlobalState::instance().getSetting((sname), (def)).toDouble()
#define stnui(sname) GlobalState::instance().getSetting((sname)).toUInt()
#define stnuid(sname, def) GlobalState::instance().getSetting((sname), (def)).toUInt()
#define stni(sname) GlobalState::instance().getSetting((sname)).toInt()
#define stnid(sname, def) GlobalState::instance().getSetting((sname), (def)).toInt()
#define stnl(sname) GlobalState::instance().getSetting((sname)).toLongLong()
#define stnld(sname, def) GlobalState::instance().getSetting((sname), (def)).toLongLong()

//manager macros
#define ftm GlobalState::instance().getFrontendManager()
#define prm GlobalState::instance().getPrinterManager()
#define ait GlobalState::instance().getAirtableManager()
#define rfd GlobalState::instance().getRfidManager()
#define usm GlobalState::instance().getUserManager()
#define ptm GlobalState::instance().getPrintManager()
#define osi GlobalState::instance().getOsInterface()

//other macros
#define gsi GlobalState::instance()


#define APP_VERSION "0.1.0-beta4"

class GlobalState : public QObject {
    Q_OBJECT
public:
    static GlobalState& instance();

    GlobalState(const GlobalState&) = delete;
    GlobalState& operator=(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    GlobalState& operator=(GlobalState&&) = delete;

    QVariant getSetting(QString sname, QVariant def = QVariant()) const;
    void loadSettings(QMap<QString, QVariant> setc);

    QString getAppVersion() const;
    QString getDataDirPath() const;
    QString getAppDirPath() const;
    QDir getDataDir() const;
    QDir getAppDir() const;
    QCoreApplication* getApp() const;
    QQmlApplicationEngine* getEng() const;
    QObject* getRoot() const;
    FrontendManager* getFrontendManager() const;
    PrinterManager* getPrinterManager() const;
    AirtableBase* getAirtableManager() const;
    RfidManager* getRfidManager() const;
    UserManager* getUserManager() const;
    PrintManager* getPrintManager() const;
    OsInterface* getOsInterface() const;


    void setApp(QCoreApplication* app);
    void setEngine(QQmlApplicationEngine* eng);
    void setRoot(QObject* root);

    void loadConfig(QJsonObject cfg);
    
private:
    explicit GlobalState(QObject *parent = nullptr);
    ~GlobalState();

    //Managers
    FrontendManager* frontman = nullptr;
    PrinterManager* printerman = nullptr;
    AirtableBase* airtableman = nullptr;
    RfidManager* rfidman = nullptr;
    UserManager* userman = nullptr;
    PrintManager* printman = nullptr;
    OsInterface* osint = nullptr;

    //Storage
    QMap<QString, QVariant> settings;
    const QMap<QString, QVariant> defaultSettings {
        {"abortTimeMaxSeconds", 420},
        {"allowPrinting", true},
        {"allowMakerspaceFilament", true},
        {"allowPersonalFilament", true},
        {"bypassCardId", ""},
        {"darkMode", false},
        {"enableGroups", false},
        {"gcodeParsingDebug", false},
        {"groupTableName", "Groups"},
        {"maxFilamentTypeCount", 2},
        {"maxInfillDensity", 0.4},
        {"maxPrintDuration", 6.0},
        {"maxPrintDurationPersonalFilament", 6.0},
        {"maxPrintWeight", 200.0},
        {"maxSimeltaneousPrinters", 2},
        {"maxUploadSizeMB", 110},
        {"minLayerHeight", 0.1},
        {"mosqCaCertFile", "ca.crt"},
        {"mosqServerCertFile", "server.crt"},
        {"mosqServerKeyFile", "server.key"},
        {"mosquittoExec", "/usr/sbin/mosquitto"},
        {"orcaSlicerDir", "/opt/orca-slicer/"},
        {"orcaSlicerExec", "/opt/orca-slicer/AppRun"},
        {"printingCertificateName", "3D Printing"},
        {"printingCertificateTableName", "Certificates"},
        {"prusaConnectionFailThreshold", 2},
        {"requireFilamentTypeCount", true},
        {"requireInfillDensity", true},
        {"requireLayerHeight", true},
        {"requirePrintDuration", true},
        {"requirePrintWeight", true},
        {"requirePrinterUseCount", true},
        {"requireTraining", true},
        {"rfidReaderPortName", "auto"},
        {"updatePrintStatuses", true}
    };
    QString dataDirPath;
    QString appDirPath;
    QCoreApplication* app = nullptr;
    QQmlApplicationEngine* engine = nullptr;
    QObject* qmlroot = nullptr;
    QJsonObject config;

};



#endif // GLOBALSTATE_HPP

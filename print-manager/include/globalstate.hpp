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
#define stnb(sname) GlobalState::instance().getSetting((sname), false).toBool()
#define stnbd(sname, def) GlobalState::instance().getSetting((sname), (def)).toBool()
#define stns(sname) GlobalState::instance().getSetting((sname), "").toString()
#define stnsd(sname, def) GlobalState::instance().getSetting((sname), (def)).toString()
#define stnd(sname) GlobalState::instance().getSetting((sname), 0.0).toDouble()
#define stndd(sname, def) GlobalState::instance().getSetting((sname), (def)).toDouble()
#define stnui(sname) GlobalState::instance().getSetting((sname), 0).toUInt();
#define stnuid(sname, def) GlobalState::instance().getSetting((sname), (def)).toUInt()
#define stni(sname) GlobalState::instance().getSetting((sname), 0).toInt();
#define stnid(sname, def) GlobalState::instance().getSetting((sname), (def)).toInt()

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


#define APP_VERSION "0.1.0-beta1"

class GlobalState : public QObject {
    Q_OBJECT
public:
    static GlobalState& instance();

    GlobalState(const GlobalState&) = delete;
    GlobalState& operator=(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    GlobalState& operator=(GlobalState&&) = delete;

    QVariant getSetting(QString sname, QVariant def) const;
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
    QString dataDirPath;
    QString appDirPath;
    QCoreApplication* app = nullptr;
    QQmlApplicationEngine* engine = nullptr;
    QObject* qmlroot = nullptr;
    QJsonObject config;

};



#endif // GLOBALSTATE_HPP

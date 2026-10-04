#include "globalstate.hpp"
#include "rfidmanager.h"
#ifdef Q_OS_WIN
#include "windowsinterface.h"
#endif
#ifdef Q_OS_LINUX
#include "linuxinterface.h"
#endif
#include <QCoreApplication>

GlobalState& GlobalState::instance() {
    static GlobalState instanced;
    return instanced;
}

GlobalState::GlobalState(QObject *parent) : QObject(parent) {
    dataDirPath = QDir::currentPath();
    appDirPath = QCoreApplication::applicationDirPath();
    frontman = new FrontendManager(this);
    printerman = new PrinterManager(this);
    rfidman = new RfidManager(this);
    userman = new UserManager(this);
    printman = new PrintManager(this);
    #ifdef Q_OS_WIN
    osint = new WindowsInterface(this);
    #endif
    #ifdef Q_OS_LINUX
    osint = new LinuxInterface(this);
    #endif

    connect(frontman, &FrontendManager::printLoaded, printman, &PrintManager::onJobReadyForPrep);
    connect(printerman, &PrinterManager::jobLoaded, printman, &PrintManager::onJobReadyForPrep);
    connect(printerman, &PrinterManager::jobLoaded, osint, &OsInterface::onNetPrintIntercepted);
    connect(rfidman, &RfidManager::cardScanned, userman, &UserManager::onCardScanned);
}

GlobalState::~GlobalState(){

}

QString GlobalState::getAppVersion() const {
    return APP_VERSION;
}

QVariant GlobalState::getSetting(QString sname, QVariant def) const {
    return settings.value(sname, def);
}

void GlobalState::loadSettings(QMap<QString, QVariant> setc) {
    settings = QMap(setc);
}

QString GlobalState::getDataDirPath() const {
    return stnsd("dataDirPath", dataDirPath);
}
QString GlobalState::getAppDirPath() const {
    return appDirPath;
}

QDir GlobalState::getDataDir() const {
    return QDir(getDataDirPath());
}
QDir GlobalState::getAppDir() const {
    return QDir(appDirPath);
}

QCoreApplication* GlobalState::getApp() const {
    return app;
}

QQmlApplicationEngine* GlobalState::getEng() const {
    return engine;
};

QObject* GlobalState::getRoot() const {
    return qmlroot;
};

FrontendManager* GlobalState::getFrontendManager() const { return frontman; }
PrinterManager* GlobalState::getPrinterManager() const { return printerman; }
AirtableBase* GlobalState::getAirtableManager() const { return airtableman; }
RfidManager* GlobalState::getRfidManager() const { return rfidman; }
UserManager* GlobalState::getUserManager() const { return userman; }
PrintManager* GlobalState::getPrintManager() const { return printman; }
OsInterface* GlobalState::getOsInterface() const { return osint; }

void GlobalState::setApp(QCoreApplication* appptr) {
    if (app == nullptr) { 
        app = appptr;
        connect(app, &QCoreApplication::aboutToQuit, printerman, &PrinterManager::closing);
    }
}

void GlobalState::setEngine(QQmlApplicationEngine* eng) {
    if (engine == nullptr) {
        engine = eng;
        frontman->setupRootContext(eng);
    }
};

void GlobalState::setRoot(QObject* root) {
    if (qmlroot == nullptr) qmlroot = root;
};

void GlobalState::loadConfig(QJsonObject cfg) {
    config = cfg;
    //TODO: Implement some sort of abstraction to allow program to work with any database
    if (!config.contains("airtable") || !config.value("airtable").isObject()) {
        Check::write("Credentials for Airtable loaded", Cl::FAIL);
        return Error("ConfigError", "Missing Airtable Credentials", El::Fatal).handle();
    }
    QJsonObject airtablec = config.value("airtable").toObject();
    if (!airtablec.contains("hostname") || !airtablec.contains("key") || !airtablec.contains("base") || !airtablec.value("hostname").isString() || !airtablec.value("key").isString() || !airtablec.value("base").isString()) {
        Check::write("Credentials for Airtable loaded", Cl::FAIL);
        return Error("ConfigError", "Missing Airtable Credentials", El::Fatal).handle();
    }
    Check::write("Credentials for Airtable loaded", Cl::OK);
    airtableman = new AirtableBase(airtablec.value("hostname").toString(), airtablec.value("key").toString(), airtablec.value("base").toString(), this);

    //Load settings
    if (config.contains("settings") && config.value("settings").isObject()) {
        QJsonObject settingsc = config.value("settings").toObject();
        QMap<QString, QVariant> settings;
        for (auto it = settingsc.constBegin(); it != settingsc.constEnd(); ++it) {
            QString key = it.key();
            QVariant val = it.value().toVariant();
            settings.insert(key, val);
            CheckLevel cll = (it.value().isBool()) ? (val.toBool()) ? Cl::OK : Cl::FAIL : Cl::WARN;
            if (val.canConvert<QString>()) Check::write("Setting " + it.key() + " value", val.toString().toUpper(), cll);
            else Check::write("Setting " + it.key(), "FOUND", cll);
        }
        loadSettings(settings);
    }


    //Load specific settings
    emit frontman->setDarkmode(stnb("darkMode"));

    rfidman->startRfid();
    //Load printer config
    printerman->loadConfig(cfg);
}
#include "globalstate.hpp"
#include <QCoreApplication>

GlobalState& GlobalState::instance() {
    static GlobalState instanced;
    return instanced;
}

GlobalState::GlobalState(QObject *parent) : QObject(parent) {
    dataDirPath = QDir::currentPath();
    appDirPath = QCoreApplication::applicationDirPath();
}

GlobalState::~GlobalState(){

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

void GlobalState::setApp(QCoreApplication* appptr) {
    app = appptr;
}
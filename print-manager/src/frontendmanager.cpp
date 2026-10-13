/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include "frontendmanager.h"
#include <QProcess>
#include <QDebug>
#include <QUrl>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QQmlContext>
#include <QCoreApplication>
#include <QTimer>
#include "gcodeparser.h"
#include "globalstate.hpp"

#ifndef Q_OS_WIN
#include <QStandardPaths>
#endif

#define root GlobalState::instance().getRoot()

FrontendManager::FrontendManager(QObject* parent) : QObject(parent) {}

void FrontendManager::setupRootContext(QQmlApplicationEngine* eng) {
    eng->rootContext()->setContextProperty("frontman", this);
    eng->rootContext()->setContextProperty("printermanager", prm);
    eng->rootContext()->setContextProperty("printersModel", prm->getModel());
    eng->rootContext()->setContextProperty("versionStr", APP_VERSION);
    #ifdef Q_OS_WIN
    eng->rootContext()->setContextProperty("isWindows", true);
    #else
    eng->rootContext()->setContextProperty("isWindows", false);
    #endif
}

AppState FrontendManager::getAppState() {
    bool ok = true;
    int prop = root->property("appstate").toInt(&ok);
    if (!ok) {
        Error::softHandle("FrontendManagerError", "Appstate is not int", El::Warning);
        return AppState::Idle;
    }
    return static_cast<AppState>(prop);
}

ScanContext FrontendManager::getScanContext() {
    bool ok = true;
    int prop = root->property("scancontext").toInt(&ok);
    if (!ok) {
        Error::softHandle("FrontendManagerError", "Scancontext is not int", El::Warning);
        return ScanContext::NoContext;
    }
    return static_cast<ScanContext>(prop);
}

void FrontendManager::setAppState(AppState state) {
    root->setProperty("appstate", state);
}

void FrontendManager::setScanContext(ScanContext ctx) {
    root->setProperty("scancontext", ctx);
}

//QML accessible functions

Q_INVOKABLE void FrontendManager::fileUploaded(const QUrl &fileUrl) {
    QString filepath = fileUrl.toLocalFile(); //get filepath from url
    Log::write("FrontendManager", "File uploaded: " + filepath);
    Eo<QMap<QString, QString>> propertiesEo = GCodeParser::parseFile(filepath); //parse the gcode

    //qDebug() << propertiesEo;
    if (propertiesEo.isError()) {
        return propertiesEo.handle();
    }
    auto properties = propertiesEo.get();
    QVariantMap propertiesForJS; //convert properties to QVariantMap for QML
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        propertiesForJS.insert(it.key(), it.value());
    }
    quint16 lpid = ptm->getLoadedPrint().printerId;
    if (prm->getPrinter(lpid) == nullptr) return;
    propertiesForJS.insert("printerName", prm->getPrinter(lpid)->getName());
    propertiesForJS.insert("connected", prm->getPrinter(lpid)->getConnectionStatus());
    propertiesForJS.insert("jobStatus", prm->getPrinter(lpid)->getJobStatus());
    Log::write("FrontendManager", "Loaded print info for: " + filepath);
    //emit signals to main loop and QML to update appstate and load print files
    //Printer is online
    emit printInfoLoaded(propertiesForJS);
    emit printLoaded(lpid, filepath, properties);
}

Q_INVOKABLE void FrontendManager::helpButtonClicked() {

}

Q_INVOKABLE void FrontendManager::setLoadedPrintFilamentProvider(bool personal) {
    ptm->setLoadedPrintFilament(personal);
    //Hackey but its only for debug so whatevs
    if (stnsd("bypassCardId", "") != "") usm->onCardScanned(stns("bypassCardId"));
}


Q_INVOKABLE void FrontendManager::orcaButtonClicked() { //Runs when orcaslicer button pressed
    osi->onOrcaButtonClicked();
}

//Qt accessible functions

void FrontendManager::showMessage(QString message, QString acceptText, int redirectState) {
    emit messageReq(message, acceptText, redirectState);
    setAppState(AppState::Message);
}

void FrontendManager::showPrintOverridePrep() {
    QVariantMap propertiesForJS; //convert properties to QVariantMap for QML
    LoadedPrint lp = ptm->getLoadedPrint();
    for (auto it = lp.printInfo.constBegin(); it != lp.printInfo.constEnd(); ++it) {
        propertiesForJS.insert(it.key(), it.value());
    }
    propertiesForJS.insert("personalFilament", lp.isPersonalFilament);
    if (prm->getPrinter(lp.printerId) != nullptr) propertiesForJS.insert("printerName", prm->getPrinter(lp.printerId)->getName());
    propertiesForJS.insert("connected", (prm->getPrinter(lp.printerId) != nullptr) ? prm->getPrinter(lp.printerId)->getConnectionStatus() : false);
    propertiesForJS.insert("jobstatus", (prm->getPrinter(lp.printerId) != nullptr) ? prm->getPrinter(lp.printerId)->getJobStatus() : Printer::JobStatus::Error);
    emit printIssuesLoaded(propertiesForJS, lp.issues.values());
    setAppState(AppState::PrepOverride);
}

void FrontendManager::setUploadProgress(const QString &filename, qint64 max, qint64 cur) {
    
    emit uploadProgressUpdated(filename, byteString(cur), byteString(max), static_cast<double>(cur)/static_cast<double>(max));
    if (getAppState() == AppState::Loading) setAppState(AppState::Uploading);
}

QString FrontendManager::byteString(qint64 bytes) const {
    double newBytes = bytes;
    quint8 mag3 = 0;
    while (newBytes / 1000 >= 1) {
        newBytes /= 1000;
        mag3++;
    }
    QString s = QString::number(newBytes, 'f', 2);
    switch (mag3) {
        case 0:
            return s + " B";
        case 1:
            return s + " KB";
        case 2:
            return s + " MB";
        case 3:
            return s + " GB";
        case 4:
            return s + " TB";
        default:
            return "Yo you should not be uploading this much fatass";
    }
}
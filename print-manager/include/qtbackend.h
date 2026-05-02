#ifndef QTBACKEND_H
#define QTBACKEND_H
/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include <QObject>

#ifdef Q_OS_WIN
//#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <QFile>
#include <QSqlDatabase>
#include <QQmlApplicationEngine>
#include <QSqlRecord>
#include "printermanager.h"
#include "errors.hpp"
#include "ltx2aQT.h"
#include <QCoreApplication>
#include "airtable.h"


enum AppState {
    Idle,
    Prep,
    Message,
    UserScan,
    StaffScan,
    Printing,
    Loading
};

struct Staff : public User {
    quint8 authLevel = 1;
};

class QTBackend : public QObject {
    Q_OBJECT
public:
    explicit QTBackend(QCoreApplication* app, QQmlApplicationEngine* eng, QObject* parent = 0);
    void setRoot(QObject* root);
    void loadConfig(QJsonObject cfg);
    void showMessage(QString message, QString acceptText="OK", int redirectState = 0);
    void cardScanned(const QString &id);
    // Error queryDatabase(const QString &query);
    // Eo<QMap<QString, QVariant>> queryDatabase(const QString &query, const QMap<QString, QVariant> &values);
    // Eo<QList<QMap<QString, QVariant>>> queryDatabaseMultirow(const QString &query, const QMap<QString, QVariant> &values);
protected:
    QString version = "0.3.3-alpha";
    QJsonObject config;
    //Services
    //QSqlDatabase db;
    AirtableBase* airtable;
    PrinterManager* pm;
    LTx2A rfidReader;
    //QML stuff
    QQmlApplicationEngine* engine;
    QObject* root;
    //App state variables
    QString loadedPrintFilepath;
    quint32 loadedPrinterId = 0;
    QString currentUserID = "";
    QString currentStaffID = "";
    User* currentUser = nullptr;
    Staff* currentStaff = nullptr;
    QFile* loadedPrint = nullptr; //Selected print file
    QMap<QString, QString> loadedPrintInfo; //Print file info
private:
    #ifdef Q_OS_WIN
    DWORD findProcessId(const QString &processName);
    void bringWindowToFront(DWORD pid);
    #endif
    double parseDuration(const QString &durationString);
    AppState appstate();


signals:
    void printLoaded(quint32 id, const QString &gcodeFilepath, const QMap<QString, QString> &printInfo);
    void printInfoLoaded(const QVariantMap &printInfo);
    void messageReq(const QString &message, const QString &buttonText, const int &redirectState);
    void setAppmode(quint8 mode);
    void setAppstate(quint8 state);
    void tPrint(const QString &newtext);
    void closing();
private slots:
    void jobLoaded(quint32 id, const QString &filepath, const QMap<QString, QString> &printInfo);
public slots:
    Q_INVOKABLE void orcaButtonClicked();
    Q_INVOKABLE void helpButtonClicked();
    Q_INVOKABLE void fileUploaded(const QUrl &fileUrl);
};

#endif // QTBACKEND_H

#ifndef FRONTEND_MANAGER_H
#define FRONTEND_MANAGER_H
/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include <QObject>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QCoreApplication>



enum ScanContext {
    NoContext,
    UserAuth,
    StaffAuth = 100,
    StaffTraining
};

enum AppState {
    Idle,
    Prep,
    PrepOverride,
    Message,
    Scan,
    Loading,
    PrinterSelect,
    Uploading
};

class FrontendManager : public QObject {
    Q_OBJECT
public:
    explicit FrontendManager(QObject* parent = nullptr);
    void setupRootContext(QQmlApplicationEngine* eng);
    
    void showMessage(QString message, QString acceptText="OK", int redirectState = 0);
    ScanContext getScanContext();
    AppState getAppState();
    void setAppState(AppState state);
    void setScanContext(ScanContext ctx);
    void showPrintOverridePrep();

signals:
    void printLoaded(quint32 id, const QString &gcodeFilepath, const QMap<QString, QString> &printInfo);
    void printInfoLoaded(const QVariantMap &printInfo);
    void printIssuesLoaded(const QVariantMap &printInfo, const QStringList &printIssues);
    void messageReq(const QString &message, const QString &buttonText, const int &redirectState);
    void uploadProgressUpdated(const QString& filename, const QString &cur, const QString &max, double pct);
    void setDarkmode(bool dm);
    void closing();
    void raiseRequested();
public slots:
    Q_INVOKABLE void orcaButtonClicked();
    Q_INVOKABLE void helpButtonClicked();
    Q_INVOKABLE void fileUploaded(const QUrl &fileUrl);
    Q_INVOKABLE void setLoadedPrintFilamentProvider(bool personal);
    void setUploadProgress(const QString &filename, qint64 max, qint64 cur);
private:
    QString byteString(qint64 bytes) const;
};



#endif

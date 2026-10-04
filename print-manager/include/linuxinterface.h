#ifndef LINUX_INTERFACE_H
#define LINUX_INTERFACE_H

#include <QString>
#include <QObject>

#ifdef Q_OS_LINUX
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QWindow>
#include <KWindowSystem>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include "osinterface.h"

class LinuxInterface : public OsInterface {
    Q_OBJECT
public:
    LinuxInterface(QObject* parent = nullptr);

public slots:
    void onOrcaButtonClicked();
    void onNetPrintIntercepted(quint32 id, const QString &_filepath, const QMap<QString, QString> &printInfo);
private slots:
    void onNotificationAction(quint32 id, const QString &actionKey);
    void onNotificationActivationToken(quint32 id, const QString &token);
private:
    quint32 lastNotificationId = 0;
    QString lastActivationToken;
    void notifyPrintReceived(const QString &title, const QString &body);
    bool raiseSelf();
    bool isProcessRunning(const QStringList &names);
    bool raiseWindowKWin(const QString &jsCondition);
    bool raiseOrcaLinux();
};

#endif
#endif

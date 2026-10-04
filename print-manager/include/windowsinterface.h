#ifndef WINDOWS_INTERFACE_H
#define WINDOWS_INTERFACE_H

#include <QString>
#include <QObject>


#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include "osinterface.h"


class WindowsInterface : public OsInterface {
    Q_OBJECT
public:
    DWORD findProcessId(const QString &processName);
    void bringWindowToFront(DWORD pid);
public slots:
    void onOrcaButtonClicked();
    void onNetPrintIntercepted(quint32 id, const QString &_filepath, const QMap<QString, QString> &printInfo);
};

#endif
#endif

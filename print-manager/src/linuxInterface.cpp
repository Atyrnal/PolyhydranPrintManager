#include "linuxinterface.h"
#include "globalstate.hpp"

#ifdef Q_OS_LINUX
#include <QStandardPaths>
#include <QTimer>


LinuxInterface::LinuxInterface(QObject* parent) : OsInterface(parent) {
    //Notification stuff
    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActionInvoked", this, SLOT(onNotificationAction(uint,QString)));

    QDBusConnection::sessionBus().connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "ActivationToken", this, SLOT(onNotificationActivationToken(uint,QString)));
}

void LinuxInterface::notifyPrintReceived(const QString &title, const QString &body) {
    QDBusInterface iface("org.freedesktop.Notifications","/org/freedesktop/Notifications","org.freedesktop.Notifications",QDBusConnection::sessionBus());
    if (!iface.isValid()) return; // no notification daemon running

    QDBusReply<uint> reply = iface.call("Notify", "Polyhydran Print Manager", lastNotificationId, "", title, body, QStringList{"default", "Open"}, QVariantMap{}, 10000);

    if (reply.isValid()) lastNotificationId = reply.value();
}

void LinuxInterface::onNotificationAction(uint id, const QString &actionKey) {
    if (id != lastNotificationId || actionKey != "default") return;

    if (!lastActivationToken.isEmpty())
        KWindowSystem::setCurrentXdgActivationToken(lastActivationToken);

    emit ftm->raiseRequested();   // un-minimize via QML

    if (QWindow *w = qobject_cast<QWindow *>(gsi.getRoot()))
        KWindowSystem::activateWindow(w);

    lastActivationToken.clear();   // tokens are single-use
}

void LinuxInterface::onNotificationActivationToken(uint id, const QString &token) {
    if (id != lastNotificationId) return;
    lastActivationToken = token;
}

void LinuxInterface::onOrcaButtonClicked() {
    if (isProcessRunning({"orca-slicer", "OrcaSlicer", "orcaslicer"})) {
        if (raiseOrcaLinux()) {
            Log::write("LinuxInterface", "Bringing running OrcaSlicer instance to front");
        } else {
            Error::handle("LinuxInterfaceError","OrcaSlicer is already running, but this desktop doesn't allow raising its window",El::Trivial);
        }
        return;   // never launch a second instance
    } else {
        QString exe;
        if (stns("orcaSlicerExec") != "") {
            exe = stns("orcaSlicerExec");
            if (QFile::exists(exe)){ //Otherwise launch it (if it is installed)
                Log::write("LinuxInterface", "Launching OrcaSlicer instance");
                QProcess::startDetached(exe);
            } else {
                Error::handle("LinuxInterfaceError", "OrcaSlicer installation not found", El::Warning);
            }
        } else {
            exe = QStandardPaths::findExecutable("orca-slicer");
            if (exe.isEmpty())
                exe = QStandardPaths::findExecutable("OrcaSlicer");
            if (exe.isEmpty())
                exe = QStandardPaths::findExecutable("orcaslicer");
            if (exe.isEmpty())
                exe = "/opt/orca-slicer/bin/orca-slicer";
            //exe = "/usr/bin/orca-slicer"; // fallback
            if (QFile::exists(exe)){ //Otherwise launch it (if it is installed)
                Log::write("LinuxInterface", "Launching OrcaSlicer instance");
                QProcess::startDetached(exe);
            } else {
                Error::handle("LinuxInterfaceError", "OrcaSlicer installation not found", El::Warning);
            }
        }
    }
}

bool LinuxInterface::isProcessRunning(const QStringList &names) {
    const QStringList pids = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool ok = false;
        pid.toUInt(&ok);
        if (!ok) continue;                       // skip non-numeric entries
        QFile f("/proc/" + pid + "/comm");
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString comm = QString::fromUtf8(f.readAll()).trimmed();
        for (const QString &n : names)
            if (comm.contains(n, Qt::CaseInsensitive)) return true;
    }
    return false;
}


bool LinuxInterface::raiseWindowKWin(const QString &jsCondition) {
    QDBusInterface kwin("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus());
    if (!kwin.isValid()) return false;   // not running under KWin

    auto *script = new QTemporaryFile(QDir::temp().filePath("raise-XXXXXX.js"), this);
    if (!script->open()) {
        script->deleteLater();
        return false;
    }

    script->write(QString(R"(
        const wins = workspace.windowList ? workspace.windowList() : workspace.clientList();
        for (const w of wins) {
            if (%1) {
                w.minimized = false;
                if (workspace.windowList) workspace.activeWindow = w;
                else workspace.activeClient = w;
                break;
            }
        }
    )").arg(jsCondition).toUtf8());
    script->flush();

    // Unique name per call, otherwise loadScript can fail if a previous one is still loaded
    const QString name = "raise-" + QString::number(QRandomGenerator::global()->generate());

    QDBusReply<int> id = kwin.call("loadScript", script->fileName(), name);
    if (!id.isValid() || id.value() < 0) {
        script->deleteLater();
        return false;
    }

    // Plasma 6 path; Plasma 5 used "/<id>" instead
    QDBusInterface s("org.kde.KWin", QString("/Scripting/Script%1").arg(id.value()), "org.kde.kwin.Script", QDBusConnection::sessionBus());
    s.call("run");

    // Give the script time to execute, then unload it and delete the temp file
    QTimer::singleShot(1000, this, [name, script]() {
        QDBusInterface k("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                         QDBusConnection::sessionBus());
        k.call("unloadScript", name);
        script->deleteLater();
    });

    return true;
}

bool LinuxInterface::raiseOrcaLinux() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();

    if (desktop.contains("kde"))
        return raiseWindowKWin("String(w.resourceClass).toLowerCase().includes('orca')");
    if (session == "x11")
        return QProcess::startDetached("wmctrl", {"-x", "-a", "orca"});
    return false;   // GNOME Wayland etc.
}

bool LinuxInterface::raiseSelf() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();

    if (desktop.contains("kde")) {
        Log::write("LinuxInterface", "Attempting raise via KWin script");
        return raiseWindowKWin(QString("w.pid === %1").arg(QCoreApplication::applicationPid()));;
    }
    if (session == "x11") {
        Log::write("LinuxInterface", "Attempting raise via wmctrl");
        return QProcess::startDetached("wmctrl", {"-x", "-a", "polyhydran"});
    }
    return false;   // GNOME Wayland etc.
}

void LinuxInterface::onNetPrintIntercepted(quint32 id, const QString &_filepath, const QMap<QString, QString> &printInfo) {
    Log::write("LinuxInterface", "attempting self raise linux");
    if (raiseSelf()) return; //Try raising via KWin Script

    emit ftm->raiseRequested();  // tries the automatic raise via QML (works on Windows), alerts otherwise

    Log::write("LinuxInterface", "sending notification");
    Printer *p = prm->getPrinter(id);
    notifyPrintReceived("Print received",QString("%1 is ready to print%2").arg(printInfo.value("filename", "Your file"), (p) ? " on " + p->getName() : ""));
}

#endif
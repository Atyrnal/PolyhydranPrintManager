/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include "qtbackend.h"
#include <QProcess>
#include <QDebug>
#include <QUrl>
#include "gcodeparser.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QQmlContext>
#include "errorhandler.hpp"
#include "ltx2aQT.h"
#include <QCoreApplication>
#include <QTimer>
#include "globalstate.hpp"

#ifndef Q_OS_WIN
#include <QStandardPaths>
#endif

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
#endif

#define PRINTING_CERT_ID "recY34WO6fex1KMxO"

#define APP_VERSION "0.1.0-alpha11"

QTBackend::QTBackend(QQmlApplicationEngine* eng, QObject* parent) : QObject(parent) {
    ErrorHandler::bk = this;

    //rfidReader.start(); //Initialize the RFID reader


    pm = new PrinterManager(parent);

    engine = eng;
    engine->rootContext()->setContextProperty("backend", this);
    engine->rootContext()->setContextProperty("printermanager", pm);
    engine->rootContext()->setContextProperty("printersModel", pm->getModel());
    engine->rootContext()->setContextProperty("versionStr", APP_VERSION);
    #ifdef Q_OS_WIN
    engine->rootContext()->setContextProperty("isWindows", true);
    #else
    engine->rootContext()->setContextProperty("isWindows", false);
    #endif

    connect(this, &QTBackend::printLoaded, this, &QTBackend::jobLoaded);
    connect(pm, &PrinterManager::jobLoaded, this, &QTBackend::jobLoaded);
    connect(pm, &PrinterManager::jobLoaded, this, &QTBackend::netPrintIntercepted);
    connect(pm, &PrinterManager::printStatusUpdated, this, &QTBackend::printStatusUpdated);

    connect(gsi.getApp(), &QCoreApplication::aboutToQuit, pm, &PrinterManager::closing);

    #ifdef Q_OS_LINUX
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "ActionInvoked",
            this, SLOT(onNotificationAction(uint,QString)));

        QDBusConnection::sessionBus().connect(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "ActivationToken",
            this, SLOT(onNotificationActivationToken(uint,QString)));
    #endif
    // QTimer::singleShot(5000, this, [this](){
    //     root->setProperty("appstate", AppState::Loading+1);
    // });
}

void QTBackend::startRfid() {
    rfidReader = new LTx2A(stnsd("rfidReaderPortName", "auto"));
    QObject::connect(gsi.getApp(), &QCoreApplication::aboutToQuit, rfidReader, &LTx2A::stop); //Connect the aboutToQuit app event to the rfidReader's stop function
    QObject::connect(rfidReader, &LTx2A::cardScanned, this, [this]() { //Connect the rfidReader cardScanned event to the lambda
        if (rfidReader->hasNext()) { //If the cards scanned queue is not empty
            QString cardid = rfidReader->getNext().replace("\"", "").trimmed();
            this->cardScanned(cardid);
        }
    });
    rfidReader->start();
}


//Utility functions

double QTBackend::parseDuration(const QString &durationString) {
    //Regex pattern to extract numbers from string
    static QRegularExpression regex(R"((?:(\d+(?:\.\d+)?)h)?\s*(?:(\d+(?:\.\d+)?)m)?\s*(?:(\d+(?:\.\d+)?)s)?)");
    QRegularExpressionMatch match = regex.match(durationString);

    if (!match.hasMatch()) return 999.99; //Max time if string is invalid so print doesnt go through
    double totalHours = 0.0;
    if (match.captured(1).length() > 0)  // hours
        totalHours += match.captured(1).toDouble();
    if (match.captured(2).length() > 0)  // minutes
        totalHours += match.captured(2).toDouble() / 60.0;
    if (match.captured(3).length() > 0)  // seconds
        totalHours += match.captured(3).toDouble() / 3600.0;
    return totalHours;
}

//TODO: Make these work for linux / KDE ve(also make the app raise itelf)
#ifdef Q_OS_WIN
DWORD QTBackend::findProcessId(const QString& exeName) { //Windows shenanigans to find process by exename
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //ProcessScanner

    if (Process32First(snapshot, &entry)) { //Scan Processes until we find one that matches
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(exeName, Qt::CaseInsensitive) == 0) { //if exename matches
                DWORD pid = entry.th32ProcessID;
                CloseHandle(snapshot); //Stop scanner
                return pid;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0; //return 0 if not found / not open
}

void QTBackend::bringWindowToFront(DWORD pid) {
    HWND hwnd = nullptr;
    HWND mainHwnd = nullptr;

    //Scan instances of specified process for one with a window
    while ((hwnd = FindWindowEx(nullptr, hwnd, nullptr, nullptr))) {
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);

        // Skip windows not belonging to target process
        if (windowPid != pid)
            continue;

        // Skip invisible windows
        if (!IsWindowVisible(hwnd))
            continue;

        // Skip child/owned windows (dialogs, popups)
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
            continue;

        // Found a visible, top-level window for this process
        mainHwnd = hwnd;
        break;
    }

    if (mainHwnd) {
        ShowWindow(mainHwnd, SW_RESTORE);//Show the window
        SetForegroundWindow(mainHwnd); //Bring it to front
        Log::write("QtBackend", "Brought OrcaSlicer main window to front");
    } else {
        Error::handle("QtBackendError", "Could not find OrcaSlicer window", El::Trivial);
    }
}
#endif

AppState QTBackend::appstate() {
    bool ok = true;
    int prop = root->property("appstate").toInt(&ok);
    if (!ok) {
        Error::softHandle("QtBackendQMLError", "Appstate is not int", El::Warning);
        return AppState::Idle;
    }
    return static_cast<AppState>(prop);
}

ScanContext QTBackend::scancontext() {
    bool ok = true;
    int prop = root->property("scancontext").toInt(&ok);
    if (!ok) {
        Error::softHandle("QtBackendQMLError", "Scancontext is not int", El::Warning);
        return ScanContext::NoContext;
    }
    return static_cast<ScanContext>(prop);
}


//QML accessible functions

Q_INVOKABLE void QTBackend::fileUploaded(const QUrl &fileUrl) {
    QString filepath = fileUrl.toLocalFile(); //get filepath from url
    Log::write("QtBackend", "File uploaded: " + filepath);
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
    if (pm->getPrinter(loadedPrint.printerId) == nullptr) return;
    propertiesForJS.insert("printerName", pm->getPrinter(loadedPrint.printerId)->getName());
    propertiesForJS.insert("connected", pm->getPrinter(loadedPrint.printerId)->getConnectionStatus());
    propertiesForJS.insert("jobstatus", pm->getPrinter(loadedPrint.printerId)->getJobStatus());
    Log::write("QtBackend", "Loaded print info for: " + filepath);
    //emit signals to main loop and QML to update appstate and load print files
    //Printer is online
    emit printInfoLoaded(propertiesForJS);
    emit printLoaded(loadedPrint.printerId, filepath, properties);
}

Q_INVOKABLE void QTBackend::helpButtonClicked() {

}

Q_INVOKABLE void QTBackend::setLoadedPrintFilamentProvider(bool personal) {
    this->loadedPrint.isPersonalFilament = personal;
}


Q_INVOKABLE void QTBackend::orcaButtonClicked() { //Runs when orcaslicer button pressed
    //Locate orcaslicer exe
    #ifdef Q_OS_WIN
    QString exeName = "orca-slicer.exe";
    QString exePath = "C:/Program Files/OrcaSlicer/orca-slicer.exe";

    //Find orcaslicer process

    DWORD pid = findProcessId(exeName);

    if (pid != 0) { //if process running, bring it to front instead of starting a new oen
        Log::write("QtBackend", "Bringing running OrcaSlicer instance to front");
        bringWindowToFront(pid);
    } else if (QFile::exists(exePath)){ //Otherwise launch it (if it is installed)
        Log::write("QtBackend", "Launching OrcaSlicer instance");
        QProcess::startDetached(exePath);
    } else {
        Error::handle("QtBackendError", "OrcaSlicer installation not found", El::Warning);
    }
    #else
    if (isProcessRunning({"orca-slicer", "OrcaSlicer", "orcaslicer"})) {
        if (raiseOrcaLinux()) {
            Log::write("QtBackend", "Bringing running OrcaSlicer instance to front");
        } else {
            Error::handle("QtBackendError","OrcaSlicer is already running, but this desktop doesn't allow raising its window",El::Trivial);
        }
        return;   // never launch a second instance
    } else {
        QString exe;
        if (stns("orcaSlicerExec") != "") {
            exe = stns("orcaSlicerExec");
            if (QFile::exists(exe)){ //Otherwise launch it (if it is installed)
                Log::write("QtBackend", "Launching OrcaSlicer instance");
                QProcess::startDetached(exe);
            } else {
                Error::handle("QtBackendError", "OrcaSlicer installation not found", El::Warning);
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
                Log::write("QtBackend", "Launching OrcaSlicer instance");
                QProcess::startDetached(exe);
            } else {
                Error::handle("QtBackendError", "OrcaSlicer installation not found", El::Warning);
            }
        }
    }

    #endif

}


#ifdef Q_OS_LINUX
bool QTBackend::isProcessRunning(const QStringList &names) {
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


bool QTBackend::raiseWindowKWin(const QString &jsCondition) {
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

bool QTBackend::raiseOrcaLinux() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();

    if (desktop.contains("kde"))
        return raiseWindowKWin("String(w.resourceClass).toLowerCase().includes('orca')");
    if (session == "x11")
        return QProcess::startDetached("wmctrl", {"-x", "-a", "orca"});
    return false;   // GNOME Wayland etc.
}

bool QTBackend::raiseSelfLinux() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();

    if (desktop.contains("kde"))
        Log::write("Attempting raise via KWin script");
        return raiseWindowKWin(QString("w.pid === %1").arg(QCoreApplication::applicationPid()));;
    if (session == "x11")
        Log::write("Attempting raise via wmctrl");
        return QProcess::startDetached("wmctrl", {"-x", "-a", "polyhydran"});
    return false;   // GNOME Wayland etc.
}
#endif

//Qt accessible functions

void QTBackend::setRoot(QObject* r) {
    root = r;
}

void QTBackend::loadConfig(QJsonObject cfg) {
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
    airtable = new AirtableBase(airtablec.value("hostname").toString(), airtablec.value("key").toString(), airtablec.value("base").toString(), this);

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
        GlobalState::instance().loadSettings(settings);
    }


    //Load specific settings
    emit this->setDarkmode(stnb("darkMode"));

    startRfid();
    //Load printer config
    pm->loadConfig(cfg);
}

void QTBackend::showMessage(QString message, QString acceptText, int redirectState) {
    emit messageReq(message, acceptText, redirectState);
    root->setProperty("appstate", AppState::Message);
}

void QTBackend::jobLoaded(quint32 id, const QString &filepath, const QMap<QString, QString> &printInfo) {
    this->loadedPrint = LoadedPrint();
    loadedPrint.filepath = filepath; //set filepath
    loadedPrint.printInfo = printInfo; //set printinfo
    loadedPrint.printerId= id;
    root->setProperty("appstate", AppState::Prep); //change QML appstate to show print info
}

void QTBackend::netPrintIntercepted(quint32 id, const QString &_filepath, const QMap<QString, QString> &printInfo) {
    /*emit this->raiseRequested();*/ //Raise only when we load a job from the printermanager, not from upload.
    #ifdef Q_OS_LINUX
        Log::write("QTBackendWindowManager", "attempting self raise linux");
        if (raiseSelfLinux()) return; //Try raising via KWin Script
    #endif
    emit this->raiseRequested();  // tries the automatic raise via QML (works on Windows), alerts otherwise

    #ifdef Q_OS_LINUX
        Log::write("QTBackendWindowManager", "sending notification");
        Printer *p = pm->getPrinter(id);
        notifyPrintReceived("Print received",QString("%1 is ready to print%2").arg(printInfo.value("filename", "Your file"), (p) ? " on " + p->getName() : ""));
    #endif
}

void QTBackend::notifyPrintReceived(const QString &title, const QString &body) {
#ifdef Q_OS_LINUX
    QDBusInterface iface("org.freedesktop.Notifications",
                         "/org/freedesktop/Notifications",
                         "org.freedesktop.Notifications",
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) return; // no notification daemon running

    QDBusReply<uint> reply = iface.call(
        "Notify",
        "Polyhydran Print Manager",      // app name
        lastNotificationId,              // replaces the previous one instead of stacking
        "",                              // icon (name or path)
        title,
        body,
        QStringList{"default", "Open"},  // "default" = clicking the notification body
        QVariantMap{},                   // hints
        10000);                          // timeout in ms

    if (reply.isValid()) lastNotificationId = reply.value();
#else
    Q_UNUSED(title); Q_UNUSED(body);
#endif
}

void QTBackend::onNotificationAction(uint id, const QString &actionKey) {
    if (id != lastNotificationId || actionKey != "default") return;

    if (!lastActivationToken.isEmpty())
        KWindowSystem::setCurrentXdgActivationToken(lastActivationToken);

    emit raiseRequested();   // un-minimize via QML

    if (QWindow *w = qobject_cast<QWindow *>(root))
        KWindowSystem::activateWindow(w);

    lastActivationToken.clear();   // tokens are single-use
}

void QTBackend::onNotificationActivationToken(uint id, const QString &token) {
    if (id != lastNotificationId) return;
    lastActivationToken = token;
}

void QTBackend::setCurrentStaff(QString id) {
    currentStaffID = id;
    staffCache.insert(id);
}

void QTBackend::completeTraining() {
    if (!currentUser.contains("id")) return;
    QVariantMap payload;
    QVariantList certificates = currentUser.value("fields", QVariantMap()).toMap().value("Certificates", QList<QString>()).toList();
    certificates.append(PRINTING_CERT_ID);
    payload.insert("Certificates", certificates);

    airtable->table("Users")->updateRecordById(currentUser.value("id").toString(), payload);

    printStartCheck(false, true);
}

void QTBackend::showPrintOverridePrep() {
    QVariantMap propertiesForJS; //convert properties to QVariantMap for QML
    for (auto it = loadedPrint.printInfo.constBegin(); it != loadedPrint.printInfo.constEnd(); ++it) {
        propertiesForJS.insert(it.key(), it.value());
    }
    propertiesForJS.insert("personalFilament", loadedPrint.isPersonalFilament);
    if (pm->getPrinter(loadedPrint.printerId) != nullptr) propertiesForJS.insert("printerName", pm->getPrinter(loadedPrint.printerId)->getName());
    propertiesForJS.insert("connected", (pm->getPrinter(loadedPrint.printerId) != nullptr) ? pm->getPrinter(loadedPrint.printerId)->getConnectionStatus() : false);
    propertiesForJS.insert("jobstatus", (pm->getPrinter(loadedPrint.printerId) != nullptr) ? pm->getPrinter(loadedPrint.printerId)->getJobStatus() : Printer::JobStatus::Error);
    emit printIssuesLoaded(propertiesForJS, loadedPrint.issues);
    root->setProperty("appstate", AppState::PrepOverride);
}



void QTBackend::lookupUser(const QString &cardid, std::function<void(UserLookup, const QVariantMap &user)> done) {
    if (staffCache.contains(cardid)) {
        return done(UserLookup::Staff, {});
    }
    airtable->table("Users")->getRecord(QString("{User Id} = '%1'").arg(cardid), QList<Sort>({{"Date", SortDir::DESC}}), [this, cardid, done](Eo<QVariantMap> eo) {
        if (eo.isError()) {
            if (eo.errorLevel() <= El::Trivial) {
                eo.softHandle();
                return done(UserLookup::NotFound, {});
            }
            eo.handle();
            return done(UserLookup::Error, {});
        }
        QVariantMap user = eo.get();
        bool isStaff = user.value("fields").toMap().value("Is Staff", false).toBool();
        if (isStaff) setCurrentStaff(cardid);
        done(isStaff ? UserLookup::Staff : UserLookup::NotStaff, user);
    });
}

void QTBackend::cardScanned(const QString &cardid) {
    ScanContext context = scancontext();
    AppState prev = appstate();

    //bool isCachedStaff = staffCache.contains(cardid);
    if (staffCache.contains(cardid)) setCurrentStaff(cardid);
    else root->setProperty("appstate", AppState::Loading);
    root->setProperty("scancontext", ScanContext::NoContext);

    Log::write("QtBackendSerial", "Card scanned, current conext:" + QString::number(context));
    if (context == ScanContext::UserAuth && prev == AppState::Scan) { //User authorizing print
        currentUserID = cardid;
        loadedPrint.userID = cardid;

        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                if (!user.isEmpty()) currentUser = user;
                printStartCheck(true);
                break;
            case UserLookup::NotStaff:
                if (!user.isEmpty()) currentUser = user;
                    printStartCheck(false, ct);
                break;
            case UserLookup::NotFound:
                loadedPrint.issues.insert("isRegistered", false);
                showMessage("Your account is not Registered\nPlease Register at the Check-In Kiosk");
                break;
            default:
                root->setProperty("appstate", AppState::Idle);
                break;
            }
        });
    } else if (context == ScanContext::StaffTraining && prev == AppState::Scan) {
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                completeTraining();
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
                root->setProperty("scancontext", ScanContext::StaffTraining);
                showMessage("This user is not Staff\nPlease ask a staff member for\nour 3D print training and have them\nscan their UCard to continue", "Training Completed", AppState::Scan);
                break;
            default:
                root->setProperty("appstate", AppState::Idle);
                break;
            }
        });
    } else if (context == ScanContext::StaffAuth && loadedPrint.userID != "" && prev == AppState::Scan) {
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                printStartCheck(true);
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
                showMessage("This user is not Staff", "OK", AppState::Scan);
                break;
            default:
                root->setProperty("appstate", prev);
                break;
            }
        });
    } else if (!loadedPrint.userID.isNull() && !loadedPrint.userID.isEmpty()) { //NoContext
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                showPrintOverridePrep();
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
            default:
                root->setProperty("appstate", prev);
                break;
            }
        });
    } else root->setProperty("appstate", prev);
}

void QTBackend::printStartCheck(bool staffApproved, bool justTrained) {
    double printDuration = parseDuration(loadedPrint.printInfo["duration"]);
    if (!staffApproved) {
        QVariantMap recordFields = currentUser.value("fields").toMap();
        QString cicsAff = recordFields.value("Affiliation to CICS", "").toString();
        bool isCICS = cicsAff == "CICS Student" || cicsAff == "CICS Faculty or Staff";
        bool training = justTrained || recordFields.value("Certificates", QList<QString>()).toList().contains(PRINTING_CERT_ID); //Need to implement some form of join or something idek

        if (stnbd("requireCics", true)) loadedPrint.issues.insert("isCICS", isCICS);
        if (stnbd("requireTraining", true))loadedPrint.issues.insert("trained", training);
        if (stnbd("requirePrintDuration", true)) loadedPrint.issues.insert("duration", printDuration);
        loadedPrint.issues.insert("personalFilament", loadedPrint.isPersonalFilament);

        if (stnbd("requireCics", true) && !isCICS) {
            if (stnbd("allowNonCicsPersonalFilament", true)) {
                if (!loadedPrint.isPersonalFilament) return showMessage("Sorry, but only CICS Community Members\nmay print using Makerspace filament.", "I Understand");
            } else {
                return showMessage("Sorry, but only CICS Community Members\ncan print at the\nPhysical Computing Makerspace", "I Understand");
            }
        }
        if (stnbd("requireTraining", true) && !training) {
            root->setProperty("scancontext", ScanContext::StaffTraining);
            return showMessage("Please ask a staff member to\napprove your print or take \nour 3D Print training", "Training Completed", AppState::Scan);
        }
        if (!staffApproved && stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDuration", 6.0) && !loadedPrint.isPersonalFilament) return showMessage("Prints cannot be longer than 6 hours\nwith Makerspace Filament");
        if (!staffApproved && stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDurationPersonalFilament", stndd("maxPrintDuration", 6.0)) && loadedPrint.isPersonalFilament && isCICS) return showMessage("Prints cannot be longer than 10 hours\n");
        if (!staffApproved && stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDurationNonCics", stndd("maxPrintDuration", 6.0)) && loadedPrint.isPersonalFilament && !isCICS) return showMessage("Prints cannot be longer than 6 hours\n");
    }
    showMessage("Printing now!");
    if (pm->getPrinter(loadedPrint.printerId) == nullptr) return Error("QTBackendError", "Loaded Printer not found", El::Critical).handle();
    //TODO: Check jobstate and update previous print accordingly
    QVariantMap printLogInfo = {
        {"User ID", currentUserID},
        {"Printer", pm->getPrinter(loadedPrint.printerId)->getName()},
        {"Printer Model", pm->getPrinter(loadedPrint.printerId)->getBrand() + " " + pm->getPrinter(loadedPrint.printerId)->getModel()},
        {"Weight", loadedPrint.printInfo["weight"].left(loadedPrint.printInfo["weight"].size()-1).toDouble()}, //Remove the g and convert to double
        {"Duration", printDuration*3600},
        {"Filament Type", (loadedPrint.printInfo.contains("filament") && loadedPrint.printInfo["filament"] != "") ? loadedPrint.printInfo["filament"] : loadedPrint.printInfo["filamentType"]},
        {"Filename", loadedPrint.printInfo["filename"]},
        {"Personal Filament", loadedPrint.isPersonalFilament},
        {"Status", "Ongoing"}
    };
    if (staffApproved) printLogInfo.insert("Staff Approver ID", currentStaffID);
    airtable->table("Print Log")->createRecord(printLogInfo);
    Log::write("QTBackend", "Starting Print");
    pm->startPrint(loadedPrint.printerId, loadedPrint.filepath);
}

void QTBackend::printStatusUpdated(quint16 _printerId, const QString &printerName, QString status) {
    if (!stnbd("updatePrintStatuses", true)) return;
    //TODO: Set to aborted if failed within a certain time of it being started
    airtable->table("Print Log")->getRecord(QString("{Printer} = '%1'").arg(printerName), QList<Sort>({{"Date", SortDir::DESC}}), [=, this](Eo<QVariantMap> recordeo){
        if (recordeo.isError()) return recordeo.softHandle();
        QVariantMap recordFields = recordeo.get().value("fields").toMap();
        QString curstatus = recordFields.value("Status", "Unknown").toString();
        QDateTime recordCreated = QDateTime::fromString(recordFields.value("Date", "2026-01-01T00:00:00.000Z").toString(), Qt::ISODateWithMs);
        std::chrono::milliseconds diff = QDateTime::currentDateTimeUtc() - recordCreated;
        if (curstatus == "Ongoing" || curstatus == "Halted" || curstatus == "Unknown") {
            if (status == "Failed" && diff <= std::chrono::milliseconds(300000)) //If print is stopped in the first 5 minutes
            airtable->table("Print Log")->updateRecordById(recordeo.get().value("id").toString(), {{"Status", "Aborted"}});
            else
            airtable->table("Print Log")->updateRecordById(recordeo.get().value("id").toString(), {{"Status", status}});
        }
    });
}

// Error QTBackend::queryDatabase(const QString &query) {
//     QSqlQuery q; //Create query
//     q.prepare(query); //Prepare query
//     if(q.exec()) { //execute the query and return result
//         return Error::None();
//     } else {
//         return Error("DatabaseQueryError", q.lastError().text(), El::Warning);
//     }
// }

// Eo<QMap<QString, QVariant>> QTBackend::queryDatabase(const QString &query, const QMap<QString, QVariant> &values) {
//     using eop = Eo<QMap<QString, QVariant>>;
//     QSqlQuery q; //Create query
//     q.prepare(query); //Prepare query
//     for (auto it = values.constBegin(); it != values.constEnd(); ++it) { //insert values (parameterized to prevent sql injection vulnerability)
//         q.bindValue((it.key().startsWith(":")) ? it.key() : ":" + it.key(), it.value());
//     }

//     QList bound = q.boundValues();
//     for (auto it = bound.constBegin(); it != bound.constEnd(); ++it) {
//         if (!it->isValid()) {
//             return eop("DatabaseQueryBindError", "Failed to bind values", El::Warning);
//         }
//     }
//     bool success = q.exec(); //execute the query
//     if (!success || !q.isActive()) {
//         return eop("DatabaseQueryError", q.lastError().text(), El::Warning);
//     }

//     if (!q.next() || !q.isValid()) {
//         return eop("DatabaseQueryError", "No valid record", El::Debug);
//     }

//     QMap<QString, QVariant> output;
//     QSqlRecord rec = q.record();
//     for (int i = 0; i < rec.count(); i++) {
//         output.insert(rec.fieldName(i), rec.value(i));
//     }

//     return eop(output);
// }

// Eo<QList<QMap<QString, QVariant>>> QTBackend::queryDatabaseMultirow(const QString &query, const QMap<QString, QVariant> &values) {
//     using eop = Eo<QList<QMap<QString, QVariant>>>;
//     QSqlQuery q; //Create query
//     q.prepare(query); //Prepare query
//     for (auto it = values.constBegin(); it != values.constEnd(); ++it) { //insert values (parameterized to prevent sql injection vulnerability)
//         q.bindValue((it.key().startsWith(":")) ? it.key() : ":" + it.key(), it.value());
//     }

//     QList bound = q.boundValues();
//     for (auto it = bound.constBegin(); it != bound.constEnd(); ++it) {
//         if (!it->isValid()) {
//             return eop("DatabaseQueryBindError", "Failed to bind values", El::Trivial);
//         }
//     }
//     bool success = q.exec(); //execute the query
//     if (!success || !q.isActive() || !q.isValid()) {
//         return eop("DatabaseQueryError", q.lastError().text(), El::Warning);
//     }

//     QList<QMap<QString, QVariant>> output;
//     while (q.next()) {
//         QSqlRecord rec = q.record();
//         QMap<QString, QVariant> r;
//         for (int i = 0; i < rec.count(); i++) {
//             r.insert(rec.fieldName(i), rec.value(i));
//         }
//         output.append(r);
//     }
//     return eop(output);
// }

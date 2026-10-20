#include "printmanager.h"
#include "frontendmanager.h"
#include "globalstate.hpp"
#include <QObject>

#define gusb(name) usm->getUserSetting((name), overrides).toBool()
#define guss(name) usm->getUserSetting((name), overrides).toString()
#define gusi(name) usm->getUserSetting((name), overrides).toInt()
#define gusd(name) usm->getUserSetting((name), overrides).toDouble()
#define gusdd(name, def) usm->getUserSetting((name), overrides, (def)).toDouble()

PrintManager::PrintManager(QObject* parent) : QObject(parent) {}

quint8 PrintManager::countFilaments(QMap<QString, QString> printProperties) {
    return ((printProperties.contains("filament") && printProperties["filament"] != "") ? printProperties["filament"] : printProperties["filamentType"]).split(",").count();
}

void PrintManager::setPrintUser(QString userid) {
    loadedPrint.userID =userid;
}
void PrintManager::addPrintIssue(QString v) {
    loadedPrint.issues.insert(v);
}

void PrintManager::setLoadedPrintFilament(bool personal) {
    loadedPrint.isPersonalFilament = personal;
}

LoadedPrint PrintManager::getLoadedPrint() const {
    return loadedPrint;
}

void PrintManager::printStartCheck(bool staffApproved, qint16 printersUsing, bool justTrained) {
    #define lmbs(x, y, z) if (ok) { message = (x); button=(y); s=(z); ok=false; }
    #define lmb(x, y) lmbs(x, y, AppState::Idle)
    #define lm(x) lmb(x, "OK")
    ftm->setAppState(AppState::Loading);
    double printDuration = parseDuration(loadedPrint.printInfo["duration"]);
    qint8 filamentTypeCount = countFilaments(loadedPrint.printInfo);
    double infillDensity = parseInfill(loadedPrint.printInfo["infillDensity"]);
    double layerHeight = parseLayerHeight(loadedPrint.printInfo["layerHeight"]);
    double weight = parseWeight(loadedPrint.printInfo);
    QVariantMap overrides = usm->getUserOverrides(usm->getCurrentUser());
    if (!staffApproved) {
        bool ok = true;
        QString message = "";
        QString button = "OK";
        AppState s = AppState::Idle;
        if (gusb("requirePrinterUseCount") && printersUsing < 0) return ftm->showMessage("Unable to fetch printers currently in use\nPlease report this to a staff member.");
    
        // QString cicsAff = usm->getCurrentUser().value("Affiliation to CICS", "").toString();
        // bool isCICS = cicsAff == "CICS Student" || cicsAff == "CICS Faculty or Staff";
        //TODO: make isTrained function in usermanager
        bool training = justTrained || usm->isTrained(usm->getCurrentUser());

        // if (stnbd("requireCics", true)) loadedPrint.issues.insert("isCICS", isCICS);
        // if (stnbd("requireTraining", true))loadedPrint.issues.insert("trained", training);
        // if (stnbd("requirePrintDuration", true)) loadedPrint.issues.insert("duration", printDuration);
        // if (stnbd("requirePrinterUseCount", true)) loadedPrint.issues.insert("printerUseCount", printersUsing);
        // if (stnbd("requireFilamentTypeCount", false)) loadedPrint.issues.insert("filamentTypeCount", filamentTypeCount);
        // loadedPrint.issues.insert("personalFilament", loadedPrint.isPersonalFilament);

        if (!gusb("allowPrinting")) {
            loadedPrint.issues.insert("Not allowed to print");
            lm("You are not allowed to print\nat the Physical Computing Makerspace.");
        }

        if (gusb("requirePrinterUseCount") && printersUsing >= gusi("maxSimeltaneousPrinters")) {
            loadedPrint.issues.insert(QString("Already using %1 printer%2").arg(QString::number(printersUsing), (printersUsing == 1) ? "" : "s"));
            lm(QString("You are currently using %1 printer%2\nwhich is the maximum.\nPlease wait for these to finish\nbefore starting another print.").arg(QString::number(printersUsing), (printersUsing == 1) ? "" : "s")) 
        }
        
        if (!loadedPrint.isPersonalFilament) {
            if (!gusb("allowMakerspaceFilament")) {
                loadedPrint.issues.insert("Non-CICS printing with makerspace filament");
                lmb("Only CICS Community Members\nmay print using Makerspace filament.","I Understand")
            }
            if (gusb("requirePrintDuration") && printDuration > gusd("maxPrintDuration")) {
                loadedPrint.issues.insert(QString("Longer than %1 hours with makerspace filament").arg(gusd("maxPrintDuration")));
                lm(QString("Prints cannot be longer than %1 hours\nwith Makerspace Filament").arg(gusd("maxPrintDuration")))
            }

        } else {
            if (!gusb("allowPersonalFilament")) {
                loadedPrint.issues.insert("Non-CICS printing");
                lmb("Only CICS Community Members\ncan print at the\nPhysical Computing Makerspace", "I Understand")
            }
            if (gusb("requirePrintDuration") && printDuration > gusdd("maxPrintDurationPersonalFilament", gusd("maxPrintDuration"))) {
                loadedPrint.issues.insert(QString("Longer than %1 hours with personal filament").arg(gusdd("maxPrintDurationPersonalFilament", gusd("maxPrintDuration"))));
                lm(QString("Prints cannot be longer than %1 hours").arg(gusdd("maxPrintDurationPersonalFilament", gusd("maxPrintDuration"))))
            }
        }

        if (gusb("requireTraining") && !training) {
            ftm->setScanContext(ScanContext::StaffTraining);
            loadedPrint.issues.insert("Untrained");
            lmbs("Please ask a staff member to\napprove your print or take \nour 3D Print training", "Training Completed", AppState::Scan)
        }

        if (gusb("requireFilamentTypeCount") && filamentTypeCount > gusi("maxFilamentTypeCount")) {
            loadedPrint.issues.insert(QString("More than %1 filament type%2 (%3)").arg(QString::number(gusi("maxFilamentTypeCount")), (filamentTypeCount == 1) ? "" : "s", QString::number(filamentTypeCount)));
            lm(QString("Prints cannot have more than %1\n filament type%2/color%2 without\nstaff approval.").arg(QString::number(gusi("maxFilamentTypeCount")), (filamentTypeCount == 1) ? "" : "s"))
        }
        //Weight, infill and layer height
        if (gusb("requirePrintWeight") && weight > gusd("maxPrintWeight")) {
            loadedPrint.issues.insert(QString("Weight exceeding %1g").arg(QString::number(gusd("maxPrintWeight"))));
            lm(QString("Prints cannot have a filament usage\n greater than %1g\n without staff approval.").arg(QString::number(gusd("maxPrintWeight"))))
        }

        if (gusb("requireInfillDensity") && infillDensity > gusd("maxInfillDensity")) {
            loadedPrint.issues.insert(QString("Infill density exceeding %1%").arg(QString::number(gusd("maxInfillDensity")*100)));
            lm(QString("Prints cannot have an infill density\n greater than %1%\n without staff approval.").arg(QString::number(gusd("maxInfillDensity")*100)))
        }
        //Log::write("PrintManager", QString("req: %1, val: %2, min: %3, truth: %4, 0: %5").arg(stnbd("requireLayerHeight", false)).arg(layerHeight).arg(stnd("minLayerHeight")).arg(layerHeight < stnd("minLayerHeight")).arg(layerHeight != 0.0));
        if (gusb("requireLayerHeight") && layerHeight < gusd("minLayerHeight") && layerHeight != 0.0) {
            loadedPrint.issues.insert(QString("Layer height less than %1mm").arg(QString::number(gusd("minLayerHeight"))));
            lm(QString("Prints cannot have a layer height\n less than %1mm\n without staff approval.").arg(QString::number(gusd("minLayerHeight"))))
        }
        if (!ok) return ftm->showMessage(message, button, s);
    } else {
        loadedPrint.staffApproved = true;
    }

    //Debug stuff

    if (printDuration == 0 || filamentTypeCount == 0 || infillDensity == 0 || layerHeight == 0 || weight == 0) { //issue with gcode parsing

        quint8 bitmask = ((((((((printDuration == 0) << 1 ) + (filamentTypeCount == 0)) << 1) + (infillDensity == 0)) << 1) + (layerHeight == 0)) << 1) + (weight == 0);
        Error::softHandle("PrintManagerParseError", "Detected invalid print properties: " + QString::number(bitmask, 2));

        if (stnb("gcodeParsingDebug")) { //Save uploaded file(s) for debug purposes
            QDir dst(gsi.getDataDirPath() + "/debug");
            QDir src(gsi.getDataDirPath() + "/uploaded");
            if (dst.exists()) {
                const QStringList files = src.entryList(QDir::Files | QDir::Hidden);
                for (const QString &name : files) {
                    const QString from = src.filePath(name);
                    const QString to   = dst.filePath(name);
                    QFile::copy(from, to);
                }
                Log::write("PrintManager", "Saved bugged GCode for review");
            }
        }
    }
    

    startLoadedPrint();
}

void PrintManager::startLoadedPrint() {
    double printDuration = parseDuration(loadedPrint.printInfo["duration"]);
    if (prm->getPrinter(loadedPrint.printerId) == nullptr) {
        ftm->showMessage("Unable to find selected printer.");
        return Error("PrintManagerError", "Loaded Printer not found", El::Critical).handle();
    }
    //TODO: Check jobstate and update previous print accordingly
    QVariantMap printLogInfo = {
        {"User ID", usm->getCurrentUserId()},
        {"Printer", prm->getPrinter(loadedPrint.printerId)->getName()},
        {"Printer Model", prm->getPrinter(loadedPrint.printerId)->getBrand() + " " + prm->getPrinter(loadedPrint.printerId)->getModel()},
        {"Weight", loadedPrint.printInfo["weight"].left(loadedPrint.printInfo["weight"].size()-1).toDouble()}, //Remove the g and convert to double
        {"Duration", printDuration*3600},
        {"Filament Type", (loadedPrint.printInfo.contains("filament") && loadedPrint.printInfo["filament"] != "") ? loadedPrint.printInfo["filament"] : loadedPrint.printInfo["filamentType"]},
        {"Filename", loadedPrint.printInfo["filename"]},
        {"Personal Filament", loadedPrint.isPersonalFilament},
        {"Status", "Ongoing"},
        {"Multicolor", countFilaments(loadedPrint.printInfo) > 1}
    };
    if (loadedPrint.staffApproved && usm->getCurrentUserId() != usm->getCurrentStaffId()) printLogInfo.insert("Staff Approver ID", usm->getCurrentStaffId());
    Log::write("PrintManager", QString("Starting Print on Printer %1").arg(prm->getPrinter(loadedPrint.printerId)->getName()));
    QObject::connect(prm, &PrinterManager::printStartResult, this, [this, printLogInfo](bool success){
        if (success) {
            Log::write("PrintManager", "Print start succeeded.");
            ait->table("Print Log")->createRecord(printLogInfo);
            ftm->showMessage("Printing now!");
        } else {
            Log::write("PrintManager", "Print start failed.");
            ftm->showMessage("Failed to start print.\nPlease try another printer.", "OK");
        }
    }, Qt::SingleShotConnection);
    prm->startPrinting(loadedPrint.printerId, loadedPrint.filepath);
}

void PrintManager::onPrintStatusUpdated(const QString &printerName, const QString &status) const {
    if (!stnb("updatePrintStatuses")) return;
    
    Log::write("PrintManager", QString("Print status updated to %1 for printer %2").arg(status, printerName));

    ait->table("Print Log")->getRecord(QString("{Printer} = '%1'").arg(printerName), QList<Sort>({{"Date", SortDir::DESC}}), [=, this](Eo<QVariantMap> recordeo){
        if (recordeo.isError()) return recordeo.softHandle();
        QVariantMap recordFields = recordeo.get().value("fields").toMap();
        QString curstatus = recordFields.value("Status", "Unknown").toString();
        QDateTime recordCreated = QDateTime::fromString(recordFields.value("Date", "2026-01-01T00:00:00.000Z").toString(), Qt::ISODateWithMs);
        std::chrono::milliseconds diff = QDateTime::currentDateTimeUtc() - recordCreated;
        if (curstatus == "Ongoing" || curstatus == "Halted" || curstatus == "Unknown") {
            if (status == "Failed" && diff <= std::chrono::milliseconds(stni("abortTimeMaxSeconds")*1000)) //If print is stopped in the first 5 minutes
                ait->table("Print Log")->updateRecordById(recordeo.get().value("id").toString(), {{"Status", "Aborted"}});
            else
            ait->table("Print Log")->updateRecordById(recordeo.get().value("id").toString(), {{"Status", status}});
        }
    });
}

double PrintManager::parseDuration(const QString &durationString) {
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

void PrintManager::onJobReadyForPrep(quint32 id, const QString &filepath, const QMap<QString, QString> &printInfo) {
    this->loadedPrint = LoadedPrint();
    loadedPrint.filepath = filepath; //set filepath
    loadedPrint.printInfo = printInfo; //set printinfo
    loadedPrint.printerId= id;
    ftm->setAppState(AppState::Prep); //change QML appstate to show print info
}

double PrintManager::parseInfill(const QString &infillString) {
    QString p = QString(infillString);
    if (p.endsWith('%')) p.chop(1);
    bool parsed = false;
    double res = p.trimmed().toDouble(&parsed);
    return (parsed) ? res / 100.0 : 0.0;
}
double PrintManager::parseLayerHeight(const QString &layerString) {
    QString p = QString(layerString);
    if (p.endsWith("mm")) p.chop(2);
    bool parsed = false;
    double res = p.trimmed().toDouble(&parsed);
    //Log::write("PrintManager", QString("Parsed: %1, Val: %2").arg(parsed).arg((parsed) ? res : 0.0));
    return (parsed) ? res : 0.0;
}

double PrintManager::parseWeight(QMap<QString, QString> printProperties) {
    QString weightString;
    if (printProperties.contains("totalWeight")) weightString = printProperties.value("totalWeight");
    else if (printProperties.contains("weight")) weightString = printProperties.value("weight");
    if (weightString.endsWith('g')) weightString.chop(1);
    bool parsed = false;
    double res = weightString.trimmed().toDouble(&parsed);
    return (parsed) ? res : 0.0;
}
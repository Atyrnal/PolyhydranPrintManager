#include "printmanager.h"
#include "globalstate.hpp"

PrintManager::PrintManager(QObject* parent) : QObject(parent) {}

void PrintManager::setPrintUser(QString userid) {
    loadedPrint.userID =userid;
}
void PrintManager::addPrintIssue(QString k, QVariant v) {
    loadedPrint.issues.insert(k, v);
}

void PrintManager::setLoadedPrintFilament(bool personal) {
    loadedPrint.isPersonalFilament = personal;
}

LoadedPrint PrintManager::getLoadedPrint() const {
    return loadedPrint;
}

void PrintManager::printStartCheck(bool staffApproved, qint16 printersUsing, bool justTrained) {
    double printDuration = parseDuration(loadedPrint.printInfo["duration"]);
    if (!staffApproved) {
        if (stnbd("requirePrinterUseCount", true) && printersUsing < 0) return ftm->showMessage("Unable to fetch printers currently in use\nPlease report this to a staff member.");
    
        QString cicsAff = usm->getCurrentUser().value("Affiliation to CICS", "").toString();
        bool isCICS = cicsAff == "CICS Student" || cicsAff == "CICS Faculty or Staff";
        bool training = justTrained || usm->getCurrentUser().value("Certificates", QList<QString>()).toList().contains(stnsd("printingCertificateRecordId", "NONE")); //Need to implement some form of join or something idek

        if (stnbd("requireCics", true)) loadedPrint.issues.insert("isCICS", isCICS);
        if (stnbd("requireTraining", true))loadedPrint.issues.insert("trained", training);
        if (stnbd("requirePrintDuration", true)) loadedPrint.issues.insert("duration", printDuration);
        if (stnbd("requirePrinterUseCount", true)) loadedPrint.issues.insert("printerUseCount", printersUsing);
        loadedPrint.issues.insert("personalFilament", loadedPrint.isPersonalFilament);

        if (stnbd("requirePrinterUseCount", true) && printersUsing >= stnid("maxSimeltaneousPrinters", 2)) {
            return ftm->showMessage(QString("You are currently using %1 printer%2\nwhich is the maximum.\nPlease wait for these to finish\nbefore starting another print.").arg(QString::number(printersUsing), (printersUsing == 1) ? "" : "s"), "I Understand");
        }
        if (stnbd("requireCics", true) && !isCICS) {
            if (stnbd("allowNonCicsPersonalFilament", true)) {
                if (!loadedPrint.isPersonalFilament) return ftm->showMessage("Sorry, but only CICS Community Members\nmay print using Makerspace filament.", "I Understand");
            } else {
                return ftm->showMessage("Sorry, but only CICS Community Members\ncan print at the\nPhysical Computing Makerspace", "I Understand");
            }
        }
        if (stnbd("requireTraining", true) && !training) {
            ftm->setScanContext(ScanContext::StaffTraining);
            return ftm->showMessage("Please ask a staff member to\napprove your print or take \nour 3D Print training", "Training Completed", AppState::Scan);
        }
        if (stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDuration", 6.0) && !loadedPrint.isPersonalFilament) return ftm->showMessage("Prints cannot be longer than 6 hours\nwith Makerspace Filament");
        if (stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDurationPersonalFilament", stndd("maxPrintDuration", 6.0)) && loadedPrint.isPersonalFilament && isCICS) return ftm->showMessage("Prints cannot be longer than 10 hours\n");
        if (stnbd("requirePrintDuration", true) && printDuration > stndd("maxPrintDurationNonCics", stndd("maxPrintDuration", 6.0)) && loadedPrint.isPersonalFilament && !isCICS) return ftm->showMessage("Prints cannot be longer than 6 hours\n");
    } else {
        loadedPrint.staffApproved = true;
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
        {"Status", "Ongoing"}
    };
    if (loadedPrint.staffApproved) printLogInfo.insert("Staff Approver ID", usm->getCurrentStaffId());
    ait->table("Print Log")->createRecord(printLogInfo);
    Log::write("PrintManager", QString("Starting Print on Printer %1").arg(prm->getPrinter(loadedPrint.printerId)->getName()));
    prm->startPrinting(loadedPrint.printerId, loadedPrint.filepath);
    ftm->showMessage("Printing now!");
}

void PrintManager::onPrintStatusUpdated(const QString &printerName, const QString &status) const {
    if (!stnbd("updatePrintStatuses", true)) return;
    
    Log::write("PrintManager", QString("Print status updated to %1 for printer %2").arg(status, printerName));

    ait->table("Print Log")->getRecord(QString("{Printer} = '%1'").arg(printerName), QList<Sort>({{"Date", SortDir::DESC}}), [=, this](Eo<QVariantMap> recordeo){
        if (recordeo.isError()) return recordeo.softHandle();
        QVariantMap recordFields = recordeo.get().value("fields").toMap();
        QString curstatus = recordFields.value("Status", "Unknown").toString();
        QDateTime recordCreated = QDateTime::fromString(recordFields.value("Date", "2026-01-01T00:00:00.000Z").toString(), Qt::ISODateWithMs);
        std::chrono::milliseconds diff = QDateTime::currentDateTimeUtc() - recordCreated;
        if (curstatus == "Ongoing" || curstatus == "Halted" || curstatus == "Unknown") {
            if (status == "Failed" && diff <= std::chrono::milliseconds(300000)) //If print is stopped in the first 5 minutes
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

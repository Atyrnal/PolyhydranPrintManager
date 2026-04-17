#include "printermanager.h"
#include "prusa.h"
#include "bambulab.h"

PrinterManager::PrinterManager(QObject* parent) : QObject(parent) {

}

// PrinterManager::~PrinterManager() {
//     for (quint32 i = 0; i < printers.length(); i++) {
//         delete printers[i];
//         if (octEmus.contains(i)) {
//             delete octEmus[i];
//         }
//     }
//     if (bblEmu != nullptr) {
//         delete bblEmu;
//     }
// }

void PrinterManager::loadConfig(QJsonObject config){
    QJsonArray prntrs = config.value("printers").toArray(QJsonArray());
    for (int i = 0; i < prntrs.size(); i++) {
        if (!prntrs[i].isObject()) continue;
        QJsonObject printer = prntrs[i].toObject();
        if (!printer.contains("brand") || !printer.value("brand").isString()) continue;
        QString brand = printer.value("brand").toString().toLower();
        if (brand == "prusa") {
            if (!printer.contains("hostname") || !printer.contains("apiKey") || !printer.value("hostname").isString() || !printer.value("apiKey").isString()) continue;
            Prusa* prs = new Prusa(
                printer.value("name").toString("Unnamed"),
                printer.value("model").toString("Unknown"),
                printer.value("hostname").toString(),
                printer.value("apiKey").toString(),
                printer.value("storageType").toString("usb")
            );
            addPrinter(prs);
        } else if (brand == "bambulab") {
            if (!printer.contains("hostname") || !printer.contains("accessCode") || !printer.value("hostname").isString() || !printer.value("accessCode").isString()) continue;
            BambuLab* bbl = new BambuLab(
                printer.value("name").toString("Unnamed"),
                printer.value("model").toString("Unknown"),
                printer.value("hostname").toString(),
                printer.value("accessCode").toString(),
                printer.value("username").toString("bblp"),
                printer.value("port").toInteger(8883)
            );
            bbl->setStorageType(printer.value("storageType").toString("sdcard"));
            addPrinter(bbl);
        } else continue;
    }
    QList<Printer*> printersVals = printers.values();
    QList<QString> printersPrinted;
    for (int i = 0; i < printersVals.length(); i++) {
        Printer* printer = printersVals[i];
        printersPrinted.append(printer->getName() + " (" +printer->getBrand() + " " + printer->getModel() + ")");
    }
    Log::write("PrinterManager", "Loaded Printer Configuration: [" + printersPrinted.join(", ") + "]");
}

Printer* PrinterManager::getPrinter(quint32 id) {
    if (!printers.contains(id)) return nullptr;
    return printers.value(id);
}

quint32 PrinterManager::addPrinter(Printer* p) {
    quint32 id = nextId++;
    printers.insert(id, p);
    if (p->getBrand() == "BambuLab") {
        BambuLab* bblp = dynamic_cast<BambuLab*>(p);
        if (bblp == nullptr) {
            p->setBrand("Unknown");
        } else {
            if (bblEmu == nullptr) {
                bblEmu = new BambuEmulator(parent());
                QObject::connect(bblEmu, &BambuEmulator::jobLoaded, this, [this](quint32 id, const QString &filepath, QMap<QString, QString> properties) {
                    properties.insert("brand", "BambuLab");
                    properties.insert("id", QString::number(id));
                    emit this->jobLoaded(id, filepath, properties);
                    QVariantMap propertiesForJS; //Convert to QVariantMap for use in QML
                    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
                        propertiesForJS.insert(it.key(), it.value());
                    }
                    emit this->jobInfoLoaded(propertiesForJS);
                });
            }
            //sqDebug() << "adding bambu printer with id:" << id;
            bblEmu->addPrinter(id, bblp);
            return id;
        }
    }
    OctoprintEmulator* emu = new OctoprintEmulator(baseOctPort+id);
    octEmus.insert(id, emu);
    QObject::connect(emu, &OctoprintEmulator::jobLoaded, this, [this, id](const QString &filepath, QMap<QString, QString> properties) {
        properties.insert("brand", printers[id]->getBrand());
        properties.insert("id", QString::number(id));
        emit this->jobLoaded(id, filepath, properties);
        QVariantMap propertiesForJS; //Convert to QVariantMap for use in QML
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            propertiesForJS.insert(it.key(), it.value());
        }
        emit this->jobInfoLoaded(propertiesForJS);
    });

    return id;
}

void PrinterManager::closing() {
    if (bblEmu != nullptr) bblEmu->closing();
}

void PrinterManager::removePrinter(quint32 id) {
    if (bblEmu != nullptr && printers[id]->getBrand() == "BambuLab") bblEmu->removePrinter(id);
    if (octEmus.contains(id)) octEmus.remove(id);
    printers.remove(id);
}

void PrinterManager::startPrint(quint32 id, const QString &filepath, QJsonObject properties) {
    if (!printers.contains(id)) return;
    Printer* p = printers[id];
    if (p->getBrand() == "BambuLab") {
        BambuLab* bbl = dynamic_cast<BambuLab*>(p);
        if (bbl != nullptr && filepath.endsWith(".gcode.3mf")) return bbl->startPrint(filepath, bbl->loadedOptions);
    }
    p->startPrint(filepath);
}



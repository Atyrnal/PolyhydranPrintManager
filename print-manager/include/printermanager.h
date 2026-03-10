#ifndef PRINTERMANAGER_H
#define PRINTERMANAGER_H


#include "octoprintemulator.h"
#include "bambuemulator.h"

class PrinterManager : public QObject {
    Q_OBJECT
public:
    PrinterManager(QObject* parent = nullptr);
    //~PrinterManager();
    quint32 addPrinter(Printer* p);
    Printer* getPrinter(quint32 id);
    void removePrinter(quint32 id);
    void loadConfig(QJsonObject config);
    void startPrint(quint32 id, const QString &filepath, QJsonObject properties = QJsonObject());
signals:
    void jobLoaded(quint32 id, const QString &filepath, QMap<QString, QString> properties);
    void jobInfoLoaded(QVariantMap properties);
public slots:
    void closing();
private:
    quint16 baseOctPort = 21111;
    quint32 nextId = 0;
    QMap<quint32, Printer*> printers;
    QMap<quint32, OctoprintEmulator*> octEmus;
    BambuEmulator* bblEmu = nullptr;
};

#endif // PRINTERMANAGER_H

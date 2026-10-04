#ifndef PRINT_MANAGER_H
#define PRINT_MANAGER_H

#include <QString>
#include <QObject>
#include <QSet>
#include <QVariantMap>

struct LoadedPrint {
    quint16 printerId;
    QString filepath;
    QMap<QString,QString> printInfo;
    QString userID;
    bool isPersonalFilament;
    QVariantMap issues;
    bool staffApproved = false;
};



class PrintManager : public QObject {
    Q_OBJECT
public:
    PrintManager(QObject* parent = nullptr);
    void printStartCheck(bool isStaff, qint16 printersUsing=0, bool justTrained=false);
    void setPrintUser(QString userid);
    void addPrintIssue(QString k, QVariant v);

    void setLoadedPrintFilament(bool personal);

    LoadedPrint getLoadedPrint() const;
public slots:
    void onPrintStatusUpdated(const QString &printerName, const QString& status) const;
    void onJobReadyForPrep(quint32 id, const QString &filepath, const QMap<QString, QString> &printInfo);
private:
    void startLoadedPrint();
    double parseDuration(const QString &durationString);
    LoadedPrint loadedPrint;
};


#endif

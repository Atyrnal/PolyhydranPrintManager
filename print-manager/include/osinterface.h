#ifndef OS_INTERFACE_H
#define OS_INTERFACE_H

#include <QObject>

class OsInterface : public QObject {
    Q_OBJECT
public:
    OsInterface(QObject* parent) : QObject(parent) {};
public slots:
    virtual void onOrcaButtonClicked() = 0;
    virtual void onNetPrintIntercepted(quint32 id, const QString &_filepath, const QMap<QString, QString> &printInfo) = 0;   
};

#endif
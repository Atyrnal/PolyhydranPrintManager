/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#ifndef PRINTER_H
#define PRINTER_H

#include <QObject>
#include <QNetworkAccessManager>

class Printer : public QObject {
    Q_OBJECT
public:
    enum JobStatus {
        Idle,
        Occupied,
        Error
    };

    Printer(QObject* parent = 0);
    Printer(QString name, QString model, QObject* parent = 0);
    Printer(QString name, QString model, QString brand, QObject* parent = 0);
    virtual void startPrint(const QString &gcodeFilepath) = 0;
    void setName(QString name);
    void setModel(QString model);
    void setBrand(QString brand);
    QString getName();
    QString getModel();
    QString getBrand();
    bool getConnectionStatus();
    virtual JobStatus getJobStatus() = 0;
signals:
    void connectionUpdated(bool status);
protected:
    QString name;
    QString model;
    QString brand;
    QNetworkAccessManager manager;
    bool connectionStatus;
};


#endif // PRINTER_H

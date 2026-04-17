#ifndef PRUSA_H
#define PRUSA_H
#include "printer.h"
#include <QNetworkAccessManager>

class Prusa : public Printer {
public:
    Q_OBJECT
    Prusa(QString ip, QString password, QObject* parent = nullptr);
    Prusa(QString ip, QString password, QString username, QObject* parent = nullptr);
    void testConnection() override;
    QJsonObject generateConfig() override;
private:
    QNetworkAccessManager netman;
};
#endif

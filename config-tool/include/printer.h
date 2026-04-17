#ifndef PRINTER_H
#define PRINTER_H
#include <QJsonObject>

class Printer : public QObject {
    Q_OBJECT
    public:
        virtual void testConnection() = 0;
        virtual QJsonObject generateConfig() = 0;
        Printer(QString ip, QString password, QString username, QObject* parent) : ip(ip), password(password), username(username), QObject(parent) {
            QObject::connect(this, &Printer::connectionTestComplete, this, [this](bool result, QString m) {
                testComplete = true;
                testMessage = m;
                connected = result;
            });
        }
        bool isConnected() { return connected; }
        bool isTested() { return testComplete; }
        QString connectionTestMessage() { return testMessage; }
    signals:
        void connectionTestComplete(bool result, QString message);
    protected:
        QString ip;
        QString username;
        QString password;
        QString model;
        QString brand;
        QString name;
        bool connected = false;
        bool testComplete = false;
        QString testMessage = "Connection not tested.";
};
#endif

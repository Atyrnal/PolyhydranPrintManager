#ifndef BAMBULAB_H
#define BAMBULAB_H
#include "printer.h"
#include <QMqttClient>

class BambuLab : public Printer {
    Q_OBJECT
public:
    BambuLab(QString ip, QString accessCode, QObject* parent = nullptr);
    BambuLab(QString ip, QString accessCode, QString username, QObject* parent = nullptr);
    void testConnection() override;
    QJsonObject generateConfig() override;
private:
    QString modelId;
    quint16 bindingPortTCP = 3000;
    quint16 mqttPort = 8883;
    QString serialNumber;
    QMqttClient* mqtt;
    QString virtualIp = "";
};
#endif // BAMBULAB_H

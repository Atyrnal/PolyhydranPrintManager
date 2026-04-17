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
    static const inline QMap<QString, QString> modelIds = QMap<QString, QString>{
        {"BL-P001", "X1C"},
        {"BL-P002", "X1"},
        {"C13",     "X1E"},
        {"C11",     "P1P"},
        {"C12",     "P1S"},
        {"O1S",     "H2S"},
        {"O1D",     "H2D"},
        {"O1C2",    "H2C"},
        {"O1E",     "H2D Pro"},
        {"N1",      "A1 Mini"},
        {"N2S",     "A1"},
        {"N7",      "P2S"}
    };
private:
    QString modelId;
    quint16 bindingPortTCP = 3000;
    quint16 mqttPort = 8883;
    QString serialNumber;
    QMqttClient* mqtt;
    QString virtualIp = "";
};
#endif // BAMBULAB_H

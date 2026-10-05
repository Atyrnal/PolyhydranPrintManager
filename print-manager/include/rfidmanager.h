#ifndef RFID_MANAGER_H
#define RFID_MANAGER_H

#include "ltx2aQT.h"
#include <QString>
#include <QObject>


class RfidManager : public QObject {
    Q_OBJECT
public:
    RfidManager(QObject* parent = nullptr);
    void startRfid();
signals:
    void cardScanned(QString cardid);
private:
    LTx2A* rfidReader = nullptr;
    bool connected = false;
    bool connectedOnce = false;
};


#endif

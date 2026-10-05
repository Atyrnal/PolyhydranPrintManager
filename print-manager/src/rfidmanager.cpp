#include "rfidmanager.h"
#include "globalstate.hpp"
#include "ltx2aQT.h"
#include <QTimer>

RfidManager::RfidManager(QObject* parent) : QObject(parent) {}

void RfidManager::startRfid() {
    //TODO: connection / lifecycle management; Include resetting the esp via serial if needed?.
    rfidReader = new LTx2A(stnsd("rfidReaderPortName", "auto"));
    QObject::connect(gsi.getApp(), &QCoreApplication::aboutToQuit, rfidReader, &LTx2A::stop); //Connect the aboutToQuit app event to the rfidReader's stop function
    QObject::connect(rfidReader, &LTx2A::cardScanned, this, [this]() { //Connect the rfidReader cardScanned event to the lambda
        if (rfidReader->hasNext()) { //If the cards scanned queue is not empty
            QString cardid = rfidReader->getNext().replace("\"", "").trimmed();
            emit cardScanned(cardid);
        }
    });
    QObject::connect(rfidReader, &LTx2A::errorOccured, this, [this](const QString &error) {
        if (!connectedOnce) {
            Check::write("Connect to RFID Scanner serial", Cl::FAIL);
            connectedOnce = true;
            Error::handle("SerialError", error, El::Critical); //Print error
        } else if (connected) {
            Error::handle("SerialError", error, El::Critical); //Print error
            connected = false;
        }
        rfidReader->stop();
        QTimer::singleShot(10000, this, [this](){ //Attempt reconnect
            rfidReader->restart();
        });
    });
    QObject::connect(rfidReader, &LTx2A::serialOpened, this, [this]() {
        if (!connectedOnce) Check::write("Connect to RFID Scanner serial", Cl::OK);
        connectedOnce = true;
        connected = true;
    });
    rfidReader->start();
}
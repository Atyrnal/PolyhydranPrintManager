#include "rfidmanager.h"
#include "globalstate.hpp"

RfidManager::RfidManager(QObject* parent) : QObject(parent) {}

void RfidManager::startRfid() {
    rfidReader = new LTx2A(stnsd("rfidReaderPortName", "auto"));
    QObject::connect(gsi.getApp(), &QCoreApplication::aboutToQuit, rfidReader, &LTx2A::stop); //Connect the aboutToQuit app event to the rfidReader's stop function
    QObject::connect(rfidReader, &LTx2A::cardScanned, this, [this]() { //Connect the rfidReader cardScanned event to the lambda
        if (rfidReader->hasNext()) { //If the cards scanned queue is not empty
            QString cardid = rfidReader->getNext().replace("\"", "").trimmed();
            emit cardScanned(cardid);
        }
    });
    rfidReader->start();
}
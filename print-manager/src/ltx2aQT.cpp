/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include "ltx2aQT.h"
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include "errorhandler.hpp"

LTx2A::LTx2A(QString portName, qint32 baud) {
    scanned = QQueue<UserEntry>();
    thread = new QThread; //New thread
    worker = new SerialWorker(portName, baud); //New SerialWorker
}

void LTx2A::start() {
    worker->moveToThread(thread); //Send the worker to a different thread so it does not interrupt UI rendering
    QObject::connect(thread, &QThread::started, worker, &SerialWorker::start); //Connect thread start signal to serialworker start slot
    QObject::connect(worker, &SerialWorker::cardScanned, this, [this](const UserEntry &data) {//Connect cardscanned event of serialworker to lambda
        scanned.enqueue(data); //Add the card id to queue
        emit cardScanned(); //Emit the cardScanned event (called a signal in QT)
    });
    QObject::connect(worker, &SerialWorker::errorOccurred, [](const QString &error) {
        if (error.toLower() == "matching serial port not found") {
            return ErrorHandler::handle(Error("SerialDeviceNotFoundError", error, El::Warning));
        } else {
            return ErrorHandler::handle(Error("SerialError", error, El::Critical));
        }
    });
    thread->start(); //Run the thread
}

void LTx2A::stop() { //Stop and remove threads
    worker->stop();
    thread->quit();
    thread->wait();
    delete worker;
    delete thread;
}

bool LTx2A::hasNext() {
    return !scanned.empty();
}

UserEntry LTx2A::getNext() { //Get the next cardid
    return scanned.dequeue();
}


SerialWorker::SerialWorker(QString portName, qint32 baud)
    : QObject(),
    portName(std::move(portName)),
    baud(std::move(baud)) {} //C++ wizard shit (initializing values)

void SerialWorker::start() {
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts(); //List of active serial ports
    QSerialPortInfo* port = nullptr;
    for (int i = 0; i < ports.length(); i++) { //Iterative over avaliable ports
        QSerialPortInfo pport = ports[i];
        if (portName != "auto") { //Select port by name
            if (pport.portName() == portName) {
                port = &pport;
                break;
            }
        } else { //Search for ESP32 Silicon Labs CP210x chip on serial port (works most of the time)
            //Check if port vendor and product ids match the CP210x
            if (pport.hasVendorIdentifier() && pport.hasProductIdentifier() && pport.vendorIdentifier() == 0x10C4 && pport.productIdentifier() == 0xEA60) {
                port = &pport;
                break;
            }
        }

    }
    if (port == nullptr || port->isNull()) { //If port not found
        emit errorOccurred("Matching Serial port not found"); //Send error event
        return;
    }
    //Serial connection settings
    serial = new QSerialPort;
    serial->setPort(*port); //Set port and info
    serial->setParity(QSerialPort::NoParity);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setStopBits(QSerialPort::StopBits::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    connect(serial, &QSerialPort::readyRead, this, &SerialWorker::handleReadyRead); //Handle reading
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) { //Handle errors from serial port
        if (err != QSerialPort::NoError) emit errorOccurred(serial->errorString()); //Send error event
    });

    if (!serial->open(QIODevice::ReadOnly)) { //Open serial connection as read only
        emit errorOccurred(serial->errorString()); //Handle error opening connection
    }
}

void SerialWorker::stop() {
    serial->close(); //End serial connection
}

void SerialWorker::handleReadyRead() { //Handle recieving data
    QByteArray data = serial->readAll(); //Read all data
    QString decoded = QString::fromUtf8(data); //decode bytes into string
    decoded.remove(QChar(0xFFFD)); //Remove missing characters
    QStringList lines = decoded.split("\n"); //Split by lines (in case there are multiple in one read buffer)
    for (int i = 0; i <  lines.length(); i++) { //For-each line
        QString line = lines[i];
        line = line.trimmed(); //Strip whitespace
        if (line.length() <= 0 || !line.contains(";")) continue; //Ignore blank / invalid LTx2A lines
        QStringList linedata = line.split(";"); //Split by LTx2A data delimeter
        if (!attentionCodes.contains(linedata.at(0).trimmed())) continue; //Ignore messages we dont care about
        QString msgCode = linedata.at(0); //First data item is the code
        linedata.pop_front(); //Remove the code from data list
        handleMessage(msgCode, linedata); //Handler for serial messages
    }
}

void SerialWorker::handleMessage(QString code, QStringList data) { //Handle message based on code and data
    if (code == "20A") {
        //Card detected once
    } else if (code == "20B") { //Card scanned successfully
        emit cardScanned(UserEntry { //Emit the cardScanned event with a struct User containing the card id
            data[0]
        });
    } else if (code == "20C") {
        //Failed to read card
    }
}


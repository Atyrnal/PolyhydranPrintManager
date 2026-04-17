#include "ltx2aQT.h"
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <errors.hpp>

LTx2A::LTx2A(QString portName, qint32 baud) {
    scanned = QQueue<QString>();
    thread = new QThread; //New thread
    worker = new SerialWorker(portName, baud); //New SerialWorker
}

void LTx2A::start() {
    worker->moveToThread(thread); //Send the worker to a different thread so it does not interrupt UI rendering
    QObject::connect(thread, &QThread::started, worker, &SerialWorker::start); //Connect thread start signal to serialworker start slot
    QObject::connect(worker, &SerialWorker::cardScanned, this, [this](const QString &data) {//Connect cardscanned event of serialworker to lambda
        scanned.enqueue(data); //Add the card id to queue
        emit cardScanned(); //Emit the cardScanned event (called a signal in QT)
    });
    QObject::connect(worker, &SerialWorker::errorOccurred, [](const QString &error) {
        qWarning() << "Serial Error Occurred: " << error; //Print error
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

QString LTx2A::getNext() { //Get the next cardid
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
    rxBuffer.append(data); //Appended new data to buffer
    int newlineindex;
    while ((newlineindex = rxBuffer.indexOf("\n")) != -1) { //Parse lines from read buffer
        QByteArray line = rxBuffer.left(newlineindex); //Grab everything to the left of the newline char
        rxBuffer.remove(0, newlineindex + 1); //Remove it from the buffer
        QString decoded = QString::fromUtf8(line).trimmed(); //Convert to string
        decoded.remove(QChar(0xFFFD)); //Remove unicode missing characters
        if (decoded.isEmpty()) continue; //Skip if blank
        //qDebug() << "Decoded from Serial: " << decoded;
        if (decoded.length() <= 0 || !decoded.contains(";")) continue; //Skip if blank or missing delimeter
        QStringList linedata = decoded.split(";"); //Split by LTx2A data delimeter
        if (!attentionCodes.contains(linedata.at(0).trimmed())) continue; //Ignore messages we dont care about
        QString msgCode = linedata.at(0); //First data item is the code
        linedata.pop_front(); //Remove the code from data list
        handleMessage(msgCode, linedata); //Handler for serial messages
    }
}

void SerialWorker::handleMessage(const QString &code, QStringList data) { //Handle message based on code and data
    //Log::write("SerialWorker", code + "; " + data.join("; "));
    if (code == "20A") {
        //Card detected once
    } else if (code == "20B") { //Card scanned successfully
        emit cardScanned(data[0].trimmed().replace(";",""));
    } else if (code == "20C") {
        //Failed to read card
    } else if (code == "20D") {
        //emit cardScanned(data[0].trimmed());
    }

        /*else if (code == "20E") { //database query result (no longer used since we do db stuff here now)
        if (data.length() < 2) { qCritical() << "Missing data on 20E message"; return;}
        emit cardScanned(parseUserdata(data[0].trimmed(), data[1]));
    }*/
}
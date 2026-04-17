#ifndef LTX2AQT_H
#define LTX2AQT_H

#include <QQueue>
#include <QSet>
#include <QtSerialPort/QSerialPort>
#include <QObject>
#include <QThread>

using namespace std;



class SerialWorker : public QObject { //Serialworker lives in a different thread than LTx2A to not interrupt rendering
    Q_OBJECT
public:
    explicit SerialWorker(QString portName, qint32 baud);
signals:
    void cardScanned(QString userID);
    void errorOccurred(QString error);
public slots:
    void start();
    void stop();
private slots:
    void handleReadyRead();
private:
    QSerialPort* serial = nullptr;
    QByteArray rxBuffer;
    QString portName;
    qint32 baud;
    inline static const QSet<QString> attentionCodes = {"20A", "20B", "20C"};
    void handleMessage(const QString &code, QStringList data);
};

class LTx2A : public QObject {
    Q_OBJECT
public:
    LTx2A(QString portName = "auto", qint32 baud = QSerialPort::BaudRate::Baud115200);
    bool hasNext();
    QString getNext();
    void start();
public slots:
    void stop();
signals:
    void cardScanned();
private:
    QQueue<QString> scanned;
    QThread* thread;
    SerialWorker* worker;
};

#endif // LTX2AQT_H

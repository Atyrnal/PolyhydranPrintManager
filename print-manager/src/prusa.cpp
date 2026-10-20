/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "prusa.h"
#include <QHttpMultiPart>
#include <QFileInfo>
#include <QJsonObject>
#include "errors.hpp"
#include <QTimer>
#include <QJsonDocument>
#include <QJsonParseError>
#include "globalstate.hpp"
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <cstdio>
#include <LibBGCode/convert/convert.hpp>
#include "orca2prusa.hpp"

Prusa::Prusa(QObject* parent) : Printer(parent) {

    //connect(&manager, &QNetworkAccessManager::authenticationRequired, this, &PrusaLink::provideAuth);
    testConnection();
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Prusa::testConnection);
    timer->start(5000);
}

Prusa::Prusa(QString name, QString model, QString hostname, QString apiKey, QString storageType, QObject* parent) : Printer(name, model, "Prusa", parent) {
    this->hostname = hostname;
    this->apiKey = apiKey;
    this->storageType = storageType;
    testConnection();
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer](){
        if (connectedOnce || connectFailCount < 10) testConnection();
        if (!connectedOnce && connectFailCount >= 10) timer->stop();
    });
    timer->start(30000);
}


void Prusa::setHostname(QString hostname) {
    this->hostname = hostname;
}

void Prusa::setApiKey(QString apiKey) {
    this->apiKey = apiKey;
}

void Prusa::setStorageType(QString storageType) {
    this->storageType = storageType;
}

void Prusa::startPrint(const QString &fileName) {
    sendGCode(fileName);
}

Printer::JobStatus Prusa::getJobStatus() {

    if (!connectionStatus) return Printer::JobStatus::Error;
    return this->jobStatus;
}

void Prusa::sendGCode(QString filepath) {
    Log::write("PrusaPrinter("+name+"@"+hostname+")", "Attempting to start print " + filepath);
    QFileInfo fi(filepath);

    if (!useBinaryGCode || fi.suffix().compare("gcode", Qt::CaseInsensitive) != 0) {
        uploadFile(filepath);
        return;
    }

    auto tmp = std::make_shared<QTemporaryDir>();
    if (!tmp->isValid()) { uploadFile(filepath); return; }
    const QString outPath = tmp->filePath(fi.completeBaseName() + ".bgcode");

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, filepath, outPath, tmp]() {
        const QString err = watcher->result();
        watcher->deleteLater();
        if (!err.isEmpty()) {
            Error::softHandle("PrusaBgcodeConvertError", err + " - uploading plain G-code instead");
            uploadFile(filepath);
            return;
        }
        uploadFile(outPath, tmp);
    });
    watcher->setFuture(QtConcurrent::run([this, filepath, outPath] {
        return convertToBgcode(filepath, outPath);
    }));
}

//TODO: allow a limited number of retries if the connection fails initially
void Prusa::testConnection() {
    //Log::write("PrusaPrinter("+name+"@"+hostname+")", "Testing connection...");
    QUrl testUrl(QString("http://%1/api/v1/status").arg(hostname));
    QNetworkRequest testReq(testUrl);
    testReq.setRawHeader("X-Api-Key", apiKey.toUtf8());
    testReq.setTransferTimeout(8000);
    QNetworkReply* testReply = manager.get(testReq);
    QObject::connect(testReply, &QNetworkReply::finished, this, [this, testReply]() {
        if (testReply->error() != QNetworkReply::NoError) {
            if (testReply->error() >= 100 && testReply->error()) Error::softHandle("PrusaPrinterConnectionTestError", testReply->errorString() + " " + QString::number(testReply->error()));
            if (connectionStatus && ++connectFailCount >= stni("prusaConnectionFailThreshold")) {
                emit this->connectionUpdated(false);
                Log::write("PrusaPrinter("+name+"@"+hostname+")", "Disconnected from printer");
            } else if (!connectedOnce && ++connectFailCount <= 1) {
                emit this->connectionUpdated(false);
                Error::handle("PrusaPrinterConnectionTestError", "Unable to connect to printer " + name, El::Warning);
            }
            this->connectionStatus = false;
            return;
        }
        connectFailCount = 0;
        //Success
        if (!connectionStatus) {
            Log::write("PrusaPrinter("+name+"@"+hostname+")", "Connected successfully");
            emit this->connectionUpdated(true);
        }
        this->connectionStatus = true;
        this->connectedOnce = true;
        int statusCode = testReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        //Log::write("PrusaPrinter("+name+"@"+hostname+")", QString("Connection test response status %1").arg(statusCode));
        if (statusCode == 200) {
            QJsonParseError p;
            QJsonDocument response = QJsonDocument::fromJson(testReply->readAll(), &p);
            if (p.error) {
                Error::handle("JsonParseError", "Unable to parse job response JSON", El::Warning);
                this->jobStatus = Printer::JobStatus::Error;
                return;
            }
            QString prev = jobState;
            this->jobState = response.object().value("printer").toObject({}).value("state").toString("NOTFOUND");
            //Log::write("PrusaPrinter("+name+"@"+hostname+")", QString("jobState %1, prev %2").arg(jobState, prev));
            //enum: [ IDLE, BUSY, PRINTING, PAUSED, FINISHED, STOPPED, ERROR, ATTENTION, READY ]
            if (jobState == "PRINTING" || jobState == "PAUSED" || jobState == "BUSY" || jobState == "ATTENTION") this->jobStatus = JobStatus::Busy;
            else if (jobState == "FINISHED" || jobState == "STOPPED" || jobState == "IDLE" || jobState == "READY") this->jobStatus = JobStatus::Idle;
            else this->jobStatus = JobStatus::Error;

            if (jobState != prev && prev != "UNDEFINED") { //Switched states
                Log::write("PrusaPrinter("+name+"@"+hostname+")", QString("jobState updated from %1 to %2").arg(prev, jobState));
                if (jobState == "PRINTING" || prev == "BUSY") {
                    if (prev == "PAUSED" || prev == "ATTENTION") //update latest print back to ongoing
                        emit this->printStatusUpdated(name, "Ongoing");
                } else if (jobState == "PAUSED" || jobState == "ATTENTION") {
                    if (prev == "PRINTING" || prev == "BUSY") //update lastest print to halted
                        emit this->printStatusUpdated(name, "Halted");
                } else if (jobState == "STOPPED" || jobState == "ERROR" || jobState == "IDLE") {
                    if (prev == "PAUSED" || prev == "PRINTING" || prev == "ATTENTION" || prev == "BUSY") //update latest print to failed
                        emit this->printStatusUpdated(name, "Failed");
                } else if (jobState == "FINISHED") { //update latest print to completed
                    emit this->printStatusUpdated(name, "Completed");
                }
            }

        } else this->jobStatus = Printer::JobStatus::Error;
        return;
    });
    // QTimer::singleShot(5000, this, [this, testReply](){
    //     if (!testReply->isFinished()) {
    //         if (connectionStatus && ++connectFailCount >= stni("prusaConnectionFailThreshold", 2)) {
    //             emit this->connectionUpdated(false);
    //             Log::write("PrusaPrinter("+name+"@"+hostname+")", "Disconnected from printer (timed out)");
    //         } else if (!connectedOnce) {
    //             emit this->connectionUpdated(false);
    //             Error::handle("PrusaPrinterConnectionTestError", "Unable to connect to printer " + name, El::Warning);
    //         }
    //         this->connectionStatus = false;
    //     }
    //     testReply->deleteLater();
    // });
}

QString Prusa::convertToBgcode(const QString &src, const QString &dst) {
    QString input = src;

    // OrcaSlicer output needs rewriting into PrusaSlicer's layout first
    QFile probe(src);
    if (probe.open(QIODevice::ReadOnly) && probe.read(512).contains("generated by OrcaSlicer")) {
        const QString pre = dst + ".pre.gcode";   // same temp dir, cleaned up with it
        const std::string err = orcaToPrusaLayout(QFile::encodeName(src).toStdString(),
                                                  QFile::encodeName(pre).toStdString());
        if (!err.empty()) return "Orca preprocessing failed: " + QString::fromStdString(err);
        input = pre;
    }
    probe.close();

    FILE *in = std::fopen(QFile::encodeName(input).constData(), "rb");
    if (!in) return "Cannot open " + src;
    FILE *out = std::fopen(QFile::encodeName(dst).constData(), "wb");
    if (!out) { std::fclose(in); return "Cannot create " + dst; }

    using namespace bgcode;
    binarize::BinarizerConfig cfg;
    // Same settings PrusaSlicer uses by default
    cfg.compression.file_metadata    = core::ECompressionType::None;
    cfg.compression.printer_metadata = core::ECompressionType::None;
    cfg.compression.print_metadata   = core::ECompressionType::None;
    cfg.compression.slicer_metadata  = core::ECompressionType::Deflate;
    cfg.compression.gcode            = core::ECompressionType::Heatshrink_12_4;
    cfg.gcode_encoding    = core::EGCodeEncodingType::MeatPackComments;
    cfg.metadata_encoding = core::EMetadataEncodingType::INI;
    cfg.checksum          = core::EChecksumType::CRC32;

    core::EResult res = convert::from_ascii_to_binary(*in, *out, cfg);
    std::fclose(in);
    std::fclose(out);

    if (res != core::EResult::Success) {
        std::string_view pr = core::translate_result(res);
        return QString("bgcode conversion failed: ") + QString::fromUtf8(pr.data(), pr.size());
    }
    return {};
}

void Prusa::uploadFile(const QString &localPath, std::shared_ptr<QTemporaryDir> tmp) {
    QFileInfo fileInfo(localPath);
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit printStartResult(false);
        Error::handle("PrusaPrinterUploadError", "Cannot open file for upload: " + localPath);
        return;
    }
    QByteArray fileData = file.readAll();
    file.close();

    QUrl uploadUrl(QString("http://%1/api/v1/files/%2/%3").arg(hostname, storageType, fileInfo.fileName()));
    QNetworkRequest uploadReq(uploadUrl);
    uploadReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    uploadReq.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(fileData.size()));
    uploadReq.setRawHeader("X-Api-Key", apiKey.toUtf8());
    uploadReq.setRawHeader("Print-After-Upload", "?1");
    uploadReq.setRawHeader("Overwrite", "?1");

    QNetworkReply *uploadReply = manager.put(uploadReq, fileData);

    connect(uploadReply, &QNetworkReply::uploadProgress, uploadReply, [this, fileInfo](qint64 sent, qint64 total) {
        if (total <= 0) return;
        emit uploadProgressUpdated(fileInfo.fileName(), total, sent);
    });

    // Capturing tmp keeps the temp dir alive until the upload finishes
    connect(uploadReply, &QNetworkReply::finished, uploadReply, [this, uploadReply, tmp]() {
        if (uploadReply->error() != QNetworkReply::NoError) {
            emit printStartResult(false);
            Error::handle("PrusaPrinterUploadError", uploadReply->errorString());
        } else {
            emit printStartResult(true);
            Log::write("PrusaPrinter("+name+"@"+hostname+")", "Print upload succeeded");
        }
        uploadReply->deleteLater();
    });
}

/*void PrusaLink::sendGCode(QString filepath, QString hostname, QString password, QString storageName) {


    QUrl uploadUrl(QString("http://%1/api/v1/storage/%2/upload").arg(PRUSALINK_HOSTNAME, storageName));
    QNetworkRequest uploadReq(uploadUrl);
    uploadReq.setRawHeader("X-Api-Key", password.toUtf8());

    QHttpMultiPart *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"" + QFileInfo(filepath).fileName() + "\""));
    QFile *file = new QFile(filepath);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for upload:" << filepath;
        delete multi;
        delete file;
        return;
    }
    filePart.setBodyDevice(file);

    multi->append(filePart);

    QNetworkReply *uploadReply = manager.put(uploadReq, multi);
    multi->setParent(uploadReply);
    file->setParent(multi);

    QObject::connect(uploadReply, &QNetworkReply::finished, uploadReply, [=]() {
        if (uploadReply->error() != QNetworkReply::NoError) {
            qWarning() << "Upload failed:" << uploadReply->errorString();
            uploadReply->deleteLater();
            return;
        }
        QByteArray resp = uploadReply->readAll();
        qDebug() << "Upload succeeded response:" << resp;

        // 2) Start the print job
        QUrl printUrl(QString("http://%1/api/v1/job").arg(hostname));
        QNetworkRequest printReq(printUrl);
        printReq.setRawHeader("X-Api-Key", password.toUtf8());
        printReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject bodyObj;
        bodyObj["storage"] = storageName;
        bodyObj["filename"] = QFileInfo(filepath).fileName();
        // bodyObj["startImmediately"] = true;

        QJsonDocument doc(bodyObj);
        QByteArray body = doc.toJson();

        QNetworkReply *printReply = manager.post(printReq, body);
        QObject::connect(printReply, &QNetworkReply::finished, [=]() {
            if (printReply->error() != QNetworkReply::NoError) {
                qWarning() << "Start print failed:" << printReply->errorString();
            } else {
                qDebug() << "Print started, response:" << printReply->readAll();
            }
            printReply->deleteLater();
        });

        uploadReply->deleteLater();
    });
}*/

/*void PrusaLink::provideAuth(QNetworkReply *, QAuthenticator *authenticator) {
    authenticator->setUser(PRUSALINK_USERNAME);        // your username
    authenticator->setPassword(PRUSALINK_PASSWORD); // your password
}*/

/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "octoprintemulator.h"
#include <QJsonObject>
#include <QTcpServer>
#include <QHttpServerRequest>
#include <QFile>
#include <QDir>
#include <QHttpServerResponse>
#include "gcodeparser.h"
#include "globalstate.hpp"

OctoprintEmulator::OctoprintEmulator(quint16 port, QObject* parent) : QObject(parent), server(), port() {
    /*server.route("/api/printer", []() {
        qDebug() << "/api/printer called!";
        return QJsonObject {
            {"state", QJsonObject{{"text", "Operational"}}},
            {"temperature", QJsonObject{{"tool0", QJsonObject{{"actual", 200}, {"target", 210}}}}}
        };
    });*/

    /*server.route("/api/server", []() {
        qDebug() << "/api/server called!";
        return QJsonObject {
            {"version", "1.5.0"},
            {"safemode", "incomplete_startup"}
        };
    });*/

    server.route("/api/version", []() { //Hey OctoPrint is over here!!! //emulate octoprint version api endpoint
        //qDebug() << "/api/version called!";
        return QJsonObject {
            {"api", "0.1"},
            {"version", "1.3.10"},
            {"text", "OctoPrint 1.3.10"}
        }; //yes this is definitely octoprint
    });

    //This is the api endpoint OrcaSlicer calls to upload the print file
    server.route("/api/files/<arg>", this, [this](const QString &location, const QHttpServerRequest &request) -> QHttpServerResponse {
        //qDebug() << "/api/files called!";

        //Ensure the content type matches the expected for file upload
        if (!request.headers().contains("Content-Type") || !request.headers().value("Content-Type").contains("multipart/form-data")) {
            return QHttpServerResponse("Expected multipart/form-data", QHttpServerResponder::StatusCode::BadRequest);
        }

        QByteArray body = request.body();

        // Extract boundary from Content-Type header (this part is safe as a string, it's ASCII)
        QString contentType = QString(request.headers().value("Content-Type").toByteArray());
        QByteArray boundary;
        static QRegularExpression re("boundary=(.+)");
        QRegularExpressionMatch match = re.match(contentType);
        if (match.hasMatch()) {
            boundary = ("--" + match.captured(1).trimmed()).toUtf8();
        } else {
            return QHttpServerResponse("No boundary found", QHttpServerResponder::StatusCode::BadRequest);
        }

        // Split body by boundary entirely in QByteArray - never convert to QString
        QList<QByteArray> parts;
        int pos = 0;
        while (true) {
            int next = body.indexOf(boundary, pos);
            if (next == -1) break;
            if (pos != 0) parts.append(body.mid(pos, next - pos));
            pos = next + boundary.size();
            if (body.mid(pos, 2) == "--") break; // final boundary
            if (body.mid(pos, 2) == "\r\n") pos += 2;
            else if (body.mid(pos, 1) == "\n") pos += 1;
        }

        QByteArray fileData;
        QString originalFileName = "uploaded.gcode";
        bool selectFlag = false;
        bool printFlag = false;

        for (const QByteArray &part : parts) {
            // Headers end at the first \r\n\r\n or \n\n - only parse headers as string (they're ASCII)
            int headerEnd = part.indexOf("\r\n\r\n");
            int skip = 4;
            if (headerEnd == -1) { headerEnd = part.indexOf("\n\n"); skip = 2; }
            if (headerEnd == -1) continue;

            QString headers = QString::fromUtf8(part.left(headerEnd)); // safe: headers are ASCII
            QByteArray partBody = part.mid(headerEnd + skip);
            // Strip trailing \r\n
            if (partBody.endsWith("\r\n")) partBody.chop(2);
            else if (partBody.endsWith("\n")) partBody.chop(1);

            if (headers.contains("name=\"file\"")) {
                // Extract filename from headers (safe as string)
                static QRegularExpression fileNameRe(R"delim(filename="([^"]+)")delim");
                QRegularExpressionMatch m = fileNameRe.match(headers);
                if (m.hasMatch()) originalFileName = m.captured(1);
                fileData = partBody; // raw bytes, never touched QString
            } else if (headers.contains("name=\"select\"")) {
                selectFlag = QString::fromUtf8(partBody).trimmed() == "true";
            } else if (headers.contains("name=\"print\"")) {
                printFlag = QString::fromUtf8(partBody).trimmed() == "true";
            }
        }
        QString filePath = gsi.getDataDirPath() + "/uploaded/" + originalFileName;
        //make the "uploaded" dir if it doesnt exist
        QDir uploadDir = QDir(gsi.getDataDirPath() + "/uploaded");
        if (!uploadDir.exists()) {
            uploadDir.mkpath(".");
        }
        //Delete all files in the upload dir (we only want to store one print file at a time)
        QFileInfoList oldfiles = uploadDir.entryInfoList(QDir::Files);
        for (int i = 0; i < oldfiles.size(); ++i) {
            const QFileInfo &fileInfo = oldfiles.at(i);
            if (!uploadDir.remove(fileInfo.fileName())) {
                Error::handle("OctoprintEmualtor", "Failed to remove file: " + fileInfo.fileName(), El::Warning);
            }
        }

        //Write the new print file based on the data
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return QHttpServerResponse("Failed to write file", QHttpServerResponder::StatusCode::InternalServerError);
        }
        file.write(fileData);
        file.close();

        this->fileInfo = new QFileInfo(filePath); //Store file info about saved file

        // Parse the gcode properties
        auto propertiesEo = GCodeParser::parseFile(fileInfo->absoluteFilePath());
        if (propertiesEo.isError()) {
            propertiesEo.handle();
            return QHttpServerResponse("Failed to parse gcode", QHttpServerResponder::StatusCode::InternalServerError);
        }
        auto properties= propertiesEo.get();
        properties.insert("filename", originalFileName); //insert the filename into the properties


        //Emit signals to main loop and QML
        emit jobLoaded(fileInfo->absoluteFilePath(), properties);

        // Build JSON response
        QJsonObject localFile{
            {"name", originalFileName},
            {"path", filePath},
            {"type", "machinecode"},
            {"origin", location},
            {"refs", QJsonObject{
                         {"resource", QString("http://localhost:5000/api/files/%1/%2").arg(location, filePath)},
                         {"download", QString("http://localhost:5000/downloads/files/%1/%2").arg(location, filePath)}
                     }}
        };
        QJsonObject files{{location, localFile}};
        QJsonObject response{
            {"files", files},
            {"done", true},
            {"effectiveSelect", true},
            {"effectivePrint", false}
        };

        //return fake OK response to OrcaSlicer so it thinks everything worked as it expected and this is an octoprint instance
        return QHttpServerResponse(response, QHttpServerResponder::StatusCode::Created);
    });

    /*server.route("/api/job", this, [this](const QHttpServerRequest &request) {
        qDebug() << "/api/job called!";
        if (!request.headers().contains("Content-Type") || request.headers().value("Content-Type") != "application/json") return QHttpServerResponse("Expected Content-Type application/json", QHttpServerResponder::StatusCode::BadRequest);
        QJsonParseError error;
        QJsonObject obj;
        QJsonDocument doc = QJsonDocument::fromJson(request.body(), &error);
        if (error.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                obj = doc.object();
            }
        } else {
            qWarning() << "Failed to parse JSON:" << error.errorString();
            return QHttpServerResponse("Failed to parse json", QHttpServerResponder::StatusCode::InternalServerError);
        }
        if (!obj.keys().contains("command")) return QHttpServerResponse("Expected command", QHttpServerResponder::StatusCode::BadRequest);
        if (obj.value("command") != "start") return QHttpServerResponse(QHttpServerResponder::StatusCode::NotImplemented);
        QMap<QString, QString> properties = GCodeParser::parseFile(fileInfo->absoluteFilePath());
        QVariantMap propertiesForJS;
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            propertiesForJS.insert(it.key(), it.value());
        }
        emit jobLoaded(fileInfo->absoluteFilePath(), properties);
        return QHttpServerResponse(QHttpServerResponder::StatusCode::NoContent);
    });*/



    /*server.route("/api/files/<arg>", this, [this](const QString &location, const QHttpServerRequest &request) -> QHttpServerResponse {
        qDebug() << "/api/files called!";
        if (!request.headers().contains("Content-Type") || !request.headers().value("Content-Type").contains("multipart/form-data")) {
            return QHttpServerResponse("Expected multipart/form-data", QHttpServerResponder::StatusCode::BadRequest);
        }

        QByteArray body = request.body();
        QString filename = "uploaded.gcode";
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            return QHttpServerResponse("Failed to write file", QHttpServerResponder::StatusCode::InternalServerError);
        }
        file.write(body);
        file.close();
        this->fileInfo = QFileInfo(filename);

        QMap<QString, QString> properties = GCodeParser::parseFile(fileInfo.absoluteFilePath());
        QVariantMap propertiesForJS;
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            propertiesForJS.insert(it.key(), it.value());
        }
        emit jobLoaded(fileInfo.absoluteFilePath(), properties);

        QJsonObject localFile{
            {"name", filename},
            {"path", filename},
            {"type", "machinecode"},
            {"origin", location},
            {"refs", QJsonObject{
                         {"resource", QString("http://localhost:5000/api/files/%1/%2").arg(location, filename)},
                         {"download", QString("http://localhost:5000/downloads/files/%1/%2").arg(location, filename)}
                     }}
        };
        QJsonObject files{{location, localFile}};
        QJsonObject response{
            {"files", files},
            {"done", true},
            {"effectiveSelect", true},
            {"effectivePrint", false}
        };

        return QHttpServerResponse(response);
    });*/

    /*server.route("<arg>", [](const QString &everything, const QHttpServerRequest &request) -> QHttpServerResponse {
        qDebug() << "Endpoint: " << everything;
        qDebug() << "Headers:" << request.headers();
        qDebug() << "Body size:" << request.body().size();

        return QHttpServerResponse(QHttpServerResponder::StatusCode::Ok);
    });*/



    QTcpServer* tcp = new QTcpServer(this); // create TCP server
    if (!tcp->listen(QHostAddress::LocalHost, port)) { // start listening
        Error::handle("OctoprintEmulator", "Failed to start TCP server on port " + QString(port), El::Critical);
        return;
    }

    if (!server.bind(tcp)) { // bind QHttpServer to TCP server
        //qCritical() << "Failed to bind QHttpServer to TCP server";
        Error::handle("OctoprintEmulator", "Failed to bind to HttpServer to TCP server", El::Critical);
        return;
    }
}

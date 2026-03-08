/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "headers/octoprintemulator.h"
#include <QJsonObject>
#include <QTcpServer>
#include <QHttpServerRequest>
#include <QFile>
#include <QDir>
#include <QHttpServerResponse>
#include "headers/gcodeparser.h"

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

        QByteArray body = request.body(); //Get request body

        // Extract multipart boundary from Content-Type (We are seperating the actual print file from other request information)
        QString contentType = QString(request.headers().value("Content-Type").toByteArray()); //Get the content-type header as bytes and make a string
        QString boundary; //Store boundary
        static QRegularExpression re("boundary=(.+)"); //search for the string "boundary="
        QRegularExpressionMatch match = re.match(contentType); //And capture whatever follows it
        if (match.hasMatch()) {
            boundary = "--" + match.captured(1); //save the boundary string
        } else { //Malformed multipart request
            return QHttpServerResponse("No boundary found", QHttpServerResponder::StatusCode::BadRequest);
        }


        // convert to string for easier processing
        QString bodyStr = QString::fromUtf8(body);

        //find filename
        static QRegularExpression fileNameRe(R"delim(filename="([^"]+)")delim"); //Find string filename=
        QRegularExpressionMatch fileNameMatch = fileNameRe.match(bodyStr); //Match what follows it
        QString originalFileName = fileNameMatch.hasMatch() ? fileNameMatch.captured(1) : "uploaded.gcode"; //if it exists, that will be our filename, otherwise default to "uploaded.gcode"
        QString filePath = "uploaded/" + originalFileName; //set the file path to save the streamed file to

        QList<QString> multipartPartsStr = bodyStr.split(boundary); //Split the request body by the boundry (into its seperate parts)
        //Convert the strings to raw bytes
        QList<QByteArray> multipartParts;
        for (auto it = multipartPartsStr.constBegin(); it != multipartPartsStr.constEnd(); ++it) { //iterator since we cant use range loop
            multipartParts.append(it->toUtf8());
        }

        // Variables to hold file and other fields
        QByteArray fileData;
        bool selectFlag = false;
        bool printFlag = false;

        for (const QByteArray &partRaw : multipartParts) { //Iterate over each multipart
            if (partRaw.trimmed().isEmpty()) continue; //if part empty skip it

            QString part = QString::fromUtf8(partRaw); //Convert part to string
            // Check if this is the file part
            if (part.contains("name=\"file\"")) {
                // Extract file data: after empty line
                int index = part.indexOf("\r\n\r\n");
                if (index < 0) index = part.indexOf("\n\n");
                if (index >= 0) {
                    fileData = partRaw.mid(index + 4); // skip headers
                    // remove any trailing boundary markers
                    int boundaryIndex = fileData.indexOf("\r\n--");
                    if (boundaryIndex >= 0) fileData.truncate(boundaryIndex);
                }
            }

            // Check select/print fields
            if (part.contains("name=\"select\"")) {
                selectFlag = part.contains("true");
            }
            if (part.contains("name=\"print\"")) {
                printFlag = part.contains("true");
            }
        }
        //make the "uploaded" dir if it doesnt exist
        QDir uploadDir = QDir("uploaded");
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
        QVariantMap propertiesForJS; //Convert to QVariantMap for use in QML
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            propertiesForJS.insert(it.key(), it.value());
        }

        //Emit signals to main loop and QML
        emit jobInfoLoaded(propertiesForJS);
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
        emit jobInfoLoaded(propertiesForJS);
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
        emit jobInfoLoaded(propertiesForJS);
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

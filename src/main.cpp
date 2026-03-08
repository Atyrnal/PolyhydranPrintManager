/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QSqlError>
#include <QSqlQuery>
#include <QQmlContext>
#include "headers/qtbackend.h"
#include <QQuickWindow>
#include <QFile>
#include <QDir>
#include <QSslSocket>

//Atyrnal 10/29/2025

//See ui/Main.qml for ui declarations

Eo<QJsonObject> readJsonFile(const QString &filepath, quint64 maxSize = 0) {
    using eop = Eo<QJsonObject>;
    QFile f = QFile(filepath);
    if (!f.exists()) return eop("JsonFileNotFoundError", "Unable to locate: " + filepath, El::Trivial);
    if (!f.open(QFile::ReadOnly)) return eop("JsonFileOpenError", "Unable to open file: " + filepath, El::Warning);
    if (maxSize > 0 && f.size() > maxSize) return eop("JsonFileSizeError", "File size " + QString::number(f.size()) + " larger than maximum " + QString::number(maxSize), El::Trivial);
    QByteArray data = f.read(f.size());
    f.close();
    QJsonParseError p;
    QJsonDocument doc = QJsonDocument::fromJson(data, &p);
    if (p.error != QJsonParseError::NoError) return eop("JsonParseError", p.errorString(), El::Warning);

    if (!doc.isObject()) return eop("JsonParseError", "Document is not Json object", El::Warning);

    QJsonObject obj = doc.object();

    return eop(obj);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv); //Create a QT gui app

    QQmlApplicationEngine engine; //Qt engine creation
    QObject::connect( //Exit if object creation fails
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    QTBackend bk(&app, &engine, &engine);
    engine.loadFromModule("PCMakerspace3DPKiosk", "Main"); //Load the QML Main.qml declarative ui file

    QObject* root = engine.rootObjects().at(0); //Get the root object (in this case the Window)
    bk.setRoot(root);

    auto config = readJsonFile(QDir(QCoreApplication::applicationDirPath()).filePath("configuration.json"), 1000000);
    if (config.isError()) {
        config.softHandle();
    } else {
        bk.loadConfig(config.get());
    }


    QQuickWindow* window = qobject_cast<QQuickWindow*>(root); //Cast to QWindow
    QIcon icon = QIcon("PCMakerspace3DPKiosk/resources/PC-Logo.ico");
    window->setIcon(icon); //Set window icon

    //runs when RFID card is scanned successfully

    bk.queryDatabase("DROP TABLE users"); //Demo stuff

    bk.queryDatabase("CREATE TABLE IF NOT EXISTS users (" //Demo Stuff
                    "id VARCHAR(50) PRIMARY KEY, "
                    "firstName VARCHAR(50), "
                    "lastName VARCHAR(50), "
                    "email VARCHAR(80), "
                    "umass BOOLEAN, "
                    "cics BOOLEAN, "
                    "trainingCompleted BOOLEAN, "
                    "authLevel SMALLINT, "
                    "printsStarted INT, "
                    "filamentUsedGrams DOUBLE, "
                    "printHours DOUBLE)"
                     ).handle();

    // bk.queryDatabase("UPDATE users SET authLevel = 2 WHERE id = :id;", {{":id", "09936544703440683676"}}).softHandle();
    //Demo Stuff

    bk.queryDatabase("INSERT INTO users (id, firstName, lastName, email, umass, cics, trainingCompleted, authLevel, printsStarted, filamentUsedGrams, printHours) "
                     "VALUES ('09936544703440683676', 'Antony', 'Rinaldi', 'ajrinaldi@umass.edu', :um, :cs, :tc, :al, 0, 0.0, 0.0);", {
                        {":um", true},
                        {":cs", true},
                        {":tc", true},
                        {":al", 2}
                     }).handle();

    //Create second user for demo
    bk.queryDatabase("INSERT INTO users (id, firstName, lastName, email, umass, cics, trainingCompleted, authLevel, printsStarted, filamentUsedGrams, printHours) "
                       "VALUES ('', 'Judge', 'Judy', 'judge@judy.com', :um, :cs, :tc, :al, 0, 0.0, 0.0);", {
                      {":um", true},
                      {":cs", true},
                      {":tc", false},
                      {":al", 0}
                     }).handle();

    // auto result = bk.queryDatabase("SELECT firstName, lastName FROM users WHERE id = :id LIMIT 1", {{":id", "09936544703440683676000"}});
    // qDebug() << result;
    // result.softHandle();

    return app.exec(); //run the app
}

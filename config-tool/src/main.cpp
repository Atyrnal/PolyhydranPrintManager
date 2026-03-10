/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFile>
#include <QDir>
#include <QSslSocket>

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

    engine.loadFromModule("PolyhydranPrintManagerConfigTool", "Startup");

    QObject* root = engine.rootObjects().at(0); //Get the root object (in this case the Window)
    QQuickWindow* window = qobject_cast<QQuickWindow*>(root); //Cast to QWindow
    QIcon icon = QIcon("PolyhydranPrintManagerConfigTool/resources/PC-Logo.ico");
    window->setIcon(icon); //Set window icon

    return app.exec(); //run the app
}

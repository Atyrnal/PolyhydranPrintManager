/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#ifndef OCTOPRINTEMULATOR_H
#define OCTOPRINTEMULATOR_H

#include <QObject>
#include <QHttpServer>
#include <QFileInfo>

class OctoprintEmulator : public QObject {
    Q_OBJECT
public:
    OctoprintEmulator(quint16 port = 5000, QObject* parent = 0);
signals:
    void jobLoaded(const QString &filepath, const QMap<QString, QString> &properties);
    void jobInfoLoaded(const QVariantMap &properties);
private:
    quint16 port;
    QHttpServer server;
    QFileInfo* fileInfo = nullptr;
};

#endif // OCTOPRINTEMULATOR_H

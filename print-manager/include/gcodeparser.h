/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#ifndef GCODEPARSER_H
#define GCODEPARSER_H

#include <QString>
#include <QFile>
#include <QVector>
#include <QMap>
#include <QFileInfo>
#include "errors.hpp"

/*struct PrintData {
    QString printer_settings;
    QString filament_settings;
    QString print_settings;
    QString filename;
    QString filament_weight;
    QString print_time;
};*/

class GCodeParser
{
public:
    static Eo<QMap<QString, QString>> parseFile(QString filepath);
private:
    static const int LINE_COUNT = 600;
    static const int BLINE_COUNT = 100;
    static inline const std::vector<QString> GCODE_TARGETS = std::vector<QString>({"total filament used [g]", "filament_type", "printer_settings_id", "estimated printing time (normal mode)", "filament_settings_id", "print_settings_id", "filament used [g]"});
    static inline const std::vector<QString> BGCODE_TARGETS = std::vector<QString>({"filament used [g]", "filament_type", "printer_model", "estimated printing time (normal mode)"});
    static QVector<QString> readGCode(QFile file);
    static QVector<QString> readGCode(QByteArray raw);
    static QVector<QString> readBGCode(QFile file);
    static QMap<QString, QString> parseGCode(QVector<QString> lines);
    static QMap<QString, QString> parseBGCode(QVector<QString> lines);
    static QMap<QString, QByteArray> extractGCode3mf(const QString &filepath);
};

#endif // GCODEPARSER_H

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
    static Eo<QMap<QString, QString>> parse3mfFile(QString filepath, quint16 platenum);
private:
    static inline const std::vector<QString> GCODE_TARGETS = std::vector<QString>({"total filament used [g]", "filament_type", "printer_settings_id", "estimated printing time (normal mode)", "filament_settings_id", "print_settings_id", "filament used [g]", "sparse_infill_density", "layer_height"});
    static inline const std::vector<QString> BGCODE_TARGETS = std::vector<QString>({"filament used [g]", "filament_type", "printer_model", "estimated printing time (normal mode)", "fill_density", "layer_height"});
    static QMap<QString, QString> streamParseGCode(const QString &filepath);
    static QMap<QString, QString> streamParseGCode(const QByteArray &raw);
    static QMap<QString, QString> streamParseBGCode(const QString &filepath);
    static QMap<QString, QString> scanGCode(QIODevice &dev);           // shared core for file + buffer
    static QMap<QString, QString> formatGCode(QMap<QString, QString> properties);
    static QMap<QString, QString> formatBGCode(QMap<QString, QString> properties);
    static void parseGCodeLine(QMap<QString, QString> &properties, const QString &line);
    static void parseBGCodeLine(QMap<QString, QString> &properties, const QString &line);
    static bool gcodeComplete(const QMap<QString, QString> &p);
    static bool bgcodeComplete(const QMap<QString, QString> &p);
    static QPair<bool, QByteArray> extractGCode3mfPlate(const QString &filepath, quint16 platenum, bool fallbackToFirst = false);
};

#endif // GCODEPARSER_H

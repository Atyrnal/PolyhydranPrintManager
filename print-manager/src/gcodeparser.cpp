/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

#include "gcodeparser.h"
#include <QFileInfo>
#include <QDebug>
#include <zip.h>


#define min(x, y) ((x<y) ? x : y)

Eo<QMap<QString, QString>> GCodeParser::parseFile(QString filepath) {
    using eop = Eo<QMap<QString, QString>>;
    if (!QFile::exists(filepath)) {
        return eop("GcodeFileNotFoundError", "Unable to locate: " + filepath, El::Warning);
    }
    QFileInfo fileInfo = QFileInfo(filepath);
    QMap<QString, QString> output;
    if (fileInfo.fileName().toLower().endsWith(".gcode")) {
        Log::write("GCodeParser", "Parsing .gcode file");
        output = parseGCode(readGCode(QFile(filepath)));
    } else if (fileInfo.fileName().toLower().endsWith(".bgcode")) {
        Log::write("GCodeParser", "Parsing .bgcode file");
        output = parseBGCode(readBGCode(QFile(filepath)));
    } else if (fileInfo.fileName().toLower().endsWith(".gcode.3mf")) {
        Log::write("GCodeParser", "Parsing .gcode.3mf file");
        QMap<QString, QByteArray> rawPlates = extractGCode3mf(filepath);
        if (rawPlates.empty()) {
            return eop("3mfGcodeNotFoundError", "Unable to locate gcode file within: " + filepath, El::Warning);
        }
        QByteArray plate1 = (rawPlates.contains("plate_1.gcode")) ? rawPlates.value("plate_1.gcode") : rawPlates.first();
        output = parseGCode(readGCode(plate1));
        output.insert("plateName", "plate_1");
    } else {
        return eop("GcodeInvalidFiletypeError", "File has invalid filetype: " + filepath, El::Warning);
    }

    output.insert("filename", QFileInfo(filepath).fileName());
    return eop(output);
}

Eo<QMap<QString, QString>> GCodeParser::parse3mfFile(QString filepath, quint16 platenum) {
    using eop = Eo<QMap<QString, QString>>;
    if (!QFile::exists(filepath)) {
        return eop("GcodeFileNotFoundError", "Unable to locate: " + filepath, El::Warning);
    }
    QFileInfo fileInfo = QFileInfo(filepath);
    QMap<QString, QString> output;
    if (fileInfo.fileName().toLower().endsWith(".gcode.3mf")) {
        //Log::write("GCodeParser", "Parsing .gcode.3mf");
        QMap<QString, QByteArray> rawPlates = extractGCode3mf(filepath);
        if (rawPlates.empty()) {
            return eop("3mfGcodeNotFoundError", "Unable to locate gcode file within: " + filepath, El::Warning);
        }
        //auto keys = rawPlates.keys();
        //Log::write("GCodeParser", "Loaded Plates: " + keys.join(", "));
        QByteArray plate;
        if (rawPlates.contains(QString("plate_%1.gcode").arg(platenum))) plate = rawPlates.value(QString("plate_%1.gcode").arg(platenum));
        else if (rawPlates.contains(QString("metadata/plate_%1.gcode").arg(platenum))) plate = rawPlates.value(QString("metadata/plate_%1.gcode").arg(platenum));
        else return eop("GcodeParseError", "Specified plate not found: " + QString("plate_%1.gcode").arg(platenum), El::Warning);
        output = parseGCode(readGCode(plate));
        output.insert("plateName", QString("plate_%1").arg(platenum));
    } else {
        return eop("GcodeInvalidFiletypeError", "File is not gcode 3mf: " + filepath, El::Warning);
    }

    output.insert("filename", QFileInfo(filepath).fileName());
    return eop(output);
}

QMap<QString, QByteArray> GCodeParser::extractGCode3mf(const QString &filepath) {
    int err = 0;
    zip_t* za = zip_open(filepath.toUtf8().constData(), ZIP_RDONLY, &err);
    QMap<QString, QByteArray> output = QMap<QString, QByteArray>();
    if (!za) return output;

    struct zip_stat st;
    zip_stat_init(&st);


    zip_int64_t num = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < num; i++) {
        if (zip_stat_index(za, i, 0, &st) != 0) continue;
        QString name = QString::fromUtf8(st.name);
        if (!name.endsWith(".gcode", Qt::CaseInsensitive)) continue;

        zip_file_t* zf = zip_fopen_index(za, i, 0);
        if (!zf) continue;

        QByteArray out;
        out.resize(st.size);

        zip_int64_t bytesRead = zip_fread(zf, out.data(), st.size);
        zip_fclose(zf);

        if (bytesRead != st.size) continue;

        output.insert(name.toLower(), out);
    }
    zip_close(za);
    return output;
}

QMap<QString, QString> GCodeParser::parseGCode(QVector<QString> lines) {
    QMap<QString, QString> properties = QMap<QString, QString>();
    //qDebug() << lines.size();
    for (int i = 0; i < min(lines.size(), 6000); i++) {
        QString line = lines[i];
        for (int p = 0; p < GCODE_TARGETS.size(); p++) {
            if (line.startsWith("; " + GCODE_TARGETS[p] + " = ")) {
               // Log::write("GCodeParser", line);
                QStringList splitLine = line.split("=");
                splitLine.pop_front();
                QString v = splitLine.join("=").trimmed();
                properties.insert(GCODE_TARGETS[p], v);
                break;
            }
        }
        if (line.startsWith("; model printing time:")) { //Bambu abnormality
            QStringList splitLine = line.split(";")[1].split(":");
            splitLine.pop_front();
            QString v = splitLine.join(":").trimmed();
            properties.insert(GCODE_TARGETS[3], v);
        }
        if (properties.size() >= GCODE_TARGETS.size()) {
            break;
        }
    }
    QMap<QString, QString> output = {
        {"printer", properties[GCODE_TARGETS[2]]},
        {"filamentType", properties[GCODE_TARGETS[1]].replace(";", ", ")},
        {"printSettings", properties[GCODE_TARGETS[5]]},
        {"duration", properties[GCODE_TARGETS[3]]},
    };
    QList<QPair<QString, QString>> filaments= QList<QPair<QString, QString>>();
    auto j = ((properties.contains(GCODE_TARGETS[0])) ? properties[GCODE_TARGETS[0]] : properties[GCODE_TARGETS[6]]).split(",");
    auto k = properties[GCODE_TARGETS[4]].replace("\"", "").split(";");
    double totalWeight = 0.0;
    for (int i = 0; i < min(j.length(), k.length()); i++) {
        j[i] = j[i].trimmed();
        k[i] = k[i].split("@").at(0).trimmed();
        bool ok = false;
        double weight = j[i].toDouble(&ok);
        if (ok && weight > 0) {
            filaments.append({k[i], j[i]+"g"});
            totalWeight += weight;
        }
    }
    QStringList filamentstr = QStringList();
    QStringList weightstr = QStringList();
    for (int i = 0; i < filaments.length(); i++) {
        filamentstr.append(filaments[i].first);
        weightstr.append(filaments[i].second);
    }
    output.insert("filament", filamentstr.join(", "));
    output.insert("weight", weightstr.join(", "));
    output.insert("filamentCount", QString::number(filamentstr.length()));
    output.insert("totalWeight", QString::number(totalWeight)+"g");
    return output;
}

QMap<QString, QString> GCodeParser::parseBGCode(QVector<QString> lines) {
    QMap<QString, QString> properties = QMap<QString, QString>();
    //qDebug() << lines.size();
    for (int i = 0; i < min(lines.size(), 2000); i++) {
        QString line = lines[i];
        for (int p = 0; p < BGCODE_TARGETS.size(); p++) {
            if (line.contains(BGCODE_TARGETS[p] + "=")) {
                QStringList splitLine = line.split("=");
                splitLine.pop_front();
                QString v = splitLine.join("=").trimmed();
                properties.insert(BGCODE_TARGETS[p], v);
                break;
            }
        }
        if (properties.size() >= BGCODE_TARGETS.size()) break;
    }
    QMap<QString, QString> output = {
        {"printer", properties[BGCODE_TARGETS[2]]},
        {"filamentType", properties[BGCODE_TARGETS[1]]},
        {"weight", properties[BGCODE_TARGETS[0]]},
        {"duration", properties[BGCODE_TARGETS[3]]},
    };
    return output;
}

QVector<QString> GCodeParser::readGCode(QFile f) {
    if (f.open(QFile::OpenModeFlag::ReadOnly)) {
        QString plainText;
        if (f.size() > LINE_COUNT*60*2) {
            f.seek(0);
            bool isBambu = false;
            QByteArray firstFewData = f.read(10*60);
            QStringList firstFewLines = QString(firstFewData).split("\n");
            for (int i = 0; i < firstFewLines.size(); i++) {
                QString line = firstFewLines[i];
                if (line.startsWith("; model printing time:")) {
                    isBambu = true;
                    break;
                }
                if (line.startsWith("; THUMBNAIL_BLOCK_START")) {
                    isBambu = false;
                    break;
                }
            }
            if (isBambu) {
                f.seek(0);
                QByteArray firstData = f.read(LINE_COUNT * 40);
                f.seek(LINE_COUNT*40 + 8000);
                QByteArray secondData = f.read(LINE_COUNT * 40);
                f.seek(f.size() - LINE_COUNT*60); //End of file
                QByteArray endData = f.read(LINE_COUNT * 60);
                plainText = QString(firstData) + "\n" +QString(secondData)+ "\n" + QString(endData);
            } else {
                f.seek(f.size() - LINE_COUNT*60);
                QByteArray data = f.read(LINE_COUNT * 60);
                plainText = QString(data);
            }

        } else {
            QByteArray data = f.read(f.size());
            plainText = QString(data);
        }
        //Log::write("GCodeParser", plainText);
        QVector<QString> lines = plainText.split("\n");
        f.close();
        return lines;
    }
    f.close();
    return QVector<QString>();
}

QVector<QString> GCodeParser::readGCode(QByteArray raw) {
    QString plainText;
    if (raw.size() > LINE_COUNT*60*2) {
        bool isBambu = false;
        QByteArray firstFewData = raw.first(10*60);
        QStringList firstFewLines = QString(firstFewData).split("\n");
        for (int i = 0; i < firstFewLines.size(); i++) {
            QString line = firstFewLines[i];
            if (line.startsWith("; model printing time:")) {
                isBambu = true;
                break;
            }
            if (line.startsWith("; THUMBNAIL_BLOCK_START")) {
                isBambu = false;
                break;
            }
        }
        if (isBambu) {
            QByteArray firstData = raw.first(LINE_COUNT*40);
            QByteArray secondData = raw.mid(LINE_COUNT*40 + 8000, LINE_COUNT*50);
            QByteArray endData = raw.last(LINE_COUNT*60);
            plainText = QString(firstData) + "\n" + QString(secondData) + "\n" + QString(endData);
        } else {
            QByteArray data = raw.last(LINE_COUNT*60);
            plainText = QString(data);
        }
    } else {
        plainText = QString(raw);
    }
    //Log::write("GCodeParser", plainText);
    return plainText.split("\n");
}

QVector<QString> GCodeParser::readBGCode(QFile f) {
    if (f.open(QFile::OpenModeFlag::ReadOnly)) {
        QByteArray data;
        data = f.read(min(f.size(), BLINE_COUNT*60));
        QString plainText = QString(data);
        plainText.replace(QChar(0xFFFD), "\n");
        QVector<QString> lines = plainText.split("\n");
        f.close();
        return lines;
    }
    f.close();
    return QVector<QString>();
}

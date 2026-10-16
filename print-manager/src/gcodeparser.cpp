/*
 *
 * Copyright (c) 2026 Antony Rinaldi
 *
*/

#include "gcodeparser.h"
#include <QFileInfo>
#include <QBuffer>
#include <QtEndian>
#include <QDebug>
#include <zip.h>

//Helpers
namespace {

enum class LineKind { Blank, Comment, Code };

// Classifies a raw line by its first non-whitespace character.
LineKind classify(const QByteArray &line) {
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return (c == ';') ? LineKind::Comment : LineKind::Code;
    }
    return LineKind::Blank;
}

// Calls fn(line) for every line of a seekable device, from the last line backwards,
// stopping at byte offset `stopAt` (which must be the start of a line). The line passed
// to fn has no trailing '\n'. fn returns false to stop early.
// Reads in 64 KiB chunks rather than seeking once per line.
template <typename Fn>
void forEachLineReverse(QIODevice &dev, qint64 stopAt, Fn fn) {
    constexpr qint64 CHUNK = 64 * 1024;
    qint64 pos = dev.size();
    QByteArray carry; // partial line left over from the start of the previous chunk

    while (pos > stopAt) {
        const qint64 len = qMin(CHUNK, pos - stopAt);
        pos -= len;
        dev.seek(pos);
        QByteArray buf = dev.read(len);
        buf.append(carry);

        qsizetype end = buf.size();
        qsizetype nl;
        // end > 0 guard matters: lastIndexOf(ch, -1) would search from the END in Qt
        while (end > 0 && (nl = buf.lastIndexOf('\n', end - 1)) != -1) {
            if (!fn(buf.sliced(nl + 1, end - nl - 1))) return;
            end = nl;
        }
        carry = buf.first(end); // may be the tail of a line that continues in the next chunk
    }
    if (!carry.isEmpty()) fn(carry); // first line of the scanned region
}

} // namespace

//Public functions
Eo<QMap<QString, QString>> GCodeParser::parseFile(QString filepath) {
    using eop = Eo<QMap<QString, QString>>;
    if (!QFile::exists(filepath)) {
        return eop("GcodeFileNotFoundError", "Unable to locate: " + filepath, El::Warning);
    }
    QFileInfo fileInfo = QFileInfo(filepath);
    const QString name = fileInfo.fileName().toLower();
    QMap<QString, QString> output;
    if (name.endsWith(".gcode")) {
        Log::write("GCodeParser", "Parsing .gcode file");
        output = formatGCode(streamParseGCode(filepath));
    } else if (name.endsWith(".bgcode")) {
        Log::write("GCodeParser", "Parsing .bgcode file");
        output = formatBGCode(streamParseBGCode(filepath));
    } else if (name.endsWith(".gcode.3mf")) {
        Log::write("GCodeParser", "Parsing .gcode.3mf file");
        const auto [found, plate] = extractGCode3mfPlate(filepath, 1, /*fallbackToFirst=*/true);
        if (!found) {
            return eop("3mfGcodeNotFoundError", "Unable to locate gcode file within: " + filepath, El::Warning);
        }
        output = formatGCode(streamParseGCode(plate));
        output.insert("plateName", "plate_1");
    } else {
        return eop("GcodeInvalidFiletypeError", "File has invalid filetype: " + filepath, El::Warning);
    }

    output.insert("filename", fileInfo.fileName());
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
        const auto [found, plate] = extractGCode3mfPlate(filepath, platenum);
        if (!found) {
            return eop("GcodeParseError", "Specified plate not found: " + QString("plate_%1.gcode").arg(platenum), El::Warning);
        }
        output = formatGCode(streamParseGCode(plate));
        output.insert("plateName", QString("plate_%1").arg(platenum));
    } else {
        return eop("GcodeInvalidFiletypeError", "File is not gcode 3mf: " + filepath, El::Warning);
    }

    output.insert("filename", fileInfo.fileName());
    return eop(output);
}

// Decompresses one entry of an open archive. Returns a null QByteArray on failure.
static QByteArray readZipEntry(zip_t *za, zip_int64_t index) {
    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat_index(za, index, 0, &st) != 0 || !(st.valid & ZIP_STAT_SIZE)) return {};

    zip_file_t *zf = zip_fopen_index(za, index, 0);
    if (!zf) return {};

    QByteArray out;
    out.resize(qsizetype(st.size));
    const zip_int64_t bytesRead = zip_fread(zf, out.data(), st.size);
    zip_fclose(zf);

    if (bytesRead < 0 || zip_uint64_t(bytesRead) != st.size) return {};
    return out;
}

// Decompresses only the requested plate's G-code from a .gcode.3mf, instead of every plate.
// Finds "plate_<n>.gcode" in any folder (Bambu/Orca put it in "Metadata/"), case-insensitive.
// If fallbackToFirst is set and that plate doesn't exist, uses the first .gcode entry instead.
// Returns {found, data}; found = false means no matching G-code entry (or not a zip).
QPair<bool, QByteArray> GCodeParser::extractGCode3mfPlate(const QString &filepath, quint16 platenum, bool fallbackToFirst) {
    int err = 0;
    zip_t *za = zip_open(filepath.toUtf8().constData(), ZIP_RDONLY, &err);
    if (!za) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        Error::handle("GcodeParserExtract3mfError", "File " + filepath + " is not in 3MF/ZIP format: " + zip_error_strerror(&error), El::Critical);
        zip_error_fini(&error);
        return {false, {}};
    }

    const QByteArray plateName = QString("plate_%1.gcode").arg(platenum).toUtf8();
    // ZIP_FL_NODIR ignores the directory part, so this matches "Metadata/plate_1.gcode" too
    zip_int64_t index = zip_name_locate(za, plateName.constData(), ZIP_FL_NOCASE | ZIP_FL_NODIR);

    if (index < 0 && fallbackToFirst) {
        const zip_int64_t num = zip_get_num_entries(za, 0);
        for (zip_int64_t i = 0; i < num; i++) {
            const char *name = zip_get_name(za, i, 0);
            if (name && QByteArray(name).toLower().endsWith(".gcode")) {
                index = i;
                break;
            }
        }
    }

    QPair<bool, QByteArray> result {false, {}};
    if (index >= 0) {
        QByteArray data = readZipEntry(za, index);
        if (!data.isNull()) result = {true, data};
    }
    zip_close(za);
    return result;
}

//Format the properties correctly
QMap<QString, QString> GCodeParser::formatGCode(QMap<QString, QString> properties) {
    QMap<QString, QString> output = {
        {"printer", properties[GCODE_TARGETS[2]]},
        {"filamentType", properties[GCODE_TARGETS[1]].replace(";", ", ")},
        {"printSettings", properties[GCODE_TARGETS[5]]},
        {"duration", properties[GCODE_TARGETS[3]]},
    };
    QList<QPair<QString, QString>> filaments;
    auto j = ((properties.contains(GCODE_TARGETS[0])) ? properties[GCODE_TARGETS[0]] : properties[GCODE_TARGETS[6]]).split(",");
    auto k = properties[GCODE_TARGETS[4]].replace("\"", "").split(";");
    double totalWeight = 0.0;
    for (int i = 0; i < std::min(j.length(), k.length()); i++) {
        j[i] = j[i].trimmed();
        k[i] = k[i].split("@").at(0).trimmed();
        bool ok = false;
        double weight = j[i].toDouble(&ok);
        if (ok && weight > 0) {
            filaments.append({k[i], j[i] + "g"});
            totalWeight += weight;
        }
    }
    QStringList filamentstr;
    QStringList weightstr;
    for (const auto &f : filaments) {
        filamentstr.append(f.first);
        weightstr.append(f.second);
    }
    output.insert("filament", filamentstr.join(", "));
    output.insert("weight", weightstr.join(", "));
    output.insert("filamentCount", QString::number(filamentstr.length()));
    output.insert("totalWeight", QString::number(totalWeight) + "g");

    output.insert("infillDensity", properties[GCODE_TARGETS[7]]);
    output.insert("layerHeight", properties[GCODE_TARGETS[8]] + "mm");

    return output;
}

QMap<QString, QString> GCodeParser::formatBGCode(QMap<QString, QString> properties) {
    QMap<QString, QString> output = {
        {"printer", properties[BGCODE_TARGETS[2]]},
        {"filamentType", properties[BGCODE_TARGETS[1]]},
        {"weight", properties[BGCODE_TARGETS[0]]},
        {"duration", properties[BGCODE_TARGETS[3]]},
    };
    output.insert("infillDensity", properties[BGCODE_TARGETS[4]]);
    output.insert("layerHeight", properties[BGCODE_TARGETS[5]] + "mm");
    return output;
}


//Line parsing
void GCodeParser::parseGCodeLine(QMap<QString, QString> &properties, const QString &line) {
    if (line.startsWith(u"; model printing time:")) { // Bambu abnormality
        if (!properties.contains(GCODE_TARGETS[3])) {
            QStringList splitLine = line.split(";")[1].split(":");
            splitLine.pop_front();
            properties.insert(GCODE_TARGETS[3], splitLine.join(":").trimmed());
        }
        return;
    }
    if (!line.startsWith(u"; ")) return;
    const QStringView rest = QStringView(line).sliced(2);

    for (const QString &target : GCODE_TARGETS) {
        // matches "; <target> = <value>" without building a new string per target
        if (!rest.startsWith(target)) continue;
        const QStringView afterKey = rest.sliced(target.size());
        if (!afterKey.startsWith(u" = ")) continue;
        if (!properties.contains(target))
            properties.insert(target, afterKey.sliced(3).trimmed().toString());
        return;
    }
}

void GCodeParser::parseBGCodeLine(QMap<QString, QString> &properties, const QString &line) {
    for (const QString &target : BGCODE_TARGETS) {
        if (line.startsWith(target + '=')) {
            if (!properties.contains(target))
                properties.insert(target, line.sliced(target.size() + 1).trimmed());
            return;
        }
    }
}

// True once every property formatGCode needs has been found.
bool GCodeParser::gcodeComplete(const QMap<QString, QString> &p) {
    // GCODE_TARGETS[0] and [6] are alternatives for the filament weight, either one counts
    for (int i = 0; i < GCODE_TARGETS.size(); i++) {
        if (i == 0 || i == 6) continue;
        if (!p.contains(GCODE_TARGETS[i])) return false;
    }
    return p.contains(GCODE_TARGETS[0]) || p.contains(GCODE_TARGETS[6]);
}

bool GCodeParser::bgcodeComplete(const QMap<QString, QString> &p) {
    for (const QString &target : BGCODE_TARGETS)
        if (!p.contains(target)) return false;
    return true;
}


// Core text G-code scan. Works on any seekable device (QFile, QBuffer).
// Pass 1: top of file, line by line, until the first G-code line.
// Pass 2: bottom of file, backwards, until the first G-code line.
// Either pass returns as soon as everything has been found.
QMap<QString, QString> GCodeParser::scanGCode(QIODevice &dev) {
    QMap<QString, QString> props;

    // --- Pass 1: from the top ---
    dev.seek(0);
    bool inThumbnail = false;
    qint64 topEnd = 0; // where pass 1 stopped; pass 2 never reads above this
    bool hitCode = false;
    while (!dev.atEnd()) {
        const QByteArray raw = dev.readLine();
        const LineKind kind = classify(raw);
        if (kind == LineKind::Blank) continue;
        if (kind == LineKind::Code) {
            hitCode = true;
            topEnd = dev.pos();
            break;
        }

        // Thumbnails are hundreds of base64 comment lines: skip without parsing them.
        // Covers "; thumbnail begin", "; thumbnail_JPG begin", "; thumbnail_QOI end", etc.
        if (raw.startsWith("; thumbnail")) {
            if (raw.contains(" begin")) inThumbnail = true;
            else if (raw.contains(" end")) inThumbnail = false;
            continue;
        }
        if (inThumbnail) continue;

        parseGCodeLine(props, QString::fromUtf8(raw).trimmed());
        if (gcodeComplete(props)) return props;
    }
    if (!hitCode) return props; // whole file was comments, nothing left to read

    // --- Pass 2: from the bottom ---
    forEachLineReverse(dev, topEnd, [&](const QByteArray &raw) {
        switch (classify(raw)) {
        case LineKind::Blank:
            return true;  // keep going
        case LineKind::Code:
            return false; // reached the end G-code, stop
        case LineKind::Comment:
            parseGCodeLine(props, QString::fromUtf8(raw).trimmed());
            return !gcodeComplete(props);
        }
        return false;
    });
    return props;
}

QMap<QString, QString> GCodeParser::streamParseGCode(const QString &filepath) {
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return scanGCode(f);
}

QMap<QString, QString> GCodeParser::streamParseGCode(const QByteArray &raw) {
    QBuffer buf;
    buf.setData(raw); // implicitly shared, no copy
    buf.open(QIODevice::ReadOnly);
    return scanGCode(buf);
}

// Binary G-code has no ';' comments. It is a sequence of blocks, and all metadata
// blocks come before the first G-code block, so we walk the block headers, read the
// metadata blocks, skip thumbnails by their size, and stop at the first G-code block.
// Layout (little-endian), per Prusa's libbgcode spec:
//   file header:  "GCDE" | u32 version | u16 checksum type (0 = none, 1 = CRC32)
//   block header: u16 type | u16 compression | u32 uncompressed size | [u32 compressed size if compression != 0]
//   then:         params (thumbnail: 6 bytes, others: 2 bytes) | data | checksum (4 bytes if CRC32)
QMap<QString, QString> GCodeParser::streamParseBGCode(const QString &filepath) {
    QMap<QString, QString> props;
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return props;

    const QByteArray fileHeader = f.read(10);
    if (fileHeader.size() != 10 || !fileHeader.startsWith("GCDE")) {
        Log::write("GCodeParser", "Not a binary G-code file: " + filepath);
        return props;
    }
    const quint16 checksumType = qFromLittleEndian<quint16>(fileHeader.constData() + 8);
    const qint64 checksumSize = (checksumType == 1) ? 4 : 0;

    enum : quint16 { FileMetadata = 0, GCodeBlock = 1, SlicerMetadata = 2,
                     PrinterMetadata = 3, PrintMetadata = 4, Thumbnail = 5 };
    enum : quint16 { NoCompression = 0, Deflate = 1 }; // 2/3 = Heatshrink (G-code blocks only)

    while (!f.atEnd()) {
        const QByteArray bh = f.read(8);
        if (bh.size() != 8) break;
        const quint16 type        = qFromLittleEndian<quint16>(bh.constData());
        const quint16 compression = qFromLittleEndian<quint16>(bh.constData() + 2);
        const quint32 uncompressedSize = qFromLittleEndian<quint32>(bh.constData() + 4);
        quint32 dataSize = uncompressedSize;
        if (compression != NoCompression) {
            const QByteArray cs = f.read(4);
            if (cs.size() != 4) break;
            dataSize = qFromLittleEndian<quint32>(cs.constData());
        }

        if (type == GCodeBlock) break; // all metadata is before this point

        const qint64 paramsSize = (type == Thumbnail) ? 6 : 2;
        if (type == Thumbnail || type > Thumbnail) { // skip without reading
            if (!f.seek(f.pos() + paramsSize + dataSize + checksumSize)) break;
            continue;
        }

        f.seek(f.pos() + paramsSize); // metadata param = encoding (0 = INI), only one exists
        QByteArray data = f.read(dataSize);
        f.seek(f.pos() + checksumSize);
        if (data.size() != qsizetype(dataSize)) break;

        if (compression == Deflate) {
            // qUncompress expects a big-endian uncompressed-size prefix before the zlib stream
            QByteArray prefixed(4, '\0');
            qToBigEndian<quint32>(uncompressedSize, prefixed.data());
            data = qUncompress(prefixed + data);
            if (data.isEmpty()) {
                Log::write("GCodeParser", "Failed to inflate bgcode metadata block");
                continue;
            }
        } else if (compression != NoCompression) {
            Log::write("GCodeParser", "Skipping bgcode metadata block with unsupported compression");
            continue;
        }

        for (const QByteArray &line : data.split('\n'))
            parseBGCodeLine(props, QString::fromUtf8(line).trimmed());
        if (bgcodeComplete(props)) break;
    }
    return props;
}
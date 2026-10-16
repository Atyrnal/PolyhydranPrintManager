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
#include <zlib.h>
#include <cstring>

// ---------------------------------------------------------------------------
// Limits. Files arrive over the network, so nothing read from a file header is
// trusted to size an allocation.
// ---------------------------------------------------------------------------
namespace {

constexpr qint64 READ_CHUNK          = 64 * 1024;               // read size for all streaming
constexpr qint64 MAX_LINE            = 64 * 1024;               // longer lines are classified, then skipped unparsed
constexpr qint64 TAIL_WINDOW         = 2 * 1024 * 1024;         // bytes kept from the end of a zipped plate
constexpr qint64 MAX_PLATE_BYTES     = 2LL * 1024 * 1024 * 1024; // max inflated bytes streamed from a 3mf plate
constexpr quint32 MAX_BGCODE_METADATA = 16 * 1024 * 1024;       // max size of one bgcode metadata block

enum class LineKind { Blank, Comment, Code };
enum class Step { Continue, Stop, Complete };

// Classifies a raw line by its first non-whitespace character. Only the start of a line
// matters, so callers may pass just the first MAX_LINE bytes of a longer line.
LineKind classify(QByteArrayView line) {
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return (c == ';') ? LineKind::Comment : LineKind::Code;
    }
    return LineKind::Blank;
}

// One line of the top-down pass. `parse` takes a trimmed comment line and returns
// true once every property has been found.
template <typename ParseFn>
Step topLine(QByteArrayView raw, bool &inThumbnail, ParseFn &&parse) {
    switch (classify(raw)) {
    case LineKind::Blank:   return Step::Continue;
    case LineKind::Code:    return Step::Stop;
    case LineKind::Comment: break;
    }
    // Long comments (e.g. "; machine_start_gcode = ...") are skipped, not parsed: none of
    // the targets is that long, and it keeps every parsed line under MAX_LINE.
    if (raw.size() > MAX_LINE) return Step::Continue;
    // Thumbnails are hundreds of base64 comment lines: skip without parsing them.
    // Covers "; thumbnail begin", "; thumbnail_JPG begin", "; thumbnail_QOI end", etc.
    if (raw.startsWith("; thumbnail")) {
        if (raw.indexOf(" begin") != -1) inThumbnail = true;
        else if (raw.indexOf(" end") != -1) inThumbnail = false;
        return Step::Continue;
    }
    if (inThumbnail) return Step::Continue;
    return parse(QString::fromUtf8(raw).trimmed()) ? Step::Complete : Step::Continue;
}

// Calls fn(line, truncated) for every line of a seekable device, from the last line
// backwards, stopping at byte offset `stopAt`. fn returns false to stop early.
// Reads in READ_CHUNK pieces. Going backwards, a long line's start (the part that decides
// what kind of line it is) arrives last, so for a line longer than MAX_LINE only its first
// MAX_LINE bytes are kept and passed with truncated = true. Memory stays at
// O(READ_CHUNK + MAX_LINE) and time stays linear however long the line is.
template <typename Fn>
void forEachLineReverse(QIODevice &dev, qint64 stopAt, Fn &&fn) {
    qint64 pos = dev.size();
    QByteArray carry;            // start of a line that continues into an earlier chunk
    bool carryTruncated = false; // carry has lost bytes from its end (the line is > MAX_LINE)

    while (pos > stopAt) {
        const qint64 len = qMin(READ_CHUNK, pos - stopAt);
        pos -= len;
        if (!dev.seek(pos)) return;
        QByteArray buf = dev.read(len);
        if (buf.size() != len) return;
        buf.append(carry);

        qsizetype end = buf.size();
        bool truncated = carryTruncated; // applies to the line that ends in carry
        qsizetype nl;
        // end > 0 guard matters: lastIndexOf(ch, -1) would search from the END in Qt
        while (end > 0 && (nl = buf.lastIndexOf('\n', end - 1)) != -1) {
            const qsizetype lineLen = end - nl - 1;
            if (!fn(QByteArrayView(buf).sliced(nl + 1, qMin(lineLen, MAX_LINE)), truncated || lineLen > MAX_LINE))
                return;
            end = nl;
            truncated = false;
        }
        carryTruncated = truncated || end > MAX_LINE;
        carry = buf.first(qMin(end, MAX_LINE)); // keep the line's start, drop the rest
    }
    if (!carry.isEmpty()) fn(QByteArrayView(carry), carryTruncated); // first line of the region
}

// Bottom-up pass: comment lines from the end of the device back to `stopAt`,
// until the first G-code line or until everything has been found.
template <typename ParseFn>
void bottomPass(QIODevice &dev, qint64 stopAt, ParseFn &&parse) {
    forEachLineReverse(dev, stopAt, [&](QByteArrayView raw, bool truncated) {
        switch (classify(raw)) {
        case LineKind::Blank:   return true;  // keep going
        case LineKind::Code:    return false; // reached the end G-code, stop
        case LineKind::Comment:
            if (truncated) return true;       // long comment: skip, like the top pass does
            return !parse(QString::fromUtf8(raw).trimmed());
        }
        return false;
    });
}

// Discards the rest of the current line (through its '\n') in bounded pieces.
bool skipRestOfLine(QIODevice &dev) {
    while (!dev.atEnd()) {
        const QByteArray part = dev.readLine(READ_CHUNK);
        if (part.isEmpty()) return false; // read error
        if (part.endsWith('\n')) return true;
    }
    return true;
}

// Inflates a zlib stream into a buffer of exactly `expectedSize` bytes.
// Fails (returns null) instead of growing if the data inflates to more than that,
// so a lying size field or a decompression bomb can't allocate more than expectedSize.
QByteArray inflateBounded(QByteArrayView in, quint32 expectedSize) {
    QByteArray out(qsizetype(expectedSize), Qt::Uninitialized);
    z_stream zs {};
    if (inflateInit(&zs) != Z_OK) return {};
    zs.next_in   = reinterpret_cast<Bytef *>(const_cast<char *>(in.data()));
    zs.avail_in  = uInt(in.size());
    zs.next_out  = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = uInt(out.size());
    const int rc = inflate(&zs, Z_FINISH);
    const uLong produced = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) return {};
    out.truncate(qsizetype(produced));
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------
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
        const auto [found, props] = streamParseGCode3mf(filepath, 1, /*fallbackToFirst=*/true);
        if (!found) {
            return eop("3mfGcodeNotFoundError", "Unable to locate gcode file within: " + filepath, El::Warning);
        }
        output = formatGCode(props);
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
        const auto [found, props] = streamParseGCode3mf(filepath, platenum);
        if (!found) {
            return eop("GcodeParseError", "Specified plate not found: " + QString("plate_%1.gcode").arg(platenum), El::Warning);
        }
        output = formatGCode(props);
        output.insert("plateName", QString("plate_%1").arg(platenum));
    } else {
        return eop("GcodeInvalidFiletypeError", "File is not gcode 3mf: " + filepath, El::Warning);
    }

    output.insert("filename", fileInfo.fileName());
    return eop(output);
}

// ---------------------------------------------------------------------------
// Turning collected properties into output
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Line parsers. First value found for a key wins, so the header (top pass)
// takes priority over the trailer.
// ---------------------------------------------------------------------------
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
        // startsWith, not contains: "layer_height=" would otherwise also match "first_layer_height="
        if (line.startsWith(target + '=')) {
            if (!properties.contains(target))
                properties.insert(target, line.sliced(target.size() + 1).trimmed());
            return;
        }
    }
}

// True once every property formatGCode needs has been found.
bool GCodeParser::gcodeComplete(const QMap<QString, QString> &p) {
    // GCODE_TARGETS[0] and [6] are alternatives for the filament weight: either one counts
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

// ---------------------------------------------------------------------------
// Plain .gcode on disk
// Pass 1: top of file, line by line, until the first G-code line.
// Pass 2: bottom of file, backwards, until the first G-code line.
// Either pass returns as soon as everything has been found.
// Time O(header + trailer), memory O(READ_CHUNK + MAX_LINE) regardless of file size.
// ---------------------------------------------------------------------------
QMap<QString, QString> GCodeParser::scanGCode(QIODevice &dev) {
    QMap<QString, QString> props;
    auto parse = [&](const QString &line) {
        parseGCodeLine(props, line);
        return gcodeComplete(props);
    };

    if (!dev.seek(0)) return props;
    bool inThumbnail = false;
    bool hitStop = false;
    qint64 topEnd = 0; // where pass 1 stopped; pass 2 never reads above this
    while (!dev.atEnd()) {
        // Bounded read: a line longer than MAX_LINE comes back as just its start, without '\n'
        const QByteArray raw = dev.readLine(MAX_LINE + 1); // +1: a MAX_LINE line plus its '\n' fits
        if (raw.isEmpty()) break; // read error
        const bool overlong = !raw.endsWith('\n') && !dev.atEnd();
        if (overlong) {
            const bool isComment = classify(raw) == LineKind::Comment;
            // Read past the rest of it either way, so pass 2 stops at a line boundary
            if (!skipRestOfLine(dev)) break;
            if (isComment) continue; // long comment (e.g. machine_start_gcode): skip it
            hitStop = true;          // long non-comment line: this is G-code
            topEnd = dev.pos();
            break;
        }
        // Without its '\n', as the other paths pass lines, so length limits match exactly
        const Step step = topLine(QByteArrayView(raw).chopped(raw.endsWith('\n') ? 1 : 0), inThumbnail, parse);
        if (step == Step::Complete) return props;
        if (step == Step::Stop) {
            hitStop = true;
            topEnd = dev.pos();
            break;
        }
    }
    if (!hitStop) return props; // reached EOF: the whole file was comments

    bottomPass(dev, topEnd, parse);
    return props;
}

QMap<QString, QString> GCodeParser::streamParseGCode(const QString &filepath) {
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return scanGCode(f);
}

// ---------------------------------------------------------------------------
// .gcode.3mf: streams one plate out of the archive with zip_fread.
// Compressed data can't be read backwards, so:
//   - top pass runs on the stream as it's inflated;
//   - if it hits G-code before finding everything, the rest of the plate is inflated
//     while only the last TAIL_WINDOW bytes are kept, and the bottom pass runs on those.
// Time O(plate size) worst case (O(header) when the header has everything, typical for
// Bambu/Orca); memory O(TAIL_WINDOW) regardless of plate size. Inflation stops after
// MAX_PLATE_BYTES, which bounds CPU time on a decompression bomb.
// Returns {found, properties}; found = false means not a zip or no matching plate.
// ---------------------------------------------------------------------------
QPair<bool, QMap<QString, QString>> GCodeParser::streamParseGCode3mf(const QString &filepath, quint16 platenum,
                                                                     bool fallbackToFirst) {
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
    zip_file_t *zf = (index >= 0) ? zip_fopen_index(za, index, 0) : nullptr;
    if (!zf) {
        zip_close(za);
        return {false, {}};
    }

    QMap<QString, QString> props;
    auto parse = [&](const QString &line) {
        parseGCodeLine(props, line);
        return gcodeComplete(props);
    };

    // One buffer, allocated once; zip_fread inflates straight into it (no per-chunk copy).
    //   top pass:   buf[0, len) holds at most one unfinished line (<= MAX_LINE) plus the new chunk
    //   tail phase: buf[0, len) holds the most recent bytes; when it fills, the last TAIL_WINDOW
    //               bytes are moved to the front with one memmove (amortized O(1) per byte)
    // Uninitialized, so pages a small plate never reaches are never touched.
    QByteArray buf(2 * TAIL_WINDOW + READ_CHUNK, Qt::Uninitialized);
    qsizetype len = 0;
    bool inThumbnail = false;
    bool topDone = false;        // top pass hit G-code
    bool complete = false;       // everything found
    bool streamOk = true;
    bool dropFirstLine = false;  // tail window starts mid-line
    bool skipping = false;       // top pass is discarding the rest of a long comment line
    qint64 total = 0;

    while (!complete) {
        if (len + READ_CHUNK > buf.size()) { // only reachable in the tail phase
            std::memmove(buf.data(), buf.constData() + len - TAIL_WINDOW, size_t(TAIL_WINDOW));
            len = TAIL_WINDOW;
            dropFirstLine = true;
        }
        const zip_int64_t n = zip_fread(zf, buf.data() + len, zip_uint64_t(READ_CHUNK));
        if (n == 0) break; // end of entry
        if (n < 0) {       // corrupt data / CRC mismatch
            Log::write("GCodeParser", "Error reading plate from " + filepath + ": " + zip_file_strerror(zf));
            streamOk = false;
            break;
        }
        total += n;
        if (total > MAX_PLATE_BYTES) {
            Log::write("GCodeParser", "Plate in " + filepath + " exceeds size limit, stopped reading");
            streamOk = false;
            break;
        }
        const qsizetype scanFrom = len;
        len += qsizetype(n);
        if (topDone) continue;

        // Top pass over the complete lines now in the buffer
        const QByteArrayView view(buf.constData(), len);
        qsizetype start = 0;
        qsizetype nl;
        qsizetype from = scanFrom; // bytes before scanFrom are known to contain no '\n'
        if (skipping) {
            // Still inside a long comment: drop bytes until its '\n' (len was 0, so view is new data)
            nl = view.indexOf('\n');
            if (nl == -1) {
                len = 0;
                continue;
            }
            skipping = false;
            start = from = nl + 1;
        }
        while ((nl = view.indexOf('\n', from)) != -1) {
            const Step step = topLine(view.sliced(start, nl - start), inThumbnail, parse);
            start = from = nl + 1;
            if (step == Step::Complete) { complete = true; break; }
            if (step == Step::Stop)     { topDone = true;  break; }
        }
        // Keep only what's left: an unfinished line, or the start of the tail
        if (start > 0) {
            std::memmove(buf.data(), buf.constData() + start, size_t(len - start));
            len -= start;
        }
        if (!topDone && !complete && len > MAX_LINE) {
            // Unfinished line longer than MAX_LINE: decide by its start
            if (classify(QByteArrayView(buf.constData(), MAX_LINE)) == LineKind::Comment) {
                skipping = true;      // long comment: discard it without buffering
                len = 0;
            } else {
                topDone = true;       // long non-comment line: this is G-code
                dropFirstLine = true; // and the tail would start in the middle of it
            }
        }
    }
    zip_fclose(zf);
    zip_close(za);

    // Done early, or the stream was cut short (then the "tail" isn't the real end of file).
    if (complete || !streamOk) return {true, props};

    buf.truncate(len);
    if (!topDone) {
        // The whole plate was comments; the last line may not end with '\n'
        if (!buf.isEmpty() && !skipping) topLine(buf, inThumbnail, parse);
        return {true, props};
    }

    if (buf.size() > TAIL_WINDOW) {
        buf.remove(0, buf.size() - TAIL_WINDOW);
        dropFirstLine = true;
    }
    if (dropFirstLine) {
        const qsizetype nl = buf.indexOf('\n');
        buf.remove(0, (nl < 0) ? buf.size() : nl + 1);
    }

    QBuffer tail(&buf);
    tail.open(QIODevice::ReadOnly);
    bottomPass(tail, 0, parse);
    return {true, props};
}

// ---------------------------------------------------------------------------
// .bgcode: binary G-code has no ';' comments. It is a sequence of blocks, and all
// metadata blocks come before the first G-code block, so we walk the block headers,
// read the metadata blocks, skip everything else by its size, and stop at the first
// G-code block.
// Layout (little-endian), per Prusa's libbgcode spec:
//   file header:  "GCDE" | u32 version | u16 checksum type (0 = none, 1 = CRC32)
//   block header: u16 type | u16 compression | u32 uncompressed size | [u32 compressed size if compression != 0]
//   then:         params (thumbnail: 6 bytes, others: 2 bytes) | data | checksum (4 bytes if CRC32)
// Sizes come from the file, so every block is checked against the file size and
// MAX_BGCODE_METADATA before anything is allocated.
// ---------------------------------------------------------------------------
QMap<QString, QString> GCodeParser::streamParseBGCode(const QString &filepath) {
    QMap<QString, QString> props;
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return props;
    const qint64 fileSize = f.size();

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
        const quint16 type             = qFromLittleEndian<quint16>(bh.constData());
        const quint16 compression      = qFromLittleEndian<quint16>(bh.constData() + 2);
        const quint32 uncompressedSize = qFromLittleEndian<quint32>(bh.constData() + 4);
        quint32 dataSize = uncompressedSize;
        if (compression != NoCompression) {
            const QByteArray cs = f.read(4);
            if (cs.size() != 4) break;
            dataSize = qFromLittleEndian<quint32>(cs.constData());
        }

        if (type == GCodeBlock) break; // all metadata is before this point

        const qint64 paramsSize = (type == Thumbnail) ? 6 : 2;
        const qint64 blockEnd = f.pos() + paramsSize + qint64(dataSize) + checksumSize;
        if (blockEnd > fileSize) {
            Log::write("GCodeParser", "Truncated or corrupt bgcode block in " + filepath);
            break;
        }

        const bool isMetadata = type == FileMetadata || type == SlicerMetadata
                             || type == PrinterMetadata || type == PrintMetadata;
        if (!isMetadata || dataSize > MAX_BGCODE_METADATA || uncompressedSize > MAX_BGCODE_METADATA) {
            if (isMetadata) Log::write("GCodeParser", "Skipping oversized bgcode metadata block in " + filepath);
            if (!f.seek(blockEnd)) break; // thumbnails, unknown types, oversized blocks
            continue;
        }

        if (!f.seek(f.pos() + paramsSize)) break; // param = encoding (0 = INI), only one exists
        QByteArray data = f.read(dataSize);       // dataSize <= MAX_BGCODE_METADATA
        if (data.size() != qsizetype(dataSize) || !f.seek(blockEnd)) break;

        if (compression == Deflate) {
            data = inflateBounded(data, uncompressedSize);
            if (data.isNull()) {
                Log::write("GCodeParser", "Failed to inflate bgcode metadata block in " + filepath);
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
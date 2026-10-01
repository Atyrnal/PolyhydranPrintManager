#include "errorhandler.hpp"
#include <QDateTime>

Error Error::handle(QString t, QString m, ErrorLevel l) {
    Error _new = Error(t, m, l);
    if (_new.isError()) ErrorHandler::handle(_new);
    return _new;
};

Error Error::softHandle(QString t, QString m, ErrorLevel l) {
    Error _new = Error(t, m, l);
    if (_new.isError()) ErrorHandler::softHandle(_new);
    return _new;
};

Error Error::buffer(QString t, QString m, ErrorLevel l) {
    Error _new = Error(t, m, l);
    if (_new.isError()) ErrorHandler::buffer(_new);
    return _new;
};

void Error::handle() const {
    if (this->isError()) ErrorHandler::handle(*this);
};

void Error::softHandle() const {
    if (this->isError()) ErrorHandler::softHandle(*this);
};

void ErrorHandler::initLogFile(const QString &path) {
    QFile* logFile = new QFile(path);
    QTextStream* logStream = nullptr;
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        logStream = new QTextStream(logFile);
        logFiles.append(QPair<QFile*, QTextStream*>(logFile, logStream));
    } else {
        delete logFile;
        logFile = nullptr;
    }
}

void ErrorHandler::initLogFileTimestamp(const QString &dirpath) {
    initLogFile(((dirpath.isEmpty()) ? "" : dirpath + "/") + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss") + ".log");
}

void ErrorHandler::writeToFile(const QString &line) {
    for (auto it = logFiles.constBegin(); it != logFiles.constEnd(); it++) {
        if (it->second == nullptr) return;
        *(it->second) << line << "\n";
        it->second->flush();
    }
}

void ErrorHandler::softHandle(const Error &err) {
    if (bufferingAll) return buffer(err);
    if (!err.isError()) return;
    printLn(err);
}

void ErrorHandler::buffer(const Error &l) {
    buf.enqueue(QSharedPointer<Error>::create(l));
}
void ErrorHandler::buffer(const class Log &l) {
    buf.enqueue(QSharedPointer<class Log>::create(l));
}
void ErrorHandler::buffer(const class Check &l) {
    buf.enqueue(QSharedPointer<Check>::create(l));
}


void ErrorHandler::flush() {
    bool prev = bufferingAll;
    bufferingAll = false;
    while (!buf.isEmpty()) {
        QSharedPointer<Loggable> l = buf.dequeue();

        if (auto e = l.dynamicCast<Error>()) {
            e->softHandle();
        } else if (auto g = l.dynamicCast<class Log>()) {
            ErrorHandler::log(*g);
        } else if (auto c = l.dynamicCast<Check>()) {
            ErrorHandler::check(*c);
        }
    }
    bufferingAll = prev;
}

void Error::buffer() const {
    if (!this->isError()) return;
    ErrorHandler::buffer(*this);
}

void Check::write() const {
    ErrorHandler::check(*this);
}

void Check::buffer() const {
    ErrorHandler::buffer(*this);
}

void Log::write() const {
    ErrorHandler::log(*this);
}

void Log::buffer() const {
    ErrorHandler::buffer(*this);
}

void ErrorHandler::bufferAll() {
    bufferingAll = true;
}

void ErrorHandler::stopBufferingAll() {
    bufferingAll = false;
}

bool ErrorHandler::isBufferingAll() {
    return bufferingAll;
}

class Log Log::write(QString t, QString m) {
    Log _new = Log(t, m);
    ErrorHandler::log(_new);
    return _new;
}

class Log Log::buffer(QString t, QString m) {
    Log _new = Log(t, m);
    ErrorHandler::buffer(_new);
    return _new;
}

class Check Check::write(QString cN, QString m, CheckLevel c) {
    Check _new = Check(cN, m, c);
    ErrorHandler::check(_new);
    return _new;
}
class Check Check::write(QString cN, CheckLevel c) {
    Check _new = Check(cN, c);
    ErrorHandler::check(_new);
    return _new;
}

void ErrorHandler::handle(const Error &err) {
    if (!err.isError()) return;
    if (bufferingAll && err.level <= El::Warning) return buffer(err);
    printLn(err);
    switch(err.level) {
    default:
    case El::None:
    case El::Debug:
        return;
    case El::Fatal:
        exit(1);
        break;
    case El::Trivial:
    case El::Warning:
    case El::Critical:
        if (err.type == "") {

        }
    }
}

void ErrorHandler::log(const class Log &log) {
    if (bufferingAll) return buffer(log);
    QString line = genLogLineLog(log.crTime, log.type, log.message);
    writeToFile(line);
    qDebug().noquote().nospace() << line << "\033[0m";
}

void ErrorHandler::check(const class Check &check) {
    QPair<QString, QString> line = genLogLineCheck(check.crTime, check.checkName, check.message, check.clevel);
    writeToFile(line.second);
    qDebug().noquote().noquote() << line.first << "\033[0m";
}

QString ErrorHandler::genLogLine(QDateTime time, const QString &lvl, const QString &content) {
    return time.toString("yyyy-MM-ddThh:mm:ss.zzzZ") + " " + lvl + " [ErrorHandler]: " + content;
}

QString ErrorHandler::genLogLineLog(QDateTime time, const QString &type, const QString &content) {
    return time.toString("yyyy-MM-ddThh:mm:ss.zzzZ") + " " + "LOG  " + " ["+type+"]: " + content;
}

QPair<QString, QString> ErrorHandler::genLogLineCheck(QDateTime time, const QString &checkName, const QString &message, CheckLevel clevel) {
    QString str = time.toString("yyyy-MM-ddThh:mm:ss.zzzZ") + " " + "CHECK" + " " + checkName.leftJustified(50) + " \t [";
    QString str2 = QString(str);
    QString centered = message.leftJustified((8 + message.size()) / 2).rightJustified(8);

    // "  hello   "
    switch (clevel) {
        case Cl::FAIL:
            str += "\033[31m";
            break;
        case Cl::WARN:
            str += "\033[33m";
            break;
        case Cl::OK:
            str += "\033[32m";
            break;
    }
    str += centered + "\033[0m]";
    str2 += centered + "]";
    return QPair(str, str2);
}

void ErrorHandler::printLn(QDateTime time, ErrorLevel lvl, const QString &content) {
    QString lvlindicator;
    QString line;
    switch (lvl) {
    default:
    case El::None:
        lvlindicator = "NONE ";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qDebug().noquote().nospace() << line  << "\033[0m";
        break;
    case El::Log:
        break;
    case El::Debug:
        lvlindicator = "DEBUG";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qDebug().noquote().nospace() << line << "\033[0m";
        break;
    case El::Trivial:
        lvlindicator = "TRIV ";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qInfo().noquote().nospace() << line << "\033[0m";
        break;
    case El::Warning:
        lvlindicator = "WARN ";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qWarning().noquote().nospace() << "\033[33m" << line << "\033[0m";
        break;
    case El::Critical:
        lvlindicator = "CRIT ";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qCritical().noquote().nospace() << "\033[31m" << line << "\033[0m";
        break;
    case El::Fatal:
        lvlindicator = "FATAL";
        line = genLogLine(time, lvlindicator, content);
        writeToFile(line);
        qFatal().noquote().nospace() << "\033[41m" << line << "\033[0m";
        break;
    }
};

void ErrorHandler::printLn(const Error &err) {
    printLn(err.crTime, err.level, err.type + ": " + err.errorString);
}

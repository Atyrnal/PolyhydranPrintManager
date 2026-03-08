#include "headers/errorhandler.hpp"
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

void Error::handle() const {
    if (this->isError()) ErrorHandler::handle(*this);
};

void Error::softHandle() const {
    if (this->isError()) ErrorHandler::softHandle(*this);
};

void ErrorHandler::softHandle(const Error &err) {
    if (!err.isError()) return;
    printLn(err);
}

void Log::write() const {
    ErrorHandler::log(*this);
}

class Log Log::write(QString t, QString m) {
    Log _new = Log(t, m);
    ErrorHandler::log(_new);
    return _new;
}

void ErrorHandler::handle(const Error &err) {
    if (!err.isError()) return;
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
    qDebug().noquote().nospace() << genLogLineLog(log.type, log.message) << "\033[0m";
}


QString ErrorHandler::genLogLine(const QString &lvl, const QString &content) {
    return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddThh:mm:ss.zzzZ") + " " + lvl + " [ErrorHandler]: " + content;
}

QString ErrorHandler::genLogLineLog(const QString &type, const QString &content) {
    return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddThh:mm:ss.zzzZ") + " " + "LOG  " + " ["+type+"]: " + content;
}

void ErrorHandler::printLn(ErrorLevel lvl, const QString &content) {
    QString lvlindicator;
    switch (lvl) {
    default:
    case El::None:
        lvlindicator = "NONE ";
        qDebug().noquote().nospace() << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    case El::Log:
        break;
    case El::Debug:
        lvlindicator = "DEBUG";
        qDebug().noquote().nospace() << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    case El::Trivial:
        lvlindicator = "TRIV ";
        qInfo().noquote().nospace() << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    case El::Warning:
        lvlindicator = "WARN ";
        qWarning().noquote().nospace() << "\033[33m" << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    case El::Critical:
        lvlindicator = "CRIT ";
        qCritical().noquote().nospace() << "\033[31m" << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    case El::Fatal:
        lvlindicator = "FATAL";
        qFatal().noquote().nospace() << "\033[41m" << genLogLine(lvlindicator, content) << "\033[0m";
        break;
    }
};

void ErrorHandler::printLn(const Error &err) {
    printLn(err.level, err.type + ": " + err.errorString);
}

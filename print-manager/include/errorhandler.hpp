#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "qtbackend.h"
#include "errors.hpp"

class ErrorHandler{
public:
    static void handle(const Error &e);
    static void softHandle(const Error &e);
    static void buffer(const Error &l);
    static void buffer(const class Log &l);
    static void buffer(const class Check &l);
    static void bufferAll();
    static bool isBufferingAll();
    static void stopBufferingAll();
    static void flush();
    template<typename T>
    static void handle(const ErrorOption<T> &eo) {
        if (!eo.isError()) return;
        Error* err = eo.error();
        if (err == nullptr) return;
        handle(*err);
    };
    template<typename T>
    static void softHandle(const ErrorOption<T> &eo) {
        if (!eo.isError()) return;
        Error* err = eo.error();
        if (err == nullptr) return;
        softHandle(*err);
    };
    static void log(const class Log &log);
    static void check(const class Check &check);
    static void initLogFile(const QString &path);
    static void initLogFileTimestamp(const QString &dirpath);
    static inline QTBackend* bk = nullptr;
private:
    static QString genLogLine(QDateTime time, const QString &lvl, const QString &content);
    static QString genLogLineLog(QDateTime time, const QString &type, const QString &content);
    static QPair<QString, QString> genLogLineCheck(QDateTime time, const QString &checkName, const QString &message, CheckLevel clevel);
    static void printLn(QDateTime time, ErrorLevel lvl, const QString &content);
    static void printLn(const Error &err);
    static void writeToFile(const QString &line);
    static inline QList<QPair<QFile*, QTextStream*>> logFiles;
    static inline QQueue<QSharedPointer<Loggable>> buf;
    static inline bool bufferingAll = false;
};

#endif // ERRORHANDLER_HPP

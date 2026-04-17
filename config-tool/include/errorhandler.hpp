#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "errors.hpp"

class ErrorHandler{
public:
    static void handle(const Error &e);
    static void softHandle(const Error &e);
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
private:
    static QString genLogLine(const QString &lvl, const QString &content);
    static QString genLogLineLog(const QString &lvl, const QString &content);
    static void printLn(ErrorLevel lvl, const QString &content);
    static void printLn(const Error &err);
};

#endif // ERRORHANDLER_HPP

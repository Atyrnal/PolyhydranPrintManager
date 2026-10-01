#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QString>
#include <QDebug>
#include <QObject>
#include <QVariant>
#include <QDateTime>

enum ErrorLevel {
    None,
    Log,
    Cond,
    Debug,
    Trivial,
    Warning,
    Critical,
    Fatal
};

enum CheckLevel {
    FAIL = 0,
    OK = 1,
    WARN = 2
};

class Loggable {
public:
    Loggable(QDateTime t): crTime(t) {};
    QDateTime crTime;
    virtual void buffer() const = 0;
};

class Error : public Loggable {
public:
    Error(QString t = "None", QString m= "", ErrorLevel l = ErrorLevel::Trivial) : Loggable(QDateTime::currentDateTimeUtc()), errorString(m), type(t), level(l) {};
    const QString errorString;
    const QString type;
    const ErrorLevel level;
    static Error None() { return Error("None", "", ErrorLevel::None); }
    static Error handle(QString t, QString m= "", ErrorLevel l = ErrorLevel::Trivial);
    static Error softHandle(QString t, QString m= "", ErrorLevel l = ErrorLevel::Trivial);
    static Error buffer(QString t, QString m= "", ErrorLevel l = ErrorLevel::Trivial);
    void handle() const;
    void softHandle() const;
    void buffer() const;
    bool isError() const {
        return !(level == ErrorLevel::None || type == "None");
    }
    friend QDebug operator<<(QDebug debug, const Error &err) {
        QDebugStateSaver saver(debug);
        debug.nospace().noquote() << err.type << "(\"" << err.errorString << "\", " << err.level <<")";
        return debug;
    };
};

class Log : public Loggable{
public:
    Log(QString t = "", QString m = "") : Loggable(QDateTime::currentDateTimeUtc()), message(m), type(t) {};
    const QString message;
    const QString type;
    const ErrorLevel level = ErrorLevel::Log;
    static Log write(QString t="", QString m="");
    static Log buffer(QString t="", QString m="");
    void write() const;
    void buffer() const;
    friend QDebug operator<<(QDebug debug, const Log &log) {
        QDebugStateSaver saver(debug);
        debug.nospace().noquote() << log.type << "(\"" << log.message << "\")";
        return debug;
    }
};

class Check : public Loggable{
public:
    Check(QString cN = "Check", QString m = "OK", CheckLevel c = CheckLevel::OK) : Loggable(QDateTime::currentDateTimeUtc()), checkName(cN), message(m), clevel(c) {};
    Check(QString cN = "Check", CheckLevel c = CheckLevel::OK) : Loggable(QDateTime::currentDateTimeUtc()), checkName(cN), clevel(c) {
        switch (clevel) {
            case CheckLevel::FAIL:
                message = "FAIL";
                break;
            case CheckLevel::OK:
                message = " OK ";
                break;
            case CheckLevel::WARN:
                message = "WARN";
                break;
        }
    };
    const QString checkName;
    QString message;
    const CheckLevel clevel;
    const ErrorLevel elevel = ErrorLevel::Cond;
    static Check write(QString cN = "Check", QString m = "OK", CheckLevel c = CheckLevel::OK);
    static Check write(QString cN = "Check", CheckLevel c = CheckLevel::OK);
    void buffer() const;
    void write() const;
    friend QDebug operator<<(QDebug debug, const Check &check) {
        QDebugStateSaver saver(debug);
        debug.nospace().noquote() << check.checkName << "(\"" << check.message << "\")";
        return debug;
    }
};

template<typename T>
class ErrorOption {
public:
    ErrorOption(Error* er) {
        errorObj = new Error(*er);
        isErr = true;
    };
    ErrorOption(T value) {
        this->value = value;
        errorObj = nullptr;
        isErr = false;
    }
    ErrorOption(T value, Error* er) {
        this->value = value;
        errorObj = new Error(*er);
        isErr = true;
    };
    ErrorOption(T value, QString errorType, QString errorMsg, ErrorLevel errorLvl = ErrorLevel :: Debug) {
        this->value = value;
        errorObj = new Error(errorType, errorMsg, errorLvl);
        isErr = true;
    };
    ErrorOption(QString errorType, QString errorMsg, ErrorLevel errorLvl = ErrorLevel :: Trivial) {
        errorObj = new Error(errorType, errorMsg, errorLvl);
        isErr = true;
    }
    ~ErrorOption() {
        if (errorObj != nullptr) {
            delete errorObj;
            errorObj = nullptr;
        }
    }
    T get() const {
        return value;
    };
    T getOrDefault(T def) const {
        return (isError()) ? def : value;
    };
    T getOr(T def) const {
        return getOrDefault(def);
    };
    QString errorType() const {
        if (errorObj == nullptr) return "None";
        return errorObj->type;
    }
    QString errorString() const {
        if (errorObj == nullptr) return "";
        return errorObj->errorString;
    };
    ErrorLevel errorLevel() const {
        if (errorObj == nullptr) return ErrorLevel::None;
        return errorObj->level;
    }
    Error* error() const {
        return errorObj;
    };
    bool isError() const {
        return isErr;
    };
    void handle() {
        if (errorObj == nullptr) return;
        errorObj->handle();
    }
    void softHandle() {
        if (errorObj == nullptr) return;
        errorObj->softHandle();
    }
    friend QDebug operator<<(QDebug debug, const ErrorOption &err) {
        QDebugStateSaver saver(debug);
        debug.nospace() << "ErrorOption(" << ((err.errorObj != nullptr && (err.errorObj->isError())) ? *err.errorObj : Error::None()) << ", " << err.value <<")";
        return debug;
    };
private:
    T value;
    Error* errorObj;
    bool isErr;
};


template<typename T>
using Eo = ErrorOption<T>;

using El = ErrorLevel;

using Cl = CheckLevel;

#endif // ERRORHANDLER_H

#ifndef AIRTABLE_H
#define AIRTABLE_H

#include <QString>
#include <QNetworkAccessManager>
#include <qstringview.h>
#include "errors.hpp"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonArray>
#include <qjsonobject.h>
#include <qnetworkreply.h>
#include <qobject.h>
#include <qstringview.h>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>

struct Print { //TODO

};

struct User {
    QString id;
    QString firstname;
    QString lastname;
    QString email;
    bool isUmass = 0;
    bool isCics = 0;
    bool isTrained = 0;
};


class AirtableTable : public QObject {
    Q_OBJECT
    friend class AirtableBase;
public:
    template<typename Func>
    void getRecord(QString filterFormula, Func callback);//QVariantMap
    template<typename Func>
    void getRecords(QString filterFormula, Func callback); //QList<QVariantMap>
    void createRecord(QVariantMap recordFields);
    // template<typename Func>
    // void createRecord(QVariantMap recordFields, Func callback);
    // void createRecords(QArray<QVariantMap> records);
    // void post(QByteArray &data);
    // template<typename Func>
    // void post(QByteArray &data, Func callback);
private:
    AirtableTable(QString dbHostname, QString dbKey, QString dbBase, QString dbTable, QObject* parent = nullptr);
    QString dbHostname;
    QString dbKey;
    QString dbBase;
    QString dbTable;
    QNetworkAccessManager netman;
    [[nodiscard]] QNetworkReply* getRaw(const QString &urlStr);
    [[nodiscard]] QNetworkReply* postRaw(const QString &urlStr, const QByteArray &data);
};


class AirtableBase : public QObject {
    Q_OBJECT
public:
    AirtableTable* table(QString id);
    AirtableBase(QString dbHostname, QString dbKey, QString dbBase, QObject* parent = nullptr);
// public slots:
//     void getTables();
private:
    QString dbHostname;
    QString dbBase;
    std::map<QString, std::unique_ptr<AirtableTable>> tables;
    QString dbKey;
    // QNetworkAccessManager netman;
    // template<typename Func>
    // void dbGet(const QString &urlStr, Func callback);
    // [[nodiscard]] QNetworkReply* dbGet(const QString &urlStr);
    // template<typename Func>
    // void dbPost(const QString &urlStr, const QByteArray &data, Func callback);
    // [[nodiscard]] QNetworkReply* dbPost(const QString &urlStr, const QByteArray &data);
    // void dbPostVoid(const QString &urlStr, const QByteArray &data);
};

class Airtable : public QObject {
    Q_OBJECT
public:
    Airtable(QString dbHostname, QString dbKey, QObject* parent=nullptr);
    AirtableBase* base(QString id);
private:
    std::map<QString, std::unique_ptr<AirtableBase>> bases;
    QString dbHostname;
    QString dbKey;
};


template<typename Func>
void AirtableTable::getRecord(QString filterFormula, Func callback) {
    using eop = Eo<QVariantMap>;
    getRecords(filterFormula, [=](Eo<QList<QVariantMap>> records){
        if (records.isError()) {
            callback(eop(records.error()));
        } else if (records.get().length() < 1) {
            callback(eop("AirtableResponseError", "No matching record", El::Trivial));
        } else {
            callback(eop(records.get().at(0)));
        }
    });
}

template<typename Func>
void AirtableTable::getRecords(QString filterFormula, Func callback) {
    using eop = Eo<QList<QVariantMap>>;
    QNetworkReply* reply = getRaw(QString("%1/v0/%2/%3?filterByFormula=%4").arg(dbHostname, dbBase, dbTable, filterFormula));
    QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
        if(reply->error() != QNetworkReply::NoError) {
            callback(eop("AirtableNetworkError", reply->errorString(), El::Warning));
            return;
        }
        QJsonParseError p;
        QJsonDocument responseDoc = QJsonDocument::fromJson(reply->readAll(), &p);
        if (p.error) {
            callback(eop("AirtableResponseError", "Unable to parse response as json: " + p.errorString(), El::Warning));
            return;
        } else if (!responseDoc.isObject()) {
            callback(eop("AirtableResponseError", "Response is not JSON object", El::Warning));
            return;
        }
        QJsonObject response = responseDoc.object();
        if (!response.contains("records") || !response.value("records").isArray()) {
            callback(eop("AirtableResponseError", "Response does not match expected scheme", El::Warning));
            return;
        }
        QJsonArray records = response.value("records").toArray();
        QList<QVariantMap> recordList;
        for (auto i = records.cbegin(); i != records.cend(); i++) {
            if ((*i).isObject()) {
                QJsonObject rec = (*i).toObject();
                if (rec.contains("createdTime") && rec.contains("fields") && rec.contains("id") && rec.value("createdTime").isString() && rec.value("fields").isObject() && rec.value("id").isString()) recordList.append((*i).toObject().toVariantMap());
            }
        }
        callback(eop(recordList));
        reply->deleteLater();
    });
}





#endif // AIRTABLE_H

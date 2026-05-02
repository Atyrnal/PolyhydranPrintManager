#include "airtable.h"
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

Airtable::Airtable(QString dbHostname, QString dbKey, QObject* parent) : QObject(parent) {
    this->dbHostname = dbHostname;
    this->dbKey = dbKey;
}

AirtableBase* Airtable::base(QString dbBase) {
    auto it = this->bases.find(dbBase);
    if (it != this->bases.end()) return it->second.get();
    auto [inserted, _] = this->bases.emplace(
        dbBase, new AirtableBase(this->dbHostname, this->dbKey, dbBase, this)
        );
    return inserted->second.get();
}

AirtableBase::AirtableBase(QString dbHostname, QString dbKey, QString dbBase, QObject* parent) : QObject(parent) {
    this->dbHostname = dbHostname;
    this->dbBase = dbBase;
    this->dbKey = dbKey;
}

AirtableTable* AirtableBase::table(QString dbTable) {
    auto it = this->tables.find(dbTable);
    if (it != this->tables.end()) return it->second.get();

    auto [inserted, _] = this->tables.emplace(
        dbTable, new AirtableTable(this->dbHostname, this->dbKey, this->dbBase, dbTable, this)
    );
    return inserted->second.get();
}


AirtableTable::AirtableTable(QString dbHostname, QString dbKey, QString dbBase, QString dbTable, QObject* parent) : QObject(parent) {
    this->dbHostname = dbHostname;
    this->dbBase = dbBase;
    this->dbTable = dbTable;
    this->dbKey = dbKey;
}


void AirtableTable::createRecord(QVariantMap recordFields) {
    QJsonObject payload;
    payload.insert("fields", QJsonObject::fromVariantMap(recordFields));
    QNetworkReply* reply = postRaw(QString("%1/v0/%2/%3").arg(dbHostname, dbBase, dbTable), QJsonDocument(payload).toJson());
    QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
        if (reply->error() != QNetworkReply::NoError) Error("AirtableNetworkError", reply->errorString(), El::Warning).softHandle();
    });
}

QNetworkReply* AirtableTable::getRaw(const QString &urlStr) {
    QUrl dbUrl = QUrl(urlStr);
    QNetworkRequest getReq(dbUrl);
    getReq.setRawHeader("Authorization", QString("Bearer %1").arg(dbKey).toUtf8());
    QNetworkReply* reply = netman.get(getReq);
    return reply;
}

QNetworkReply* AirtableTable::postRaw(const QString &urlStr, const QByteArray &data) {
    QUrl dbUrl = QUrl(urlStr);
    QNetworkRequest postReq(dbUrl);
    postReq.setRawHeader("Authorization", QString("Bearer %1").arg(dbKey).toUtf8());
    postReq.setHeader (QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = netman.post(postReq, data);
    return reply;
}

// template<typename Func>
// void AirtableTable::post(const QByteArray &data, Func slot) {
//     QNetworkReply* reply = dbPost(QString(%1/v0/%2/%3), data);
//     QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
//         slot(reply);
//         reply->deleteLater();
//     });
// }

// QNetworkReply* Airtable::dbPost(const QString &urlStr, const QByteArray &data) {
//     QUrl dbUrl = QUrl::fromEncoded(urlStr.toUtf8());
//     QNetworkRequest postReq(dbUrl);
//     postReq.setRawHeader("Authorization", QString("Bearer %1").arg(dbKey).toUtf8());
//     postReq.setHeader (QNetworkRequest::ContentTypeHeader, "application/json");
//     QNetworkReply* reply = netman.post(postReq, data);
//     return reply;
// }

// void Airtable::dbPostVoid(const QString &urlStr, const QByteArray &data) {
//     QNetworkReply* reply = dbPost(urlStr, data);
//     reply->deleteLater();
// }

// void Airtable::getUser(QString userID) {
//     QString path = QString("/v0/%1/%2").arg(dbBase, dbTables.value("users"));
//     QString urlStr = dbHostname + path + "?filterByFormula=%7BUser+Id%7D+%3D+'" + userID + "'";
//     dbGet(urlStr, [=](QNetworkReply* reply) {
//         if (reply->error() != QNetworkReply::NoError) { //Log if the upload succeeded or failed
//             Error::handle("AirtableGetError", "Network request failed:" + reply->errorString(), El::Critical);
//         }
//         QByteArray resp = reply->readAll();
//         emit userRetrieved(parseUserdata(userID, resp));
//     });
// }

// void Airtable::logPrint(QString userID, Print print) {
//     Log::write("Logging print");
//     QString path = QString("/v0/%1/%2").arg(dbBase, dbTables.value("print_log"));
//     QString urlStr = dbHostname + path;
//     QJsonObject data{
//         {"fields", QJsonObject{
//             { "Card ID", userID }
//         }}
//     };
//     QJsonDocument doc(data);
//     QByteArray bytes = doc.toJson(QJsonDocument::Compact);
//     dbPost(urlStr, bytes, [=](QNetworkReply* reply) {
//         if (reply->error()) Error::handle("AirtablePostError", reply->errorString() + "\nresponse:" + reply->readAll(), El::Critical);
//     });

// }

// void Airtable::getTables() {
//     QString path = QString("/v0/meta/bases/%1/tables").arg(dbBase);
//     QString urlStr = dbHostname + path;
//     dbGet(urlStr, [=](QNetworkReply* reply) {
//         if (reply->error() != QNetworkReply::NoError) { //Log if the upload succeeded or failed
//             Error::handle("AirtableGetError", "Network request failed:" + reply->errorString(), El::Critical);
//             return;
//         }
//         QByteArray resp = reply->readAll();
//         Log::write(resp);
//     });
// }


// Eo<User> Airtable::parseUserdata(const QString &id, const QString &data) {
//     using eop = Eo<User>;
//     QJsonParseError p;
//     //qDebug() << "About to parse JSON";
//     QJsonDocument result = QJsonDocument::fromJson(data.toUtf8(), &p);
//     if (p.error != QJsonParseError::NoError) {
//         return eop("JsonParseError", "Failed to parse JSON: " + p.errorString() + "\n" + data, El::Critical);
//     }
//     if (!result.isObject()) {
//         return eop("JsonParseError", "Parse result is not JSON Object", El::Critical);
//     }
//     QJsonObject o = result.object();
//     if (!o.contains("records") || !o.value("records").isArray()) {
//         return eop("AirtableParseError", "JSON missing required array property: records", El::Warning);
//     }
//     QJsonArray records = o.value("records").toArray();
//     if (records.isEmpty()) {
//         return eop("AirtableFetchError", "No matching record found", El::Trivial); //User not in database, needs to register
//     }
//     if (!records.at(0).isObject()) {
//         return eop("AirtableParseError", "User value is not JSON Object", El::Warning);
//     }
//     QJsonObject user = records.at(0).toObject();
//     //qDebug() << "About to parsing user";
//     return userFromJson(user);

// }

// Eo<User> Airtable::userFromJson(QJsonObject user) {
//     using eop = Eo<User>;
//     if (!user.value("fields").isObject()) return eop("AirtableUserParseError", "User missing requried object property fields", El::Warning);
//     QJsonObject fields = user.value("fields").toObject();
//     if (!fields.value("User ID").isString()) return eop("AirtableUserParseError", "User missing requried property User ID", El::Warning);
//     QString uid = fields.value("User ID").toString();
//     QString fn = fields.value("First Name").toString("");
//     QString ln = fields.value("Last Name").toString("");
//     //qDebug() << "User parsing successful: " << fn;
//     return eop(User {
//         uid,
//         fn,
//         ln
//     });
// }
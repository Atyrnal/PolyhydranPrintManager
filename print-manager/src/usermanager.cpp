#include "usermanager.h"
#include "globalstate.hpp"
#include <QJsonObject>


UserManager::UserManager(QObject* parent) : QObject(parent) {}

QVariantMap UserManager::getCurrentUser() const {
    return QVariantMap(currentUser);
}
QString UserManager::getCurrentUserId() const {
    return currentUserID;
}
QString UserManager::getCurrentStaffId() const {
    return currentStaffID;
}

void UserManager::setCurrentStaff(QString id) {
    currentStaffID = id;
    staffCache.insert(id);
}

void UserManager::setCurrentUserId(QString id) {
    currentUserID = id;
};
void UserManager::setCurrentUser(QVariantMap user) {
    currentUser = user;
}
bool UserManager::checkStaffCache(QString cardid) const {
    return staffCache.contains(cardid);
};



void UserManager::lookupUser(const QString &cardid, std::function<void(UserLookup, const QVariantMap &user)> done) {
    if (staffCache.contains(cardid)) {
        return done(UserLookup::Staff, {});
    }
    ait->table("Users")->getRecord(QString("{User Id} = '%1'").arg(cardid), QList<Sort>({{"Date", SortDir::DESC}}), [this, cardid, done](Eo<QVariantMap> eo) {
        if (eo.isError()) {
            if (eo.errorLevel() <= El::Trivial) {
                eo.softHandle();
                return done(UserLookup::NotFound, {});
            }
            eo.handle();
            return done(UserLookup::Error, {});
        }
        QVariantMap user = eo.get().value("fields").toMap();
        bool isStaff = user.value("Is Staff", false).toBool();
        if (isStaff) setCurrentStaff(cardid);
        done(isStaff ? UserLookup::Staff : UserLookup::NotStaff, user);
    });
}

void UserManager::lookupOngoingPrints(const QString &cardid, std::function<void(qint16 ct)> done) {
    ait->table("Print Log")->getRecords(QString("AND({User Id} = '%1', {Status}='Ongoing')").arg(cardid), QList<Sort>({{"Date", SortDir::DESC}}), [this, cardid, done](Eo<QList<QVariantMap>> eo){
        if (eo.isError()) {
            eo.handle();
            return done(-1);
        }
        QList<QVariantMap> records = eo.get();
        QSet<QString> curPrinters;
        for (auto it = records.constBegin(); it != records.constEnd(); ++it) {
            curPrinters.insert(it->value("fields").toMap().value("Printer", "").toString());
        }
        done(curPrinters.size());
    });
}


void UserManager::completeTraining() {
    if (!currentUser.contains("id")) return;
    QVariantMap payload;
    QVariantList certificates = currentUser.value("fields", QVariantMap()).toMap().value("Certificates", QList<QString>()).toList();
    certificates.append(stns("printingCertificateName"));
    payload.insert("Certificates", certificates);

    ait->table("Users")->updateRecordById(currentUser.value("id").toString(), payload);

    lookupOngoingPrints(currentUser.value("id").toString(), [this](qint16 ct){
        ptm->printStartCheck(false, ct, true);
    });
}





void UserManager::onCardScanned(QString cardid) {
    ScanContext context = ftm->getScanContext();
    AppState prev = ftm->getAppState();

    //bool isCachedStaff = staffCache.contains(cardid);
    if (checkStaffCache(cardid)) setCurrentStaff(cardid);
    else ftm->setAppState(AppState::Loading);
    ftm->setScanContext(ScanContext::NoContext);

    Log::write("UserManager", "Card scanned, current conext:" + QString::number(context));
    if (context == ScanContext::UserAuth && prev == AppState::Scan) { //User authorizing print
        setCurrentUserId(cardid);
        ptm->setPrintUser(cardid);

        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                if (!user.isEmpty()) setCurrentUser(user);
                ptm->printStartCheck(true);
                break;
            case UserLookup::NotStaff:
                if (!user.isEmpty()) currentUser = user;
                lookupOngoingPrints(cardid, [this](qint16 ct){
                    ptm->printStartCheck(false, ct);
                });
                break;
            case UserLookup::NotFound:
                ptm->addPrintIssue("Not registered");
                ftm->showMessage("Your account is not Registered\nPlease Register at the Check-In Kiosk");
                break;
            default:
                ftm->setAppState(AppState::Idle);
                break;
            }
        });
    } else if (context == ScanContext::StaffTraining && prev == AppState::Scan) {
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                completeTraining();
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
                ftm->setScanContext(ScanContext::StaffTraining);
                ftm->showMessage("This user is not Staff\nPlease ask a staff member for\nour 3D print training and have them\nscan their UCard to continue", "Training Completed", AppState::Scan);
                break;
            default:
                ftm->setAppState(AppState::Idle);
                break;
            }
        });
    } else if (context == ScanContext::StaffAuth && ptm->getLoadedPrint().userID != "" && prev == AppState::Scan) {
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                ptm->printStartCheck(true);
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
                ftm->showMessage("This user is not Staff", "OK", AppState::Scan);
                break;
            default:
                ftm->setAppState(prev);
                break;
            }
        });
    } else if (!ptm->getLoadedPrint().userID.isNull() && !ptm->getLoadedPrint().userID.isEmpty()) { //NoContext
        lookupUser(cardid, [=, this](UserLookup r, const QVariantMap &user) {
            switch(r) {
            case UserLookup::Staff:
                setCurrentStaff(cardid);
                ftm->showPrintOverridePrep();
                break;
            case UserLookup::NotStaff:
            case UserLookup::NotFound:
            default:
                ftm->setAppState(prev);
                break;
            }
        });
    } else ftm->setAppState(prev);
}

void UserManager::loadGroupOverrides() {
    //Load training certificate
    if (stnb("requireTraining")) {
        ait->table(stns("printingCertificateTableName"))->getRecords([this](Eo<QList<QVariantMap>> eo){
            if (eo.isError()) {
                eo.handle();
                return;
            }
            QList<QVariantMap> records = eo.get();
            for (auto it = records.constBegin(); it != records.constEnd(); ++it) {
                QString recid = it->value("id").toString();
                QString name = it->value("Certificate Name").toString();
                if (name == stns("printingCertificateName")) {
                    trainingCertificateId = recid;
                    break;
                }
            }
        });
    }

    if (!stnb("enableGroups")) return;
    ait->table(stns("groupTableName"))->getRecords([this](Eo<QList<QVariantMap>> eo){
        if (eo.isError()) {
            eo.handle();
            return;
        }
        QList<QVariantMap> records = eo.get();
        for (auto it = records.constBegin(); it != records.constEnd(); ++it) {
            QString recid = it->value("id").toString();
            QString jsonString = it->value("fields").toMap().value("Overrides", "{}").toString();
            QByteArray data = jsonString.toUtf8();
            QJsonParseError p;
            QJsonDocument doc = QJsonDocument::fromJson(data, &p);
            if (p.error != QJsonParseError::NoError) continue; 
            if (!doc.isObject()) continue;

            QJsonObject obj = doc.object();

            QMap<QString, QVariant> overrides;
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                QString key = it.key();
                QVariant val = it.value().toVariant();
                overrides.insert(key, val);
            }

            groupOverrides.insert(recid, overrides);
        }
        Log::write("UserManager", QString("Loaded %1 groups").arg(groupOverrides.size()));
    });

}

void UserManager::loadCustomGroups(QJsonObject customGroups) {
    for (auto it = customGroups.constBegin(); it != customGroups.constEnd(); ++it) {
        const QJsonValue value = it.value();
        if (!value.isObject()) { Log::write("UserManager", "Group is not obj"); continue; }
        QJsonObject cgrp = value.toObject();
        if (!cgrp.contains("criteria") || !cgrp.contains("overrides") || !cgrp.value("criteria").isObject() || !cgrp.value("overrides").isObject()) { Log::write("UserManager", "Group is not obj"); continue; };
        QJsonObject criteria = cgrp.value("criteria").toObject();
        QMap<QString, QVariant> overrides = cgrp.value("overrides").toObject().toVariantMap();
        if (!criteria.contains("field") || !criteria.contains("method") || !criteria.contains("values") || !criteria.value("field").isString() || !criteria.value("method").isString() || !criteria.value("values").isArray()) continue;
        QString field = criteria.value("field").toString();
        QString method = criteria.value("method").toString().toLower();
        QJsonArray values = criteria.value("values").toArray();

        CriteriaMethod m;
        if (method == "equals") m = CriteriaMethod::Equals;
        else if (method == "contains") m = CriteriaMethod::Contains;
        else if (method == "startswith") m = CriteriaMethod::StartsWith;
        else if (method == "endswith") m = CriteriaMethod::EndsWith;
        else continue;
        

        customGroupOverrides.append(QPair<CustomGroupCriteria, QVariantMap>({field, m, values.toVariantList()}, overrides));
    }
    Log::write("UserManager", QString("Loaded %1 custom groups").arg(customGroupOverrides.size()));
}

QVariantMap mergeOverrides(const QVariantMap &base, const QVariantMap &overlay) {
    QVariantMap result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        //TODO: find a good way to settle conflicts
        if (result.contains(it.key())) {
            result[it.key()] = it.value();
        } else {
            result[it.key()] = it.value();
        }
    }
    return result;
}

QMap<QString, QVariant> UserManager::getUserOverrides(QVariantMap user) const {
    QVariantMap overrides = QVariantMap();
    //Check custom groups first
    for (auto it = customGroupOverrides.constBegin(); it != customGroupOverrides.constEnd(); ++it) {
        CustomGroupCriteria cr = it->first;
        if (!user.contains(cr.field)) continue;
        bool satisfied = false;
        for (quint32 i = 0; i < cr.accepted.size(); i++) {
            switch (cr.method) {
            case CriteriaMethod::Equals:
                if (user.value(cr.field) == cr.accepted[i]) satisfied = true;
                break;
            case CriteriaMethod::Contains:
                if (user.value(cr.field).toString().contains(cr.accepted[i].toString())) satisfied = true;
                break;
            case CriteriaMethod::StartsWith:
                if (user.value(cr.field).toString().startsWith(cr.accepted[i].toString())) satisfied = true;
                break;
            case CriteriaMethod::EndsWith:
                if (user.value(cr.field).toString().endsWith(cr.accepted[i].toString())) satisfied = true;
                break;
            }
            if (satisfied) break;
        }
        if (satisfied) overrides = mergeOverrides(overrides, it->second);
        //if (satisfied) Log::write("UserManager", "User satisfies custom group criteria");
    }
    //Log::write("UserManager", "CustomGroups: " + QString::number(customGroupOverrides.size()));

    if (!user.contains(stns("groupTableName"))) return overrides;
    QVariantList groupList = user.value(stns("groupTableName")).toJsonArray().toVariantList();
    for (auto it = groupList.constBegin(); it != groupList.constEnd(); ++it) {
        QString recid = it->toString();
        if (groupOverrides.contains(recid)) overrides = mergeOverrides(overrides, groupOverrides.value(recid));
    }

    return overrides;
}

QVariant UserManager::getUserSetting(QString name, QVariantMap overrides, QVariant def) {
    if (overrides.contains(name)) return overrides.value(name);
    else return gsi.getSetting(name, def);
}

bool UserManager::isTrained(QVariantMap user) const {
    if (!stnb("requireTraining")) return false;
    return user.value("Certificates", QList<QString>()).toList().contains(trainingCertificateId);
}
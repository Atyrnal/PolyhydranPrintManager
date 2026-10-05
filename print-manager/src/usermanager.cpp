#include "usermanager.h"
#include "globalstate.hpp"


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
    certificates.append(stnsd("printingCertificateRecordId", "NONE"));
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
                ptm->addPrintIssue("isRegistered", false);
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
#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <QString>
#include <QObject>
#include <QSet>
#include <QVariantMap>

enum class UserLookup { Staff, NotStaff, NotFound, Error };

class UserManager : public QObject {
    Q_OBJECT
public:
    UserManager(QObject* parent = nullptr);
    void startRfid();
    void lookupUser(const QString &cardid, std::function<void(UserLookup, const QVariantMap &user)> done);
    void lookupOngoingPrints(const QString &cardid, std::function<void(qint16 ct)> done);
    void setCurrentStaff(QString id);
    void setCurrentUserId(QString id);
    void setCurrentUser(QVariantMap user);
    bool checkStaffCache(QString cardid) const;


    QVariantMap getCurrentUser() const;
    QString getCurrentUserId() const;
    QString getCurrentStaffId() const;

    void completeTraining();

public slots:
    void onCardScanned(QString cardid);

private:
    QString currentUserID = "";
    QString currentStaffID = "";
    QVariantMap currentUser;
    QSet<QString> staffCache;

};


#endif

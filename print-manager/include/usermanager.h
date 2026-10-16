#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <QString>
#include <QObject>
#include <QSet>
#include <QVariantMap>

enum class UserLookup { Staff, NotStaff, NotFound, Error };

enum class CriteriaMethod { Equals, Contains, StartsWith, EndsWith };

struct CustomGroupCriteria {
    QString field;
    CriteriaMethod method;
    QList<QVariant> accepted;
};

class UserManager : public QObject {
    Q_OBJECT
public:
    UserManager(QObject* parent = nullptr);
    void loadGroupOverrides();
    void loadCustomGroups(QJsonObject customGroups);
    void lookupUser(const QString &cardid, std::function<void(UserLookup, const QVariantMap &user)> done);
    void lookupOngoingPrints(const QString &cardid, std::function<void(qint16 ct)> done);
    void setCurrentStaff(QString id);
    void setCurrentUserId(QString id);
    void setCurrentUser(QVariantMap user);
    bool checkStaffCache(QString cardid) const;
    QMap<QString, QVariant> getUserOverrides(QVariantMap user) const;
    QVariant getUserSetting(QString name, QVariantMap overrides, QVariant def = QVariant());
    bool isTrained(QVariantMap user) const;


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
    QMap<QString, QMap<QString, QVariant>> groupOverrides;
    QString trainingCertificateId;
    QList<QPair<CustomGroupCriteria, QMap<QString, QVariant>>> customGroupOverrides;
};


#endif

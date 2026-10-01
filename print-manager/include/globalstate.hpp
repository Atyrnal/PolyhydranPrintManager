#ifndef GLOBALSTATE_HPP
#define GLOBALSTATE_HPP

#include <QObject>
#include <QMap>
#include <QVariant>

//Settings macros
#define stnb(sname) GlobalState::instance().getSetting((sname), false).toBool()
#define stnbd(sname, def) GlobalState::instance().getSetting((sname), (def)).toBool()
#define stns(sname) GlobalState::instance().getSetting((sname), "").toString()
#define stnsd(sname, def) GlobalState::instance().getSetting((sname), (def)).toString()
#define stnd(sname) GlobalState::instance().getSetting((sname), 0.0).toDouble()
#define stndd(sname, def) GlobalState::instance().getSetting((sname), (def)).toDouble()
#define stnui(sname) GlobalState::instance().getSetting((sname), 0).toUInt();
#define stnuid(sname, def) GlobalState::instance().getSetting((sname), (def)).toUInt()
#define stni(sname) GlobalState::instance().getSetting((sname), 0).toInt();
#define stnid(sname, def) GlobalState::instance().getSetting((sname), (def)).toInt()

class GlobalState : public QObject {
public:
    static GlobalState& instance();

    GlobalState(const GlobalState&) = delete;
    GlobalState& operator=(const GlobalState&) = delete;
    GlobalState(GlobalState&&) = delete;
    GlobalState& operator=(GlobalState&&) = delete;

    QVariant getSetting(QString sname, QVariant def) const;
    void loadSettings(QMap<QString, QVariant> setc);
private:
    explicit GlobalState(QObject *parent = nullptr);
    ~GlobalState();
    QMap<QString, QVariant> settings;
};



#endif // GLOBALSTATE_HPP

#include "globalstate.hpp"

GlobalState& GlobalState::instance() {
    static GlobalState instanced;
    return instanced;
}

GlobalState::GlobalState(QObject *parent) : QObject(parent) {
}

GlobalState::~GlobalState(){

}

QVariant GlobalState::getSetting(QString sname, QVariant def) const {
    return settings.value(sname, def);
}

void GlobalState::loadSettings(QMap<QString, QVariant> setc) {
    settings = QMap(setc);
}
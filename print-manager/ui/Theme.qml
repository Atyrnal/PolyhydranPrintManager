pragma Singleton
import QtQuick
QtObject {
    property bool isDark : true
    property color background : (isDark) ? "#161619" : "#f0f0f0"
    property color backgroundActive : (isDark) ? "#121215" : "#bfbfbf"
    property color primary : "#8188cc"
    property color primaryActive : "#50568a"
    property color primaryText : "#ffffff"
    property color text : (isDark) ? "#ffffff" : "#000000"
    property color alternate : "#7e7e7e"
    property color alternateActive : "#616161"
    property color alternateText : "#ffffff"
}


pragma Singleton
import QtQuick
QtObject {
    property bool isDark : Application.styleHints.colorScheme === Qt.ColorScheme.Dark
    property color background : (isDark) ? "#292929" : "#f0f0f0"
    property color backgroundActive : (isDark) ? "#424242" : "#bfbfbf"
    property color primary : "#ff2056"
    property color primaryColor : "#cf1945"
    property color primaryText : "#ffffff"
    property color text : (isDark) ? "#ffffff" : "#000000"
    property color alternate : "#7e7e7e"
    property color alternateActive : "#616161"
    property color alternateText : "#ffffff"
}

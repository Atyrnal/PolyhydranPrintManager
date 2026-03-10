import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: startupWindow
    visible: true
    height: 300
    width: 600
    title: qsTr("Polyhydran Print Manager Configuration Utility")
    flags: Qt.FramelessWindowHint
    property bool darkMode : Application.styleHints.colorScheme === Qt.ColorScheme.Dark
    Material.theme: darkMode ? Material.Dark : Material.Light

    VideoOutput {
        id: videoOutput
        height:200
        width:200
        x: 50
        y: 50
        MediaPlayer {
            id: player
            source: darkMode ? "../resources/SPolyLight.webm" : "../resources/SPolyDark.webm"
            videoOutput: videoOutput
            loops: MediaPlayer.Infinite
            autoPlay: true
        }
    }


    Text {
        x: 260
        height: 200
        width:300
        y: 50
        text: "Polyhydran"
    }




}

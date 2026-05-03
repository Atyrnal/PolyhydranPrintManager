/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyhydranPrintManager

//UI declarations

ApplicationWindow { //Root app window
    id: rootWindow
    visible: true
    width: 800
    height: 600
    title: qsTr("Polyhydran Print Manager")
    //visibility: Window.FullScreen //Make fullscreen

    enum AppState {
        Idle,
        Prep,
        Message,
        UserScan,
        StaffScan,
        Printing,
        Loading,
        PrinterSelection
    }

    enum AppMode {
        User,
        Staff
    }

    onClosing: function(close) { //Dont let user close kiosk app
        if (isWindows) close.accepted = false
    }

    Timer {
        id: stateTimeoutTimer
        interval: 120000
        repeat: false
        onTriggered: {
            rootWindow.appstate = Main.AppState.Idle
        }
    }

    //Handle Timeout (When the app hasnt been used for a certain period, go back to the idle state
    onAppstateChanged: {
        switch (rootWindow.appstate) {
        case Main.AppState.Prep:
        case Main.AppState.PrinterSelection:
        case Main.AppState.Printing:
        case Main.AppState.UserScan:
        case Main.AppState.StaffScan:
        case Main.AppState.Loading:
            stateTimeoutTimer.restart()
            break

        case Main.AppState.Idle:
            stateTimeoutTimer.stop()
            break

        case Main.AppState.Message:
            stateTimeoutTimer.stop()
            break;

        default:
            stateTimeoutTimer.restart()
            break
        }
    }

    Material.theme : Material.Dark

    property int appstate: Main.AppState.Idle; //Track app state
    property int appmode : Main.AppMode.User; //Main.AppMode.User;
    property int transitionDuration: 1000;

    Connections {
        target: backend
        function onSetAppstate(state) {
            appstate = state
        }
        function onSetAppmode(mode) {
            appmode = mode
        }
    }

    Item { //Container
        id: rootItem
        anchors.fill: parent;
        StackLayout {
            anchors.fill: parent
            currentIndex: appmode
            Rectangle {
                color: Theme.background


                Item { //polyhydran small branding
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: 15
                    anchors.leftMargin: 15
                    height: childrenRect.height
                    width: childrenRect.width
                    visible: rootWindow.appstate !== Main.AppState.Idle
                    FontLoader {
                        id: playfair
                        source: "../resources/PlayfairDisplay-Regular.ttf"
                    }

                    Image {
                        id : polyhydranLogo
                        source: (Theme.isDark) ? "../resources/StellatedPolyhedronWhite.png" : "../resources/StellatedPolyhedronDark.png"
                        height: 64
                        sourceSize: Qt.size(1080, 1080)
                        fillMode: Image.PreserveAspectFit
                        opacity: 1
                        smooth: true
                        antialiasing: true
                    }
                    Text {
                        id: polyhydranLabel
                        anchors.left: polyhydranLogo.right
                        anchors.leftMargin: 5
                        anchors.verticalCenter: polyhydranLogo.verticalCenter
                        anchors.verticalCenterOffset: -12
                        font.family: playfair.name
                        font.pointSize: 24
                        text : "Polyhydran"
                        color: Theme.text
                        horizontalAlignment: Text.AlignLeft
                    }

                    Text {
                        anchors.top: polyhydranLabel.bottom
                        anchors.horizontalCenter: polyhydranLabel.horizontalCenter
                        font.pointSize: 10
                        font.bold: true
                        text : "PRINT MANAGER"
                        color: Theme.text
                        horizontalAlignment: Text.AlignHCenter
                    }

                }

                StackLayout {

                currentIndex: (appstate > 3) ? appstate - 1 : appstate
                anchors.fill: parent

                Idle {
                    id: idleFrame
                    showMessage: function(message, nextstate) {
                        messageFrame.showMessage(message, nextstate)
                        rootWindow.appstate = Main.AppState.Message
                    }
                }

                Prep {
                    id: prepFrame
                }

                Message {
                    id: messageFrame
                }

                Scan {
                    id: scanFrame
                    isStaff: rootWindow.appstate === Main.AppState.StaffScan
                }

                Item {
                    id: printingFrame

                    Text {
                        id: printingText
                        text: "Printing now!" //In the future this will be set to user name based on airtable data
                        font.pointSize: 100
                        anchors.horizontalCenter: parent.horizontalCenter;
                        anchors.verticalCenter: parent.verticalCenter;
                        color: Theme.text
                    }
                }

                Item {
                    id: loadingFrame

                    Image {
                        id: spinnyThing
                        source: "../resources/progress_activity.svg"
                        width: 96
                        height: 96
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit

                        NumberAnimation on rotation {
                            loops: Animation.Infinite
                            from: 0
                            to: 360
                            duration: 750
                        }
                    }

                    Text {
                        text: "Loading"
                        font.pointSize: 24
                        anchors.top: spinnyThing.bottom
                        anchors.topMargin: 15
                        color: Theme.text
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                PrinterSelection {
                    id: printerSelectFrame
                }
            }
            }
        }
    }
}

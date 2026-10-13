/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
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
        PrepOverride,
        Message,
        Scan,
        Loading,
        PrinterSelection,
        Uploading
    }

    enum ScanContext {
        NoContext,
        UserAuth,
        StaffAuth = 100,
        StaffTraining
    }

    enum AppMode {
        User,
        Staff
    }

    onClosing: function(close) { //Dont let user close kiosk app
        if (rootWindow.visibility !== Window.Minimized) rootWindow.showMinimized()
        close.accepted = false
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
        case Main.AppState.Scan:
        case Main.AppState.Loading:
            stateTimeoutTimer.restart()
            break

        case Main.AppState.Uploading:
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
    property int scancontext: Main.ScanContext.NoContext;
    property int transitionDuration: 1000;

    Connections {
        target: frontman
        function onSetDarkmode(dm) {
            Theme.isDark = dm
        }
    }

    Item { //Container
        id: rootItem
        anchors.fill: parent;
        StackLayout {
            anchors.fill: parent
            currentIndex: rootWindow.appmode
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
                        source: "../resources/StellatedPolyhedronWhite.png"
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

                currentIndex: rootWindow.appstate
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

                PrepOverride {
                    id: prepOverrideFrame
                }

                Message {
                    id: messageFrame
                }

                Scan {
                    id: scanFrame
                    isStaff: rootWindow.scancontext >= Main.ScanContext.StaffAuth
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

                Item {
                    id: uploadingFrame

                    property double progressPct: 0.50;
                    property string maxSize : "999.99 TB";
                    property string curSize : "000.00 B";
                    property string file : "File_if_you_see_this_there_is_an_issue.gcode.3mf";

                    Connections {
                        target: frontman
                        function onUploadProgressUpdated(filename, cur, max, pct) {
                            uploadingFrame.file = filename;
                            uploadingFrame.maxSize = max;
                            uploadingFrame.curSize = cur;
                            uploadingFrame.progressPct = pct;
                        }
                    }

                    Image {
                        id: spinnyThing2
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
                        id: uploadText
                        text: "Uploading \"" + uploadingFrame.file + "\""
                        font.pointSize: 14
                        anchors.top: spinnyThing2.bottom
                        anchors.topMargin: 15
                        color: Theme.text
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Rectangle {
                        id: progBg
                        anchors.top: uploadText.bottom
                        anchors.topMargin: 10
                        anchors.horizontalCenter:parent.horizontalCenter
                        border.color: Theme.primary
                        color: Theme.background
                        border.width: 2
                        radius: 5
                        width: 400
                        height: 15
                    }

                    Rectangle {
                        id: progFg
                        anchors.top: progBg.top
                        anchors.left: progBg.left
                        radius: 5
                        border.width: 0
                        color: Theme.primary
                        height: 15
                        width: 400 * uploadingFrame.progressPct
                    }

                    Text {
                        id: uploadProgTxt
                        text: uploadingFrame.curSize + " / " + uploadingFrame.maxSize
                        font.pointSize: 12
                        color: Theme.text
                        anchors.top: progBg.bottom
                        anchors.topMargin: 8
                        anchors.horizontalCenter: progBg.horizontalCenter
                    }
                 
                    
                }
            }
            }
        }
    }
}

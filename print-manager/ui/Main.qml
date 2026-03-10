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
        Loading
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
            if (acceptMessageButton.text === "Training Completed") {
                stateTimeoutTimer.stop()
                break;
            } else {
                stateTimeoutTimer.restart()
                break;
            }

        default:
            stateTimeoutTimer.restart()
            break
        }
    }

    Material.theme : Material.Dark

    property int appstate: Main.AppState.Idle; //Track app state
    property int appmode : Main.AppMode.Staff //Main.AppMode.User;
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
                color: "#871C1C"
                Image {
                    id : pcmLogo
                    source: "../resources/PC-LogoText.svg"
                    height: 100
                    sourceSize: Qt.size(409, 510)
                    fillMode: Image.PreserveAspectFit
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: 25
                    anchors.leftMargin: 25
                    visible: true
                    opacity: 1

                    /*MultiEffect { //Rainbow effect
                        anchors.fill: pcmLogo
                        source: pcmLogo
                        colorization: 1.0 //Color overlay (on the white svg)
                        colorizationColor: Qt.hsva(hueValue, 1.0, 1.0, 1.0)

                        property real hueValue: 0

                        SequentialAnimation on hueValue { //Infinite rainbow color changing animation
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: 0
                                to: 1
                                duration: 256000 //Takes 256 seconds per loop
                            }
                        }
                    }*/
                }

                StackLayout {

                currentIndex: (appstate > 3) ? appstate - 1 : appstate
                anchors.fill: parent

                Item { //idleFrame container
                    id: idleFrame

                    Text {
                        id: idleText
                        anchors.centerIn: parent
                        font.pointSize: 36
                        color: "#fff"
                        text: "Welcome to the\nPhysical Computing Makerspace!"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item {

                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 100
                        width: 630
                        height: 50

                        IconButtonC {
                            id: orcaSlicerButton
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            onClicked: backend.orcaButtonClicked();
                            width: 200
                            height: 50
                            radius: 5
                            color: "#871c1c"
                            pressed_color : "#6b1616"
                            image_source: "../resources/OrcaSlicer.svg"
                            label_text: "Open OrcaSlicer"
                        }

                        IconButtonC {
                            id: helpButton
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            onClicked: {
                                backend.helpButtonClicked();
                                messageText.text = "Start by slicing your .obj file\nor selecting a sliced file\nThen scan your ID and follow the\nInstructions on screen to start your print";
                                messageFrame.nextState = Main.AppState.Idle
                                rootWindow.appstate = Main.AppState.Message
                            }
                            width: 200
                            height: 50
                            radius: 5
                            color: "#871c1c"
                            pressed_color : "#6b1616"
                            image_source: "../resources/help.svg"
                            label_text: "Help"
                        }

                        IconButtonC {
                            id: uploadButton
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            onClicked: gcodeFileDialog.open()
                            width: 200
                            height: 50
                            radius: 5
                            color: "#871c1c"
                            pressed_color : "#6b1616"
                            label_text : "Upload GCode"
                            image_source: "../resources/upload_file.svg"

                            FileDialog {
                                id: gcodeFileDialog
                                title: "Select a file"
                                nameFilters: ["GCode File (*.gcode *.bgcode *.gcode.3mf)"]
                                onAccepted: {
                                    rootWindow.appstate = Main.AppState.Loading
                                    backend.fileUploaded(gcodeFileDialog.selectedFile)
                                }
                            }
                        }
                    }
                }

                Item {
                    id: prepFrame

                    Text {
                        id: prepLabel
                        text: "Print Information"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.horizontalCenter: parent.horizontalCenter;
                        anchors.top: parent.top
                        anchors.topMargin: 160
                        font.pointSize: 36
                        font.bold: true
                        color: "#fff"
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: prepLabel.bottom
                        anchors.topMargin: 20
                        width: printInfoText.implicitWidth + 20
                        height: printInfoText.implicitHeight + 20
                        color : "#871C1C"
                        border.width: 2
                        border.color: "#fff"
                        radius: 2
                        Text {
                            id: printInfoText
                            text: "No print information found"
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 10
                            color: "#fff"
                            font.pointSize: 18
                        }
                    }

                    Connections {
                        target: backend
                        function onPrintInfoLoaded(printInfo) {
                            console.log("Print has been loaded!")
                            console.log(printInfo)
                            let op = `Filename: ${printInfo.filename}\nPrinter: ${printInfo.printer}\nFilament: ${printInfo.filamentType}\nWeight: ${printInfo.weight}g\nDuration: ${printInfo.duration}`;
                            if (printInfo.hasOwnProperty("printSettings")) op += `\nPrint Settings: ${printInfo.printSettings}`
                            printInfoText.text = op
                        }
                    }

                    Connections {
                        target: printermanager
                        function onJobInfoLoaded(printInfo) {
                            console.log("Job has been loaded!")
                            console.log(printInfo)
                            let op = `Filename: ${printInfo.filename}\nPrinter: ${printInfo.printer}\nFilament: ${printInfo.filamentType}\nWeight: ${printInfo.weight}g\nDuration: ${printInfo.duration}`;
                            if (printInfo.hasOwnProperty("printSettings")) op += `\nPrint Settings: ${printInfo.printSettings}`
                            printInfoText.text = op

                            rootWindow.flags |= Qt.WindowStaysOnTopHint
                            rootWindow.show()
                            rootWindow.raise()
                            rootWindow.requestActivate()
                            rootWindow.flags &= ~Qt.WindowStaysOnTopHint
                        }
                    }

                    RoundButtonC {
                        id: cancelPrepButton
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.bottomMargin: 10
                        onClicked: {
                            rootWindow.appstate = Main.AppState.Idle
                            printInfoText.text = "No print information found"
                        }
                        width: 160
                        height: 40
                        radius: 5
                        color: "#871c1c"
                        pressed_color : "#6b1616"
                        label_text: "Cancel"
                    }

                    RoundButtonC {
                        id: beginPrintButton
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.bottomMargin: 10
                        onClicked: {
                            rootWindow.appstate = Main.AppState.UserScan
                            printInfoText.text = "No print information found"
                        }
                        width: 160
                        height: 40
                        radius: 5
                        color: "#871c1c"
                        pressed_color : "#6b1616"
                        label_text : "Print"
                    }
                }

                Item {
                    id: messageFrame
                    property int nextState: Main.AppState.Idle

                    Text {
                        id: messageText
                        text: "Unknown Error"
                        font.pointSize: 24
                        anchors.horizontalCenter: parent.horizontalCenter;
                        anchors.verticalCenter: parent.verticalCenter;
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#fff"
                    }

                    Connections {
                        target: backend
                        function onMessageReq(msg, btnText, newState) {
                            messageText.text = msg
                            acceptMessageButton.text = btnText
                            messageFrame.nextState = newState
                        }
                    }

                    RoundButtonC {
                        id: acceptMessageButton
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 50
                        onClicked: {
                            rootWindow.appstate = messageFrame.nextState
                            messageText.text = "Unknown Error"
                            messageFrame.nextState = Main.AppState.Idle
                        }
                        width: 160
                        height: 40
                        radius: 5
                        color: "#871c1c"
                        pressed_color : "#6b1616"
                        label_text: "OK"

                    }

                    RoundButtonC {
                        id: cancelMessageButton
                        visible: parent.nextState != Main.AppState.Idle
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.bottomMargin: 10
                        onClicked: {
                            rootWindow.appstate = Main.AppState.Idle
                            message.text = "Unknown Error"
                        }
                        width: 160
                        height: 40
                        radius: 5
                        color: "#871c1c"
                        pressed_color : "#6b1616"
                        label_text: "Cancel"
                    }
                }

                Item { //scanFrame container
                    id: scanFrame

                    Image {
                        id: ltx2a
                        source: "../resources/ltx2a_ledoff.svg" //image source relative to Main.qml (image files must be explicitly included in CMakeLists.txt)
                        height: 300
                        fillMode: Image.PreserveAspectFit
                        anchors.left: parent.left
                        anchors.leftMargin: 100
                        anchors.top: parent.top
                        anchors.topMargin: 165
                    }

                    Image {
                        id: card
                        source: "../resources/card_filled.svg"
                        height: 150
                        fillMode: Image.PreserveAspectFit
                        anchors.right: parent.right
                        anchors.rightMargin: 150 + 235*offset
                        anchors.top: parent.top
                        anchors.topMargin: 260
                        property real offset: 0

                        SequentialAnimation on offset { //Animate the card moving and LTx2A lighting up
                            loops: Animation.Infinite
                            NumberAnimation { //Animate the offset (and therefore the horizontal position of card)
                                from: 0;
                                to: 1;
                                duration: 1000;
                                easing.type: Easing.InOutCubic;
                                easing.amplitude: 1;
                                easing.period: 1.0;
                            }
                            ScriptAction {
                                script: ltx2a.source = "../resources/ltx2a_ledon.svg" //Change image of LTx2A
                            }
                            NumberAnimation { //Do nothing for 1 seconds
                                from: 1;
                                to : 1;
                                duration: 1000;
                            }
                            ScriptAction {
                                script: ltx2a.source = "../resources/ltx2a_ledoff.svg"
                            }
                            NumberAnimation { //Animate card back to starting pos
                                from: 1;
                                to: 0;
                                duration: 1000;
                                easing.type: Easing.InOutCubic
                                easing.amplitude: 1;
                                easing.period: 1.0;
                            }
                            NumberAnimation { //Do nothing for 2 seconds
                                from: 0;
                                to : 0;
                                duration: 2000;
                            }
                        }
                    }

                    Text {
                        id: tapText
                        text: (rootWindow.appstate == Main.AppState.UserScan) ? "Tap your ID or UCard" : "Tap Staff Card"
                        font.pointSize: 40
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 20
                        anchors.right: parent.right
                        anchors.rightMargin: 50
                        color: "#ffffff"
                    }

                    RoundButtonC {
                        id: cancelScanButton
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.bottomMargin: 10
                        onClicked: {
                            rootWindow.appstate = Main.AppState.Idle
                            printInfoText.text = "No print information found"
                        }
                        width: 160
                        height: 40
                        radius: 5
                        color : "#871c1c"
                        pressed_color : "#6b1616"
                        label_text: "Cancel"
                    }
                }

                Item {
                    id: printingFrame

                    Text {
                        id: printingText
                        text: "Printing now!" //In the future this will be set to user name based on airtable data
                        font.pointSize: 100
                        anchors.horizontalCenter: parent.horizontalCenter;
                        anchors.verticalCenter: parent.verticalCenter;
                        color: "#ffffff"
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
                        color: "#fff"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
            }
            Rectangle {
                id:terminalContainer
                color: "#000005"
                property string firstname: "John Doe";
                property int authlevel: 1; //0 for normal user, 1 for employee, 2 for program admin, 3 for supervisor
                FontLoader {
                    id: codeFont
                    source: "../resources/JetBrainsMono-Regular.ttf"
                }

                Text {
                    id: pcmArt
                    text: "--/\\\\\\\\\\\\\\\\\\\\\\\\\\-------/\\\\\\\\\\\\\\\\\\--/\\\\\\\\------------/\\\\\\\\-
 -\\/\\\\\\/////////\\\\\\---/\\\\\\///////--\\/\\\\\\\\\\\\--------/\\\\\\\\\\\\-
  -\\/\\\\\\-------\\/\\\\\\-/\\\\\\/----------\\/\\\\\\//\\\\\\----/\\\\\\//\\\\\\-
   -\\/\\\\\\\\\\\\\\\\\\\\\\\\\\/-\\/\\\\\\-----------\\/\\\\\\\\///\\\\\\/\\\\\\/-\\/\\\\\\-
    -\\/\\\\\\/////////---\\/\\\\\\-----------\\/\\\\\\--\\///\\\\\\/---\\/\\\\\\-
     -\\/\\\\\\------------\\/\\\\\\-----------\\/\\\\\\----\\///-----\\/\\\\\\-
      -\\/\\\\\\------------\\/\\\\\\-----------\\/\\\\\\-------------\\/\\\\\\-
       -\\/\\\\\\------------\\///\\\\\\---------\\/\\\\\\-------------\\/\\\\\\-
        -\\/\\\\\\--------------\\///\\\\\\\\\\\\\\\\\\-\\/\\\\\\-------------\\/\\\\\\-
         -\\///-----------------\\/////////--\\///--------------\\///--"
                    anchors.top : parent.top
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignTop
                    color : "#fff"
                    font.pixelSize: 12
                    font.family: codeFont.name
                    font.preferShaping: false

                    // Rectangle {
                    //     border.width: 1
                    //     border.color: "#f00"
                    //     anchors.fill: parent
                    // }
                }

                Item {
                    id: display
                    anchors.top: pcmArt.bottom
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    height: parent.height - pcmArt.height - terminalInput.anchors.bottomMargin - pcmArt.anchors.topMargin - anchors.topMargin - terminalInput.anchors.topMargin - terminalInput.height
                    ScrollableLabel {
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: terminalInput.width;
                        id: displayLeft
                        text: ""
                        font.family: codeFont.name
                        font.pixelSize: 16
                        font.preferShaping: false
                        color: "#fff"
                    }
                    Text {

                        function getTitle(aul) {
                            // console.log(parent.height);
                            // console.log(terminalContainer.height - pcmArt.height - terminalInput.anchors.bottomMargin - pcmArt.anchors.topMargin - parent.anchors.topMargin - terminalInput.anchors.topMargin - terminalInput.height)
                            switch(aul) {
                                case 0:
                                case 1:
                                default:
                                    return "Employee"
                                case 2:
                                      return "Program Administrator"
                                case 3:
                                      return "Supervisor"
                            }
                        }

                        anchors.fill: parent
                        id: displayCenter
                        text: "PCM 3DPK Management Staff Utility\nWelcome " + getTitle(terminalContainer.authlevel) + " " + terminalContainer.firstname
                        font.family: codeFont.name
                        font.pixelSize: 24
                        font.preferShaping: false
                        color: "#fff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }



                    Connections {
                        target: backend
                        function onSetTerminalContent(ltxt, ctxt) {
                            displayLeft.text = preprocess(ltxt)
                            displayCenter.text = ctxt
                        }
                        function onSetTerminalUser(fn, au) {
                            terminalContainer.firstname = fn
                            terminalContainer.authlevel = au
                        }
                        function onTPrint(ln) {
                            displayLeft.text = displayLeft.text + preprocess(ln)
                        }
                        function preprocess(s) {
                            let ns = String(s);
                            ns = ns
                                .replace(/&/g, "&amp;")
                                .replace(/\t/g, "&nbsp;&nbsp;&nbsp;&nbsp;")
                                .replace(/\n/g, "<br>")
                            return ns
                        }

                    }


                }

                TextInput {
                    id: terminalInput
                    anchors.top: display.bottom
                    anchors.topMargin: 5
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 10
                    width: parent.width - 20
                    property string prefix: ">"
                    font.pixelSize: 16
                    font.preferShaping: false
                    color: "#fff"
                    text: prefix
                    cursorVisible: true
                    focus: true

                    Keys.onPressed: function(event){
                        if (event.key === Qt.Key_Backspace) {
                            if (cursorPosition <= prefix.length) {
                                // ignore backspace at the start
                                event.accepted = true
                            } else {
                                event.accepted = false
                            }
                        } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                            if (terminalInput.text.length <= prefix.length) return;
                            displayCenter.text = ""
                            displayLeft.text = displayLeft.text + "<br>" + sanitized(terminalInput.text) + "<br>";
                            backend.processCommand(terminalInput.text.substr(prefix.length));
                            terminalInput.text = prefix
                            cursorPosition = prefix.length
                            event.accepted = true
                        }
                    }

                    function sanitized(s) {
                        let ns = String(s);
                        ns = ns
                        .replace(/&/g, "&amp;")
                        .replace(/</g, "&lt;")
                        .replace(/>/g, "&gt;")
                        return ns
                    }

                    Keys.onLeftPressed: function(event) {
                        if (cursorPosition <= prefix.length) {
                            cursorPosition = prefix.length
                            event.accepted = true
                        } else {
                            event.accepted = false
                        }
                    }

                    onCursorPositionChanged: {
                        if (cursorPosition < prefix.length) cursorPosition = prefix.length
                    }

                    onTextChanged: {
                        if (!text.startsWith(prefix)) text = prefix
                    }

                }
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import PolyhydranPrintManager

Item {
    // anchors.fill: parent
    // anchors.centerIn: parent
    property bool error: false
    id:prep

    Item {
        height: childrenRect.height
        width: parent.width
        anchors.fill: parent
        anchors.centerIn: parent
        Text {
            id: prepLabel
            text: "Print Information"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 120
            font.pointSize: 36
            font.bold: true
            color: Theme.text
        }

        Rectangle {
            anchors.top: prepLabel.bottom
            anchors.topMargin: 20
            width: printInfoText.implicitWidth + 20
            height: printInfoText.implicitHeight + 20
            color : Theme.background
            anchors.horizontalCenter: parent.horizontalCenter
            border.width: 2
            border.color: Theme.text
            radius: 2
            id: printInfoRect
            Text {
                id: printInfoText
                text: "No print information found"
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                color: Theme.text
                font.pointSize: 18
                visible:true
            }
        }
        Text {
                id:err
                anchors.horizontalCenter: printInfoRect.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.top: printInfoRect.bottom
                anchors.topMargin: 10
                text: "Unknown Error"
                font.pointSize: 14
                color: "#ff2828"
                visible:prep.error
            }
        Item {
            visible: !prep.error
            width: printInfoRect.width
            height: 200
            anchors.top:printInfoRect.bottom
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            Text {
                text: "Filament Provider"
                width: parent.width
                horizontalAlignment : Text.AlignHCenter
                font.pointSize: 16
                color: Theme.text
                font.bold:true
                id: fpText
            }

            Item {
                anchors.top: fpText.bottom
                anchors.topMargin: 0
                anchors.horizontalCenter: parent.horizontalCenter
                width:childrenRect.width
                height:childrenRect.height
                RowLayout {
                    RadioButton {
                        checked:true
                        Material.theme: Theme.isDark ? Material.Dark : Material.Light
                        Material.foreground: Theme.text
                        Material.primary: Theme.primary
                        Material.accent : Theme.primary
                        text: "Makerspace Filament"
                        id: msfButton
                        onClicked: {
                            frontman.setLoadedPrintFilamentProvider(psfButton.checked)
                        }
                    }
                    RadioButton {
                        checked:false
                        Material.theme: Theme.isDark ? Material.Dark : Material.Light
                        Material.foreground: Theme.text
                        Material.primary: Theme.primary
                        Material.accent : Theme.primary
                        text: "Personal Filament"
                        id: psfButton
                        onClicked: {
                            frontman.setLoadedPrintFilamentProvider(psfButton.checked)
                        }
                    }
                }

                ButtonGroup {
                    id: filamentProvider
                    buttons: [msfButton, psfButton]
                }
            }
        }

    }

    function truncateFilename(fullFilename : string) : string {
        let filename;
        const maxLen = 45;
        if (fullFilename.length > maxLen) {
            if (fullFilename.endsWith(".gcode.3mf")) {
                filename = fullFilename.slice(0,33) + "...gcode.3mf";
            } else {
                let dotparts = fullFilename.split(".")
                filename = fullFilename.slice(0,maxLen-2-dotparts[dotparts.length -1].length) + "..." + dotparts[dotparts.length-1]
            }

        } else filename = fullFilename
        return filename
    }

    function loadPrintInfo(printInfo) {
        let op = `Filename: ${truncateFilename(printInfo.filename)}\nPrinter: ${printInfo.printer}\nFilament: ${(printInfo.hasOwnProperty("filament")) ? printInfo.filament : printInfo.filamentType}\nWeight: ${(printInfo.weight.trim().endsWith("g")) ? printInfo.weight : printInfo.weight + "g"}\nDuration: ${printInfo.duration}`;
        if (printInfo.hasOwnProperty("printerName")) op += `\nPrinter Name: ${printInfo.printerName}`
        if (printInfo.hasOwnProperty("printSettings")) op += `\nPrint Settings: ${printInfo.printSettings}`
        if (printInfo.hasOwnProperty("personalFilament")) {
            msfButton.checked = !printInfo.personalFilament
            psfButton.checked = printInfo.personalFilament
        } else {
            msfButton.checked = true
            psfButton.checked = false
        }
        error = !printInfo.connected || printInfo.jobStatus >= 100
        if (error) {
            if (!printInfo.connected) {
                err.text = "Selected printer is not connected"
            } else if (printInfo.jobStatus >= 100) {
                if (printInfo.jobStatus < 200) {
                    err.text = "Selected printer is busy"
                } else {
                    err.text = "Selected printer encountered an error"
                }
            } else {
                err.text = "Solar bit flip: no causality"//lol
            }
        }

        printInfoText.text = op
    }

    function raiseWindow() {
        //rootWindow.flags |= Qt.WindowStaysOnTopHint
        //Demand attention
        if (rootWindow.visibility === Window.Minimized)
            rootWindow.showNormal()
        else
            rootWindow.show()

        rootWindow.raise()
        rootWindow.requestActivate()
        rootWindow.alert(0)
        //rootWindow.flags &= ~Qt.WindowStaysOnTopHint
    }

    Connections {
        target: frontman
        function onPrintInfoLoaded(printInfo) {
            prep.loadPrintInfo(printInfo)
        }
        function onRaiseRequested(){
            prep.raiseWindow()
        }
    }

    Connections {
        target: printermanager
        function onJobInfoLoaded(printInfo) {
            prep.loadPrintInfo(printInfo)
            // prep.raiseWindow()
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
            prep.error = false
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        text_color: Theme.primaryText
        label_text: "Cancel"
    }

    RoundButtonC {
        enabled: !prep.error
        id: beginPrintButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        onClicked: {
            if (enabled) {
                rootWindow.scancontext = Main.ScanContext.UserAuth
                rootWindow.appstate = Main.AppState.Scan
                printInfoText.text = "No print information found"
                prep.error = false
                frontman.setLoadedPrintFilamentProvider(psfButton.checked)
            }
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        label_text : "Print"
        text_color : Theme.primaryText
    }
}

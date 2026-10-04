import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import PolyhydranPrintManager

Item {
    // anchors.fill: parent
    // anchors.centerIn: parent
    property bool error: false
    property var printIssues: ({})
    id:prepOverride

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
            anchors.topMargin: 90
            font.pointSize: 24
            font.bold: true
            color: Theme.text
        }

        Rectangle {
            anchors.top: prepLabel.bottom
            anchors.topMargin: 20
            width: ((prepOverride.error) ? err.implicitWidth : printInfoText.implicitWidth) + 20
            height: ((prepOverride.error) ? err.implicitHeight : printInfoText.implicitHeight) + 20
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
                font.pointSize: 12
                visible:!prepOverride.error
            }
            Text {
                id:err
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                text: "Selected printer is not connected"
                font.pointSize: 20
                color: "#ff2828"
                visible:prepOverride.error
            }
        }
        Rectangle {
            anchors.top: printInfoRect.bottom
            anchors.topMargin: 20
            width: printIssuesText.implicitWidth + 20
            height: printIssuesText.implicitHeight + 20
            color : Theme.background
            anchors.horizontalCenter: parent.horizontalCenter
            border.width: 2
            border.color: Theme.text
            radius: 2
            visible:!prepOverride.error
            id: printIssuesRect
            Text {
                id:printIssuesText
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                anchors.top: parent.top
                anchors.topMargin: 10
                text: "No issues found"
                font.pointSize: 12
                color: (text === "No issues found") ? "#77ff55": "#ffbb28"

            }
        }
        Item {
            visible: !prepOverride.error
            width: printInfoRect.width
            height: 200
            anchors.top:printIssuesRect.bottom
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
                        text: "Makerspace Filament"
                        id: msfButton
                        onClicked: {
                            backend.setLoadedPrintFilamentProvider(psfButton.checked)
                            prepOverride.printIssues.personalFilament = false
                            loadPrintIssues(prepOverride.printIssues)
                        }
                    }
                    RadioButton {
                        checked:false
                        text: "Personal Filament"
                        id: psfButton
                        onClicked: {
                            backend.setLoadedPrintFilamentProvider(psfButton.checked)
                            prepOverride.printIssues.personalFilament = true
                            loadPrintIssues(prepOverride.printIssues)
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

    function loadPrintIssues(issuesInfo) {
        let op = ""
        if (issuesInfo.hasOwnProperty("isRegistered") && !issuesInfo.isRegistered) { printIssuesText.text = "User not registered"; return; }
        if (issuesInfo.hasOwnProperty("trained") && !issuesInfo.trained) op+="\nTraining not completed"
        if (issuesInfo.hasOwnProperty("isCICS") && issuesInfo.hasOwnProperty("duration") && issuesInfo.hasOwnProperty("personalFilament")) {
            if (issuesInfo.isCICS) {
                if (issuesInfo.personalFilament && issuesInfo.duration > 10) op+="\nPrint longer than 10 hours"
                if (!issuesInfo.personalFilament && issuesInfo.duration > 6) op+="\nPrinting longer than 6 hours with Makerspace Filament"
            } else {
                if(!issuesInfo.personalFilament) op+="\nNon-CICS printing with Makerspace Filament"
                if(issuesInfo.duration > 6) op+="\nNon-CICS printing longer than 6 hours"
            }
        }     
        if (op.length > 1) {
            printIssuesText.text = op.slice(1)
        } else {
            printIssuesText.text = "No issues found"
        }

    }

    Connections {
        target: backend
        function onPrintIssuesLoaded(printInfo, issuesInfo) {
            prepOverride.loadPrintInfo(printInfo)
            prepOverride.printIssues = issuesInfo
            prepOverride.loadPrintIssues(issuesInfo)
        }
    }

    RoundButtonC {
        id: cancelPrepOverrideButton
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.bottomMargin: 10
        onClicked: {
            rootWindow.appstate = Main.AppState.Idle
            printInfoText.text = "No print information found"
            printIssuesText.text = "No Issues"
            prepOverride.error = false
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        label_text: "Cancel"
        text_color: Theme.primaryText
    }

    RoundButtonC {
        visible: !prepOverride.error
        id: beginPrintOverrideButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        onClicked: {
            rootWindow.scancontext = Main.ScanContext.StaffAuth
            rootWindow.appstate = Main.AppState.Scan
            printInfoText.text = "No print information found"
            printIssuesText.text = "No Issues"
            backend.setLoadedPrintFilamentProvider(psfButton.checked)
            prepOverride.error = false
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        label_text : "Override"
        text_color: Theme.primaryText
    }
}

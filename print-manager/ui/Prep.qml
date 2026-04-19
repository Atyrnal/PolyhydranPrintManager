import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    anchors.fill: parent
    anchors.centerIn: parent
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
            anchors.topMargin: 160
            font.pointSize: 36
            font.bold: true
            color: "#fff"
        }

        Rectangle {
            anchors.top: prepLabel.bottom
            anchors.topMargin: 20
            width: printInfoText.implicitWidth + 20
            height: printInfoText.implicitHeight + 20
            color : "#161619"
            anchors.horizontalCenter: parent.horizontalCenter
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
    }

    Connections {
        target: backend
        function onPrintInfoLoaded(printInfo) {
            console.log("Print has been loaded!")
            console.log(printInfo)
            let op = `Filename: ${printInfo.filename}\nPrinter: ${printInfo.printer}\nFilament: ${(printInfo.hasOwnProperty("filament")) ? printInfo.filament : printInfo.filamentType}\nWeight: ${(printInfo.weight.trim().endsWith("g")) ? printInfo.weight : printInfo.weight + "g"}\nDuration: ${printInfo.duration}`;
            if (printInfo.hasOwnProperty("printSettings")) op += `\nPrint Settings: ${printInfo.printSettings}`
            printInfoText.text = op
        }
    }

    Connections {
        target: printermanager
        function onJobInfoLoaded(printInfo) {
            console.log("Job has been loaded!")
            console.log(printInfo)
            let op = `Filename: ${printInfo.filename}\nPrinter: ${printInfo.printer}\nFilament: ${(printInfo.hasOwnProperty("filament")) ? printInfo.filament : printInfo.filamentType}\nWeight: ${(printInfo.weight.trim().endsWith("g")) ? printInfo.weight : printInfo.weight + "g"}\nDuration: ${printInfo.duration}`;
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
        border_width: 0
        color: "#8188cc"
        pressed_color : "#50568a"
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
        border_width: 0
        color: "#8188cc"
        pressed_color : "#50568a"
        label_text : "Print"
    }
}

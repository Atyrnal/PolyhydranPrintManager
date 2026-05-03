import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import PolyhydranPrintManager

Item {
    // anchors.fill: parent
    // anchors.centerIn: parent
    Item {
        height: childrenRect.height
        width: parent.width
        anchors.fill: parent
        anchors.centerIn: parent
        Text {
            id: printerSelectionLabel
            text: "Select Printer"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 120
            font.pointSize: 36
            font.bold: true
            color: Theme.text
        }

        PrinterSelect {
            model: printersModel
            anchors.top: printerSelectionLabel.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            width: 600
            height: 300
            columns: [
                { label: "Printer Name", role:"name", width: 150},
                { label: "Brand", role:"brand", width: 100},
                { label: "Model", role:"model", width: 250},
                { label: "Job Status", role:"jobStatus", width: 100}
            ]
        }
    }

    RoundButtonC {
        id: cancelPrintSelectionButton
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.bottomMargin: 10
        onClicked: {
            rootWindow.appstate = Main.AppState.Idle
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        label_text: "Cancel"
    }

    RoundButtonC {
        id: selectPrinterButton
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        onClicked: {
            rootWindow.appstate = Main.AppState.Prep
        }
        width: 160
        height: 40
        radius: 5
        border_width: 0
        color: Theme.primary
        pressed_color : Theme.primaryActive
        label_text : "Select"
    }
}

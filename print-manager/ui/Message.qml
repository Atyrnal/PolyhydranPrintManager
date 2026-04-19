import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import PolyhydranPrintManager

Item {
    property int nextState: Main.AppState.Idle
    property var showMessage: function (message, nextsState) {
        messageText.text = message
        nextState = nextsState
    }

    Text {
        id: messageText
        text: "Unknown Error"
        font.pointSize: 20
        anchors.horizontalCenter: parent.horizontalCenter;
        anchors.verticalCenter: parent.verticalCenter;
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: Theme.text
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
        color: Theme.primary
        pressed_color : Theme.primaryActive
        border_width: 0
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
        color: Theme.primary
        pressed_color : Theme.primaryActive
        border_width: 0
        label_text: "Cancel"
    }
}


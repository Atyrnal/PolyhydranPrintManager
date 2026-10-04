import QtQuick
import QtQuick.Controls

RoundButton {
    id: buttonRoot
    radius: 5
    width: 200
    height: 100
    property alias label_text: label.text
    property alias text_color : label.color
    property color color: "#cccccc"
    property color pressed_color : "#aaaaaa"
    property alias border_width : bgrect.border.width
    property alias border_color : bgrect.border.color
    background: Rectangle {
        id: bgrect
        color: buttonRoot.down ? buttonRoot.pressed_color : buttonRoot.color
        border.width: 1
        border.color: "#fff"
        radius: buttonRoot.radius
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
        }
    }

    Text {
        id : label
        text: "Button"
        color: "#fff"
        anchors.centerIn: parent
    }

}

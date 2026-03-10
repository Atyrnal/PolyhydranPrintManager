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
    property alias image_source : image.source

    background: Rectangle {
        id: bgrect
        color: parent.down ? parent.pressed_color : parent.color
        border.width: 1
        border.color: "#fff"
        radius: parent.radius
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
        }
    }

    Item {
        anchors.centerIn: parent
        anchors.fill: parent
        Image {
            id: image
            height: Math.min(parent.height, parent.width) / 2
            width: Math.min(parent.height, parent.width) / 2
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: (parent.width - width - label.width - label.anchors.leftMargin) / 2
        }

        Text {
            id: label
            text: "Icon Button"
            color: "#fff"
            anchors.left: image.right
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
        }
    }

}

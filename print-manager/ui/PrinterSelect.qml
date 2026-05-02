import QtQuick

ListView {
    id:printerSelectRoot
    clip: true
    spacing: 2
    property var columns: [];
    property double rowHeight: 40;
    property color textColor: "#ffffff"
    property color headerColor: "#050508"
    property color rowColor1: "#101013"
    property color rowColor2: "#131317"

    headerPositioning: ListView.OverlayHeader

    header: Row {
        spacing: 0
        Repeater {
            model: printerSelectRoot.columns
            delegate: Rectangle {
                width: modelData.width
                height: rowHeight
                color: headerColor
                Text {
                    anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                    text: modelData.label
                    color: textColor
                    font.bold: true
                }
            }
        }
    }

    delegate: Rectangle {
        width: printerSelectRoot.width
        height: rowHeight
        color: index % 2 === 0 ? rowColor1 : rowColor2

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            Repeater {
                model: printerSelectRoot.columns
                delegate: Item {
                    width: modelData.width;
                    height: rowHeight
                    Text {
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: tableRow[modelData.role] ?? ""
                        font.pixelSize: 14
                        color: textColor
                    }
                }
            }
        }

        // Hover highlight
        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: hoverHandler.hovered ? 0.04 : 0
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }
        HoverHandler { id: hoverHandler }
    }
}
import QtQuick

Item { //scanFrame container
    property bool isStaff : false
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
        text: (isStaff) ? "Tap your ID or UCard" : "Tap Staff Card"
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
        border_width: 0
        color: "#8188cc"
        pressed_color : "#50568a"
        label_text: "Cancel"
    }
}
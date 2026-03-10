import QtQuick

Item {
    id: root
    property alias text: label.text
    property alias font: label.font
    property alias color: label.color

    onTextChanged: {
        console.log(text)
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: label.height
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        onContentHeightChanged: {
            if (atBottom) {
                Qt.callLater(() => {
                    flick.contentY = Math.max(0, contentHeight - height)
                })
            }
        }

        property bool atBottom: contentY >= contentHeight - height - 1

        onContentYChanged: {
            atBottom = contentY >= contentHeight - height - 1
        }

        onMovementEnded: {
            snapToLine()
        }

        onFlickEnded: {
            snapToLine()
        }

        function snapToLine() {
            var lineHeight = label.font.pixelSize * 1.2 // Approximate line height
            var targetY = Math.round(contentY / lineHeight) * lineHeight
            targetY = Math.max(0, Math.min(targetY, contentHeight - height))
            snapAnimation.to = targetY
            snapAnimation.start()
        }

        NumberAnimation {
            id: snapAnimation
            target: flick
            property: "contentY"
            duration: 150
            easing.type: Easing.OutQuad
        }

        Text {
            id: label
            width: flick.width
            y: Math.max(0, flick.height - height)
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignLeft
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Item { //idleFrame container

    FontLoader {
        id: playfair
        source: "../resources/PlayfairDisplay-Regular.ttf"
    }


    Item { //Branding container
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: parent.width
        height: 256
        RowLayout {
            anchors.fill: parent
            anchors.centerIn: parent
            Item { //Polyhydran branding
                Layout.fillWidth: true
                Layout.fillHeight: true
                Item {
                    anchors.centerIn: parent
                    width: childrenRect.width
                    height: childrenRect.height
                    Image {
                        id : polyhydranLogo2
                        source: "../resources/StellatedPolyhedronWhite.png"
                        height: 96
                        sourceSize: Qt.size(1080, 1080)
                        fillMode: Image.PreserveAspectFit
                        anchors.left: parent.left
                        //anchors.verticalCenter: parent.verticalCenter
                        visible: true
                        opacity: 1
                        smooth: true
                        antialiasing: true
                    }

                    Text {
                        id: polyhydranLabel2
                        anchors.left: polyhydranLogo2.right
                        anchors.leftMargin: 5
                        anchors.verticalCenter: polyhydranLogo2.verticalCenter
                        anchors.verticalCenterOffset: -18
                        font.family: playfair.name
                        font.pointSize: 32
                        text : "Polyhydran"
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignLeft
                    }

                    Text {
                        anchors.top: polyhydranLabel2.bottom
                        anchors.horizontalCenter: polyhydranLabel2.horizontalCenter
                        font.pointSize: 14
                        font.bold: true
                        text : "PRINT MANAGER"
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
            Rectangle {
                width: 2
                height: parent.height/3
                color: "#ffffff"
            }

            Item { //Customer branding
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    anchors.centerIn: parent
                    width: childrenRect.width
                    height: childrenRect.height
                    FontLoader {
                        id: inter
                        source: "../resources/Inter-Regular.ttf"
                    }
                    Image {
                        id : customerLogo
                        source: "../resources/pcm_logo_v2_white_purple_128.png"
                        height: 128
                        fillMode: Image.PreserveAspectFit
                        anchors.left: parent.left
                        //anchors.verticalCenter: parent.verticalCenter
                        visible: true
                        opacity: 1
                    }
                    ColumnLayout {
                        anchors.left: customerLogo.right
                        anchors.verticalCenter: customerLogo.verticalCenter
                        anchors.leftMargin: 24
                        Text {
                            font.family: inter.name
                            font.pointSize: 18
                            text : "Physical"
                            color: "#ffffff"

                            horizontalAlignment: Text.AlignLeft
                        }
                        Text {
                            font.family: inter.name
                            font.pointSize: 18
                            text : "Computing"
                            color: "#ffffff"

                            horizontalAlignment: Text.AlignLeft
                        }
                        Text {
                            font.family: inter.name
                            font.pointSize: 18
                            text : "Makerspace"
                            color: "#ffffff"
                            font.bold:true
                            horizontalAlignment: Text.AlignLeft
                        }

                    }
                }
            }
        }

    }



    Text {
        id: idleText
        anchors.centerIn: parent
        font.pointSize: 30
        color: "#fff"
        text: "Welcome to the\nPhysical Computing Makerspace!"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Item {

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 100
        width: 630
        height: 50

        IconButtonC {
            id: orcaSlicerButton
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            onClicked: backend.orcaButtonClicked();
            width: 200
            height: 50
            radius: 5
            color: "#8188cc"
            border_width: 0
            pressed_color : "#50568a"
            image_source: "../resources/OrcaSlicer.svg"
            label_text: "Open OrcaSlicer"
        }

        IconButtonC {
            id: helpButton
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            onClicked: {
                backend.helpButtonClicked();
                messageText.text = "Start by slicing your .obj file\nor selecting a sliced file\nThen scan your ID and follow the\nInstructions on screen to start your print";
                messageFrame.nextState = Main.AppState.Idle
                rootWindow.appstate = Main.AppState.Message
            }
            width: 200
            height: 50
            radius: 5
            border_width: 0
            color: "#8188cc"
            pressed_color : "#50568a"
            image_source: "../resources/help.svg"
            label_text: "Help"
        }

        IconButtonC {
            id: uploadButton
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: gcodeFileDialog.open()
            width: 200
            height: 50
            radius: 5
            border_width: 0
            color: "#8188cc"
            pressed_color : "#50568a"
            label_text : "Upload GCode"
            image_source: "../resources/upload_file.svg"

            FileDialog {
                id: gcodeFileDialog
                title: "Select a file"
                nameFilters: ["GCode File (*.gcode *.bgcode *.gcode.3mf)"]
                onAccepted: {
                    rootWindow.appstate = Main.AppState.Loading
                    backend.fileUploaded(gcodeFileDialog.selectedFile)
                }
            }
        }
    }
}

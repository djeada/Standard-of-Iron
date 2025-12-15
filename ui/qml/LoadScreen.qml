import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: load_screen

    property real progress: 0.0
    property bool is_loading: false
    property string stage_text: "Loading..."
    property bool use_real_progress: true  // Toggle between real and fake progress

    anchors.fill: parent
    color: "#000000"
    visible: is_loading

    // Background image
    Image {
        id: background_image
        anchors.fill: parent
        source: "qrc:/assets/visuals/load_screen.png"
        fillMode: Image.PreserveAspectCrop
    }

    // Dark overlay for better text visibility
    Rectangle {
        anchors.fill: parent
        color: "#40000000"
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: parent.height * 0.3
        spacing: 20
        width: parent.width * 0.6

        Text {
            text: qsTr("Loading...")
            color: "#ecf0f1"
            font.pixelSize: 48
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Progress bar container
        Rectangle {
            width: parent.width
            height: 40
            color: "#2c3e50"
            border.color: "#34495e"
            border.width: 2
            radius: 4

            // Progress fill
            Rectangle {
                id: progress_fill
                
                readonly property real available_width: parent.width - 8
                
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 4
                width: Math.max(0, Math.min(available_width, available_width * load_screen.progress))
                color: "#f39c12"
                radius: 2

                // Shine effect
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#40ffffff" }
                        GradientStop { position: 0.5; color: "#00ffffff" }
                        GradientStop { position: 1.0; color: "#40ffffff" }
                    }
                    radius: parent.radius
                }
            }

            // Progress text
            Text {
                anchors.centerIn: parent
                text: Math.floor(load_screen.progress * 100) + "%"
                color: "#ecf0f1"
                font.pixelSize: 18
                font.bold: true
            }
        }

        Text {
            text: load_screen.stage_text
            color: "#bdc3c7"
            font.pixelSize: 18
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Reset progress when loading starts
    onIs_loadingChanged: {
        if (is_loading) {
            if (!use_real_progress) {
                progress = 0.0;
            }
        }
    }

    // Function to complete loading immediately
    function complete_loading() {
        load_screen.progress = 1.0;
        complete_timer.start();
    }

    Timer {
        id: complete_timer
        interval: 300
        repeat: false
        onTriggered: {
            load_screen.is_loading = false;
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: loadScreen

    property real progress: 0.0
    property bool isLoading: false
    
    // Progress animation constants
    readonly property real initialSpeed: 0.02
    readonly property real progressThreshold1: 0.5
    readonly property real progressThreshold2: 0.7
    readonly property real progressThreshold3: 0.9
    readonly property real speedSlow: 0.015
    readonly property real speedSlower: 0.01
    readonly property real speedSlowest: 0.005
    readonly property real minIncrement: 0.001

    anchors.fill: parent
    color: "#000000"
    visible: isLoading

    // Background image
    Image {
        id: backgroundImage
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
                id: progressFill
                
                readonly property real availableWidth: parent.width - 8
                
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 4
                width: Math.max(0, Math.min(availableWidth, availableWidth * loadScreen.progress))
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
                text: Math.floor(loadScreen.progress * 100) + "%"
                color: "#ecf0f1"
                font.pixelSize: 18
                font.bold: true
            }
        }

        Text {
            text: qsTr("Preparing battlefield...")
            color: "#bdc3c7"
            font.pixelSize: 18
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // Fake progress animation
    Timer {
        id: progressTimer
        interval: 50
        repeat: true
        running: loadScreen.isLoading

        property real speed: loadScreen.initialSpeed

        onTriggered: {
            if (loadScreen.progress < 1.0) {
                // Start fast, then slow down exponentially
                var remaining = 1.0 - loadScreen.progress;
                var increment = speed * remaining;
                
                // Ensure minimum progress increment
                increment = Math.max(loadScreen.minIncrement, increment);
                
                loadScreen.progress = Math.min(1.0, loadScreen.progress + increment);
                
                // Slow down as we get closer to 100%
                if (loadScreen.progress > loadScreen.progressThreshold3) {
                    speed = loadScreen.speedSlowest;
                } else if (loadScreen.progress > loadScreen.progressThreshold2) {
                    speed = loadScreen.speedSlower;
                } else if (loadScreen.progress > loadScreen.progressThreshold1) {
                    speed = loadScreen.speedSlow;
                }
            }
        }
    }

    // Reset progress when loading starts
    onIsLoadingChanged: {
        if (isLoading) {
            progress = 0.0;
            progressTimer.speed = loadScreen.initialSpeed;
        }
    }

    // Function to complete loading immediately
    function completeLoading() {
        loadScreen.progress = 1.0;
        completeTimer.start();
    }

    Timer {
        id: completeTimer
        interval: 300
        repeat: false
        onTriggered: {
            loadScreen.isLoading = false;
        }
    }
}

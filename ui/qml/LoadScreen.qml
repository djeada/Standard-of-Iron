import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: load_screen

    property real progress: 0
    property real display_progress: 0
    property real target_progress: 0
    property bool is_loading: false
    property string stage_text: "Loading..."
    property bool use_real_progress: true
    property var _bgSources: ["qrc:/StandardOfIron/assets/visuals/load_screen.png", "qrc:/assets/visuals/load_screen.png", "assets/visuals/load_screen.png", "qrc:/qt/qml/StandardOfIron/assets/visuals/load_screen.png"]
    property int _bgIndex: 0

    function complete_loading() {
        load_screen.target_progress = 1;
        load_screen.display_progress = 1;
    }

    anchors.fill: parent
    color: "#000000"
    visible: is_loading
    onIs_loadingChanged: {
        if (is_loading) {
            target_progress = 0;
            display_progress = 0;
        } else {
            target_progress = 1;
            display_progress = 1;
        }
    }
    onProgressChanged: {
        if (!use_real_progress)
            return ;

        target_progress = Math.max(target_progress, progress);
    }

    Timer {
        id: progressTicker

        interval: 16
        running: is_loading
        repeat: true
        onTriggered: {
            var target = target_progress;
            if (use_real_progress && target < 0.98)
                target = Math.min(0.98, Math.max(target, display_progress + 0.002));

            var delta = target - display_progress;
            var step = delta * 0.15;
            if (step < 0.001 && delta > 0)
                step = 0.001;

            display_progress = Math.min(1, Math.max(0, display_progress + step));
        }
    }

    Image {
        id: background_image

        anchors.fill: parent
        source: load_screen._bgSources[load_screen._bgIndex]
        cache: true
        asynchronous: false
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            if (status === Image.Error && load_screen._bgIndex + 1 < load_screen._bgSources.length) {
                load_screen._bgIndex += 1;
                source = load_screen._bgSources[load_screen._bgIndex];
            }
        }
    }

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

        Rectangle {
            width: parent.width
            height: 40
            color: "#2c3e50"
            border.color: "#34495e"
            border.width: 2
            radius: 4

            Rectangle {
                id: progress_fill

                readonly property real available_width: parent.width - 8

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 4
                width: Math.max(0, Math.min(available_width, available_width * load_screen.display_progress))
                color: "#f39c12"
                radius: 2

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#40ffffff"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#00ffffff"
                        }

                        GradientStop {
                            position: 1
                            color: "#40ffffff"
                        }

                    }

                }

            }

            Text {
                anchors.centerIn: parent
                text: Math.floor(load_screen.display_progress * 100) + "%"
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

}

import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: load_screen

    property real progress: 0
    property real display_progress: 0
    property real target_progress: 0
    property bool is_loading: false
    property string stage_text: qsTr("Loading...")
    property bool use_real_progress: true
    property var bg_sources: ["qrc:/StandardOfIron/assets/visuals/load_screen.png", "qrc:/assets/visuals/load_screen.png", "assets/visuals/load_screen.png", "qrc:/qt/qml/StandardOfIron/assets/visuals/load_screen.png"]
    property int bg_index: 0
    readonly property var hs: StyleGuide.historical

    function complete_loading() {
        load_screen.target_progress = 1;
        load_screen.display_progress = 1;
    }

    anchors.fill: parent
    color: "#0B0806"
    visible: is_loading
    onIs_loadingChanged: {
        if (is_loading) {
            target_progress = 0;
            display_progress = 0;
            tip_plate.draw_tip();
        } else {
            target_progress = 1;
            display_progress = 1;
        }
    }
    onProgressChanged: {
        if (!use_real_progress)
            return;
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
        source: load_screen.bg_sources[load_screen.bg_index]
        cache: true
        asynchronous: false
        fillMode: Image.PreserveAspectCrop
        onStatusChanged: {
            if (status === Image.Error && load_screen.bg_index + 1 < load_screen.bg_sources.length) {
                load_screen.bg_index += 1;
                source = load_screen.bg_sources[load_screen.bg_index];
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#800B0806"
            }

            GradientStop {
                position: 0.34
                color: "#000B0806"
            }

            GradientStop {
                position: 0.72
                color: "#660B0806"
            }

            GradientStop {
                position: 1
                color: "#EE0B0806"
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: Theme.accentBright
            }

            GradientStop {
                position: 0.55
                color: load_screen.hs.bronze
            }

            GradientStop {
                position: 1
                color: "#00000000"
            }
        }
        opacity: 0.9
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: load_screen.hs.bronze
        opacity: 0.35
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: parent.height * 0.28
        spacing: 18
        width: Math.min(parent.width * 0.6, 620)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Loading...")
            color: Theme.textMain
            font.family: Design.Typography.titleFamily
            font.capitalization: Font.AllUppercase
            font.pixelSize: Design.Typography.hero
            font.weight: Design.Typography.bold
            font.hintingPreference: Design.Typography.titleHinting
            font.kerning: true
            font.letterSpacing: Design.Typography.trackingHero
            style: Text.Outline
            styleColor: "#120D09"
        }

        Item {
            width: parent.width
            height: 10

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.horizontalCenter
                anchors.rightMargin: 14
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        position: 0
                        color: Qt.rgba(load_screen.hs.bronze.r, load_screen.hs.bronze.g, load_screen.hs.bronze.b, 0)
                    }

                    GradientStop {
                        position: 1
                        color: Qt.rgba(load_screen.hs.bronze.r, load_screen.hs.bronze.g, load_screen.hs.bronze.b, 0.8)
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 7
                height: 7
                rotation: 45
                color: load_screen.hs.bronze
                opacity: 0.9
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.horizontalCenter
                anchors.right: parent.right
                anchors.leftMargin: 14
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        position: 0
                        color: Qt.rgba(load_screen.hs.bronze.r, load_screen.hs.bronze.g, load_screen.hs.bronze.b, 0.8)
                    }

                    GradientStop {
                        position: 1
                        color: Qt.rgba(load_screen.hs.bronze.r, load_screen.hs.bronze.g, load_screen.hs.bronze.b, 0)
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 28
            color: "#CC120D09"
            border.color: load_screen.hs.bronze
            border.width: 1
            radius: 4

            Rectangle {
                id: progress_fill

                readonly property real available_width: parent.width - 6

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 3
                width: Math.max(0, Math.min(available_width, available_width * load_screen.display_progress))
                radius: 2

                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        position: 0
                        color: "#8F2F2A"
                    }

                    GradientStop {
                        position: 1
                        color: Theme.accent
                    }
                }

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
                text: Design.Numerals.percent(Math.floor(load_screen.display_progress * 100))
                color: Theme.textMain
                font.pixelSize: Design.Typography.label
                font.bold: true
                font.letterSpacing: 1.2
                style: Text.Outline
                styleColor: "#120D09"
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: load_screen.stage_text
            color: Theme.textSubLite
            font.pixelSize: Design.Typography.label
            font.letterSpacing: 0.6
        }
    }

    Item {
        id: tip_plate

        function draw_tip() {
            if (typeof LoadingTips === "undefined")
                return;
            var drawn = LoadingTips.next();
            if (drawn.length > 0)
                tip_text.text = drawn;
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.max(30, parent.height * 0.075)
        width: Math.min(parent.width * 0.72, 820)
        height: tip_column.implicitHeight
        visible: tip_text.text.length > 0
        opacity: 1

        Component.onCompleted: tip_plate.draw_tip()

        SequentialAnimation {
            id: tip_turn

            NumberAnimation {
                target: tip_plate
                property: "opacity"
                to: 0
                duration: Design.A11y.reducedMotion ? 0 : 420
                easing.type: Easing.InQuad
            }

            ScriptAction {
                script: tip_plate.draw_tip()
            }

            NumberAnimation {
                target: tip_plate
                property: "opacity"
                to: 1
                duration: Design.A11y.reducedMotion ? 0 : 420
                easing.type: Easing.OutQuad
            }
        }

        Timer {
            interval: 7000
            running: load_screen.is_loading && tip_plate.visible
            repeat: true
            onTriggered: {
                if (!tip_turn.running)
                    tip_turn.start();
            }
        }

        Column {
            id: tip_column

            width: parent.width
            spacing: 10

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 5
                height: 5
                rotation: 45
                color: load_screen.hs.bronze
                opacity: 0.85
            }

            Text {
                id: tip_text

                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: ""
                color: Theme.textSubLite
                font.pixelSize: Design.Typography.body
                font.italic: true
                font.letterSpacing: 0.4
                style: Text.Outline
                styleColor: "#120D09"
            }
        }
    }
}

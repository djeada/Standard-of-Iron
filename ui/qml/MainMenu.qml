import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property bool game_started: false
    readonly property bool compact: width < 820
    readonly property bool narrow: width < 620
    readonly property int side_margin: Math.max(18, Math.min(56, width * 0.055))
    readonly property int top_margin: Math.max(18, Math.min(48, height * 0.055))
    readonly property int command_width: root.compact ? width - side_margin * 2 : Math.min(560, Math.max(480, width * 0.42))
    readonly property var hs: StyleGuide.historical

    signal open_skirmish()
    signal open_campaign()
    signal open_objectives()
    signal open_settings()
    signal load_save()
    signal save_game()
    signal exit_requested()

    anchors.fill: parent
    z: 10
    focus: true

    function trigger_selection(index) {
        var m = menuModel.get(index);
        if (!m || (m.requiresGame && !root.game_started))
            return;

        if (m.idStr === "skirmish")
            root.open_skirmish();
        else if (m.idStr === "campaign")
            root.open_campaign();
        else if (m.idStr === "objectives")
            root.open_objectives();
        else if (m.idStr === "save")
            root.save_game();
        else if (m.idStr === "load")
            root.load_save();
        else if (m.idStr === "settings")
            root.open_settings();
        else if (m.idStr === "exit")
            root.exit_requested();
    }

    function move_selection(direction) {
        var next = commandList.currentIndex + direction;
        while (next >= 0 && next < menuModel.count) {
            var m = menuModel.get(next);
            if (!m.requiresGame || root.game_started) {
                commandList.currentIndex = next;
                return;
            }
            next += direction;
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Down) {
            move_selection(1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            move_selection(-1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            trigger_selection(commandList.currentIndex);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            if (typeof mainWindow !== "undefined" && mainWindow.menuVisible && mainWindow.gameStarted) {
                mainWindow.menuVisible = false;
                event.accepted = true;
            }
        }
    }

    ListModel {
        id: menuModel

        ListElement {
            idStr: "skirmish"
            title: QT_TR_NOOP("Play Skirmish")
            subtitle: QT_TR_NOOP("Choose the field and deploy armies")
            detail: QT_TR_NOOP("Battle")
            requiresGame: false
            accent: "#B6362F"
        }

        ListElement {
            idStr: "campaign"
            title: QT_TR_NOOP("Campaign")
            subtitle: QT_TR_NOOP("March through the Second Punic War")
            detail: QT_TR_NOOP("War Map")
            requiresGame: false
            accent: "#C29555"
        }

        ListElement {
            idStr: "objectives"
            title: QT_TR_NOOP("Objectives")
            subtitle: QT_TR_NOOP("Review active orders")
            detail: QT_TR_NOOP("Orders")
            requiresGame: true
            accent: "#8C6A3E"
        }

        ListElement {
            idStr: "save"
            title: QT_TR_NOOP("Save Game")
            subtitle: QT_TR_NOOP("Record the current campaign state")
            detail: QT_TR_NOOP("Archive")
            requiresGame: true
            accent: "#A7814A"
        }

        ListElement {
            idStr: "load"
            title: QT_TR_NOOP("Load Game")
            subtitle: QT_TR_NOOP("Return to a saved command")
            detail: QT_TR_NOOP("Return")
            requiresGame: false
            accent: "#8A7047"
        }

        ListElement {
            idStr: "settings"
            title: QT_TR_NOOP("Settings")
            subtitle: QT_TR_NOOP("Display, audio, and controls")
            detail: QT_TR_NOOP("Options")
            requiresGame: false
            accent: "#8C6A3E"
        }

        ListElement {
            idStr: "exit"
            title: QT_TR_NOOP("Exit")
            subtitle: QT_TR_NOOP("Leave the war table")
            detail: QT_TR_NOOP("Retire")
            requiresGame: false
            accent: "#6E2B25"
        }
    }

    Image {
        id: backdrop

        anchors.fill: parent
        source: "qrc:/StandardOfIron/assets/visuals/load_screen.png"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
        opacity: 0.74
    }

    Rectangle {
        anchors.fill: parent
        color: "#120D09"
        opacity: backdrop.status === Image.Ready ? 0.28 : 1
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: "#15100C"
            }

            GradientStop {
                position: root.compact ? 0.78 : 0.48
                color: "#20160FDD"
            }

            GradientStop {
                position: 1
                color: "#07050433"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#00000000"
            }

            GradientStop {
                position: 1
                color: "#070504AA"
            }
        }
        opacity: root.compact ? 0.25 : 0.55
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 3
        color: hs.bronze
        opacity: 0.85
    }

    RowLayout {
        id: stage

        anchors.fill: parent
        anchors.leftMargin: root.side_margin
        anchors.rightMargin: root.side_margin
        anchors.top_margin: root.top_margin
        anchors.bottomMargin: root.top_margin
        spacing: root.compact ? 0 : Math.max(24, root.width * 0.035)

        ColumnLayout {
            id: commandColumn

            Layout.fillHeight: true
            Layout.preferredWidth: root.compact ? stage.width : root.command_width
            Layout.maximumWidth: root.compact ? stage.width : root.command_width
            spacing: Math.max(12, Math.min(22, root.height * 0.022))

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.narrow ? 114 : 146

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7

                    Row {
                        width: parent.width
                        height: 34
                        spacing: 10

                        Image {
                            width: 32
                            height: 32
                            source: "qrc:/StandardOfIron/assets/visuals/emblems/rome.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("SPQR  /  QART-HADAST")
                            color: Theme.accentBright
                            font.pixelSize: root.narrow ? 12 : 13
                            font.bold: true
                        }

                        Image {
                            width: 32
                            height: 32
                            source: "qrc:/StandardOfIron/assets/visuals/emblems/cartaghe.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("STANDARD OF IRON")
                        color: Theme.textMain
                        font.family: "serif"
                        font.pixelSize: root.narrow ? 36 : 52
                        font.bold: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        style: Text.Outline
                        styleColor: "#120D09"
                    }

                    Rectangle {
                        width: Math.min(parent.width, 360)
                        height: 2
                        color: hs.bronze
                        opacity: 0.85
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Rome and Carthage at the edge of empire")
                        color: Theme.textSubLite
                        font.pixelSize: root.narrow ? 14 : 16
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }
            }

            ListView {
                id: commandList

                Layout.fillWidth: true
                Layout.fillHeight: true
                model: menuModel
                currentIndex: 0
                spacing: 10
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                delegate: Item {
                    id: commandItem

                    required property int index
                    required property string idStr
                    required property string title
                    required property string subtitle
                    required property string detail
                    required property bool requiresGame
                    required property string accent

                    readonly property bool itemEnabled: !requiresGame || root.game_started
                    readonly property bool selected: ListView.isCurrentItem
                    readonly property int rowHeight: root.narrow ? 60 : 70

                    width: commandList.width
                    height: rowHeight
                    opacity: itemEnabled ? 1 : 0.46

                    Rectangle {
                        anchors.fill: parent
                        radius: 7
                        color: "transparent"
                        border.width: selected ? 2 : 1
                        border.color: selected ? hs.bronze : menuMouse.containsMouse ? Theme.thumbBr : Qt.rgba(0.67, 0.51, 0.29, 0.62)

                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                position: 0
                                color: selected ? "#8F2F2A" : (menuMouse.containsMouse ? "#3B2F24" : "#17110CEE")
                            }

                            GradientStop {
                                position: 0.7
                                color: selected ? "#4B2119" : (menuMouse.containsMouse ? "#241B13EE" : "#120D09DD")
                            }

                            GradientStop {
                                position: 1
                                color: selected ? "#1D1610" : "#090604DD"
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: selected ? 7 : 4
                            radius: 2
                            color: accent
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 1
                            color: Theme.accentBright
                            opacity: selected ? 0.6 : 0.16
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: "#000000"
                            opacity: 0.42
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 14
                            anchors.top_margin: 9
                            anchors.bottomMargin: 9
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 3

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr(title)
                                    color: selected ? Theme.textMain : Theme.textBright
                                    font.family: "serif"
                                    font.pixelSize: root.narrow ? 19 : 22
                                    font.bold: true
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr(subtitle)
                                    color: selected ? Theme.accentBright : Theme.textSubLite
                                    font.pixelSize: root.narrow ? 12 : 14
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            Text {
                                Layout.preferredWidth: root.narrow ? 64 : 92
                                text: qsTr(detail)
                                color: selected ? Theme.textMain : Theme.textDim
                                font.pixelSize: 10
                                font.bold: selected
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                visible: commandItem.width > 390
                            }

                            Item {
                                Layout.preferredWidth: 16

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: selected ? 14 : 9
                                    height: selected ? 14 : 9
                                    rotation: 45
                                    color: selected ? Theme.accentBright : Theme.textHint
                                    opacity: selected ? 0.95 : 0.45
                                }
                            }
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }
                        }

                        Behavior on border.color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }
                        }
                    }

                    MouseArea {
                        id: menuMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: itemEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                        onEntered: {
                            if (itemEnabled)
                                commandList.currentIndex = commandItem.index;
                        }
                        onClicked: {
                            if (itemEnabled)
                                root.trigger_selection(commandItem.index);
                        }
                    }
                }
            }
        }

        Item {
            id: visualColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.compact

            Item {
                anchors.right: parent.right
                anchors.top: parent.top
                width: Math.min(parent.width, 500)
                height: Math.min(parent.height, 620)

                Image {
                    id: commanderPortrait

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    source: "qrc:/StandardOfIron/assets/visuals/hannibal.png"
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    opacity: 0.92
                    clip: true
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: "#120D09EE"
                        }

                        GradientStop {
                            position: 0.45
                            color: "#120D0944"
                        }

                        GradientStop {
                            position: 1
                            color: "#120D0900"
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 124
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#120D0900"
                        }

                        GradientStop {
                            position: 1
                            color: "#120D09EE"
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    color: hs.bronze
                    opacity: 0.7
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 22
                    spacing: 14

                    Image {
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        source: "qrc:/StandardOfIron/assets/visuals/emblems/rome.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        color: hs.bronze
                        opacity: 0.7
                    }

                    Image {
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        source: "qrc:/StandardOfIron/assets/visuals/emblems/cartaghe.png"
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("SECOND PUNIC WAR")
                            color: Theme.textMain
                            font.family: "serif"
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Legions, fleets, elephants, and contested supply lines")
                            color: Theme.textSubLite
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}

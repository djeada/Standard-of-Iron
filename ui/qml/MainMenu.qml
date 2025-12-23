import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15
import StandardOfIron 1.0

Item {
    id: root

    property bool gameStarted: false

    signal openSkirmish()
    signal openCampaign()
    signal openObjectives()
    signal openSettings()
    signal loadSave()
    signal saveGame()
    signal exitRequested()

    anchors.fill: parent
    z: 10
    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Down) {
            var newIndex = container.selectedIndex + 1;
            while (newIndex < menuModel.count) {
                var m = menuModel.get(newIndex);
                if (!m.requiresGame || root.gameStarted) {
                    container.selectedIndex = newIndex;
                    break;
                }
                newIndex++;
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            var newIndex = container.selectedIndex - 1;
            while (newIndex >= 0) {
                var m = menuModel.get(newIndex);
                if (!m.requiresGame || root.gameStarted) {
                    container.selectedIndex = newIndex;
                    break;
                }
                newIndex--;
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            var m = menuModel.get(container.selectedIndex);
            if (m.requiresGame && !root.gameStarted) {
                event.accepted = true;
                return ;
            }
            if (m.idStr === "skirmish")
                root.openSkirmish();
            else if (m.idStr === "campaign")
                root.openCampaign();
            else if (m.idStr === "objectives")
                root.openObjectives();
            else if (m.idStr === "save")
                root.saveGame();
            else if (m.idStr === "load")
                root.loadSave();
            else if (m.idStr === "settings")
                root.openSettings();
            else if (m.idStr === "exit")
                root.exitRequested();
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            if (typeof mainWindow !== 'undefined' && mainWindow.menuVisible && mainWindow.gameStarted) {
                mainWindow.menuVisible = false;
                event.accepted = true;
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        property int selectedIndex: 0

        width: Math.min(parent.width * 0.78, 1100)
        height: Math.min(parent.height * 0.78, 700)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98
        clip: true

        GridLayout {
            id: grid

            anchors.fill: parent
            anchors.margins: Theme.spacingXLarge
            rowSpacing: Theme.spacingMedium
            columnSpacing: 18
            columns: parent.width > 900 ? 2 : 1

            ColumnLayout {
                Layout.preferredWidth: parent.width > 900 ? parent.width * 0.45 : parent.width
                spacing: Theme.spacingLarge

                ColumnLayout {
                    spacing: Theme.spacingSmall

                    Label {
                        text: qsTr("STANDARD OF IRON")
                        color: Theme.textMain
                        font.pointSize: Theme.fontSizeHero
                        font.bold: true
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }

                    Label {
                        text: qsTr("A tiny but ambitious RTS")
                        color: Theme.textSub
                        font.pointSize: Theme.fontSizeMedium
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }

                }

                ListModel {
                    id: menuModel

                    ListElement {
                        idStr: "skirmish"
                        title: QT_TR_NOOP("Play — Skirmish")
                        subtitle: QT_TR_NOOP("Select a map and start")
                        requiresGame: false
                    }

                    ListElement {
                        idStr: "campaign"
                        title: QT_TR_NOOP("Play — Campaign")
                        subtitle: QT_TR_NOOP("Story missions and battles")
                        requiresGame: false
                    }

                    ListElement {
                        idStr: "objectives"
                        title: QT_TR_NOOP("Objectives")
                        subtitle: QT_TR_NOOP("View current mission objectives")
                        requiresGame: true
                    }

                    ListElement {
                        idStr: "save"
                        title: QT_TR_NOOP("Save Game")
                        subtitle: QT_TR_NOOP("Save your current progress")
                        requiresGame: true
                    }

                    ListElement {
                        idStr: "load"
                        title: QT_TR_NOOP("Load Game")
                        subtitle: QT_TR_NOOP("Resume a previous game")
                        requiresGame: false
                    }

                    ListElement {
                        idStr: "settings"
                        title: QT_TR_NOOP("Settings")
                        subtitle: QT_TR_NOOP("Adjust graphics & controls")
                        requiresGame: false
                    }

                    ListElement {
                        idStr: "exit"
                        title: QT_TR_NOOP("Exit")
                        subtitle: QT_TR_NOOP("Quit the game")
                        requiresGame: false
                    }

                }

                Repeater {
                    model: menuModel

                    delegate: Item {
                        id: menuItem

                        property int idx: index
                        property bool itemEnabled: !model.requiresGame || root.gameStarted

                        Layout.fillWidth: true
                        Layout.preferredHeight: container.width > 900 ? 64 : 56

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusLarge
                            clip: true
                            color: container.selectedIndex === idx ? Theme.selectedBg : menuItemMouse.containsPress ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
                            border.width: 1
                            border.color: container.selectedIndex === idx ? Theme.selectedBr : Theme.cardBorder
                            opacity: itemEnabled ? 1 : 0.4

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall
                                spacing: Theme.spacingMedium

                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny

                                    Text {
                                        text: qsTr(model.title)
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: itemEnabled ? (container.selectedIndex === idx ? Theme.textMain : Theme.textBright) : Theme.textDim
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: container.selectedIndex === idx
                                    }

                                    Text {
                                        text: qsTr(model.subtitle)
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: itemEnabled ? (container.selectedIndex === idx ? Theme.accentBright : Theme.textSubLite) : Theme.textHint
                                        font.pointSize: Theme.fontSizeSmall
                                    }

                                }

                                Text {
                                    text: "›"
                                    font.pointSize: Theme.fontSizeTitle
                                    color: itemEnabled ? (container.selectedIndex === idx ? Theme.textMain : Theme.textHint) : Theme.textHint
                                    opacity: itemEnabled ? 1 : 0.3
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
                            id: menuItemMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            cursorShape: itemEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                            onEntered: container.selectedIndex = idx
                            onClicked: {
                                if (!itemEnabled)
                                    return ;

                                if (model.idStr === "skirmish")
                                    root.openSkirmish();
                                else if (model.idStr === "campaign")
                                    root.openCampaign();
                                else if (model.idStr === "objectives")
                                    root.openObjectives();
                                else if (model.idStr === "save")
                                    root.saveGame();
                                else if (model.idStr === "load")
                                    root.loadSave();
                                else if (model.idStr === "settings")
                                    root.openSettings();
                                else if (model.idStr === "exit")
                                    root.exitRequested();
                            }
                        }

                    }

                }

                Item {
                    Layout.fillHeight: true
                }

                RowLayout {
                    spacing: Theme.spacingSmall

                    Label {
                        text: qsTr("v0.9 — prototype")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeSmall
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: Qt.formatDateTime(new Date(), "yyyy-MM-dd")
                        color: Theme.textHint
                        font.pointSize: Theme.fontSizeSmall
                        elide: Label.ElideRight
                    }

                }

            }

            Rectangle {
                color: Qt.rgba(0, 0, 0, 0)
                radius: Theme.radiusMedium
                Layout.preferredWidth: parent.width > 900 ? parent.width * 0.45 : parent.width

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    spacing: Theme.spacingMedium

                    Rectangle {
                        id: promo

                        color: Theme.cardBase
                        radius: Theme.radiusLarge
                        border.color: Theme.border
                        border.width: 1
                        Layout.preferredHeight: 260
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingSmall

                            Label {
                                text: qsTr("Featured")
                                color: Theme.accent
                                font.pointSize: Theme.fontSizeMedium
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }

                            Label {
                                text: qsTr("Skirmish Mode")
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeTitle
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }

                            Text {
                                text: qsTr("Pick a map, adjust your forces and jump into battle. Modern controls and responsive UI.")
                                color: Theme.textSubLite
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                        }

                    }

                    Rectangle {
                        color: Theme.cardBase
                        radius: Theme.radiusLarge
                        border.color: Theme.border
                        border.width: 1
                        Layout.preferredHeight: 120
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSmall
                            spacing: Theme.spacingSmall

                            Label {
                                text: qsTr("Tips")
                                color: Theme.accent
                                font.pointSize: Theme.fontSizeMedium
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }

                            Text {
                                text: qsTr("Hover menu items or use Up/Down and Enter to navigate. Play opens map selection.")
                                color: Theme.textSubLite
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                        }

                    }

                }

            }

        }

    }

}

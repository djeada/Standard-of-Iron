// MainMenu.qml (full, fixed to prevent text overflow and ensure clickability)
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15

Item {
    id: root
    anchors.fill: parent
    z: 10
    focus: true

    signal openSkirmish()
    signal openSettings()
    signal loadSave()
    signal exitRequested()

    // Dim background
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    // Main panel
    Rectangle {
        id: container
        width: Math.min(parent.width * 0.78, 1100)
        height: Math.min(parent.height * 0.78, 700)
        anchors.centerIn: parent
        radius: 14
        color: "#071018"
        border.color: "#0f2430"
        border.width: 1
        opacity: 0.98
        clip: true                         // prevent any children from drawing outside

        property int selectedIndex: 0

        GridLayout {
            id: grid
            anchors.fill: parent
            anchors.margins: 20
            rowSpacing: 12
            columnSpacing: 18
            columns: parent.width > 900 ? 2 : 1

            // LEFT COLUMN (menu)
            ColumnLayout {
                Layout.preferredWidth: parent.width > 900 ? parent.width * 0.45 : parent.width
                spacing: 16

                // Title
                ColumnLayout {
                    spacing: 6
                    Label {
                        text: "STANDARD OF IRON"
                        color: "#eaf6ff"
                        font.pointSize: 28
                        font.bold: true
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }
                    Label {
                        text: "A tiny but ambitious RTS"
                        color: "#86a7b6"
                        font.pointSize: 12
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }
                }

                // Menu model
                ListModel {
                    id: menuModel
                    ListElement { idStr: "skirmish"; title: "Play — Skirmish"; subtitle: "Select a map and start" }
                    ListElement { idStr: "load";     title: "Load Save";       subtitle: "Resume a previous game" }
                    ListElement { idStr: "settings"; title: "Settings";        subtitle: "Adjust graphics & controls" }
                    ListElement { idStr: "exit";     title: "Exit";            subtitle: "Quit the game" }
                }

                // Menu list
                Repeater {
                    model: menuModel
                    delegate: Item {
                        id: menuItem
                        property int idx: index
                        Layout.fillWidth: true
                        Layout.preferredHeight: container.width > 900 ? 64 : 56

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            clip: true
                            color: container.selectedIndex === idx ? "#1f8bf5"
                                   : menuItemMouse.containsPress ? "#184c7a" : "transparent"
                            border.width: 1
                            border.color: container.selectedIndex === idx ? "#1b74d1" : "#12323a"
                            Behavior on color { ColorAnimation { duration: 160 } }
                            Behavior on border.color { ColorAnimation { duration: 160 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12

                                // left spacer
                                Item { Layout.fillWidth: true; Layout.preferredWidth: 1 }

                                // titles
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: model.title
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: container.selectedIndex === idx ? "white" : "#dff0ff"
                                        font.pointSize: 14
                                        font.bold: container.selectedIndex === idx
                                    }
                                    Text {
                                        text: model.subtitle
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: container.selectedIndex === idx ? "#d0e8ff" : "#79a6b7"
                                        font.pointSize: 11
                                    }
                                }

                                // chevron
                                Text {
                                    text: "›"
                                    font.pointSize: 20
                                    color: container.selectedIndex === idx ? "white" : "#2a5e6e"
                                }
                            }
                        }

                        // click area
                        MouseArea {
                            id: menuItemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onEntered: container.selectedIndex = idx
                            onClicked: {
                                if (model.idStr === "skirmish")      root.openSkirmish();
                                else if (model.idStr === "load")     root.loadSave();
                                else if (model.idStr === "settings") root.openSettings();
                                else if (model.idStr === "exit")     root.exitRequested();
                            }
                        }
                    }
                }

                // flexible spacer
                Item { Layout.fillHeight: true }

                // footer
                RowLayout {
                    spacing: 8
                    Label { text: "v0.9 — prototype"; color: "#4f6a75"; font.pointSize: 11 }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: Qt.formatDateTime(new Date(), "yyyy-MM-dd")
                        color: "#2f5260"
                        font.pointSize: 10
                        elide: Label.ElideRight
                    }
                }
            }

            // RIGHT COLUMN (cards)
            Rectangle {
                color: "transparent"
                radius: 6
                Layout.preferredWidth: parent.width > 900 ? parent.width * 0.45 : parent.width

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    // featured card
                    Rectangle {
                        id: promo
                        color: "#061214"
                        radius: 8
                        border.color: "#0f2b34"
                        border.width: 1
                        Layout.preferredHeight: 260
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            Label {
                                text: "Featured"
                                color: "#9fd9ff"
                                font.pointSize: 12
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Label {
                                text: "Skirmish Mode"
                                color: "#eaf6ff"
                                font.pointSize: 20
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Text {
                                text: "Pick a map, adjust your forces and jump into battle. Modern controls and responsive UI."
                                color: "#7daebc"
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3           // keep height tidy
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // tips card
                    Rectangle {
                        color: "#061418"
                        radius: 8
                        border.color: "#0f2b34"
                        border.width: 1
                        Layout.preferredHeight: 120
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Label {
                                text: "Tips"
                                color: "#9fd9ff"
                                font.pointSize: 12
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Text {
                                text: "Hover menu items or use Up/Down and Enter to navigate. Play opens map selection."
                                color: "#79a6b7"
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

        // keyboard navigation
        Keys.onPressed: {
            if (event.key === Qt.Key_Down) {
                container.selectedIndex = Math.min(container.selectedIndex + 1, menuModel.count - 1)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                container.selectedIndex = Math.max(container.selectedIndex - 1, 0)
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                var m = menuModel.get(container.selectedIndex)
                if (m.idStr === "skirmish")      root.openSkirmish()
                else if (m.idStr === "load")     root.loadSave()
                else if (m.idStr === "settings") root.openSettings()
                else if (m.idStr === "exit")     root.exitRequested()
                event.accepted = true
            }
        }
    }
}


import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Window 2.15
import StandardOfIron.UI 1.0

Item {
    id: root
    anchors.fill: parent
    z: 10
    focus: true

    signal openSkirmish()
    signal openSettings()
    signal loadSave()
    signal exitRequested()

    
    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    
    Rectangle {
        id: container
        width: Math.min(parent.width * 0.78, 1100)
        height: Math.min(parent.height * 0.78, 700)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98
        clip: true                         

        property int selectedIndex: 0

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
                        text: "STANDARD OF IRON"
                        color: Theme.textMain
                        font.pointSize: Theme.fontSizeHero
                        font.bold: true
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }
                    Label {
                        text: "A tiny but ambitious RTS"
                        color: Theme.textSub
                        font.pointSize: Theme.fontSizeMedium
                        horizontalAlignment: Text.AlignLeft
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                    }
                }

                
                ListModel {
                    id: menuModel
                    ListElement { idStr: "skirmish"; title: "Play — Skirmish"; subtitle: "Select a map and start" }
                    ListElement { idStr: "load";     title: "Load Save";       subtitle: "Resume a previous game" }
                    ListElement { idStr: "settings"; title: "Settings";        subtitle: "Adjust graphics & controls" }
                    ListElement { idStr: "exit";     title: "Exit";            subtitle: "Quit the game" }
                }

                
                Repeater {
                    model: menuModel
                    delegate: Item {
                        id: menuItem
                        property int idx: index
                        Layout.fillWidth: true
                        Layout.preferredHeight: container.width > 900 ? 64 : 56

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusLarge
                            clip: true
                            color: container.selectedIndex === idx ? Theme.selectedBg
                                : menuItemMouse.containsPress ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
                            border.width: 1
                            border.color: container.selectedIndex === idx ? Theme.selectedBr : Theme.cardBorder
                            Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animNormal } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall
                                spacing: Theme.spacingMedium

                                
                                Item { Layout.fillWidth: true; Layout.preferredWidth: 1 }

                                
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny
                                    Text {
                                        text: model.title
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: container.selectedIndex === idx ? Theme.textMain : Theme.textBright
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: container.selectedIndex === idx
                                    }
                                    Text {
                                        text: model.subtitle
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: container.selectedIndex === idx ? Theme.accentBright : Theme.textSubLite
                                        font.pointSize: Theme.fontSizeSmall
                                    }
                                }

                                
                                Text {
                                    text: "›"
                                    font.pointSize: Theme.fontSizeTitle
                                    color: container.selectedIndex === idx ? Theme.textMain : Theme.textHint
                                }
                            }
                        }

                        
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

                
                Item { Layout.fillHeight: true }

                
                RowLayout {
                    spacing: Theme.spacingSmall
                    Label { text: "v0.9 — prototype"; color: Theme.textDim; font.pointSize: Theme.fontSizeSmall }
                    Item { Layout.fillWidth: true }
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
                                text: "Featured"
                                color: Theme.accent
                                font.pointSize: Theme.fontSizeMedium
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Label {
                                text: "Skirmish Mode"
                                color: Theme.textMain
                                font.pointSize: Theme.fontSizeTitle
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Text {
                                text: "Pick a map, adjust your forces and jump into battle. Modern controls and responsive UI."
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
                                text: "Tips"
                                color: Theme.accent
                                font.pointSize: Theme.fontSizeMedium
                                Layout.fillWidth: true
                                elide: Label.ElideRight
                            }
                            Text {
                                text: "Hover menu items or use Up/Down and Enter to navigate. Play opens map selection."
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

    
    Keys.onPressed: function(event) {
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
        } else if (event.key === Qt.Key_Escape) {
            
            
            if (typeof mainWindow !== 'undefined' && mainWindow.menuVisible && mainWindow.gameStarted) {
                mainWindow.menuVisible = false
                event.accepted = true
            }
        }
    }
}

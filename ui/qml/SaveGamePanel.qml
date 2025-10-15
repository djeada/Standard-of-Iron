import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron.UI 1.0

Item {
    id: root

    signal cancelled()
    signal saveRequested(string slotName)

    anchors.fill: parent
    z: 25

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.7, 900)
        height: Math.min(parent.height * 0.8, 600)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingXLarge
            spacing: Theme.spacingLarge

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: "Save Game"
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                Button {
                    text: "Cancel"
                    onClicked: root.cancelled()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            // Save slot name input
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: "Save Name:"
                    color: Theme.textSub
                    font.pointSize: Theme.fontSizeMedium
                }

                TextField {
                    id: saveNameField
                    
                    Layout.fillWidth: true
                    placeholderText: "Enter save name..."
                    text: "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm")
                    font.pointSize: Theme.fontSizeMedium
                    
                    background: Rectangle {
                        color: Theme.cardBase
                        border.color: saveNameField.activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                        radius: Theme.radiusMedium
                    }
                    
                    color: Theme.textMain
                }

                Button {
                    text: "Save"
                    enabled: saveNameField.text.length > 0
                    highlighted: true
                    onClicked: {
                        if (saveListModel.slotExists(saveNameField.text)) {
                            confirmOverwriteDialog.slotName = saveNameField.text
                            confirmOverwriteDialog.open()
                        } else {
                            root.saveRequested(saveNameField.text)
                        }
                    }
                }
            }

            // Existing saves list
            Label {
                text: "Existing Saves"
                color: Theme.textSub
                font.pointSize: Theme.fontSizeMedium
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.cardBase
                border.color: Theme.border
                border.width: 1
                radius: Theme.radiusLarge

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    clip: true

                    ListView {
                        id: saveListView

                        model: ListModel {
                            id: saveListModel

                            function slotExists(name) {
                                for (var i = 0; i < count; i++) {
                                    if (get(i).slotName === name) {
                                        return true
                                    }
                                }
                                return false
                            }

                            Component.onCompleted: {
                                // Populate with existing saves
                                if (typeof game !== 'undefined' && game.getSaveSlots) {
                                    var slots = game.getSaveSlots()
                                    for (var i = 0; i < slots.length; i++) {
                                        append({
                                            slotName: slots[i].name,
                                            timestamp: slots[i].timestamp,
                                            mapName: slots[i].mapName || "Unknown Map"
                                        })
                                    }
                                }
                            }
                        }

                        spacing: Theme.spacingSmall
                        delegate: Rectangle {
                            width: saveListView.width
                            height: 80
                            color: mouseArea.containsMouse ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
                            radius: Theme.radiusMedium
                            border.color: Theme.cardBorder
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny

                                    Label {
                                        text: model.slotName
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: model.mapName
                                        color: Theme.textSub
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: "Last saved: " + Qt.formatDateTime(new Date(model.timestamp), "yyyy-MM-dd hh:mm:ss")
                                        color: Theme.textHint
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }
                                }

                                Button {
                                    text: "Overwrite"
                                    onClicked: {
                                        confirmOverwriteDialog.slotName = model.slotName
                                        confirmOverwriteDialog.open()
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    saveNameField.text = model.slotName
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmOverwriteDialog

        property string slotName: ""

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 400)
        title: "Confirm Overwrite"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            root.saveRequested(slotName)
        }

        contentItem: Rectangle {
            color: Theme.cardBase
            implicitHeight: warningText.implicitHeight + 40

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                Label {
                    id: warningText
                    text: "Are you sure you want to overwrite the save:\n\"" + confirmOverwriteDialog.slotName + "\"?"
                    color: Theme.textMain
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pointSize: Theme.fontSizeMedium
                }
            }
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled()
            event.accepted = true
        }
    }

    Component.onCompleted: {
        forceActiveFocus()
    }
}

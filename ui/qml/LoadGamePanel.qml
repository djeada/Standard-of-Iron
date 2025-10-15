import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron.UI 1.0

Item {
    id: root

    signal cancelled()
    signal loadRequested(string slotName)

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
                    text: "Load Game"
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

            // Save slots list
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
                        id: loadListView

                        property int selectedIndex: -1

                        model: ListModel {
                            id: loadListModel

                            Component.onCompleted: {
                                // Populate with existing saves
                                if (typeof game !== 'undefined' && game.getSaveSlots) {
                                    var slots = game.getSaveSlots()
                                    for (var i = 0; i < slots.length; i++) {
                                        append({
                                            slotName: slots[i].name,
                                            timestamp: slots[i].timestamp,
                                            mapName: slots[i].mapName || "Unknown Map",
                                            playTime: slots[i].playTime || "Unknown"
                                        })
                                    }
                                }
                                
                                if (count === 0) {
                                    append({
                                        slotName: "No saves found",
                                        timestamp: 0,
                                        mapName: "",
                                        playTime: "",
                                        isEmpty: true
                                    })
                                }
                            }
                        }

                        spacing: Theme.spacingSmall
                        delegate: Rectangle {
                            width: loadListView.width
                            height: model.isEmpty ? 100 : 120
                            color: loadListView.selectedIndex === index ? Theme.selectedBg : 
                                   mouseArea.containsMouse ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
                            radius: Theme.radiusMedium
                            border.color: loadListView.selectedIndex === index ? Theme.selectedBr : Theme.cardBorder
                            border.width: 1
                            visible: !model.isEmpty || loadListModel.count === 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny
                                    visible: !model.isEmpty

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
                                        font.pointSize: Theme.fontSizeMedium
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingLarge

                                        Label {
                                            text: "Last saved: " + Qt.formatDateTime(new Date(model.timestamp), "yyyy-MM-dd hh:mm:ss")
                                            color: Theme.textHint
                                            font.pointSize: Theme.fontSizeSmall
                                            Layout.fillWidth: true
                                            elide: Label.ElideRight
                                        }

                                        Label {
                                            text: model.playTime !== "" ? "Play time: " + model.playTime : ""
                                            color: Theme.textHint
                                            font.pointSize: Theme.fontSizeSmall
                                            visible: model.playTime !== ""
                                        }
                                    }
                                }

                                Label {
                                    text: model.slotName
                                    color: Theme.textDim
                                    font.pointSize: Theme.fontSizeLarge
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: model.isEmpty
                                }

                                Button {
                                    text: "Load"
                                    highlighted: true
                                    visible: !model.isEmpty
                                    onClicked: {
                                        root.loadRequested(model.slotName)
                                    }
                                }

                                Button {
                                    text: "Delete"
                                    visible: !model.isEmpty
                                    onClicked: {
                                        confirmDeleteDialog.slotName = model.slotName
                                        confirmDeleteDialog.slotIndex = index
                                        confirmDeleteDialog.open()
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: !model.isEmpty
                                onClicked: {
                                    loadListView.selectedIndex = index
                                }
                                onDoubleClicked: {
                                    if (!model.isEmpty) {
                                        root.loadRequested(model.slotName)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Load button at bottom
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: loadListView.selectedIndex >= 0 && !loadListModel.get(loadListView.selectedIndex).isEmpty ? 
                          "Selected: " + loadListModel.get(loadListView.selectedIndex).slotName : 
                          "Select a save to load"
                    color: Theme.textSub
                    font.pointSize: Theme.fontSizeMedium
                }

                Button {
                    text: "Load Selected"
                    enabled: loadListView.selectedIndex >= 0 && !loadListModel.get(loadListView.selectedIndex).isEmpty
                    highlighted: true
                    onClicked: {
                        if (loadListView.selectedIndex >= 0) {
                            root.loadRequested(loadListModel.get(loadListView.selectedIndex).slotName)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmDeleteDialog

        property string slotName: ""
        property int slotIndex: -1

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 400)
        title: "Confirm Delete"
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            if (typeof game !== 'undefined' && game.deleteSaveSlot) {
                game.deleteSaveSlot(slotName)
                loadListModel.remove(slotIndex)
                
                // Add empty message if no saves left
                if (loadListModel.count === 0) {
                    loadListModel.append({
                        slotName: "No saves found",
                        timestamp: 0,
                        mapName: "",
                        playTime: "",
                        isEmpty: true
                    })
                }
            }
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
                    text: "Are you sure you want to delete the save:\n\"" + confirmDeleteDialog.slotName + "\"?\n\nThis action cannot be undone."
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
        } else if (event.key === Qt.Key_Down) {
            if (loadListView.selectedIndex < loadListModel.count - 1) {
                loadListView.selectedIndex++
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (loadListView.selectedIndex > 0) {
                loadListView.selectedIndex--
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (loadListView.selectedIndex >= 0 && !loadListModel.get(loadListView.selectedIndex).isEmpty) {
                root.loadRequested(loadListModel.get(loadListView.selectedIndex).slotName)
            }
            event.accepted = true
        }
    }

    Component.onCompleted: {
        forceActiveFocus()
        if (loadListModel.count > 0 && !loadListModel.get(0).isEmpty) {
            loadListView.selectedIndex = 0
        }
    }
}

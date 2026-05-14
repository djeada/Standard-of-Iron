import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0

Item {
    id: root

    signal cancelled
    signal load_requested(string slot_name)

    anchors.fill: parent
    z: 25
    onVisibleChanged: {
        if (!visible)
            return;
        if (typeof loadListModel !== 'undefined')
            loadListModel.load_from_game();
        if (typeof loadListView !== 'undefined')
            loadListView.selected_index = loadListModel.count > 0 && !loadListModel.get(0).isEmpty ? 0 : -1;
    }
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            if (loadListView.selected_index < loadListModel.count - 1)
                loadListView.selected_index++;
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (loadListView.selected_index > 0)
                loadListView.selected_index--;
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (loadListView.selected_index >= 0 && !loadListModel.get(loadListView.selected_index).isEmpty)
                root.load_requested(loadListModel.get(loadListView.selected_index).slot_name);
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
        if (loadListModel.count > 0 && !loadListModel.get(0).isEmpty)
            loadListView.selected_index = 0;
    }

    Connections {
        function onSave_slots_changed() {
            if (typeof loadListModel === 'undefined')
                return;
            var previousSlot = "";
            if (typeof loadListView !== 'undefined' && loadListView.selected_index >= 0 && loadListView.selected_index < loadListModel.count) {
                var current = loadListModel.get(loadListView.selected_index);
                if (current && !current.isEmpty)
                    previousSlot = current.slot_name;
            }
            loadListModel.load_from_game();
            if (typeof loadListView === 'undefined')
                return;
            var newIndex = -1;
            if (previousSlot !== "") {
                for (var i = 0; i < loadListModel.count; ++i) {
                    var slot = loadListModel.get(i);
                    if (!slot.isEmpty && slot.slot_name === previousSlot) {
                        newIndex = i;
                        break;
                    }
                }
            }
            if (newIndex === -1) {
                if (loadListModel.count > 0 && !loadListModel.get(0).isEmpty)
                    newIndex = 0;
            }
            loadListView.selected_index = newIndex;
        }

        target: typeof game !== 'undefined' ? game : null
    }

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

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Load Game")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: qsTr("Cancel")
                    button_style: "secondary"
                    onClicked: root.cancelled()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
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
                        id: loadListView

                        property int selected_index: -1

                        spacing: Theme.spacingSmall

                        model: ListModel {
                            id: loadListModel

                            function load_from_game() {
                                clear();
                                if (typeof game === 'undefined' || !game.get_save_slots) {
                                    append({
                                            "slot_name": qsTr("No saves found"),
                                            "title": "",
                                            "timestamp": 0,
                                            "map_name": "",
                                            "playTime": "",
                                            "thumbnail": "",
                                            "isEmpty": true
                                        });
                                    return;
                                }
                                var slots = game.get_save_slots();
                                for (var i = 0; i < slots.length; i++) {
                                    append({
                                            "slot_name": slots[i].slot_name || slots[i].name,
                                            "title": slots[i].title || slots[i].name || slots[i].slot_name || "Untitled Save",
                                            "timestamp": slots[i].timestamp,
                                            "map_name": slots[i].map_name || "Unknown Map",
                                            "playTime": slots[i].playTime || "",
                                            "thumbnail": slots[i].thumbnail || "",
                                            "isEmpty": false
                                        });
                                }
                                if (count === 0)
                                    append({
                                            "slot_name": qsTr("No saves found"),
                                            "title": "",
                                            "timestamp": 0,
                                            "map_name": "",
                                            "playTime": "",
                                            "thumbnail": "",
                                            "isEmpty": true
                                        });
                            }

                            Component.onCompleted: {
                                load_from_game();
                            }
                        }

                        delegate: Rectangle {
                            width: loadListView.width
                            height: model.isEmpty ? 100 : 130
                            color: loadListView.selected_index === index ? Theme.selectedBg : mouseArea.containsMouse ? Theme.hoverBg : Qt.rgba(0, 0, 0, 0)
                            radius: Theme.radiusMedium
                            border.color: loadListView.selected_index === index ? Theme.selectedBr : Theme.cardBorder
                            border.width: 1
                            visible: !model.isEmpty || loadListModel.count === 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium

                                Rectangle {
                                    id: loadThumbnail

                                    Layout.preferredWidth: 128
                                    Layout.preferredHeight: 80
                                    radius: Theme.radiusSmall
                                    color: Theme.cardBase
                                    border.color: Theme.cardBorder
                                    border.width: 1
                                    clip: true
                                    visible: !model.isEmpty

                                    Image {
                                        id: loadThumbnailImage

                                        anchors.fill: parent
                                        anchors.margins: 2
                                        fillMode: Image.PreserveAspectCrop
                                        source: model.thumbnail && model.thumbnail.length > 0 ? "data:image/png;base64," + model.thumbnail : ""
                                        visible: source !== ""
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: !loadThumbnailImage.visible
                                        text: qsTr("No Preview")
                                        color: Theme.textHint
                                        font.pointSize: Theme.fontSizeTiny
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny
                                    visible: !model.isEmpty

                                    Label {
                                        text: model.title
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Slot: %1").arg(model.slot_name)
                                        color: Theme.textSub
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: model.map_name
                                        color: Theme.textSub
                                        font.pointSize: Theme.fontSizeMedium
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingLarge

                                        Label {
                                            text: qsTr("Last saved: %1").arg(Qt.formatDateTime(new Date(model.timestamp), "yyyy-MM-dd hh:mm:ss"))
                                            color: Theme.textHint
                                            font.pointSize: Theme.fontSizeSmall
                                            Layout.fillWidth: true
                                            elide: Label.ElideRight
                                        }

                                        Label {
                                            text: model.playTime !== "" ? qsTr("Play time: %1").arg(model.playTime) : ""
                                            color: Theme.textHint
                                            font.pointSize: Theme.fontSizeSmall
                                            visible: model.playTime !== ""
                                        }
                                    }
                                }

                                Label {
                                    text: model.slot_name
                                    color: Theme.textDim
                                    font.pointSize: Theme.fontSizeLarge
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: model.isEmpty
                                }

                                StyledButton {
                                    text: qsTr("Load")
                                    button_style: "small"
                                    visible: !model.isEmpty
                                    onClicked: {
                                        root.load_requested(model.slot_name);
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Delete")
                                    button_style: "danger"
                                    implicitWidth: 80
                                    visible: !model.isEmpty
                                    onClicked: {
                                        confirmDeleteDialog.slot_name = model.slot_name;
                                        confirmDeleteDialog.slot_index = index;
                                        confirmDeleteDialog.open();
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: !model.isEmpty
                                onClicked: {
                                    loadListView.selected_index = index;
                                }
                                onDoubleClicked: {
                                    if (!model.isEmpty)
                                        root.load_requested(model.slot_name);
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: loadListView.selected_index >= 0 && !loadListModel.get(loadListView.selected_index).isEmpty ? qsTr("Selected: %1").arg(loadListModel.get(loadListView.selected_index).title) : qsTr("Select a save to load")
                    color: Theme.textSub
                    font.pointSize: Theme.fontSizeMedium
                }

                StyledButton {
                    text: qsTr("Load Selected")
                    enabled: loadListView.selected_index >= 0 && !loadListModel.get(loadListView.selected_index).isEmpty
                    onClicked: {
                        if (loadListView.selected_index >= 0 && !loadListModel.get(loadListView.selected_index).isEmpty)
                            root.load_requested(loadListModel.get(loadListView.selected_index).slot_name);
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmDeleteDialog

        property string slot_name: ""
        property int slot_index: -1

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 400)
        title: qsTr("Confirm Delete")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (typeof game !== 'undefined' && game.delete_save_slot) {
                if (game.delete_save_slot(slot_name)) {
                    loadListModel.remove(slot_index);
                    if (loadListModel.count === 0)
                        loadListModel.append({
                                "slot_name": qsTr("No saves found"),
                                "title": "",
                                "timestamp": 0,
                                "map_name": "",
                                "playTime": "",
                                "thumbnail": "",
                                "isEmpty": true
                            });
                    if (loadListView.selected_index >= loadListModel.count)
                        loadListView.selected_index = loadListModel.count > 0 && !loadListModel.get(0).isEmpty ? loadListModel.count - 1 : -1;
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

                    text: qsTr("Are you sure you want to delete the save:\n\"%1\"?\n\nThis action cannot be undone.").arg(confirmDeleteDialog.slot_name)
                    color: Theme.textMain
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pointSize: Theme.fontSizeMedium
                }
            }
        }
    }
}

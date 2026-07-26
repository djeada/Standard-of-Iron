import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0

Item {
    id: root

    signal cancelled
    signal load_requested(string slot_name)

    property string status_message: ""

    function format_play_time(seconds) {
        if (!seconds || seconds <= 0)
            return "";
        var total = Math.floor(seconds);
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        return hours > 0 ? qsTr("%1h %2m").arg(hours).arg(minutes) : qsTr("%1m").arg(minutes);
    }

    function describe_mode(mode, kind) {
        var mode_text = mode === "campaign" ? qsTr("Campaign") : qsTr("Skirmish");
        if (kind === "autosave")
            return qsTr("%1 - autosave").arg(mode_text);
        if (kind === "quicksave")
            return qsTr("%1 - quicksave").arg(mode_text);
        return mode_text;
    }

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
                                            "mode": "",
                                            "kind": "",
                                            "playTime": "",
                                            "thumbnail": "",
                                            "isEmpty": true
                                        });
                                    return;
                                }
                                var entries = game.get_save_slots();
                                for (var i = 0; i < entries.length; i++) {
                                    append({
                                            "slot_name": entries[i].slot_name,
                                            "title": entries[i].title || entries[i].slot_name || "Untitled Save",
                                            "timestamp": entries[i].timestamp,
                                            "map_name": entries[i].map_name || "Unknown Map",
                                            "mode": entries[i].mode || "",
                                            "kind": entries[i].kind || "manual",
                                            "playTime": root.format_play_time(entries[i].play_time_seconds),
                                            "thumbnail": entries[i].thumbnail || "",
                                            "isEmpty": false
                                        });
                                }
                                if (count === 0)
                                    append({
                                            "slot_name": qsTr("No saves found"),
                                            "title": "",
                                            "timestamp": 0,
                                            "map_name": "",
                                            "mode": "",
                                            "kind": "",
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
                                        text: model.isEmpty ? "" : qsTr("%1 - %2").arg(model.map_name).arg(root.describe_mode(model.mode, model.kind))
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
                                    text: qsTr("Export")
                                    button_style: "small"
                                    visible: !model.isEmpty
                                    onClicked: {
                                        if (typeof game === 'undefined' || !game.export_save_slot)
                                            return;
                                        var path = game.export_save_slot(model.slot_name);
                                        root.status_message = path !== "" ? qsTr("Exported to %1").arg(path) : qsTr("Export failed");
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Verify")
                                    button_style: "small"
                                    visible: !model.isEmpty
                                    onClicked: {
                                        if (typeof game === 'undefined' || !game.verify_save_slot)
                                            return;
                                        root.status_message = game.verify_save_slot(model.slot_name) ? qsTr("\"%1\" is intact").arg(model.slot_name) : qsTr("\"%1\" is corrupted and cannot be loaded").arg(model.slot_name);
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

                StyledButton {
                    text: qsTr("Import...")
                    button_style: "secondary"
                    onClicked: importDialog.open()
                }

                Label {
                    text: root.status_message
                    color: Theme.textHint
                    font.pointSize: Theme.fontSizeSmall
                    Layout.fillWidth: true
                    elide: Label.ElideRight
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

    Dialog {
        id: importDialog

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 520)
        title: qsTr("Import Save")
        modal: true
        standardButtons: Dialog.Close
        onOpened: importModel.reload()

        contentItem: Rectangle {
            color: Theme.cardBase
            implicitHeight: 260

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingSmall

                Label {
                    text: qsTr("Save files found in the exports folder:")
                    color: Theme.textSub
                    font.pointSize: Theme.fontSizeSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.spacingTiny

                    model: ListModel {
                        id: importModel

                        function reload() {
                            clear();
                            if (typeof game === 'undefined' || !game.list_exported_saves)
                                return;
                            var files = game.list_exported_saves();
                            for (var i = 0; i < files.length; i++)
                                append({
                                        "path": files[i].path,
                                        "name": files[i].name
                                    });
                        }
                    }

                    delegate: RowLayout {
                        width: ListView.view ? ListView.view.width : 0
                        spacing: Theme.spacingSmall

                        Label {
                            text: model.name
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeSmall
                            Layout.fillWidth: true
                            elide: Label.ElideMiddle
                        }

                        StyledButton {
                            text: qsTr("Import")
                            button_style: "small"
                            onClicked: {
                                var slot = game.import_save_file(model.path);
                                root.status_message = slot !== "" ? qsTr("Imported as \"%1\"").arg(slot) : qsTr("Import failed");
                                if (slot !== "")
                                    importDialog.close();
                            }
                        }
                    }
                }

                Label {
                    text: importModel.count === 0 ? qsTr("No importable save files were found.") : ""
                    color: Theme.textHint
                    font.pointSize: Theme.fontSizeSmall
                    visible: importModel.count === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio
import StandardOfIron.Core 1.0

Item {
    id: root

    signal cancelled
    signal load_requested(string slot_name)

    readonly property var saves: (typeof game !== 'undefined' && game) ? game.saves : null
    readonly property bool storage_ready: root.saves ? root.saves.storage_healthy : false

    property string status_message: ""
    property string verify_result_title: ""
    property string verify_result_message: ""
    property bool verify_result_success: false

    readonly property var selected_slot: (loadListView.selected_index >= 0 && loadListView.selected_index < loadListModel.count) ? loadListModel.get(loadListView.selected_index) : null
    readonly property bool can_load: root.selected_slot !== null && root.selected_slot.loadable

    function set_status(text) {
        root.status_message = text;
        status_timer.restart();
    }

    function load_selected() {
        if (!root.can_load)
            return;
        root.load_requested(root.selected_slot.slot_name);
    }

    anchors.fill: parent
    z: 25
    onVisibleChanged: {
        if (!visible)
            return;
        root.status_message = "";
        loadListModel.load_from_game();
        loadListView.selected_index = loadListModel.first_loadable_index();
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
            root.load_selected();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
        loadListView.selected_index = loadListModel.first_loadable_index();
    }

    Timer {
        id: status_timer

        interval: 8000
        onTriggered: root.status_message = ""
    }

    Connections {
        function onSave_slots_changed() {
            var previous = root.selected_slot ? root.selected_slot.slot_name : "";
            loadListModel.load_from_game();
            var restored = -1;
            if (previous !== "") {
                for (var i = 0; i < loadListModel.count; ++i) {
                    if (loadListModel.get(i).slot_name === previous) {
                        restored = i;
                        break;
                    }
                }
            }
            loadListView.selected_index = restored >= 0 ? restored : loadListModel.first_loadable_index();
        }

        target: root.saves
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.7, 900)
        height: Math.min(parent.height * 0.8, 640)
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
                    text: qsTr("Load game")
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.hero
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
                visible: !root.storage_ready || (root.saves && root.saves.recovered_database_path !== "")
                radius: Theme.radiusMedium
                color: Qt.rgba(0.55, 0.16, 0.14, 0.18)
                border.color: Theme.dangerBr
                border.width: 1
                implicitHeight: storage_notice.implicitHeight + Theme.spacingMedium * 2

                Label {
                    id: storage_notice

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Theme.spacingMedium
                    text: !root.storage_ready ? qsTr("Saves are unavailable: %1").arg(root.saves ? root.saves.storage_error : "") : qsTr("The save database could not be read and a new one was started. Your previous file was kept at %1.").arg(root.saves ? root.saves.recovered_database_path : "")
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.body
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.cardBase
                border.color: Theme.border
                border.width: 1
                radius: Theme.radiusLarge

                Label {
                    anchors.centerIn: parent
                    width: parent.width - Theme.spacingXLarge * 2
                    visible: loadListModel.count === 0
                    text: qsTr("No saved games yet. Save a battle from the menu, or press the quicksave key while you play.")
                    color: Theme.textHint
                    font.pixelSize: Design.Typography.bodyLarge
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

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

                            function first_loadable_index() {
                                for (var i = 0; i < count; ++i) {
                                    if (get(i).loadable)
                                        return i;
                                }
                                return -1;
                            }

                            function load_from_game() {
                                clear();
                                if (!root.saves || !root.saves.get_save_slots)
                                    return;
                                var entries = root.saves.get_save_slots();
                                for (var i = 0; i < entries.length; i++) {
                                    var entry = entries[i];
                                    append({
                                            "slot_name": entry.slot_name,
                                            "title": entry.title || entry.slot_name || qsTr("Untitled save"),
                                            "timestamp": entry.timestamp || "",
                                            "map_name": entry.map_name || qsTr("Unknown map"),
                                            "mode": entry.mode || "",
                                            "mission_label": (entry.metadata && entry.metadata.mission_title) ? entry.metadata.mission_title : "",
                                            "kind": entry.kind || "manual",
                                            "play_time_seconds": entry.play_time_seconds || 0,
                                            "loadable": entry.loadable === undefined ? true : entry.loadable,
                                            "thumbnail": entry.thumbnail || ""
                                        });
                                }
                            }

                            Component.onCompleted: load_from_game()
                        }

                        delegate: SaveSlotRow {
                            id: load_row

                            width: loadListView.width
                            selected: loadListView.selected_index === index
                            slot_name: model.slot_name
                            title: model.title
                            map_name: model.map_name
                            mode: model.mode
                            mission_label: model.mission_label
                            kind: model.kind
                            timestamp: model.timestamp
                            play_time_seconds: model.play_time_seconds
                            thumbnail: model.thumbnail
                            loadable: model.loadable
                            blocked_reason: qsTr("Saved by a different version of the game. It is kept on disk, but this build cannot open it.")
                            onClicked: loadListView.selected_index = index
                            onDouble_clicked: {
                                if (model.loadable)
                                    root.load_requested(model.slot_name);
                                else
                                    Design.UiSound.warning();
                            }

                            actions: RowLayout {
                                spacing: Theme.spacingTiny

                                StyledButton {
                                    text: qsTr("Export")
                                    button_style: "secondary"
                                    implicitWidth: 84
                                    onClicked: {
                                        if (!root.saves || !root.saves.export_save_slot)
                                            return;
                                        var exported = root.saves.export_save_slot(load_row.slot_name);
                                        root.set_status(exported.ok ? qsTr("Exported to %1").arg(exported.path) : qsTr("Export failed: %1").arg(exported.reason));
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Verify")
                                    button_style: "secondary"
                                    implicitWidth: 84
                                    onClicked: {
                                        if (!root.saves || !root.saves.check_save_slot)
                                            return;
                                        var check = root.saves.check_save_slot(load_row.slot_name);
                                        root.verify_result_success = check.ok;
                                        root.verify_result_title = check.ok ? qsTr("This save will load") : qsTr("This save will not load");
                                        root.verify_result_message = check.ok ? qsTr("\"%1\" decompressed cleanly and contains a complete battlefield.").arg(load_row.slot_name) : check.reason;
                                        verifyResultDialog.open();
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Delete")
                                    button_style: "danger"
                                    implicitWidth: 84
                                    onClicked: {
                                        confirmDeleteDialog.slot_name = load_row.slot_name;
                                        confirmDeleteDialog.open();
                                    }
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
                    font.pixelSize: Design.Typography.body
                    Layout.fillWidth: true
                    elide: Label.ElideMiddle
                }

                StyledButton {
                    text: qsTr("Load")
                    button_style: "primary"
                    blocked: !root.can_load
                    disabledReason: root.selected_slot === null ? qsTr("Pick a saved game first.") : qsTr("This save was written by a different version of the game.")
                    onClicked: root.load_selected()
                }
            }
        }
    }

    Design.IronDialog {
        id: confirmDeleteDialog

        property string slot_name: ""

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 460)
        title: qsTr("Delete this save?")
        tone: "danger"
        message: qsTr("\"%1\" will be deleted. This cannot be undone.").arg(confirmDeleteDialog.slot_name)
        primaryAction: qsTr("Delete")
        secondaryAction: qsTr("Keep it")
        onPrimaryActivated: {
            UiAudio.play_confirm(typeof game !== 'undefined' ? game.audio_system : null);
            if (root.saves && root.saves.delete_save_slot && root.saves.delete_save_slot(confirmDeleteDialog.slot_name))
                root.set_status(qsTr("Deleted \"%1\"").arg(confirmDeleteDialog.slot_name));
        }
        onSecondaryActivated: UiAudio.play_back(typeof game !== 'undefined' ? game.audio_system : null)
    }

    Design.IronDialog {
        id: verifyResultDialog

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 520)
        title: root.verify_result_title
        tone: root.verify_result_success ? "info" : "danger"
        message: root.verify_result_message
        primaryAction: qsTr("Close")
    }

    Dialog {
        id: importDialog

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 520)
        title: qsTr("Import save")
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
                    font.pixelSize: Design.Typography.body
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
                            if (!root.saves || !root.saves.list_exported_saves)
                                return;
                            var files = root.saves.list_exported_saves();
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
                            font.pixelSize: Design.Typography.body
                            Layout.fillWidth: true
                            elide: Label.ElideMiddle
                        }

                        StyledButton {
                            text: qsTr("Import")
                            button_style: "small"
                            onClicked: {
                                var imported = root.saves.import_save_file(model.path);
                                root.set_status(imported.ok ? qsTr("Imported as \"%1\"").arg(imported.slot_name) : qsTr("Import failed: %1").arg(imported.reason));
                                if (imported.ok)
                                    importDialog.close();
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("No importable save files were found.")
                    color: Theme.textHint
                    font.pixelSize: Design.Typography.body
                    visible: importModel.count === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}

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
    signal save_requested(string slot_name)

    readonly property var saves: (typeof game !== 'undefined' && game) ? game.saves : null
    readonly property bool storage_ready: root.saves ? root.saves.storage_healthy : false

    property string name_problem: ""
    property string overwrite_target: ""

    function refresh_name_state() {
        var name = saveNameField.text.trim();
        root.name_problem = root.saves ? root.saves.slot_name_rejection(name) : "";
        root.overwrite_target = (root.name_problem === "" && saveListModel.slot_exists(name)) ? name : "";
    }

    function default_save_name() {
        return "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm");
    }

    function submit() {
        if (!root.storage_ready || root.name_problem !== "")
            return;
        var name = saveNameField.text.trim();
        if (root.overwrite_target !== "") {
            confirmOverwriteDialog.slot_name = name;
            confirmOverwriteDialog.open();
            return;
        }
        root.save_requested(name);
    }

    anchors.fill: parent
    z: 25
    onVisibleChanged: {
        if (!visible)
            return;
        saveListModel.load_from_game();
        saveNameField.text = root.default_save_name();
        root.refresh_name_state();
        saveNameField.forceActiveFocus();
    }
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
        refresh_name_state();
    }

    Connections {
        function onSave_slots_changed() {
            saveListModel.load_from_game();
            root.refresh_name_state();
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
                    text: qsTr("Save game")
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
                visible: !root.storage_ready
                radius: Theme.radiusMedium
                color: Qt.rgba(0.55, 0.16, 0.14, 0.18)
                border.color: Theme.dangerBr
                border.width: 1
                implicitHeight: storage_warning.implicitHeight + Theme.spacingMedium * 2

                Label {
                    id: storage_warning

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Theme.spacingMedium
                    text: qsTr("Saves are unavailable: %1").arg(root.saves ? root.saves.storage_error : "")
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.body
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingTiny
                enabled: root.storage_ready

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium

                    Label {
                        text: qsTr("Save name")
                        color: Theme.textSub
                        font.pixelSize: Design.Typography.bodyLarge
                    }

                    TextField {
                        id: saveNameField

                        Layout.fillWidth: true
                        placeholderText: qsTr("Name this save...")
                        font.pixelSize: Design.Typography.bodyLarge
                        color: Theme.textMain
                        selectByMouse: true
                        maximumLength: 64
                        onTextChanged: root.refresh_name_state()
                        onAccepted: root.submit()

                        background: Rectangle {
                            color: Theme.cardBase
                            border.color: root.name_problem !== "" ? Theme.dangerBr : saveNameField.activeFocus ? Theme.accent : Theme.border
                            border.width: 1
                            radius: Theme.radiusMedium
                        }
                    }

                    StyledButton {
                        text: root.overwrite_target !== "" ? qsTr("Overwrite") : qsTr("Save")
                        button_style: root.overwrite_target !== "" ? "danger" : "primary"
                        blocked: root.name_problem !== ""
                        disabledReason: root.name_problem
                        onClicked: root.submit()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.name_problem !== "" ? root.name_problem : root.overwrite_target !== "" ? qsTr("A save called \"%1\" already exists. Saving replaces it.").arg(root.overwrite_target) : qsTr("Press Enter to save.")
                    color: root.name_problem !== "" ? Theme.dangerBr : root.overwrite_target !== "" ? Theme.textSub : Theme.textHint
                    font.pixelSize: Design.Typography.body
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Existing saves")
                    color: Theme.textSub
                    font.pixelSize: Design.Typography.bodyLarge
                    Layout.fillWidth: true
                }

                Label {
                    text: saveListModel.count === 1 ? qsTr("%1 save").arg(saveListModel.count) : qsTr("%1 saves").arg(saveListModel.count)
                    color: Theme.textHint
                    font.pixelSize: Design.Typography.body
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
                    visible: saveListModel.count === 0
                    text: qsTr("No saves yet. The name above will create the first one.")
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
                        id: saveListView

                        spacing: Theme.spacingSmall

                        model: ListModel {
                            id: saveListModel

                            function slot_exists(name) {
                                var wanted = (name || "").trim();
                                for (var i = 0; i < count; i++) {
                                    if (get(i).slot_name === wanted)
                                        return true;
                                }
                                return false;
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
                            id: save_row

                            width: saveListView.width
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
                            blocked_reason: model.loadable ? "" : qsTr("Saved by a different version of the game. It is kept, but this build cannot open it.")
                            onClicked: {
                                if (model.kind === "manual")
                                    saveNameField.text = model.slot_name;
                            }

                            actions: RowLayout {
                                spacing: Theme.spacingTiny

                                Item {
                                    visible: save_row.kind !== "manual"
                                    implicitWidth: 96
                                    implicitHeight: 1
                                }

                                StyledButton {
                                    visible: save_row.kind === "manual"
                                    text: qsTr("Overwrite")
                                    button_style: "danger"
                                    implicitWidth: 96
                                    onClicked: {
                                        confirmOverwriteDialog.slot_name = save_row.slot_name;
                                        confirmOverwriteDialog.open();
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Delete")
                                    button_style: "secondary"
                                    implicitWidth: 84
                                    onClicked: {
                                        confirmDeleteDialog.slot_name = save_row.slot_name;
                                        confirmDeleteDialog.open();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Design.IronDialog {
        id: confirmOverwriteDialog

        property string slot_name: ""

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 460)
        title: qsTr("Replace this save?")
        tone: "danger"
        message: qsTr("\"%1\" will be replaced by the current battle. The version stored now cannot be recovered.").arg(confirmOverwriteDialog.slot_name)
        primaryAction: qsTr("Replace")
        secondaryAction: qsTr("Keep it")
        onPrimaryActivated: {
            UiAudio.play_confirm(typeof game !== 'undefined' ? game.audio_system : null);
            root.save_requested(confirmOverwriteDialog.slot_name);
        }
        onSecondaryActivated: UiAudio.play_back(typeof game !== 'undefined' ? game.audio_system : null)
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
            if (root.saves && root.saves.delete_save_slot)
                root.saves.delete_save_slot(confirmDeleteDialog.slot_name);
        }
        onSecondaryActivated: UiAudio.play_back(typeof game !== 'undefined' ? game.audio_system : null)
    }
}

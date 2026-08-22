import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Item {
    id: root

    signal cancelled
    signal save_requested(string slot_name)

    anchors.fill: parent
    z: 25
    onVisibleChanged: {
        if (!visible)
            return;
        if (typeof saveListModel !== 'undefined')
            saveListModel.load_from_game();
        if (typeof saveNameField !== 'undefined' && saveNameField)
            saveNameField.text = "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm");
    }
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        forceActiveFocus();
    }

    Connections {
        function onSave_slots_changed() {
            if (typeof saveListModel !== 'undefined')
                saveListModel.load_from_game();
        }

        target: typeof game !== 'undefined' ? game.saves : null
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
                    text: qsTr("Save Game")
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

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Save Name:")
                    color: Theme.textSub
                    font.pixelSize: Design.Typography.bodyLarge
                }

                TextField {
                    id: saveNameField

                    Layout.fillWidth: true
                    placeholderText: qsTr("Enter save name...")
                    text: "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm")
                    font.pixelSize: Design.Typography.bodyLarge
                    color: Theme.textMain

                    background: Rectangle {
                        color: Theme.cardBase
                        border.color: saveNameField.activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                        radius: Theme.radiusMedium
                    }
                }

                StyledButton {
                    text: qsTr("Save")
                    blocked: saveNameField.text.length === 0
                    disabledReason: qsTr("Name the save first.")
                    onClicked: {
                        if (saveListModel.slot_exists(saveNameField.text)) {
                            confirmOverwriteDialog.slot_name = saveNameField.text;
                            confirmOverwriteDialog.open();
                        } else {
                            root.save_requested(saveNameField.text);
                        }
                    }
                }
            }

            Label {
                text: qsTr("Existing Saves")
                color: Theme.textSub
                font.pixelSize: Design.Typography.bodyLarge
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

                        spacing: Theme.spacingSmall

                        model: ListModel {
                            id: saveListModel

                            function slot_exists(name) {
                                for (var i = 0; i < count; i++) {
                                    if (get(i).slot_name === name)
                                        return true;
                                }
                                return false;
                            }

                            function load_from_game() {
                                clear();
                                if (typeof game === 'undefined' || !game.saves.get_save_slots)
                                    return;
                                var entries = game.saves.get_save_slots();
                                for (var i = 0; i < entries.length; i++) {
                                    append({
                                            "slot_name": entries[i].slot_name,
                                            "title": entries[i].title || entries[i].slot_name || qsTr("Untitled Save"),
                                            "timestamp": entries[i].timestamp,
                                            "map_name": entries[i].map_name || qsTr("Unknown Map"),
                                            "mode": entries[i].mode || "",
                                            "kind": entries[i].kind || "manual",
                                            "stored_size": entries[i].stored_size || 0,
                                            "uncompressed_size": entries[i].uncompressed_size || 0,
                                            "thumbnail": entries[i].thumbnail || ""
                                        });
                                }
                            }

                            Component.onCompleted: {
                                load_from_game();
                            }
                        }

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

                                Rectangle {
                                    id: thumbnailContainer

                                    Layout.preferredWidth: 96
                                    Layout.preferredHeight: 64
                                    radius: Theme.radiusSmall
                                    color: Theme.cardBase
                                    border.color: Theme.cardBorder
                                    border.width: 1
                                    clip: true

                                    Image {
                                        id: thumbnailImage

                                        anchors.fill: parent
                                        anchors.margins: 2
                                        fillMode: Image.PreserveAspectCrop
                                        source: model.thumbnail && model.thumbnail.length > 0 ? "data:image/png;base64," + model.thumbnail : ""
                                        visible: source !== ""
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        visible: !thumbnailImage.visible
                                        text: qsTr("No Preview")
                                        color: Theme.textHint
                                        font.pixelSize: Design.Typography.label
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny

                                    Label {
                                        text: model.title
                                        color: Theme.textMain
                                        font.pixelSize: Design.Typography.subheading
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Slot: %1").arg(model.slot_name)
                                        color: Theme.textSub
                                        font.pixelSize: Design.Typography.body
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: model.uncompressed_size > 0 ? qsTr("%1 - %2 (%3 KB on disk)").arg(model.map_name).arg(model.mode === "campaign" ? qsTr("Campaign") : qsTr("Skirmish")).arg(Design.Numerals.roman(Math.max(1, Math.round(model.stored_size / 1024)))) : model.map_name
                                        color: Theme.textSub
                                        font.pixelSize: Design.Typography.body
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Last saved: %1").arg(Qt.formatDateTime(new Date(model.timestamp), "yyyy-MM-dd hh:mm:ss"))
                                        color: Theme.textHint
                                        font.pixelSize: Design.Typography.body
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }
                                }

                                StyledButton {
                                    text: qsTr("Overwrite")
                                    button_style: "danger"
                                    implicitWidth: 100
                                    onClicked: {
                                        confirmOverwriteDialog.slot_name = model.slot_name;
                                        confirmOverwriteDialog.open();
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    Design.UiSound.activate();
                                    saveNameField.text = model.slot_name;
                                }
                                onContainsMouseChanged: {
                                    if (containsMouse)
                                        Design.UiSound.hover();
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

        property string slot_name: ""

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.5, 400)
        title: qsTr("Confirm Overwrite")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        onRejected: UiAudio.play_back(typeof game !== 'undefined' ? game.audio_system : null)
        onAccepted: {
            UiAudio.play_confirm(typeof game !== 'undefined' ? game.audio_system : null);
            root.save_requested(slot_name);
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

                    text: qsTr("Are you sure you want to overwrite the save:\n\"%1\"?").arg(confirmOverwriteDialog.slot_name)
                    color: Theme.textMain
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pixelSize: Design.Typography.bodyLarge
                }
            }
        }
    }
}

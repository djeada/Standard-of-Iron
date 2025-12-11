import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0

Item {
    id: root

    signal cancelled()
    signal saveRequested(string slotName)

    anchors.fill: parent
    z: 25
    onVisibleChanged: {
        if (!visible)
            return ;

        if (typeof saveListModel !== 'undefined')
            saveListModel.loadFromGame();

        if (typeof saveNameField !== 'undefined' && saveNameField)
            saveNameField.text = "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm");

    }
    Keys.onPressed: function(event) {
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
                saveListModel.loadFromGame();

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
                    text: qsTr("Save Game")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Cancel")
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
                    font.pointSize: Theme.fontSizeMedium
                }

                TextField {
                    id: saveNameField

                    Layout.fillWidth: true
                    placeholderText: qsTr("Enter save name...")
                    text: "Save_" + Qt.formatDateTime(new Date(), "yyyy-MM-dd_HH-mm")
                    font.pointSize: Theme.fontSizeMedium
                    color: Theme.textMain

                    background: Rectangle {
                        color: Theme.cardBase
                        border.color: saveNameField.activeFocus ? Theme.accent : Theme.border
                        border.width: 1
                        radius: Theme.radiusMedium
                    }

                }

                Button {
                    text: qsTr("Save")
                    enabled: saveNameField.text.length > 0
                    highlighted: true
                    onClicked: {
                        if (saveListModel.slotExists(saveNameField.text)) {
                            confirmOverwriteDialog.slotName = saveNameField.text;
                            confirmOverwriteDialog.open();
                        } else {
                            root.saveRequested(saveNameField.text);
                        }
                    }
                }

            }

            Label {
                text: qsTr("Existing Saves")
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

                        spacing: Theme.spacingSmall

                        model: ListModel {
                            id: saveListModel

                            function slotExists(name) {
                                for (var i = 0; i < count; i++) {
                                    if (get(i).slotName === name)
                                        return true;

                                }
                                return false;
                            }

                            function loadFromGame() {
                                clear();
                                if (typeof game === 'undefined' || !game.get_save_slots)
                                    return ;

                                var slots = game.get_save_slots();
                                for (var i = 0; i < slots.length; i++) {
                                    append({
                                        "slotName": slots[i].slotName || slots[i].name,
                                        "title": slots[i].title || slots[i].name || slots[i].slotName || "Untitled Save",
                                        "timestamp": slots[i].timestamp,
                                        "map_name": slots[i].map_name || "Unknown Map",
                                        "thumbnail": slots[i].thumbnail || ""
                                    });
                                }
                            }

                            Component.onCompleted: {
                                loadFromGame();
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
                                        font.pointSize: Theme.fontSizeTiny
                                    }

                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny

                                    Label {
                                        text: model.title
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Slot: %1").arg(model.slotName)
                                        color: Theme.textSub
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: model.map_name
                                        color: Theme.textSub
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Last saved: %1").arg(Qt.formatDateTime(new Date(model.timestamp), "yyyy-MM-dd hh:mm:ss"))
                                        color: Theme.textHint
                                        font.pointSize: Theme.fontSizeSmall
                                        Layout.fillWidth: true
                                        elide: Label.ElideRight
                                    }

                                }

                                Button {
                                    text: qsTr("Overwrite")
                                    onClicked: {
                                        confirmOverwriteDialog.slotName = model.slotName;
                                        confirmOverwriteDialog.open();
                                    }
                                }

                            }

                            MouseArea {
                                id: mouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    saveNameField.text = model.slotName;
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
        title: qsTr("Confirm Overwrite")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            root.saveRequested(slotName);
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

                    text: qsTr("Are you sure you want to overwrite the save:\n\"%1\"?").arg(confirmOverwriteDialog.slotName)
                    color: Theme.textMain
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pointSize: Theme.fontSizeMedium
                }

            }

        }

    }

}

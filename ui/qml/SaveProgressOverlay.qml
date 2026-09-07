import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Item {
    id: root

    readonly property var saves: (typeof game !== 'undefined' && game) ? game.saves : null

    property string notice_text: ""
    property bool notice_is_bad: false
    readonly property bool saving: root.saves ? root.saves.save_in_progress : false

    function show_notice(text, bad) {
        root.notice_text = text;
        root.notice_is_bad = bad === true;
        notice_timer.restart();
    }

    anchors.fill: parent
    visible: root.saving || root.notice_text !== ""

    Connections {
        function onSave_completed(slot_name, success, error) {
            if (success) {
                root.show_notice(qsTr("Saved \"%1\"").arg(slot_name), false);
                return;
            }
            root.show_notice(error !== "" ? error : qsTr("Saving \"%1\" failed.").arg(slot_name), true);
        }

        target: root.saves
    }

    Timer {
        id: notice_timer

        interval: root.notice_is_bad ? 9000 : 2600
        onTriggered: root.notice_text = ""
    }

    Rectangle {
        id: card

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingLarge
        width: 320
        height: content.implicitHeight + Theme.spacingLarge * 2
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: root.notice_is_bad && !root.saving ? Theme.dangerBr : Theme.panelBr
        border.width: 1
        opacity: 0.96

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingSmall

            Label {
                text: root.saving ? qsTr("Saving \"%1\"").arg(root.saves ? root.saves.save_progress_slot : "") : root.notice_is_bad ? qsTr("Save failed") : qsTr("Saved")
                color: Theme.textMain
                font.pixelSize: Design.Typography.bodyLarge
                font.bold: true
                Layout.fillWidth: true
                elide: Label.ElideRight
            }

            Label {
                text: root.saving ? (root.saves ? root.saves.save_progress_stage : "") : root.notice_text
                color: root.notice_is_bad && !root.saving ? Theme.dangerBr : Theme.textSub
                font.pixelSize: Design.Typography.body
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: text !== ""
            }

            ProgressBar {
                visible: root.saving
                from: 0
                to: 100
                value: root.saves ? root.saves.save_progress_percent : 0
                Layout.fillWidth: true
            }

            StyledButton {
                text: qsTr("Cancel")
                button_style: "secondary"
                visible: root.saving
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    if (root.saves && root.saves.cancel_active_save)
                        root.saves.cancel_active_save();
                }
            }
        }
    }
}

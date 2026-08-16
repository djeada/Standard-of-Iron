import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property string last_error: ""
    property bool show_error: false

    anchors.fill: parent
    visible: (typeof game !== 'undefined' && game.save_in_progress) || root.show_error

    Connections {
        function onSave_completed(slot_name, success, error) {
            if (success) {
                root.show_error = false;
                root.last_error = "";
                error_timer.stop();
                return;
            }
            root.last_error = error;
            root.show_error = true;
            error_timer.restart();
        }

        target: typeof game !== 'undefined' ? game : null
    }

    Timer {
        id: error_timer

        interval: 6000
        onTriggered: root.show_error = false
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
        border.color: root.show_error ? Theme.dangerBr : Theme.panelBr
        border.width: 1
        opacity: 0.96

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingSmall

            Label {
                text: root.show_error ? qsTr("Save failed") : qsTr("Saving \"%1\"").arg(typeof game !== 'undefined' ? game.save_progress_slot : "")
                color: Theme.textMain
                font.pixelSize: Design.Typography.bodyLarge
                font.bold: true
                Layout.fillWidth: true
                elide: Label.ElideRight
            }

            Label {
                text: root.show_error ? root.last_error : (typeof game !== 'undefined' ? game.save_progress_stage : "")
                color: root.show_error ? Theme.dangerBr : Theme.textSub
                font.pixelSize: Design.Typography.body
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ProgressBar {
                visible: !root.show_error
                from: 0
                to: 100
                value: typeof game !== 'undefined' ? game.save_progress_percent : 0
                Layout.fillWidth: true
            }

            StyledButton {
                text: qsTr("Cancel")
                button_style: "secondary"
                visible: !root.show_error
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    if (typeof game !== 'undefined' && game.cancel_active_save)
                        game.cancel_active_save();
                }
            }
        }
    }
}

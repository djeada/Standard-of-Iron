import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property string capturing_action: ""

    property string pending_action: ""
    property string pending_shortcut: ""
    property var pending_conflicts: []

    property var actions: InputBindings.actions

    implicitHeight: layout.implicitHeight

    function action_name(actionId) {
        for (var i = 0; i < root.actions.length; ++i) {
            if (root.actions[i].id === actionId)
                return root.actions[i].name;
        }
        return actionId;
    }

    function conflict_summary(ids) {
        var names = [];
        for (var i = 0; i < ids.length; ++i)
            names.push(root.action_name(ids[i]));
        return names.join(", ");
    }

    function begin_capture(actionId) {
        root.pending_action = "";
        root.pending_conflicts = [];
        root.capturing_action = actionId;
        captureArea.forceActiveFocus();
    }

    function cancel_capture() {
        root.capturing_action = "";
        root.pending_action = "";
        root.pending_conflicts = [];
    }

    function try_assign(actionId, shortcut) {
        if (!shortcut) {
            root.cancel_capture();
            return;
        }
        var conflicts = InputBindings.conflicts_for(actionId, shortcut);
        if (conflicts.length === 0) {
            InputBindings.assign(actionId, shortcut);
            root.cancel_capture();
            return;
        }
        root.capturing_action = "";
        root.pending_action = actionId;
        root.pending_shortcut = shortcut;
        root.pending_conflicts = conflicts;
    }

    function confirm_pending() {
        InputBindings.assign_overriding(root.pending_action, root.pending_shortcut);
        root.cancel_capture();
    }

    function handle_capture_key(event) {
        if (root.capturing_action === "")
            return;
        event.accepted = true;
        if (event.isAutoRepeat)
            return;
        if (event.key === Qt.Key_Escape) {
            root.cancel_capture();
            return;
        }
        if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
            InputBindings.clear_binding(root.capturing_action);
            root.cancel_capture();
            return;
        }
        if (InputBindings.is_modifier_key(event.key))
            return;
        root.try_assign(root.capturing_action, InputBindings.encode_key(event.key, event.modifiers));
    }

    function handle_capture_key_release(event) {
        if (root.capturing_action === "")
            return;
        if (!InputBindings.is_modifier_key(event.key))
            return;
        event.accepted = true;
        root.try_assign(root.capturing_action, InputBindings.encode_key(event.key, Qt.NoModifier));
    }

    Item {
        id: captureArea

        anchors.fill: parent
        focus: root.capturing_action !== ""
        Keys.enabled: root.capturing_action !== ""
        Keys.onPressed: function (event) {
            root.handle_capture_key(event);
        }
        Keys.onReleased: function (event) {
            root.handle_capture_key_release(event);
        }
    }

    ColumnLayout {
        id: layout

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Theme.spacingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMedium

            Label {
                Layout.fillWidth: true
                text: root.capturing_action !== "" ? qsTr("Press a key or click a mouse button. Backspace clears it, Esc cancels.") : qsTr("Select a command to rebind it.")
                color: root.capturing_action !== "" ? Theme.accent : Theme.textSub
                font.pointSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            StyledButton {
                text: qsTr("Reset all")
                button_style: "secondary"
                enabled: !InputBindings.isDefault
                onClicked: {
                    root.cancel_capture();
                    InputBindings.reset_to_defaults();
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: conflictBanner.implicitHeight + Theme.spacingMedium * 2
            visible: root.pending_action !== ""
            radius: Theme.radiusMedium
            color: Theme.cardBase
            border.width: 1
            border.color: Theme.warning

            ColumnLayout {
                id: conflictBanner

                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingSmall

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 is already used by %2.").arg(InputBindings.describe(root.pending_shortcut)).arg(root.conflict_summary(root.pending_conflicts))
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeMedium
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: Theme.spacingSmall

                    StyledButton {
                        text: qsTr("Use it here anyway")
                        button_style: "primary"
                        onClicked: root.confirm_pending()
                    }

                    StyledButton {
                        text: qsTr("Keep as it is")
                        button_style: "secondary"
                        onClicked: root.cancel_capture()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Taking it leaves the other command unbound.")
                    color: Theme.textSub
                    font.pointSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Repeater {
            model: root.actions

            delegate: ColumnLayout {
                id: bindingRow

                required property var modelData
                required property int index

                readonly property bool starts_category: index === 0 || root.actions[index - 1].category !== modelData.category
                readonly property bool is_capturing: root.capturing_action === modelData.id

                Layout.fillWidth: true
                spacing: Theme.spacingTiny

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: bindingRow.index === 0 ? 0 : Theme.spacingMedium
                    visible: bindingRow.starts_category
                    text: modelData.category
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeMedium
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.textSub
                            font.pointSize: Theme.fontSizeMedium
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: modelData.description !== "" || modelData.conflicts.length > 0
                            text: modelData.conflicts.length > 0 ? qsTr("Conflicts with %1").arg(root.conflict_summary(modelData.conflicts)) : modelData.description
                            color: modelData.conflicts.length > 0 ? Theme.warning : Theme.textSub
                            font.pointSize: Theme.fontSizeSmall
                            opacity: modelData.conflicts.length > 0 ? 1.0 : 0.7
                            wrapMode: Text.WordWrap
                        }
                    }

                    StyledButton {
                        Layout.minimumWidth: Design.Metrics.space32 * 4
                        text: bindingRow.is_capturing ? qsTr("Press a key…") : (modelData.unbound ? qsTr("Unbound") : modelData.displayShortcut)
                        button_style: bindingRow.is_capturing ? "primary" : "secondary"
                        onClicked: bindingRow.is_capturing ? root.cancel_capture() : root.begin_capture(bindingRow.modelData.id)

                        MouseArea {
                            anchors.fill: parent
                            enabled: bindingRow.is_capturing
                            acceptedButtons: Qt.RightButton | Qt.MiddleButton | Qt.BackButton | Qt.ForwardButton
                            onPressed: function (mouse) {
                                root.try_assign(bindingRow.modelData.id, InputBindings.encode_mouse(mouse.button, mouse.modifiers));
                                mouse.accepted = true;
                            }
                        }
                    }

                    StyledButton {
                        text: qsTr("Default")
                        button_style: "small"
                        enabled: !modelData.isDefault
                        onClicked: {
                            root.cancel_capture();
                            InputBindings.reset_action(bindingRow.modelData.id);
                        }
                    }
                }
            }
        }
    }
}

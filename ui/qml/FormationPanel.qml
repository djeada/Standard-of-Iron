import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: formationPanel

    property bool placing: false
    property var options: ({})
    property var intents: []
    property bool advanced_expanded: false

    readonly property string doctrine_name: options.doctrine_display_name !== undefined ? options.doctrine_display_name : ""
    readonly property string warning: options.warning !== undefined ? options.warning : ""
    readonly property string active_intent: options.intent !== undefined ? options.intent : "faction_default"

    visible: placing
    implicitWidth: card.implicitWidth
    implicitHeight: card.implicitHeight

    function game_ready() {
        return typeof game !== 'undefined' && game.placement !== undefined;
    }

    function refresh() {
        if (!game_ready())
            return;
        options = game.placement.formation_options;
        intents = game.placement.available_formation_intents;
    }

    function intent_label(intent_id) {
        if (!game_ready() || !game.placement.formation_intent_display_name)
            return intent_id;
        return game.placement.formation_intent_display_name(intent_id);
    }

    function intent_blocked_reason(intent_id) {
        if (!game_ready() || !game.placement.formation_intent_unavailable_reason)
            return "";
        return game.placement.formation_intent_unavailable_reason(intent_id);
    }

    function intent_purpose(intent_id) {
        switch (intent_id) {
        case "faction_default":
            return qsTr("The deployment this faction fights in by default. No setup needed.");
        case "line":
            return qsTr("Widest frontage. Best for meeting an advance head on.");
        case "column":
            return qsTr("Narrow and deep. Best for moving through gates and passes.");
        case "defensive":
            return qsTr("Compressed frontage with reserves. Best for holding ground.");
        case "assault":
            return qsTr("Weighted front with skirmishers ahead. Best for breaking a line.");
        case "encirclement":
            return qsTr("Wide flanks that close around a target. Needs cavalry.");
        case "siege_escort":
            return qsTr("Engines protected behind infantry. Needs a siege engine.");
        }
        return "";
    }

    onPlacingChanged: refresh()

    Connections {
        function onFormation_options_changed() {
            formationPanel.refresh();
        }

        ignoreUnknownSignals: true
        target: formationPanel.game_ready() ? game.placement : null
    }

    Rectangle {
        id: card

        color: Design.Theme.panelIron
        radius: Design.Metrics.radiusMedium
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderStrong
        implicitWidth: content.implicitWidth + Design.Metrics.space16 * 2
        implicitHeight: content.implicitHeight + Design.Metrics.space16 * 2

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Design.Metrics.space16
            spacing: Design.Metrics.space8

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Text {
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.medium
                    text: qsTr("Formation")
                }

                Text {
                    Layout.fillWidth: true
                    color: Design.Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: formationPanel.doctrine_name
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                spacing: Design.Metrics.space8

                Repeater {
                    model: formationPanel.intents

                    Design.IronButton {
                        readonly property string blocked_reason: formationPanel.intent_blocked_reason(modelData)

                        ToolTip.delay: Design.Metrics.tooltipDelay
                        ToolTip.text: enabled ? formationPanel.intent_purpose(modelData) : blocked_reason
                        ToolTip.visible: hovered && ToolTip.text.length > 0
                        disabledReason: blocked_reason
                        enabled: blocked_reason.length === 0
                        implicitWidth: Math.max(96, contentItem.implicitWidth + Design.Metrics.space16)
                        text: formationPanel.intent_label(modelData)
                        tone: formationPanel.active_intent === modelData ? "primary" : "secondary"

                        onClicked: {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_intent(modelData);
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                color: Design.Theme.warning !== undefined ? Design.Theme.warning : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                text: formationPanel.warning
                visible: formationPanel.warning.length > 0
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                text: qsTr("Drag to set frontage and facing • Wheel: depth • Alt: strong flank • Shift: keep order • Ctrl: tighter • Esc: cancel")
                wrapMode: Text.WordWrap
            }

            Design.IronDivider {
                Layout.fillWidth: true
            }

            Design.IronButton {
                Layout.fillWidth: true
                text: formationPanel.advanced_expanded ? qsTr("Hide advanced options") : qsTr("Advanced options")

                onClicked: formationPanel.advanced_expanded = !formationPanel.advanced_expanded
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                columnSpacing: Design.Metrics.space8
                columns: 2
                rowSpacing: Design.Metrics.space8
                visible: formationPanel.advanced_expanded

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Frontage")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Narrow"), qsTr("Balanced"), qsTr("Wide")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_frontage_preset(["narrow", "balanced", "wide"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Depth")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Shallow"), qsTr("Balanced"), qsTr("Deep")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_depth_preset(["shallow", "balanced", "deep"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Spacing")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Tight"), qsTr("Normal"), qsTr("Loose")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_spacing_preset(["tight", "normal", "loose"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Cavalry")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Balanced"), qsTr("Left"), qsTr("Split"), qsTr("Right")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_flank_preference(["balanced", "left", "split", "right"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Ranged")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Rear"), qsTr("Front"), qsTr("Skirmish")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_ranged_placement(["rear", "front", "skirmish"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Reserve")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Automatic"), qsTr("None"), qsTr("One row"), qsTr("Two rows")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_reserve_rows(index - 1);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Movement")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Reform at destination"), qsTr("Maintain formation")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_movement_policy(["reform_at_destination", "maintain_formation"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Mixed armies")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Majority doctrine"), qsTr("By role"), qsTr("Separate contingents"), qsTr("Commander")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_mixed_policy(["majority_doctrine", "composite_by_role", "separate_contingents", "commander_doctrine"][index]);
                    }
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Doctrine")
                }

                Design.IronDropdown {
                    Layout.fillWidth: true
                    model: [qsTr("Automatic"), qsTr("Rome"), qsTr("Carthage"), qsTr("Iron Sepulcher")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_doctrine_override(["", "rome", "carthage", "iron_sepulcher"][index]);
                    }
                }

                Design.IronButton {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: qsTr("Reset to faction default")

                    onClicked: {
                        if (formationPanel.game_ready())
                            game.placement.reset_formation_options();
                    }
                }
            }
        }
    }
}

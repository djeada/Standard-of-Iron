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

    readonly property int unit_count: options.unit_count !== undefined ? options.unit_count : 0
    readonly property int placed_count: options.placed_count !== undefined ? options.placed_count : 0
    readonly property int slot_count: options.slot_count !== undefined ? options.slot_count : 0
    readonly property int blocked_slots: options.blocked_slots !== undefined ? options.blocked_slots : 0
    readonly property int adjusted_slots: options.adjusted_slots !== undefined ? options.adjusted_slots : 0
    readonly property int ranks: options.ranks !== undefined ? options.ranks : 0
    readonly property int files: options.files !== undefined ? options.files : 0
    readonly property real plan_frontage: options.plan_frontage !== undefined ? options.plan_frontage : 0
    readonly property real plan_depth: options.plan_depth !== undefined ? options.plan_depth : 0
    readonly property bool plan_valid: options.plan_valid !== undefined ? options.plan_valid : false

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

    function measure(value) {
        return qsTr("%1 m").arg(Math.round(value));
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

                Design.IronBadge {
                    text: qsTr("%1 units").arg(formationPanel.unit_count)
                    tone: Design.Theme.accent
                    visible: formationPanel.unit_count > 0
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
                        ToolTip.text: enabled ? qsTr("[%1] %2").arg(index + 1).arg(formationPanel.intent_purpose(modelData)) : blocked_reason
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

            Rectangle {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                color: Design.Theme.backgroundRaised
                radius: Design.Metrics.radiusSmall
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.borderSubtle
                implicitHeight: readout.implicitHeight + Design.Metrics.space12
                visible: formationPanel.slot_count > 0

                RowLayout {
                    id: readout

                    anchors.fill: parent
                    anchors.margins: Design.Metrics.space8
                    spacing: Design.Metrics.space12

                    Repeater {
                        model: [{
                                "label": qsTr("Ranks"),
                                "value": formationPanel.ranks + " × " + formationPanel.files,
                                "tone": Design.Theme.textPrimary
                            }, {
                                "label": qsTr("Frontage"),
                                "value": formationPanel.measure(formationPanel.plan_frontage),
                                "tone": Design.Theme.textPrimary
                            }, {
                                "label": qsTr("Depth"),
                                "value": formationPanel.measure(formationPanel.plan_depth),
                                "tone": Design.Theme.textPrimary
                            }, {
                                "label": qsTr("Placed"),
                                "value": formationPanel.placed_count + "/" + formationPanel.slot_count,
                                "tone": formationPanel.blocked_slots > 0 ? Design.Theme.danger : Design.Theme.success
                            }]

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                text: modelData.label
                            }

                            Text {
                                color: modelData.tone
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.label
                                font.weight: Design.Typography.medium
                                text: modelData.value
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                color: Design.Theme.warning
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
                text: qsTr("%1 position(s) nudged to fit the ground.").arg(formationPanel.adjusted_slots)
                visible: formationPanel.adjusted_slots > 0 && formationPanel.blocked_slots === 0
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                text: qsTr("Drag to set frontage and facing • 1-9: preset • Wheel: depth • Alt: strong flank • Shift: keep order • Ctrl: tighter • Esc: cancel")
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
                    currentIndex: formationPanel.options.frontage_index !== undefined ? formationPanel.options.frontage_index : 1
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
                    currentIndex: formationPanel.options.depth_index !== undefined ? formationPanel.options.depth_index : 1
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
                    currentIndex: formationPanel.options.spacing_index !== undefined ? formationPanel.options.spacing_index : 1
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
                    currentIndex: formationPanel.options.flank_index !== undefined ? formationPanel.options.flank_index : 0
                    model: [qsTr("Balanced"), qsTr("Left"), qsTr("Right"), qsTr("Split")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_flank_preference(["balanced", "left", "right", "split"][index]);
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
                    currentIndex: formationPanel.options.ranged_index !== undefined ? formationPanel.options.ranged_index : 1
                    model: [qsTr("Front"), qsTr("Rear"), qsTr("Skirmish")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_ranged_placement(["front", "rear", "skirmish"][index]);
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
                    currentIndex: formationPanel.options.reserve_index !== undefined ? formationPanel.options.reserve_index : 0
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
                    currentIndex: formationPanel.options.movement_index !== undefined ? formationPanel.options.movement_index : 0
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
                    currentIndex: formationPanel.options.mixed_index !== undefined ? formationPanel.options.mixed_index : 3
                    model: [qsTr("By role"), qsTr("Separate contingents"), qsTr("Commander"), qsTr("Majority doctrine")]

                    onActivated: function (index) {
                        if (formationPanel.game_ready())
                            game.placement.set_formation_mixed_policy(["composite_by_role", "separate_contingents", "commander_doctrine", "majority_doctrine"][index]);
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
                    currentIndex: {
                        if (formationPanel.options.doctrine_locked !== true)
                            return 0;
                        var order = ["rome", "carthage", "iron_sepulcher"];
                        var found = order.indexOf(formationPanel.options.doctrine);
                        return found < 0 ? 0 : found + 1;
                    }
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

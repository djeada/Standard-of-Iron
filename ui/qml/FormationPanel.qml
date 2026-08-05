import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: formationPanel

    property bool placing: false
    property var options: ({})
    property var intents: []
    property bool advanced_expanded: false
    property string hovered_intent: ""
    property var doctrines: []

    property real max_height: 0

    readonly property int content_width: Design.A11y.scaled(432)

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

    readonly property string described_intent: hovered_intent.length > 0 ? hovered_intent : active_intent

    readonly property var doctrine_names: {
        var out = [];
        for (var i = 0; i < doctrines.length; ++i)
            out.push(doctrines[i].name);
        return out;
    }

    readonly property int doctrine_index: {
        if (options.doctrine_locked !== true)
            return 0;
        for (var i = 0; i < doctrines.length; ++i) {
            if (doctrines[i].id === options.doctrine)
                return i;
        }
        return 0;
    }

    visible: placing
    implicitWidth: card.width
    implicitHeight: card.height

    function game_ready() {
        return typeof game !== 'undefined' && game.placement !== undefined;
    }

    function refresh() {
        if (!game_ready())
            return;
        options = game.placement.formation_options;
        intents = game.placement.formation_intents;
        if (doctrines.length === 0 && game.placement.formation_doctrine_options)
            doctrines = game.placement.formation_doctrine_options();
    }

    function doctrine_id_at(index) {
        return index >= 0 && index < doctrines.length ? doctrines[index].id : "";
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

    function intent_shape(intent_id) {
        switch (intent_id) {
        case "faction_default":
            return ".#####." + ".#####." + "..###.." + ".......";
        case "line":
            return "#######" + "#######" + "......." + ".......";
        case "column":
            return "..###.." + "..###.." + "..###.." + "..###..";
        case "defensive":
            return ".#####." + ".#####." + "......." + "..###..";
        case "assault":
            return "..@.@.." + ".#####." + "..###.." + "...#...";
        case "encirclement":
            return "##...##" + "#.....#" + ".#####." + ".#####.";
        case "siege_escort":
            return "..@@@.." + ".#####." + "#######" + ".......";
        }
        return "";
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

    onPlacingChanged: {
        hovered_intent = "";
        refresh();
    }

    Connections {
        function onFormation_options_changed() {
            formationPanel.refresh();
        }

        ignoreUnknownSignals: true
        target: formationPanel.game_ready() ? game.placement : null
    }

    Rectangle {
        id: card

        border.color: Design.Theme.borderStrong
        border.width: Design.Metrics.borderThin
        color: Design.Theme.panelIron
        height: viewport.height + Design.Metrics.space16 * 2
        radius: Design.Metrics.radiusMedium
        width: viewport.width + Design.Metrics.space16 * 2

        Flickable {
            id: viewport

            readonly property real ceiling: formationPanel.max_height > 0 ? Math.max(Design.A11y.scaled(120), formationPanel.max_height - Design.Metrics.space16 * 2) : body.height

            boundsBehavior: Flickable.StopAtBounds
            clip: true
            contentHeight: body.height
            contentWidth: width
            height: Math.min(body.height, ceiling)
            width: formationPanel.content_width
            x: Design.Metrics.space16
            y: Design.Metrics.space16

            ScrollBar.vertical: ScrollBar {
                policy: viewport.contentHeight > viewport.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            Column {
                id: body

                spacing: Design.Metrics.space4
                width: formationPanel.content_width

                Item {
                    height: title.implicitHeight
                    width: parent.width

                    Text {
                        id: title

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.label
                        font.weight: Design.Typography.bold
                        text: qsTr("Choose a formation")
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        text: formationPanel.unit_count > 0 ? qsTr("%1 · %2 units").arg(formationPanel.doctrine_name).arg(formationPanel.unit_count) : formationPanel.doctrine_name
                    }
                }

                Grid {
                    id: choices

                    readonly property int cellWidth: Math.floor((formationPanel.content_width - columnSpacing * (columns - 1)) / columns)

                    columnSpacing: Design.Metrics.space4
                    columns: 4
                    rowSpacing: Design.Metrics.space4
                    width: parent.width

                    Repeater {
                        model: formationPanel.intents

                        Rectangle {
                            id: choice

                            readonly property string blocked_reason: formationPanel.intent_blocked_reason(modelData)
                            readonly property bool available: blocked_reason.length === 0
                            readonly property bool selected: formationPanel.active_intent === modelData

                            border.color: !available ? Design.Theme.borderSubtle : choice.selected ? Design.Theme.accent : hover.containsMouse ? Design.Theme.borderStrong : Design.Theme.borderSubtle
                            border.width: choice.selected ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                            color: !available ? Design.Theme.surfaceDisabled : choice.selected ? Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, 0.16) : hover.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundRaised
                            height: Design.A11y.scaled(50)
                            radius: Design.Metrics.radiusSmall
                            width: choices.cellWidth

                            Accessible.name: formationPanel.intent_label(modelData)
                            Accessible.description: choice.available ? formationPanel.intent_purpose(modelData) : choice.blocked_reason
                            Accessible.role: Accessible.Button

                            FormationShape {
                                id: glyph

                                accentTone: choice.available ? Design.Theme.accent : Design.Theme.textDisabled
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: Design.Metrics.space8
                                dotSize: Design.A11y.scaled(4)
                                gap: Design.A11y.scaled(2)
                                opacity: choice.available ? 1 : 0.45
                                pattern: formationPanel.intent_shape(modelData)
                                tone: choice.selected ? Design.Theme.textPrimary : Design.Theme.textSecondary
                            }

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: Design.Metrics.space4
                                anchors.left: parent.left
                                anchors.leftMargin: Design.Metrics.space2
                                anchors.right: parent.right
                                anchors.rightMargin: Design.Metrics.space2
                                color: !choice.available ? Design.Theme.textDisabled : choice.selected ? Design.Theme.textPrimary : Design.Theme.textSecondary
                                elide: Text.ElideRight
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: choice.selected ? Design.Typography.medium : Design.Typography.regular
                                horizontalAlignment: Text.AlignHCenter
                                text: formationPanel.intent_label(modelData)
                            }

                            Design.IronHotkeyLabel {
                                anchors.left: parent.left
                                anchors.leftMargin: Design.Metrics.space2
                                anchors.top: parent.top
                                anchors.topMargin: Design.Metrics.space2
                                opacity: choice.available ? 1 : 0.5
                                text: String(index + 1)
                            }

                            MouseArea {
                                id: hover

                                anchors.fill: parent
                                cursorShape: choice.available ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                hoverEnabled: true

                                onEntered: formationPanel.hovered_intent = modelData
                                onExited: {
                                    if (formationPanel.hovered_intent === modelData)
                                        formationPanel.hovered_intent = "";
                                }
                                onClicked: {
                                    if (!choice.available) {
                                        Design.UiSound.warning();
                                        return;
                                    }
                                    Design.UiSound.activate();
                                    if (formationPanel.game_ready())
                                        game.placement.set_formation_intent(modelData);
                                }
                            }
                        }
                    }
                }

                Item {
                    height: Math.max(purpose.implicitHeight, Design.A11y.scaled(28))
                    width: parent.width

                    Text {
                        id: purpose

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        text: formationPanel.intent_purpose(formationPanel.described_intent)
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    border.color: Design.Theme.borderSubtle
                    border.width: Design.Metrics.borderThin
                    color: Design.Theme.backgroundRaised
                    height: readout.height + Design.Metrics.space8 * 2
                    radius: Design.Metrics.radiusSmall
                    visible: formationPanel.slot_count > 0
                    width: parent.width

                    Row {
                        id: readout

                        readonly property int cellWidth: Math.floor((parent.width - Design.Metrics.space8 * 2) / 4)

                        spacing: 0
                        x: Design.Metrics.space8
                        y: Design.Metrics.space8

                        FormationReadoutCell {
                            label: qsTr("Shape")
                            tone: Design.Theme.textPrimary
                            value: qsTr("%1 × %2").arg(formationPanel.ranks).arg(formationPanel.files)
                            width: readout.cellWidth
                        }

                        FormationReadoutCell {
                            label: qsTr("Frontage")
                            tone: Design.Theme.textPrimary
                            value: formationPanel.measure(formationPanel.plan_frontage)
                            width: readout.cellWidth
                        }

                        FormationReadoutCell {
                            label: qsTr("Depth")
                            tone: Design.Theme.textPrimary
                            value: formationPanel.measure(formationPanel.plan_depth)
                            width: readout.cellWidth
                        }

                        FormationReadoutCell {
                            label: qsTr("In place")
                            tone: formationPanel.blocked_slots > 0 ? Design.Theme.danger : Design.Theme.success
                            value: formationPanel.placed_count + "/" + formationPanel.slot_count
                            width: readout.cellWidth
                        }
                    }
                }

                Text {
                    color: Design.Theme.warning
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: formationPanel.warning
                    visible: formationPanel.warning.length > 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("%1 position(s) nudged to fit the ground.").arg(formationPanel.adjusted_slots)
                    visible: formationPanel.adjusted_slots > 0 && formationPanel.blocked_slots === 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                Design.IronDivider {
                    width: parent.width
                }

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    lineHeight: 1.25
                    text: qsTr("Drag on the ground to set width and facing.") + "\n" + qsTr("Click to deploy · Wheel to turn · Ctrl+wheel for depth · Right-click or Esc to cancel")
                    width: parent.width
                    wrapMode: Text.WordWrap
                }

                Design.IronButton {
                    text: formationPanel.advanced_expanded ? qsTr("Hide fine tuning") : qsTr("Fine tuning")
                    width: parent.width

                    onClicked: formationPanel.advanced_expanded = !formationPanel.advanced_expanded
                }

                Column {
                    spacing: Design.Metrics.space4
                    visible: formationPanel.advanced_expanded
                    width: parent.width

                    FormationOptionRow {
                        hint: qsTr("How wide the deployment spreads before it starts adding ranks behind.")
                        label: qsTr("Frontage")
                        model: [qsTr("Narrow"), qsTr("Balanced"), qsTr("Wide")]
                        selectedIndex: formationPanel.options.frontage_index !== undefined ? formationPanel.options.frontage_index : 1
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_frontage_preset(["narrow", "balanced", "wide"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("How many ranks stand behind the front. Deeper hits harder but covers less ground.")
                        label: qsTr("Depth")
                        model: [qsTr("Shallow"), qsTr("Balanced"), qsTr("Deep")]
                        selectedIndex: formationPanel.options.depth_index !== undefined ? formationPanel.options.depth_index : 1
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_depth_preset(["shallow", "balanced", "deep"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("The gap between units. Tight holds shape better; loose loses less to area attacks.")
                        label: qsTr("Spacing")
                        model: [qsTr("Tight"), qsTr("Normal"), qsTr("Loose")]
                        selectedIndex: formationPanel.options.spacing_index !== undefined ? formationPanel.options.spacing_index : 1
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_spacing_preset(["tight", "normal", "loose"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Which wing the cavalry weights. Split puts an equal body on each wing.")
                        label: qsTr("Cavalry wing")
                        model: [qsTr("Balanced"), qsTr("Left"), qsTr("Right"), qsTr("Split")]
                        selectedIndex: formationPanel.options.flank_index !== undefined ? formationPanel.options.flank_index : 0
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_flank_preference(["balanced", "left", "right", "split"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Where archers and slingers stand. Skirmish sends them out ahead and pulls them back on contact.")
                        label: qsTr("Missile troops")
                        model: [qsTr("In front"), qsTr("Behind"), qsTr("Skirmish ahead")]
                        selectedIndex: formationPanel.options.ranged_index !== undefined ? formationPanel.options.ranged_index : 1
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_ranged_placement(["front", "rear", "skirmish"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Rows held back from the fighting line to plug gaps.")
                        label: qsTr("Reserve rows")
                        model: [qsTr("Doctrine decides"), qsTr("None"), qsTr("One row"), qsTr("Two rows")]
                        selectedIndex: formationPanel.options.reserve_index !== undefined ? formationPanel.options.reserve_index : 0
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_reserve_rows(index - 1);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Reform is faster and copes with chokepoints. Maintain keeps the shape on the march but moves slower.")
                        label: qsTr("On the march")
                        model: [qsTr("Reform on arrival"), qsTr("Hold the shape")]
                        selectedIndex: formationPanel.options.movement_index !== undefined ? formationPanel.options.movement_index : 0
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_movement_policy(["reform_at_destination", "maintain_formation"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("How a selection drawn from several factions is arranged.")
                        label: qsTr("Mixed armies")
                        model: [qsTr("By role"), qsTr("Separate contingents"), qsTr("Follow the commander"), qsTr("Follow the majority")]
                        selectedIndex: formationPanel.options.mixed_index !== undefined ? formationPanel.options.mixed_index : 3
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_mixed_policy(["composite_by_role", "separate_contingents", "commander_doctrine", "majority_doctrine"][index]);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Sorting groups the same troop types together. Keeping the order leaves each unit where it already stands in the line.")
                        label: qsTr("Slot order")
                        model: [qsTr("Sort by type"), qsTr("Keep current order")]
                        selectedIndex: formationPanel.options.preserve_index !== undefined ? formationPanel.options.preserve_index : 0
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_preserve_order(index === 1);
                        }
                    }

                    FormationOptionRow {
                        hint: qsTr("Which drill book shapes the deployment. Automatic follows the troops you selected.")
                        label: qsTr("Doctrine")
                        model: formationPanel.doctrine_names
                        selectedIndex: formationPanel.doctrine_index
                        width: parent.width

                        onActivated: function (index) {
                            if (formationPanel.game_ready())
                                game.placement.set_formation_doctrine_override(formationPanel.doctrine_id_at(index));
                        }
                    }

                    Design.IronButton {
                        text: qsTr("Reset to faction default")
                        width: parent.width

                        onClicked: {
                            if (formationPanel.game_ready())
                                game.placement.reset_formation_options();
                        }
                    }
                }
            }
        }
    }
}

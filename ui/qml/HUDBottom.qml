import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

RowLayout {
    id: bottomRoot

    property string current_command_mode: "normal"
    property int selection_tick
    property bool has_movable_units
    property var action_states: ({})
    property var selection_groups: []
    property int selection_count: 0

    readonly property int zoneMinimumWidth: Design.A11y.scaled(170)
    readonly property int zoneWidth: Math.max(bottomRoot.zoneMinimumWidth, Math.floor((bottomRoot.width - bottomRoot.spacing * 2) / 3))

    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)
    signal unit_profile_requested(string unit_type, string nation, bool from_selection)

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function update_action_states() {
        action_states = (game_ready() && game.orders.action_states) ? game.orders.action_states() : ({});
    }

    function refresh_selection() {
        var model = game_ready() ? game.selected_units_model : null;
        if (!model) {
            selection_groups = [];
            selection_count = 0;
            return;
        }
        selection_groups = model.grouped_by_type ? model.grouped_by_type() : [];
        selection_count = model.rowCount();
    }

    function action_state(actionId) {
        var state = action_states ? action_states[actionId] : null;
        return state || {
            "enabled": false,
            "active": false,
            "mixed": false,
            "placing": false,
            "passive": false,
            "eligibleCount": 0,
            "activeCount": 0,
            "readyCount": 0,
            "detail": ({})
        };
    }

    function detail_of(state) {
        return state && state.detail ? state.detail : ({});
    }

    function measure(value) {
        return qsTr("%1 m").arg(Math.round(value));
    }

    function seconds(value) {
        return qsTr("%1s").arg(Math.max(0, Math.ceil(value)));
    }

    function fact(term, text) {
        return {
            "term": term,
            "text": text
        };
    }

    function command_details(entry, state) {
        var d = bottomRoot.detail_of(state);
        switch (entry.id) {
        case "guard":
            return [bottomRoot.fact(qsTr("Give it"), qsTr("Press Guard, then click the ground to hold. Right-click cancels.")), bottomRoot.fact(qsTr("Reach"), qsTr("They fight what comes within %1 of that spot, and drop a target that leaves it.").arg(bottomRoot.measure(d.radius !== undefined ? d.radius : 10))), bottomRoot.fact(qsTr("After"), qsTr("They walk back to the spot when the fight ends.")), bottomRoot.fact(qsTr("Release"), qsTr("Press Guard again, or order a move or attack. Stop does not lift it.")), bottomRoot.fact(qsTr("Troops"), qsTr("Any soldier, and the commander.")), bottomRoot.fact(qsTr("vs Hold"), qsTr("Guards step out to meet the enemy; Hold never moves."))];
        case "hold":
            return [bottomRoot.fact(qsTr("Give it"), qsTr("Press Hold. There is nothing to click.")), bottomRoot.fact(qsTr("Stance"), qsTr("They kneel, strike whatever reaches them, and never pursue.")), bottomRoot.fact(qsTr("Dug in"), qsTr("Archers reach %1% further and spearmen %2%; both hit %3% harder, take %4% more punishment, and braced spears break a charge.").arg(d.archerRangeBonusPercent !== undefined ? d.archerRangeBonusPercent : 50).arg(d.spearmanRangeBonusPercent !== undefined ? d.spearmanRangeBonusPercent : 100).arg(d.damageBonusPercent !== undefined ? d.damageBonusPercent : 50).arg(d.healthBonusPercent !== undefined ? d.healthBonusPercent : 20)), bottomRoot.fact(qsTr("Release"), qsTr("Press Hold again, or order a move, attack or Stop. Standing up takes a moment.")), bottomRoot.fact(qsTr("Troops"), qsTr("Archers and spearmen only.")), bottomRoot.fact(qsTr("vs Guard"), qsTr("Guard would chase; Hold trades that for reach and staying power."))];
        case "patrol":
            return [bottomRoot.fact(qsTr("Give it"), qsTr("Press Patrol, left-click the first waypoint, then left-click the second. Right-click cancels.")), bottomRoot.fact(qsTr("Route"), qsTr("They march between the two points for good, attacking whatever crosses the line.")), bottomRoot.fact(qsTr("Release"), qsTr("Any other order, or Stop, clears the route.")), bottomRoot.fact(qsTr("Troops"), qsTr("Every soldier and the commander."))];
        case "aura":
            return [bottomRoot.fact(qsTr("Effect"), d.summary !== undefined && d.summary !== "" ? d.summary : qsTr("Nearby troops fight with steadier morale and the commander's own bonus.")), bottomRoot.fact(qsTr("Reach"), qsTr("%1 around the commander, and it moves with him.").arg(bottomRoot.measure(d.radius !== undefined ? d.radius : 12))), bottomRoot.fact(qsTr("Lasts"), bottomRoot.seconds(d.duration !== undefined ? d.duration : 15)), bottomRoot.fact(qsTr("Recharge"), qsTr("%1 once it fades. A wounded commander cannot call it.").arg(bottomRoot.seconds(d.cooldown !== undefined ? d.cooldown : 60))), bottomRoot.fact(qsTr("Troops"), qsTr("Your own living troops inside the ring; a glow marks each one."))];
        }
        return entry.details || [];
    }

    readonly property var gatherPriorities: ["", "cut_tree", "collect_stone", "collect_iron_ore", "harvest_grain"]
    readonly property var tutorial: (typeof game !== 'undefined' && game && game.tutorial) ? game.tutorial : null
    readonly property var tutorialFocusActions: (bottomRoot.tutorial && bottomRoot.tutorial.active) ? bottomRoot.tutorial.focus_actions : []
    readonly property string tutorialFocusRegion: (bottomRoot.tutorial && bottomRoot.tutorial.active) ? bottomRoot.tutorial.focus_region : ""

    function tutorial_spotlights(actionId) {
        return bottomRoot.tutorialFocusActions.indexOf(actionId) >= 0;
    }

    function gather_priority_label(priority) {
        if (priority === "cut_tree")
            return qsTr("Wood only");
        if (priority === "collect_stone")
            return qsTr("Stone only");
        if (priority === "collect_iron_ore")
            return qsTr("Iron only");
        if (priority === "harvest_grain")
            return qsTr("Food first");
        return qsTr("Any resource");
    }

    function advance_auto_gather(state) {
        if (!bottomRoot.game_ready() || !game.activity)
            return;
        var current = bottomRoot.detail_of(state).priority || "";
        if (!state.active) {
            game.activity.set_auto_gather(true, bottomRoot.gatherPriorities[0]);
            return;
        }
        var next = bottomRoot.gatherPriorities.indexOf(current) + 1;
        if (next >= bottomRoot.gatherPriorities.length) {
            game.activity.set_auto_gather(false);
            return;
        }
        game.activity.set_auto_gather(true, bottomRoot.gatherPriorities[next]);
    }

    function command_status(entry, state) {
        var d = bottomRoot.detail_of(state);
        if (entry.id === "patrol") {
            if (d.waypointStage === 1)
                return qsTr("Click waypoint 1");
            if (d.waypointStage === 2)
                return qsTr("Click waypoint 2");
            if (state.active)
                return qsTr("On patrol");
            return "";
        }
        if (entry.id === "aura") {
            if (state.active)
                return qsTr("Active %1").arg(bottomRoot.seconds(d.remaining !== undefined ? d.remaining : 0));
            if (d.cooldownRemaining !== undefined && d.cooldownRemaining > 0)
                return qsTr("Ready in %1").arg(bottomRoot.seconds(d.cooldownRemaining));
            if (state.enabled)
                return qsTr("Ready");
            return "";
        }
        if (entry.id === "auto_gather" && state.active)
            return bottomRoot.gather_priority_label(d.priority || "");
        if (entry.id === "guard" && state.active)
            return qsTr("Holding a spot");
        if (entry.id === "hold" && state.active)
            return qsTr("Dug in");
        return "";
    }

    function command_cooldown(entry, state) {
        if (entry.id !== "aura")
            return 0;
        var d = bottomRoot.detail_of(state);
        if (!d.cooldown || d.cooldown <= 0 || !d.cooldownRemaining)
            return 0;
        return Math.max(0, Math.min(1, d.cooldownRemaining / d.cooldown));
    }

    function aura_is_ticking() {
        var d = bottomRoot.detail_of(bottomRoot.action_state("aura"));
        return (d.remaining !== undefined && d.remaining > 0) || (d.cooldownRemaining !== undefined && d.cooldownRemaining > 0);
    }

    function unavailable_reason(entry, state) {
        if (state.passive && entry.passiveReason)
            return entry.passiveReason;
        if (entry.needsTroops && !has_movable_units)
            return qsTr("Select troops first");
        return entry.unavailable;
    }

    function command_banner_text() {
        if (!has_movable_units)
            return qsTr("No troops selected");
        if (current_command_mode === "normal")
            return qsTr("Orders ready");
        if (current_command_mode === "patrol") {
            var stage = bottomRoot.detail_of(bottomRoot.action_state("patrol")).waypointStage;
            if (stage === 2)
                return qsTr("Patrol: click the second waypoint");
            if (stage === 1)
                return qsTr("Patrol: click the first waypoint");
            return qsTr("On patrol");
        }
        if (current_command_mode === "guard") {
            if (bottomRoot.action_state("guard").placing)
                return qsTr("Guard: click the spot to hold");
            return qsTr("Guarding a spot");
        }
        var labels = ({
                "attack": qsTr("Attack order"),
                "heal": qsTr("Medic order"),
                "build": qsTr("Engineer order"),
                "repair": qsTr("Repair order"),
                "dismantle": qsTr("Dismantle order"),
                "collect": qsTr("Collection order"),
                "deliver": qsTr("Barracks delivery"),
                "formation": qsTr("Formation order"),
                "rally": qsTr("Rally order")
            });
        return labels[current_command_mode] || qsTr("Stop command");
    }

    function banner_tone() {
        if (!has_movable_units)
            return Design.Theme.textDisabled;
        if (current_command_mode === "normal")
            return Design.Theme.borderSubtle;
        if (current_command_mode === "attack")
            return Design.Theme.danger;
        return Design.Theme.accent;
    }

    readonly property var commands: [{
            "id": "attack",
            "label": qsTr("Attack"),
            "binding": "rts.order_attack",
            "needsTroops": true,
            "mode": "attack",
            "hint": qsTr("Send the selected troops at one enemy unit or building."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Attack, then left-click the target. Right-click cancels. A right-click on an enemy does the same thing.")
                }, {
                    "term": qsTr("Scope"),
                    "text": qsTr("They chase the target until it dies or you order otherwise.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Every fighting unit; healers and builders sit it out.")
                }],
            "unavailable": qsTr("Attack is not available for the current selection")
        }, {
            "id": "guard",
            "label": qsTr("Guard"),
            "binding": "rts.order_guard",
            "needsTroops": true,
            "mode": "guard",
            "hint": qsTr("Anchor troops to a spot. They step out to meet what comes near, then walk back."),
            "unavailable": qsTr("Guard is not available for the current selection")
        }, {
            "id": "patrol",
            "label": qsTr("Patrol"),
            "binding": "rts.order_patrol",
            "needsTroops": true,
            "mode": "patrol",
            "hint": qsTr("March a beat between two points and fight whatever crosses it."),
            "unavailable": qsTr("Patrol is not available for the current selection")
        }, {
            "id": "heal",
            "label": qsTr("Medic"),
            "needsTroops": true,
            "hint": qsTr("Healers tend the wounded around them on their own."),
            "details": [{
                    "term": qsTr("Passive"),
                    "text": qsTr("There is nothing to press: a healer mends nearby allies whenever they are hurt.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Healers only. Keep them behind the line and they will work.")
                }],
            "passiveReason": qsTr("Healers automatically heal nearby allies. This is passive, not a manual order."),
            "unavailable": qsTr("Select healer units")
        }, {
            "id": "stop",
            "label": qsTr("Stop"),
            "binding": "rts.order_stop",
            "needsTroops": true,
            "ignoreActionState": true,
            "hint": qsTr("Drop everything the selected troops are doing and stand still."),
            "details": [{
                    "term": qsTr("Clears"),
                    "text": qsTr("Movement, the current target, Patrol, Hold, formation and gathering orders.")
                }, {
                    "term": qsTr("Keeps"),
                    "text": qsTr("Guard: press Guard again to release troops from their anchor.")
                }],
            "unavailable": qsTr("Select troops first"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.orders.stop)
                    game.orders.stop();
            }
        }, {
            "id": "hold",
            "label": qsTr("Hold"),
            "binding": "rts.order_hold",
            "needsTroops": true,
            "hint": qsTr("Stand fast. Troops dig in where they are, hit harder and further, and never pursue."),
            "unavailable": qsTr("Hold is only available to archers and spearmen"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.orders.hold)
                    game.orders.hold();
            }
        }, {
            "id": "build",
            "label": qsTr("Build"),
            "needsTroops": true,
            "mode": "build",
            "hint": qsTr("Open the builder's structure list and place one."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Pick a structure, move the outline onto flat clear ground, scroll or press [ and ] to rotate, left-click to confirm. Right-click cancels.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Builders only.")
                }],
            "unavailable": qsTr("Build is only available to builders"),
            "invoke": function () {
                if (!bottomRoot.game_ready())
                    return;
                if (game.placement.is_placing_construction && game.placement.on_construction_cancel) {
                    game.placement.on_construction_cancel();
                    return;
                }
                if (bottomRoot.current_command_mode === "build") {
                    bottomRoot.command_mode_changed("normal");
                    return;
                }
                if (game.orders.build)
                    game.orders.build();
            }
        }, {
            "id": "collect",
            "label": qsTr("Collect"),
            "needsTroops": true,
            "activeFromPlacing": true,
            "hint": qsTr("Send a builder to fell a tree, break a boulder, work an ore seam, reap a ripe farm or slaughter a sheep."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Collect, then left-click the node. Right-click cancels.")
                }, {
                    "term": qsTr("Scope"),
                    "text": qsTr("The load only counts once it is dropped at the yard beside a barracks.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Builders only.")
                }],
            "unavailable": qsTr("Collect is only available to builders"),
            "invoke": function () {
                if (!bottomRoot.game_ready())
                    return;
                if (bottomRoot.action_state("collect").placing) {
                    if (game.placement.on_construction_cancel)
                        game.placement.on_construction_cancel();
                    bottomRoot.command_mode_changed("normal");
                    return;
                }
                if (game.placement.start_builder_construction)
                    game.placement.start_builder_construction("collect");
            }
        }, {
            "id": "auto_gather",
            "label": qsTr("Auto Gather"),
            "shortLabel": qsTr("Auto"),
            "needsTroops": true,
            "hint": qsTr("Builders keep finding and working the nearest resource on their own."),
            "details": [{
                    "term": qsTr("Scope"),
                    "text": qsTr("Runs until you stop it; any new order cancels it.")
                }, {
                    "term": qsTr("Pick a resource"),
                    "text": qsTr("Press again to cycle any resource, wood, stone, iron, food, then off.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Builders only.")
                }],
            "unavailable": qsTr("Auto Gather is only available to builders"),
            "invoke": function () {
                bottomRoot.advance_auto_gather(bottomRoot.action_state("auto_gather"));
                bottomRoot.update_action_states();
            }
        }, {
            "id": "repair",
            "label": qsTr("Repair"),
            "needsTroops": true,
            "activeFromPlacing": true,
            "hint": qsTr("Send a builder to mend one of your damaged buildings."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Repair, then left-click the damaged building. Right-click cancels.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Builders only.")
                }],
            "unavailable": qsTr("Repair is only available to builders"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity)
                    game.activity.begin_repair_order();
            }
        }, {
            "id": "dismantle",
            "label": qsTr("Dismantle"),
            "shortLabel": qsTr("Scrap"),
            "needsTroops": true,
            "activeFromPlacing": true,
            "hint": qsTr("Send builders to take one of your own buildings apart and get part of its cost back."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Dismantle, then left-click your building. Right-click cancels.")
                }, {
                    "term": qsTr("Pays back"),
                    "text": qsTr("Part of what it cost, once the work finishes. Calling the crew off pays nothing.")
                }, {
                    "term": qsTr("Crew"),
                    "text": qsTr("Builders only; up to three of them speed it up.")
                }],
            "unavailable": qsTr("Dismantle is only available to builders"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity)
                    game.activity.begin_dismantle_order();
            }
        }, {
            "id": "divide",
            "label": qsTr("Split"),
            "needsTroops": true,
            "hint": qsTr("Break the selected squad into two smaller ones so they can work or fight in two places at once."),
            "details": [{
                    "term": qsTr("Each half"),
                    "text": qsTr("Carries half the men, half the health and does its work at half the speed.")
                }, {
                    "term": qsTr("Needs"),
                    "text": qsTr("A squad of at least four still standing.")
                }],
            "unavailable": qsTr("This squad is too small to divide"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity)
                    game.activity.divide_selected_squads();
            }
        }, {
            "id": "join",
            "label": qsTr("Join"),
            "needsTroops": true,
            "hint": qsTr("Fold two understrength squads of the same kind back into one."),
            "details": [{
                    "term": qsTr("Needs"),
                    "text": qsTr("Two squads of the same kind, both below full strength and standing close together.")
                }, {
                    "term": qsTr("Result"),
                    "text": qsTr("One squad with their men and health added together, up to a full establishment.")
                }],
            "unavailable": qsTr("Select two understrength squads of the same kind"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity)
                    game.activity.merge_selected_squads();
            }
        }, {
            "id": "deliver",
            "label": qsTr("Deliver"),
            "needsTroops": true,
            "mode": "deliver",
            "hint": qsTr("Send civilians to a barracks to refill the reserve it recruits from."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Deliver, then left-click a friendly barracks. Right-click cancels.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Civilians only; other selected units ignore it.")
                }],
            "unavailable": qsTr("Deliver is only available to civilians")
        }, {
            "id": "rally",
            "label": qsTr("Rally"),
            "binding": "rts.commander_rally",
            "hint": qsTr("The commander plants a flag and the army marches to it."),
            "details": [{
                    "term": qsTr("Give it"),
                    "text": qsTr("Press Rally, then left-click where the flag should stand. He walks there and plants it.")
                }, {
                    "term": qsTr("Scope"),
                    "text": qsTr("Troops near the flag march in; it also steadies wavering men.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Needs your commander selected.")
                }],
            "unavailable": qsTr("Select a commander to use rally"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.commander.start_flag_rally)
                    game.commander.start_flag_rally();
            }
        }, {
            "id": "gate",
            "label": qsTr("Gate"),
            "hint": qsTr("Cycle the selected gates: automatic, held open, held shut."),
            "details": [{
                    "term": qsTr("Scope"),
                    "text": qsTr("Automatic gates open for your own troops and shut behind them.")
                }, {
                    "term": qsTr("Troops"),
                    "text": qsTr("Select one of your own gates.")
                }],
            "unavailable": qsTr("Select one of your gates to control it"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.orders.gate) {
                    game.orders.gate();
                    bottomRoot.update_action_states();
                }
            }
        }, {
            "id": "aura",
            "label": qsTr("Aura"),
            "hint": qsTr("The commander empowers the troops around him for a while, then must recharge."),
            "unavailable": qsTr("Select a ready commander to activate the aura"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.commander.trigger_aura) {
                    game.commander.trigger_aura();
                    bottomRoot.update_action_states();
                }
            }
        }]

    readonly property var primaryCommandIds: ["attack", "guard", "patrol", "hold", "stop"]
    readonly property var contextualCommandIds: ["build", "collect", "auto_gather", "repair", "dismantle", "divide", "join", "deliver", "rally", "aura", "gate", "heal"]
    readonly property var primaryCommands: bottomRoot.commands_for_ids(bottomRoot.primaryCommandIds)
    readonly property var contextualCommands: (bottomRoot.selection_tick, bottomRoot.action_states, bottomRoot.contextual_commands())

    function command_by_id(actionId) {
        for (var i = 0; i < bottomRoot.commands.length; ++i) {
            if (bottomRoot.commands[i].id === actionId)
                return bottomRoot.commands[i];
        }
        return null;
    }

    function commands_for_ids(ids) {
        var out = [];
        for (var i = 0; i < ids.length; ++i) {
            var entry = bottomRoot.command_by_id(ids[i]);
            if (entry)
                out.push(entry);
        }
        return out;
    }

    function contextual_commands() {
        var out = [];
        for (var i = 0; i < bottomRoot.contextualCommandIds.length; ++i) {
            var entry = bottomRoot.command_by_id(bottomRoot.contextualCommandIds[i]);
            if (!entry)
                continue;
            var state = bottomRoot.action_state(entry.id);
            var isCurrentMode = entry.mode && bottomRoot.current_command_mode === entry.mode;
            if (state.eligibleCount > 0 || state.active || state.mixed || state.placing || state.passive || isCurrentMode || bottomRoot.tutorial_spotlights(entry.id))
                out.push(entry);
        }
        return out;
    }

    function hotkey_for(entry) {
        if (!entry.binding)
            return "";
        return InputBindings.display_shortcut_for(entry.binding);
    }

    function invoke_command(entry) {
        if (entry.invoke) {
            entry.invoke();
            return;
        }
        if (entry.mode) {
            var wasActive = bottomRoot.current_command_mode === entry.mode;
            bottomRoot.command_mode_changed(wasActive ? "normal" : entry.mode);
        }
    }

    Component.onCompleted: {
        update_action_states();
        refresh_selection();
    }
    onSelection_tickChanged: {
        update_action_states();
        refresh_selection();
    }

    anchors.fill: parent
    anchors.leftMargin: Design.Metrics.hudZoneMargin
    anchors.rightMargin: Design.Metrics.hudZoneMargin
    anchors.topMargin: Design.Metrics.space4
    anchors.bottomMargin: Design.Metrics.space8
    spacing: Design.Metrics.space8

    Connections {
        function onCursor_mode_changed() {
            bottomRoot.update_action_states();
        }

        function onSelected_units_changed() {
            bottomRoot.refresh_selection();
        }

        target: bottomRoot.game_ready() ? game : null
    }

    Timer {
        interval: 500
        repeat: true
        running: bottomRoot.aura_is_ticking()
        onTriggered: bottomRoot.update_action_states()
    }

    Connections {
        function onPlacing_construction_changed() {
            bottomRoot.update_action_states();
        }

        function onPlacing_formation_changed() {
            bottomRoot.update_action_states();
        }

        target: bottomRoot.game_ready() ? game.placement : null
    }

    Design.IronSelectionSummary {
        objectName: "selectionZone"

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: bottomRoot.zoneWidth
        Layout.minimumWidth: bottomRoot.zoneMinimumWidth

        model: bottomRoot.game_ready() ? game.selected_units_model : null
        groups: bottomRoot.selection_groups
        unitCount: bottomRoot.selection_count
        inspected: bottomRoot.game_ready() && game.activity ? game.activity.inspect_target : null
        profileLookup: bottomRoot.game_ready() && game.activity && game.activity.unit_profile ? game.activity.unit_profile : null
        onProfileRequested: function (unitType, nation) {
            bottomRoot.unit_profile_requested(unitType, nation, true);
        }
        onUnitActivated: function (unitId) {
            if (bottomRoot.game_ready() && game.orders.select_unit_by_id)
                game.orders.select_unit_by_id(unitId);
        }
        onGroupActivated: function (unitType) {
            if (bottomRoot.game_ready() && game.orders.select_by_type)
                game.orders.select_by_type(unitType);
        }
    }

    ColumnLayout {
        id: commandDeck

        readonly property int commandColumns: Math.max(bottomRoot.primaryCommands.length, bottomRoot.contextualCommands.length)
        readonly property int commandCardWidth: Math.max(Design.A11y.scaled(24), Math.floor((bottomRoot.zoneWidth - Design.Metrics.space4 * (commandDeck.commandColumns - 1)) / commandDeck.commandColumns))
        readonly property int primaryColumns: Math.max(1, bottomRoot.primaryCommands.length)
        readonly property int primaryCardWidth: Math.max(Design.A11y.scaled(24), Math.floor((bottomRoot.zoneWidth - Design.Metrics.space4 * (commandDeck.primaryColumns - 1)) / commandDeck.primaryColumns))

        objectName: "commandDeck"

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: bottomRoot.zoneWidth
        Layout.minimumWidth: bottomRoot.zoneMinimumWidth
        spacing: Design.Metrics.space4

        Design.IronPanel {
            id: commandBanner

            readonly property var target: bottomRoot.game_ready() && game.activity ? game.activity.selection_target : null
            readonly property bool showTarget: bottomRoot.selection_count > 0 && bottomRoot.current_command_mode === "normal" && !!target && target.valid === true

            Layout.fillWidth: true
            objectName: "commandModeBanner"

            Layout.preferredHeight: Design.Metrics.controlHeight
            contentPadding: 0
            raised: bottomRoot.current_command_mode !== "normal"
            border.color: showTarget ? Design.Theme.danger : bottomRoot.banner_tone()
            opacity: bottomRoot.has_movable_units || commandBanner.showTarget ? 1 : 0.6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Design.Metrics.space12
                anchors.rightMargin: Design.Metrics.space12
                visible: !commandBanner.showTarget
                spacing: Design.Metrics.space8

                Text {
                    text: qsTr("ORDERS")
                    color: bottomRoot.current_command_mode === "normal" ? Design.Theme.textSecondary : bottomRoot.banner_tone()
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }

                Rectangle {
                    Layout.preferredWidth: Design.Metrics.borderThin
                    Layout.preferredHeight: Design.Metrics.space16
                    color: bottomRoot.banner_tone()
                }

                Item {
                    Layout.preferredWidth: Design.Metrics.iconMedium
                    Layout.preferredHeight: Design.Metrics.iconMedium
                    visible: bottomRoot.current_command_mode !== "normal"

                    Image {
                        id: commandModeArt

                        anchors.fill: parent
                        source: Design.Icons.command(bottomRoot.current_command_mode)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: status === Image.Ready
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !commandModeArt.visible
                        text: Design.Icons.commandGlyph(bottomRoot.current_command_mode)
                        color: bottomRoot.banner_tone()
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.subheading
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: bottomRoot.command_banner_text()
                    color: bottomRoot.has_movable_units ? Design.Theme.textPrimary : Design.Theme.textDisabled
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: bottomRoot.current_command_mode === "normal" ? Design.Typography.medium : Design.Typography.bold
                    elide: Text.ElideRight
                }

                Text {
                    visible: bottomRoot.current_command_mode !== "normal"
                    text: qsTr("Right-click to cancel")
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }
            }

            RowLayout {
                objectName: "commandBannerTarget"
                anchors.fill: parent
                anchors.leftMargin: Design.Metrics.space8
                anchors.rightMargin: Design.Metrics.space8
                visible: commandBanner.showTarget
                spacing: Design.Metrics.space8

                Text {
                    text: qsTr("TARGET")
                    color: Design.Theme.danger
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }

                Text {
                    Layout.fillWidth: true
                    text: commandBanner.showTarget ? commandBanner.target.name : ""
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                    elide: Text.ElideRight
                }

                Text {
                    visible: commandBanner.showTarget && commandBanner.target.attackedBySelection > 0
                    text: commandBanner.showTarget ? qsTr("%n attacking", "", commandBanner.target.attackedBySelection) : ""
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }

                Design.IronProgressBar {
                    Layout.preferredWidth: Math.max(Design.A11y.scaled(56), commandBanner.width * 0.2)
                    Layout.preferredHeight: Design.Metrics.space4
                    value: commandBanner.showTarget ? commandBanner.target.healthRatio : 0
                    fillColor: Design.Theme.danger
                }

                Text {
                    text: commandBanner.showTarget ? qsTr("%1%").arg(Math.round(commandBanner.target.healthRatio * 100)) : ""
                    color: Design.Theme.danger
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                }
            }
        }

        Row {
            id: primaryCommandRow

            objectName: "primaryCommandRow"
            Layout.fillWidth: true
            Layout.preferredHeight: Design.Metrics.commandButtonSize
            spacing: Design.Metrics.space4

            Repeater {
                model: bottomRoot.primaryCommands

                delegate: Design.IronCommandButton {
                    id: primaryCommandButton

                    required property var modelData
                    readonly property var state: bottomRoot.action_state(primaryCommandButton.modelData.id)

                    objectName: "primaryCommand_" + primaryCommandButton.modelData.id
                    width: commandDeck.primaryCardWidth
                    height: Design.Metrics.commandButtonSize
                    iconOnly: false

                    actionId: primaryCommandButton.modelData.id
                    label: primaryCommandButton.modelData.label
                    shortLabel: primaryCommandButton.modelData.shortLabel || ""
                    hotkey: bottomRoot.hotkey_for(primaryCommandButton.modelData)
                    hint: primaryCommandButton.modelData.hint || ""
                    details: bottomRoot.command_details(primaryCommandButton.modelData, primaryCommandButton.state)
                    statusText: bottomRoot.command_status(primaryCommandButton.modelData, primaryCommandButton.state)
                    cooldown: bottomRoot.command_cooldown(primaryCommandButton.modelData, primaryCommandButton.state)
                    disabledReason: bottomRoot.unavailable_reason(primaryCommandButton.modelData, primaryCommandButton.state)

                    blocked: (primaryCommandButton.modelData.needsTroops && !bottomRoot.has_movable_units) || (!primaryCommandButton.modelData.ignoreActionState && !primaryCommandButton.state.enabled)
                    active: primaryCommandButton.modelData.activeFromPlacing ? primaryCommandButton.state.placing : (primaryCommandButton.modelData.mode ? (bottomRoot.current_command_mode === primaryCommandButton.modelData.mode && bottomRoot.has_movable_units) : primaryCommandButton.state.active)
                    mixed: primaryCommandButton.state.mixed
                    placing: primaryCommandButton.state.placing
                    eligibleCount: primaryCommandButton.state.eligibleCount
                    activeCount: primaryCommandButton.state.activeCount
                    spotlit: bottomRoot.tutorial_spotlights(primaryCommandButton.modelData.id)

                    onClicked: bottomRoot.invoke_command(primaryCommandButton.modelData)
                }
            }
        }

        Item {
            id: contextualCommandArea

            objectName: "contextualCommandArea"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Design.Metrics.commandButtonSize

            Row {
                id: contextualCommandRow

                objectName: "contextualCommandRow"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: Design.Metrics.commandButtonSize
                spacing: Design.Metrics.space4
                visible: bottomRoot.contextualCommands.length > 0

                Repeater {
                    model: bottomRoot.contextualCommands

                    delegate: Design.IronCommandButton {
                        id: contextualCommandButton

                        required property var modelData
                        readonly property var state: bottomRoot.action_state(contextualCommandButton.modelData.id)

                        objectName: "contextCommand_" + contextualCommandButton.modelData.id
                        width: commandDeck.commandCardWidth
                        height: Design.Metrics.commandButtonSize
                        iconOnly: false

                        actionId: contextualCommandButton.modelData.id
                        label: contextualCommandButton.modelData.label
                        shortLabel: contextualCommandButton.modelData.shortLabel || ""
                        hotkey: bottomRoot.hotkey_for(contextualCommandButton.modelData)
                        hint: contextualCommandButton.modelData.hint || ""
                        details: bottomRoot.command_details(contextualCommandButton.modelData, contextualCommandButton.state)
                        statusText: contextualCommandButton.state.passive ? qsTr("Automatic") : bottomRoot.command_status(contextualCommandButton.modelData, contextualCommandButton.state)
                        cooldown: bottomRoot.command_cooldown(contextualCommandButton.modelData, contextualCommandButton.state)
                        disabledReason: bottomRoot.unavailable_reason(contextualCommandButton.modelData, contextualCommandButton.state)

                        blocked: contextualCommandButton.state.passive || (contextualCommandButton.modelData.needsTroops && !bottomRoot.has_movable_units) || (!contextualCommandButton.modelData.ignoreActionState && !contextualCommandButton.state.enabled)
                        active: contextualCommandButton.modelData.activeFromPlacing ? contextualCommandButton.state.placing : (contextualCommandButton.modelData.mode ? (bottomRoot.current_command_mode === contextualCommandButton.modelData.mode && bottomRoot.has_movable_units) : contextualCommandButton.state.active)
                        mixed: contextualCommandButton.state.mixed
                        placing: contextualCommandButton.state.placing
                        eligibleCount: contextualCommandButton.state.eligibleCount
                        activeCount: contextualCommandButton.state.activeCount
                        spotlit: bottomRoot.tutorial_spotlights(contextualCommandButton.modelData.id)

                        onClicked: bottomRoot.invoke_command(contextualCommandButton.modelData)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: bottomRoot.contextualCommands.length === 0
                text: bottomRoot.selection_count > 0 ? qsTr("No specialist actions for this selection") : qsTr("Select troops to reveal their specialist actions")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }
        }
    }

    ProductionPanel {
        objectName: "productionZone"

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: bottomRoot.zoneWidth
        Layout.minimumWidth: bottomRoot.zoneMinimumWidth

        Design.IronSpotlight {
            active: bottomRoot.tutorialFocusRegion === "production"
            inset: -Design.Metrics.space4
            cornerRadius: Design.Metrics.radiusMedium
        }
        selection_tick: bottomRoot.selection_tick
        production: bottomRoot.game_ready() ? game.production : null
        placement: bottomRoot.game_ready() ? game.placement : null
        player_state: bottomRoot.game_ready() ? game.selected_player_state : null
        onRecruit_unit: function (unit_type) {
            bottomRoot.recruit_unit(unit_type);
        }
        onUnit_details_requested: function (unit_type, nation) {
            bottomRoot.unit_profile_requested(unit_type, nation, false);
        }
        onRally_mode_toggled: {
            if (!bottomRoot.game_ready() || typeof gameView === 'undefined')
                return;
            if (gameView.cursor_mode === "place_barracks_rally") {
                if (game.commander.cancel_barracks_rally)
                    game.commander.cancel_barracks_rally();
            } else if (game.commander.begin_barracks_rally) {
                game.commander.begin_barracks_rally();
            }
        }
        onBuild_tower: {
            if (bottomRoot.game_ready() && game.placement.start_building_placement)
                game.placement.start_building_placement("defense_tower");
        }
        onBuilder_construction: function (item_type) {
            if (bottomRoot.game_ready() && game.placement.start_builder_construction)
                game.placement.start_builder_construction(item_type);
        }
    }
}

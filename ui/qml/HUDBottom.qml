import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

RowLayout {
    id: bottomRoot

    property string current_command_mode
    property int selection_tick
    property bool has_movable_units
    property var action_states: ({})
    property var selection_groups: []
    property int selection_count: 0

    readonly property int orderGridHeight: Math.max(Design.Metrics.commandButtonSize, bottomRoot.height - Design.Metrics.controlHeight - Design.Metrics.space8)

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
            "readyCount": 0
        };
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
        var labels = ({
                "attack": qsTr("Attack order"),
                "guard": qsTr("Guard order"),
                "patrol": qsTr("Patrol order"),
                "heal": qsTr("Medic order"),
                "build": qsTr("Engineer order"),
                "repair": qsTr("Repair order"),
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
            "hint": qsTr("Attack enemy units or buildings. Only eligible selected troops receive the order."),
            "unavailable": qsTr("Attack is not available for the current selection")
        }, {
            "id": "guard",
            "label": qsTr("Guard"),
            "binding": "rts.order_guard",
            "needsTroops": true,
            "mode": "guard",
            "hint": qsTr("Guard a position. Only guard-capable troops receive the order."),
            "unavailable": qsTr("Guard is not available for the current selection")
        }, {
            "id": "patrol",
            "label": qsTr("Patrol"),
            "binding": "rts.order_patrol",
            "needsTroops": true,
            "mode": "patrol",
            "hint": qsTr("Patrol between waypoints."),
            "unavailable": qsTr("Patrol is not available for the current selection")
        }, {
            "id": "heal",
            "label": qsTr("Medic"),
            "needsTroops": true,
            "alwaysDisabled": true,
            "hint": qsTr("Healers tend nearby allies on their own."),
            "passiveReason": qsTr("Healers automatically heal nearby allies. This is passive, not a manual order."),
            "unavailable": qsTr("Select healer units")
        }, {
            "id": "stop",
            "label": qsTr("Stop"),
            "binding": "rts.order_stop",
            "needsTroops": true,
            "ignoreActionState": true,
            "hint": qsTr("Stop all actions immediately."),
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
            "hint": qsTr("Hold position and defend."),
            "unavailable": qsTr("Hold is not available for the current selection"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.orders.hold)
                    game.orders.hold();
            }
        }, {
            "id": "build",
            "label": qsTr("Build"),
            "needsTroops": true,
            "mode": "build",
            "hint": qsTr("Open builder orders and place structures."),
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
            "hint": qsTr("Click a tree, boulder or ore deposit. Right-click to cancel."),
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
            "needsTroops": true,
            "hint": qsTr("Builders keep finding and collecting the nearest resource on their own. Any new order cancels it."),
            "unavailable": qsTr("Auto Gather is only available to builders"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity) {
                    game.activity.toggle_auto_gather();
                    bottomRoot.update_action_states();
                }
            }
        }, {
            "id": "repair",
            "label": qsTr("Repair"),
            "needsTroops": true,
            "activeFromPlacing": true,
            "hint": qsTr("Click a damaged building of yours. Right-click to cancel."),
            "unavailable": qsTr("Repair is only available to builders"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.activity)
                    game.activity.begin_repair_order();
            }
        }, {
            "id": "deliver",
            "label": qsTr("Deliver"),
            "needsTroops": true,
            "mode": "deliver",
            "hint": qsTr("Click a friendly barracks. Only selected civilians receive the order."),
            "unavailable": qsTr("Deliver is only available to civilians")
        }, {
            "id": "rally",
            "label": qsTr("Rally"),
            "binding": "rts.commander_rally",
            "hint": qsTr("The commander plants a rally flag; troops march to it once placed."),
            "unavailable": qsTr("Select a commander to use rally"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.commander.start_flag_rally)
                    game.commander.start_flag_rally();
            }
        }, {
            "id": "gate",
            "label": qsTr("Gate"),
            "hint": qsTr("Cycle the selected gates: automatic, held open, held shut."),
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
            "hint": qsTr("Temporarily empower nearby troops. A glow marks every affected soldier."),
            "unavailable": qsTr("Select a ready commander to activate the aura"),
            "invoke": function () {
                if (bottomRoot.game_ready() && game.commander.trigger_aura) {
                    game.commander.trigger_aura();
                    bottomRoot.update_action_states();
                }
            }
        }]

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
    anchors.margins: Design.Metrics.space8
    spacing: Design.Metrics.space12

    Connections {
        function onCursor_mode_changed() {
            bottomRoot.update_action_states();
        }

        function onSelected_units_changed() {
            bottomRoot.refresh_selection();
        }

        target: bottomRoot.game_ready() ? game : null
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
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(240, bottomRoot.width * 0.28)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop

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
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(340, bottomRoot.width * 0.4)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        spacing: Design.Metrics.space8

        Design.IronPanel {
            id: commandBanner

            readonly property var target: bottomRoot.game_ready() && game.activity ? game.activity.selection_target : null
            readonly property bool showTarget: bottomRoot.selection_count > 0 && bottomRoot.current_command_mode === "normal" && !!target && target.valid === true

            Layout.fillWidth: true
            Layout.preferredHeight: Design.Metrics.controlHeight
            raised: bottomRoot.current_command_mode !== "normal"
            border.color: showTarget ? Design.Theme.danger : bottomRoot.banner_tone()
            opacity: bottomRoot.has_movable_units || commandBanner.showTarget ? 1 : 0.6

            Text {
                anchors.centerIn: parent
                visible: !commandBanner.showTarget
                text: bottomRoot.command_banner_text()
                color: bottomRoot.has_movable_units ? Design.Theme.textPrimary : Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: bottomRoot.current_command_mode === "normal" ? Design.Typography.medium : Design.Typography.bold
            }

            RowLayout {
                objectName: "commandBannerTarget"
                anchors.fill: parent
                anchors.leftMargin: Design.Metrics.space12
                anchors.rightMargin: Design.Metrics.space12
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
                    font.pixelSize: Design.Typography.label
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
                    Layout.preferredWidth: Math.max(72, commandBanner.width * 0.22)
                    Layout.preferredHeight: Design.Metrics.space8
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

        GridLayout {
            id: orderGrid

            Layout.fillWidth: true

            readonly property int rowHeight: Design.Metrics.commandButtonSize + rowSpacing
            readonly property int maxRows: Math.max(1, Math.floor((bottomRoot.orderGridHeight + rowSpacing) / rowHeight))

            columns: Math.max(3, Math.ceil(bottomRoot.commands.length / maxRows))
            rowSpacing: Design.Metrics.space4
            columnSpacing: Design.Metrics.space4

            Repeater {
                model: bottomRoot.commands

                delegate: Design.IronCommandButton {
                    id: commandButton

                    required property var modelData

                    readonly property var state: bottomRoot.action_state(modelData.id)

                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.Metrics.commandButtonSize

                    actionId: modelData.id
                    label: modelData.label
                    hotkey: bottomRoot.hotkey_for(modelData)
                    hint: modelData.hint || ""
                    disabledReason: bottomRoot.unavailable_reason(modelData, state)

                    enabled: !modelData.alwaysDisabled
                    blocked: (modelData.needsTroops && !bottomRoot.has_movable_units) || (!modelData.ignoreActionState && !state.enabled)
                    active: modelData.activeFromPlacing ? state.placing : (modelData.mode ? (bottomRoot.current_command_mode === modelData.mode && bottomRoot.has_movable_units) : state.active)
                    mixed: state.mixed
                    placing: state.placing
                    eligibleCount: state.eligibleCount
                    activeCount: state.activeCount

                    onClicked: bottomRoot.invoke_command(modelData)
                }
            }
        }
    }

    ProductionPanel {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(280, bottomRoot.width * 0.32)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
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

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import QtQuick.Window 2.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

Item {
    id: hud

    property bool game_is_paused: false
    readonly property real current_speed: (typeof game !== 'undefined' && game && game.time_scale > 0) ? game.time_scale : 1
    property string current_command_mode: "normal"
    property int top_panel_height: topPanel.height
    property int bottom_panel_height: bottomPanel.height

    readonly property int left_stack_bottom: waveTracker.visible ? waveTracker.y + waveTracker.height : topPanel.height

    readonly property int right_stack_bottom: topPanel.height + Design.Metrics.space8 + (Design.Metrics.space24 * 8) + Design.Metrics.space8 + hudTop.minimapLegendHeight

    readonly property bool minimap_drag_active: hudTop.minimapDragActive

    readonly property bool commander_message_showing: commanderMessage.showing

    readonly property int right_column_bottom: commanderMessage.showing ? Math.round(commanderMessage.y + commanderMessage.height) : hud.right_stack_bottom

    property bool overlay_active: false

    property bool objectives_visible: false

    readonly property bool tutorial_active: typeof game !== 'undefined' && !!game.tutorial && game.tutorial.active

    readonly property Item keyboard_owner: (hudVictory.visible && !hudVictory.collapsed) ? hudVictory : null

    function toggle_objectives() {
        hud.objectives_visible = !hud.objectives_visible;
        if (hud.objectives_visible)
            Design.UiSound.panelOpen();
        else
            Design.UiSound.panelClose();
    }

    function right_stack_margin(card_height) {
        var preferred = hud.right_stack_bottom - topPanel.height;
        var latest = hud.height - topPanel.height - hud.bottom_panel_height - Design.Metrics.space8 - card_height;
        return Math.max(Design.Metrics.space8, Math.min(preferred, latest));
    }
    property int selection_tick: 0
    property bool has_movable_units: false
    property bool commander_rpg_mode: typeof game !== 'undefined' && game.commander.mode_state === "active"
    property var commander_status: ({})
    readonly property var economy: typeof game !== 'undefined' && game && game.economy ? game.economy : null
    property bool commander_rally_overlay_blocked: commander_rpg_mode && typeof game !== 'undefined' && (game.cursor_mode === "place_commander_rally" || game.cursor_mode === "place_barracks_rally")

    readonly property bool camera_legend_visible: Core.UiHints.showing["camera_legend"] === true

    function show_unit_profile(unit_type, nation, from_selection) {
        unitInspectPanel.show_availability = from_selection === true;
        unitInspectPanel.load(unit_type, nation);
        unitInspectPanel.visible = true;
    }

    signal pause_toggled
    signal speed_changed(real speed)
    signal camera_settings_requested
    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)
    signal return_to_main_menu_requested
    signal campaign_requested
    signal retry_requested
    signal hud_became_visible
    signal help_requested

    function item_covers_pointer(item, x, y) {
        if (!item || !item.visible || item.width <= 0 || item.height <= 0)
            return false;
        var local = hud.mapToItem(item, x, y);
        return local.x >= 0 && local.y >= 0 && local.x < item.width && local.y < item.height;
    }

    function blocks_edge_scroll(x, y) {
        if (!hud.visible)
            return false;
        var minimapZone = hudTop.minimapZone;
        return minimapZone.width > 0 && x >= minimapZone.x && y >= minimapZone.y && x < minimapZone.x + minimapZone.width && y < minimapZone.y + minimapZone.height;
    }

    function blocks_world_pointer(x, y) {
        if (!hud.visible)
            return false;
        if (y < hud.top_panel_height)
            return true;
        if (y > (hud.height - hud.bottom_panel_height))
            return true;
        if (hud.blocks_edge_scroll(x, y))
            return true;
        var floating = [cameraLegend, commanderMessage, waveTracker, economyCoach, objectivesCard, hudVictory.strip];
        for (var i = 0; i < floating.length; ++i) {
            if (hud.item_covers_pointer(floating[i], x, y))
                return true;
        }
        return false;
    }

    function refresh_command_mode() {
        var actual_mode = "normal";
        if (has_movable_units && typeof game !== 'undefined' && game.orders.command_mode)
            actual_mode = game.orders.command_mode();
        if (current_command_mode !== actual_mode)
            current_command_mode = actual_mode;
    }

    onVisibleChanged: {
        if (!visible)
            return;
        if (!hud.commander_rpg_mode)
            Core.UiHints.show_once("camera_legend");
        hud_became_visible();
    }

    Connections {
        function onSelected_units_changed() {
            selection_tick += 1;
            has_movable_units = typeof game !== 'undefined' && game.orders.has_commandable_selection ? game.orders.has_commandable_selection() : false;
            refresh_command_mode();
            Core.UiHints.on_selection_changed();
        }

        target: (typeof game !== 'undefined') ? game : null
    }

    Timer {
        id: productionRefresh

        interval: 100
        repeat: true
        running: true
        onTriggered: {
            selection_tick += 1;
            refresh_command_mode();
        }
    }

    Timer {
        id: commanderStatusPoll

        interval: 33
        repeat: true
        running: typeof game !== 'undefined' && game.commander.mode_state === "active" && !!game.commander.status
        triggeredOnStart: true
        onTriggered: hud.commander_status = game.commander.status()
    }

    QtObject {
        id: fpsMeter

        property int frames: 0
        property real fps: 0
    }

    Connections {
        target: Window.window ? Window.window : null
        function onFrameSwapped() {
            fpsMeter.frames += 1;
        }
    }

    Timer {
        id: fpsPoll

        interval: 500
        repeat: true
        running: visible && Core.UiPreferences.showFps && Window.window !== null
        triggeredOnStart: true
        onTriggered: {
            fpsMeter.fps = Math.round(fpsMeter.frames * (1000 / interval));
            fpsMeter.frames = 0;
            fpsReadout.text = qsTr("%1 FPS").arg(fpsMeter.fps);
        }
    }

    Text {
        id: fpsReadout

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.bottomMargin: hud.bottom_panel_height + Design.Metrics.space8
        visible: Core.UiPreferences.showFps
        text: ""
        color: Design.Theme.textSecondary
        font.family: "monospace"
        font.pixelSize: Design.Typography.caption
    }

    Item {
        id: topPanel

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(50, parent.height * 0.08)

        HUDTop {
            id: hudTop

            anchors.fill: parent
            game_is_paused: hud.game_is_paused
            current_speed: hud.current_speed
            onPause_toggled: {
                hud.pause_toggled();
            }
            onSpeed_changed: function (s) {
                hud.speed_changed(s);
            }
            onEconomy_help_requested: economyHelpPanel.visible = true
            onHelp_requested: hud.help_requested()
            objectives_visible: hud.objectives_visible
            onObjectives_toggled: hud.toggle_objectives()
            camera_legend_visible: hud.camera_legend_visible
            onCamera_legend_toggled: {
                if (hud.camera_legend_visible)
                    Core.UiHints.dismiss("camera_legend");
                else
                    Core.UiHints.reveal("camera_legend");
            }
        }
    }

    Item {
        id: bottomPanel

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        height: Design.Metrics.bottomBarHeight(parent.height, hud.commander_rpg_mode)
        clip: true

        Loader {
            id: bottomPanelLoader
            anchors.fill: parent
            sourceComponent: {
                if (typeof game === 'undefined')
                    return rtsBottomHudComponent;
                if (game.is_spectator_mode)
                    return spectatorBottomHudComponent;
                return game.commander.mode_state === "active" ? commanderBottomHudComponent : rtsBottomHudComponent;
            }
        }

        Component {
            id: spectatorBottomHudComponent

            HUDBottomSpectator {
                objectName: "spectatorBottomHud"
                anchors.fill: parent
                selection_tick: hud.selection_tick
                onFollow_requested: function (owner_id) {
                    if (typeof game !== 'undefined')
                        game.selected_player_id = owner_id;
                }
            }
        }

        Component {
            id: rtsBottomHudComponent

            HUDBottom {
                anchors.fill: parent
                current_command_mode: hud.current_command_mode
                selection_tick: hud.selection_tick
                has_movable_units: hud.has_movable_units
                onCommand_mode_changed: function (m) {
                    hud.current_command_mode = m;
                    hud.command_mode_changed(m);
                }
                onRecruit_unit: function (unit_type) {
                    hud.recruit_unit(unit_type);
                }
                onUnit_profile_requested: function (unit_type, nation, from_selection) {
                    hud.show_unit_profile(unit_type, nation, from_selection);
                }
            }
        }

        Component {
            id: commanderBottomHudComponent

            HUDBottomCommander {
                anchors.fill: parent
                external_status: hud.commander_status
            }
        }
    }

    WaveTracker {
        id: waveTracker

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.topMargin: Design.Metrics.space8
        anchors.leftMargin: Design.Metrics.hudZoneMargin

        visible: has_waves && !hud.commander_rpg_mode && !(typeof game !== 'undefined' && game.tutorial && game.tutorial.holds_mission_clock)
    }

    MissionDeadline {
        id: missionDeadline

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.topMargin: Design.Metrics.space8 + (waveTracker.visible ? waveTracker.height + Design.Metrics.space8 : 0)

        visible: has_deadline && !hud.commander_rpg_mode
    }

    EconomyCoach {
        id: economyCoach

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.topMargin: Design.Metrics.space8 + (waveTracker.visible ? waveTracker.height + Design.Metrics.space8 : 0) + (missionDeadline.visible ? missionDeadline.height + Design.Metrics.space8 : 0)

        economy: hud.economy
        gate: !hud.commander_rpg_mode && !!hud.economy && hud.economy.coach_visible
        onHelp_requested: economyHelpPanel.visible = true
    }

    CameraLegend {
        id: cameraLegend

        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: Design.Metrics.space12

        gate: !hud.commander_rpg_mode && !hud.overlay_active && !hud.tutorial_active
        onOpen_settings_requested: {
            cameraLegend.dismiss();
            hud.camera_settings_requested();
        }
    }

    UnitInspectPanel {
        id: unitInspectPanel

        anchors.fill: parent
        visible: false
        onVisibleChanged: {
            if (visible) {
                unitInspectPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
        }
        onClose_requested: unitInspectPanel.visible = false
    }

    EconomyHelpPanel {
        id: economyHelpPanel

        anchors.fill: parent
        economy: hud.economy
        visible: false
        onVisibleChanged: {
            if (visible) {
                economyHelpPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
        }
        onClose_requested: economyHelpPanel.visible = false
    }

    FormationPanel {
        id: formationPanel

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16

        max_height: Math.max(0, bottomPanel.y - (waveTracker.y + waveTracker.height) - Design.Metrics.space16)
        placing: typeof game !== 'undefined' && game.placement !== undefined && game.placement.is_placing_formation
        visible: placing && !hud.commander_rpg_mode
    }

    FormationStatusBadge {
        id: formationStatusBadge

        anchors.bottom: bottomPanel.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        gate: has_formation && any_selected && !formationPanel.placing && !hud.commander_rpg_mode
    }

    Design.IronPanel {
        id: objectivesCard

        property var mission_objectives: null

        readonly property var mission: (typeof game !== 'undefined' && game && game.mission) ? game.mission : null
        readonly property bool staged: objectivesCard.mission !== null && objectivesCard.mission.staged
        readonly property int max_height: Math.max(Design.Metrics.space24 * 6, bottomPanel.y - objectivesCard.y - Design.Metrics.space16 - (formationStatusBadge.visible ? formationStatusBadge.height + Design.Metrics.space12 : 0))
        readonly property int natural_height: objectivesHeader.implicitHeight + objectivesLayout.spacing + objectivesBody.contentHeight + objectivesCard.contentPadding * 2 + Design.Metrics.space4

        function refresh() {
            objectivesCard.mission_objectives = (typeof game !== 'undefined' && game && game.setup && game.setup.current_mission_objectives) ? game.setup.current_mission_objectives() : null;
        }

        function objective_list(key) {
            return (objectivesCard.mission_objectives && objectivesCard.mission_objectives[key]) ? objectivesCard.mission_objectives[key] : [];
        }

        function optional_objectives_with_progress() {
            var list = objectivesCard.objective_list("optional_objectives");
            var waves = (typeof game !== 'undefined' && game && game.waves) ? game.waves : null;
            var live = {};
            var tracked = objectivesCard.mission ? objectivesCard.mission.optional : [];
            for (var t = 0; t < tracked.length; ++t)
                live[tracked[t].index] = tracked[t];
            var out = [];
            for (var i = 0; i < list.length; ++i) {
                var entry = list[i];
                var row = {
                    "description": entry.description,
                    "state": "optional"
                };
                var status = live[i];
                if (status) {
                    if (status.detail)
                        row.detail = status.detail;
                    else if (status.required > 1)
                        row.detail = qsTr("%1 of %2").arg(status.progress).arg(status.required);
                    if (status.detail || status.required > 1)
                        row.progress = status.fraction;
                    if (status.complete)
                        row.state = "complete";
                }
                var wave_count = entry.wave_count !== undefined ? Number(entry.wave_count) : 0;
                if (waves && wave_count > 0) {
                    var cleared = Math.max(0, Math.min(wave_count, waves.cleared_phases || 0));
                    row.detail = qsTr("Waves %1 of %2").arg(cleared).arg(wave_count);
                    row.progress = cleared / wave_count;
                    if (cleared >= wave_count)
                        row.state = "complete";
                }
                out.push(row);
            }
            return out;
        }

        anchors.top: topPanel.bottom
        anchors.left: parent.left
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        anchors.topMargin: Design.Metrics.space8 + (waveTracker.visible ? waveTracker.height + Design.Metrics.space8 : 0) + (missionDeadline.visible ? missionDeadline.height + Design.Metrics.space8 : 0) + (economyCoach.visible ? economyCoach.height + Design.Metrics.space8 : 0)
        width: Math.max(Design.Metrics.space24 * 10, Math.min(Design.A11y.scaled(380), Math.round(hud.width * 0.36)))
        height: Math.min(objectivesCard.max_height, objectivesCard.natural_height)
        visible: hud.objectives_visible && !hud.commander_rpg_mode && !formationPanel.placing && !hud.tutorial_active && !(typeof game !== 'undefined' && game.is_spectator_mode)
        raised: true
        z: 50
        accessibleName: qsTr("Objectives")

        onVisibleChanged: {
            if (visible)
                objectivesCard.refresh();
        }

        Connections {
            function onCurrent_mission_changed() {
                objectivesCard.refresh();
            }

            ignoreUnknownSignals: true
            target: (typeof game !== 'undefined' && game && game.setup) ? game.setup : null
        }

        ColumnLayout {
            id: objectivesLayout

            anchors.fill: parent
            spacing: Design.Metrics.space8

            RowLayout {
                id: objectivesHeader

                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Text {
                    text: Design.Icons.briefing
                    color: Design.Theme.accent
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                }

                Text {
                    Layout.fillWidth: true
                    text: (objectivesCard.mission_objectives && objectivesCard.mission_objectives.title) ? objectivesCard.mission_objectives.title : qsTr("Objectives")
                    color: Design.Theme.textPrimary
                    elide: Text.ElideRight
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.subheading
                    font.weight: Design.Typography.bold
                }

                Design.IronIconButton {
                    iconText: Design.Icons.close
                    tooltip: qsTr("Hide the objectives (O)")
                    onClicked: hud.objectives_visible = false
                }
            }

            Design.BriefingLayout {
                id: objectivesBody

                Layout.fillWidth: true
                Layout.fillHeight: true

                factionId: typeof game !== 'undefined' && game ? game.local_player_nation : ""
                title: ""
                summary: ""
                victoryConditions: objectivesCard.objective_list("victory_conditions")
                defeatConditions: objectivesCard.objective_list("defeat_conditions")
                optionalObjectives: objectivesCard.optional_objectives_with_progress()
                stages: objectivesCard.staged ? objectivesCard.mission.stages : objectivesCard.objective_list("stages")
                stagesAreVictoryConditions: objectivesCard.staged && objectivesCard.mission.stages_mirror_victory_conditions
                victoryMode: (objectivesCard.mission_objectives && objectivesCard.mission_objectives.victory_mode) ? objectivesCard.mission_objectives.victory_mode : "any"
            }
        }
    }

    WorldProjector {
        id: worldProjector

        anchors.fill: parent
        camera: typeof game !== 'undefined' ? game.camera : null
        topInset: topPanel.height
        bottomInset: bottomPanel.height
        active: floatingNumbers.visible || rpgFpvOverlay.visible || tutorialFocusOverlay.visible
    }

    RpgFpvOverlay {
        id: rpgFpvOverlay
        anchors.fill: parent
        bottomInset: bottomPanel.height
        topInset: topPanel.height
        status: hud.commander_status
        projector: worldProjector
        visible: hud.commander_rpg_mode && !hud.commander_rally_overlay_blocked
    }

    TutorialFocusOverlay {
        id: tutorialFocusOverlay

        anchors.fill: parent
        projector: worldProjector
        points: (typeof game !== 'undefined' && game.tutorial && game.tutorial.active) ? game.tutorial.focus_points : []
        topInset: topPanel.height
        bottomInset: bottomPanel.height
        z: 1
        visible: !hud.commander_rpg_mode && points.length > 0
    }

    FloatingNumbers {
        id: floatingNumbers

        anchors.fill: parent
        source: typeof game !== 'undefined' ? game.activity : null
        projector: worldProjector
        combatEnabled: Design.A11y.damageNumbers
        economyEnabled: Design.A11y.economyNumbers && !hud.commander_rpg_mode
        visible: !hud.commander_rally_overlay_blocked
    }

    CommanderMessagePanel {
        id: commanderMessage

        anchors.right: parent.right
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.top: topPanel.bottom
        anchors.topMargin: hud.right_stack_margin(commanderMessage.height)

        z: 200
    }

    HUDVictory {
        id: hudVictory

        anchors.fill: parent
        strip_top_margin: topPanel.height + Design.Metrics.space8
        onReturn_to_main_menu_requested: {
            hud.return_to_main_menu_requested();
        }
        onCampaign_requested: {
            hud.campaign_requested();
        }
        onRetry_requested: {
            hud.retry_requested();
        }

        Connections {
            function onHud_became_visible() {
                if (typeof game !== 'undefined' && game.victory_state === "")
                    hudVictory.force_hide();
            }

            target: hud
        }
    }
}

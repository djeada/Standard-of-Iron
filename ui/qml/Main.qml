import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

ApplicationWindow {
    id: mainWindow

    property alias game_view: gameViewItem
    property bool menu_visible: true
    property bool game_started: false
    property bool game_paused: false
    readonly property bool edge_scroll_disabled: gameViewItem.camera_pan_active || !mainWindow.active

    property bool suppress_modals: false

    readonly property bool overlay_active: mainWindow.menu_visible || mapSelect.visible || missions_screen.visible || campaign_screen.visible || save_game_panel.visible || load_game_panel.visible || settingsPanel.visible || objectivesPanel.visible || help_panel.visible || commander_preview.visible
    readonly property bool simulation_suspended: mainWindow.game_started && (mainWindow.game_paused || mainWindow.overlay_active)

    property bool capture_view_ready: false
    property bool capture_view_settled: false
    property bool capture_tutorial_requested: false

    function show_view(name) {
        mainWindow.suppress_modals = true;
        if (typeof game !== 'undefined' && game.clear_error)
            game.clear_error();
        error_dialog.close();
        mainWindow.menu_visible = (name === "menu");
        mapSelect.visible = (name === "skirmish");
        missions_screen.visible = (name === "missions");
        campaign_screen.visible = (name === "campaign");
        settingsPanel.visible = (name === "settings");
        load_game_panel.visible = (name === "load");
        save_game_panel.visible = (name === "save");
        objectivesPanel.visible = (name === "briefing");
        help_panel.visible = (name === "help");
        commander_preview.visible = (name === "commander");
        if (name === "hud" || name === "rpg" || name === "tutorial") {
            mainWindow.game_started = true;
            mainWindow.menu_visible = false;
        }
        if (name === "tutorial") {
            var tutorial = (typeof game !== 'undefined') ? game.tutorial : null;
            if (tutorial && !mainWindow.capture_tutorial_requested) {
                mainWindow.capture_tutorial_requested = true;
                mainWindow.start_tutorial();
            }
            mainWindow.capture_view_ready = !!tutorial && tutorial.active && !game.is_loading;
            mainWindow.sync_audio_context();
            return;
        }
        if (name === "rpg") {
            mainWindow.game_paused = false;
            if (typeof game !== 'undefined' && game.commander.mode_state !== "active" && game.commander.toggle_mode)
                game.commander.toggle_mode();
            mainWindow.capture_view_ready = typeof game !== 'undefined' && !game.is_loading && game.commander.mode_state === "active";
        } else if (name === "hud") {
            mainWindow.capture_view_ready = typeof game !== 'undefined' && !game.is_loading;
        } else {
            mainWindow.capture_view_ready = true;
        }
        mainWindow.sync_audio_context();
    }

    function start_tutorial() {
        if (typeof game === 'undefined' || !game.tutorial || !game.tutorial.start)
            return;
        game.tutorial.start();
        help_panel.visible = false;
        mainWindow.menu_visible = false;
        mainWindow.game_started = true;
        mainWindow.game_paused = false;
        gameViewItem.forceActiveFocus();
    }

    function open_help(from_menu) {
        help_panel.from_menu = from_menu;
        help_panel.visible = true;
        if (from_menu)
            mainWindow.menu_visible = false;
    }

    function return_to_main_menu() {
        mainWindow.menu_visible = true;
        mainMenu.forceActiveFocus();
    }

    function request_menu_toggle() {
        if (typeof game !== 'undefined' && game.placement) {
            if (game.placement.is_placing_construction && game.placement.on_construction_cancel) {
                game.placement.on_construction_cancel();
                return true;
            }
            if (game.placement.is_placing_formation && game.placement.on_formation_cancel) {
                game.placement.on_formation_cancel();
                return true;
            }
        }
        if (gameViewItem.is_rally_placement && gameViewItem.is_rally_placement()) {
            gameViewItem.cancel_rally_placement();
            return true;
        }
        if (mainWindow.menu_visible) {
            if (mainWindow.game_started)
                mainWindow.menu_visible = false;
            return true;
        }
        if (mainWindow.overlay_active)
            return false;
        mainWindow.return_to_main_menu();
        return true;
    }

    function note_objectives_opened() {
        if (typeof game !== 'undefined' && game.tutorial && game.tutorial.note_objectives_opened)
            game.tutorial.note_objectives_opened();
    }

    function sync_audio_context() {
        if (typeof game === 'undefined' || !game.set_audio_frontend_context)
            return;
        if (campaign_screen.visible) {
            game.set_audio_frontend_context("campaign");
        } else if (mainWindow.menu_visible || mapSelect.visible || missions_screen.visible || (!mainWindow.game_started && (save_game_panel.visible || load_game_panel.visible || settingsPanel.visible || objectivesPanel.visible || help_panel.visible))) {
            game.set_audio_frontend_context("menu");
        } else {
            game.set_audio_frontend_context("battle");
        }
    }

    width: 1280
    height: 720
    readonly property string window_mode: UiPreferences.displayWindowMode
    flags: window_mode === "borderless" ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    visibility: {
        if (window_mode === "windowed")
            return Window.Windowed;
        if (window_mode === "borderless")
            return Window.Maximized;
        return Window.FullScreen;
    }
    visible: true
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true
    title: qsTr("Standard of Iron - RTS game")
    color: Theme.bg
    Component.onCompleted: {
        Design.UiSound.audioSystem = (typeof game !== 'undefined') ? game.audio_system : null;
        sync_audio_context();
    }

    onMenu_visibleChanged: {
        if (menu_visible)
            Design.Notifications.clear();
    }

    onSimulation_suspendedChanged: {
        gameViewItem.set_paused(mainWindow.simulation_suspended);
    }

    Design.GameShell {
        anchors.fill: parent
        z: -10

        faction: mainWindow.game_started && typeof game !== 'undefined' ? game.local_player_nation : ""
    }

    GameView {
        id: gameViewItem

        anchors.fill: parent
        z: 0
        focus: !mainWindow.overlay_active
        visible: game_started
    }

    HUD {
        id: hud

        anchors.fill: parent
        z: 1
        visible: !mainWindow.menu_visible && game_started
        overlay_active: mainWindow.overlay_active
        onActiveFocusChanged: {
            if (activeFocus)
                gameViewItem.forceActiveFocus();
        }
        onPause_toggled: {
            mainWindow.game_paused = !mainWindow.game_paused;
            gameViewItem.forceActiveFocus();
        }
        onSpeed_changed: function (speed) {
            gameViewItem.set_game_speed(speed);
            gameViewItem.forceActiveFocus();
        }
        onCommand_mode_changed: function (mode) {
            console.log("Main: Command mode changed to:", mode);
            if (typeof game !== 'undefined') {
                console.log("Main: Setting game.cursor_mode property to", mode);
                game.cursor_mode = mode;
            } else {
                console.log("Main: game is undefined");
            }
            gameViewItem.forceActiveFocus();
        }
        onRecruit_unit: function (unit_type) {
            if (typeof game !== 'undefined' && game.production.recruit_near_selected)
                game.production.recruit_near_selected(unit_type);
            gameViewItem.forceActiveFocus();
        }
        onReturn_to_main_menu_requested: {
            mainWindow.return_to_main_menu();
        }
        onCampaign_requested: {
            mainWindow.menu_visible = false;
            campaign_screen.visible = true;
            mainWindow.sync_audio_context();
        }
        onHelp_requested: mainWindow.open_help(false)
        onCamera_settings_requested: mainWindow.show_view("settings")
    }

    MouseArea {
        id: commanderCursorOverlay

        anchors.fill: parent
        z: 11
        visible: game_started && !mainWindow.menu_visible && typeof game !== 'undefined' && game.commander.mode_state === "active" && game.cursor_mode !== "place_commander_rally" && game.cursor_mode !== "place_barracks_rally" && !mapSelect.visible && !missions_screen.visible && !campaign_screen.visible && !save_game_panel.visible && !load_game_panel.visible && !settingsPanel.visible && !objectivesPanel.visible && !error_dialog.visible
        enabled: visible
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        propagateComposedEvents: true
        preventStealing: false
        cursorShape: Qt.BlankCursor
    }

    TutorialOverlay {
        id: tutorial_overlay

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: hud.left_stack_bottom + Design.Metrics.space8
        anchors.leftMargin: Design.Metrics.hudZoneMargin
        max_height: Math.max(Design.Metrics.space24 * 6, hud.height - hud.bottom_panel_height - anchors.topMargin - Design.Metrics.space12)
        z: 10.5
        visible: active && mainWindow.game_started && !mainWindow.overlay_active && !hud.commander_rpg_mode
        game_is_paused: mainWindow.game_paused
        onPause_requested: {
            mainWindow.game_paused = !mainWindow.game_paused;
            hud.game_is_paused = mainWindow.game_paused;
            gameViewItem.forceActiveFocus();
        }
        onHelp_requested: mainWindow.open_help(false)
    }

    Design.NotificationHost {
        id: notification_host

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: hud.visible ? hud.right_stack_bottom + Design.Metrics.space8 : Design.Metrics.space24 * 3
        anchors.rightMargin: Design.Metrics.space16
        z: 12
        visible: mainWindow.game_started && !mainWindow.menu_visible
    }

    Rectangle {
        id: pause_overlay

        anchors.fill: parent
        z: 10
        visible: mainWindow.game_paused && game_started
        color: Qt.rgba(8 / 255, 6 / 255, 4 / 255, 0.72)

        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 150
            color: Theme.panelBase
            radius: 8
            border.color: Theme.panelBr
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: qsTr("PAUSED")
                    color: Theme.textMain
                    font.pixelSize: Design.Typography.hero
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: qsTr("Press %1 to resume").arg(InputBindings.display_shortcut_for("rts.pause"))
                    color: Theme.textSubLite
                    font.pixelSize: Design.Typography.label
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    LoadScreen {
        id: load_screen

        anchors.fill: parent
        z: 15
        is_loading: (typeof game !== 'undefined') ? game.is_loading : false
        progress: (typeof game !== 'undefined') ? game.loading_progress : 0
        stage_text: (typeof game !== 'undefined') ? game.loading_stage_text : "Loading..."

        Connections {
            function onIs_loading_changed() {
                if (!game.is_loading)
                    load_screen.complete_loading();
            }

            target: game
        }
    }

    MainMenu {
        id: mainMenu

        anchors.fill: parent
        z: 20
        visible: mainWindow.menu_visible
        game_started: mainWindow.game_started
        Component.onCompleted: {
            if (mainWindow.menu_visible)
                mainMenu.forceActiveFocus();
        }
        onVisibleChanged: {
            if (visible)
                mainMenu.forceActiveFocus();
            else if (mainMenu.game_started && !mainWindow.overlay_active)
                gameViewItem.forceActiveFocus();
            mainWindow.sync_audio_context();
        }
        onResume_requested: function () {
            if (mainWindow.game_started)
                mainWindow.menu_visible = false;
        }
        onOpen_skirmish: function () {
            mapSelect.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_missions: function () {
            missions_screen.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_campaign: function () {
            campaign_screen.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_tutorial: function () {
            mainWindow.start_tutorial();
        }
        onOpen_help: function () {
            mainWindow.open_help(true);
        }
        onSave_game: function () {
            if (mainWindow.game_started) {
                save_game_panel.visible = true;
                mainWindow.menu_visible = false;
            }
        }
        onLoad_save: function () {
            load_game_panel.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_settings: function () {
            settingsPanel.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_objectives: function () {
            objectivesPanel.visible = true;
            mainWindow.menu_visible = false;
            mainWindow.note_objectives_opened();
        }
        onExit_requested: function () {
            if (typeof game !== 'undefined' && game.exit_game)
                game.exit_game();
        }
    }

    MapSelect {
        id: mapSelect

        anchors.fill: parent
        z: 21
        visible: false
        onVisibleChanged: {
            if (visible) {
                mapSelect.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onMap_chosen: function (map_path, player_configs) {
            console.log("Main: onMap_chosen received", map_path, "with", player_configs.length, "player configs");
            if (typeof game !== 'undefined' && game.setup.start_skirmish)
                game.setup.start_skirmish(map_path, player_configs);
            mapSelect.visible = false;
            mainWindow.menu_visible = false;
            mainWindow.game_started = true;
            mainWindow.game_paused = false;
            gameViewItem.forceActiveFocus();
        }
        onObserve_requested: function (map_path) {
            if (typeof game === 'undefined' || !game.setup.start_observed_skirmish)
                return;
            if (!game.setup.start_observed_skirmish(map_path))
                return;
            mapSelect.visible = false;
            mainWindow.menu_visible = false;
            mainWindow.game_started = true;
            mainWindow.game_paused = false;
            gameViewItem.forceActiveFocus();
        }
        onCancelled: function () {
            Design.UiSound.back();
            mapSelect.visible = false;
            mainWindow.return_to_main_menu();
        }
    }

    CampaignScreen {
        id: campaign_screen

        anchors.fill: parent
        z: 21
        visible: false
        onVisibleChanged: {
            if (visible) {
                campaign_screen.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onMission_selected: function (campaign_id, mission_id) {
            console.log("Main: Campaign mission selected:", campaign_id + "/" + mission_id);
            if (typeof game !== 'undefined' && game.setup.start_campaign_mission) {
                game.setup.start_campaign_mission(campaign_id + "/" + mission_id);
                campaign_screen.visible = false;
                mainWindow.menu_visible = false;
                mainWindow.game_started = true;
                mainWindow.game_paused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function () {
            Design.UiSound.back();
            campaign_screen.visible = false;
            mainWindow.return_to_main_menu();
        }
    }

    MissionsScreen {
        id: missions_screen

        anchors.fill: parent
        z: 21
        visible: false
        onVisibleChanged: {
            if (visible) {
                missions_screen.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onMission_chosen: function (file_path) {
            if (typeof game === 'undefined' || !game.setup.start_mission_file)
                return;
            game.setup.start_mission_file(file_path);
            missions_screen.visible = false;
            mainWindow.menu_visible = false;
            mainWindow.game_started = true;
            mainWindow.game_paused = false;
            gameViewItem.forceActiveFocus();
        }
        onCancelled: function () {
            Design.UiSound.back();
            missions_screen.visible = false;
            mainWindow.return_to_main_menu();
        }
    }

    SaveGamePanel {
        id: save_game_panel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                save_game_panel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onSave_requested: function (slot_name) {
            console.log("Main: Save requested for slot:", slot_name);
            if (typeof game !== 'undefined' && game.saves.save_to_slot)
                game.saves.save_to_slot(slot_name);
            save_game_panel.visible = false;
            mainWindow.return_to_main_menu();
        }
        onCancelled: function () {
            Design.UiSound.back();
            save_game_panel.visible = false;
            mainWindow.return_to_main_menu();
        }
    }

    LoadGamePanel {
        id: load_game_panel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                load_game_panel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onLoad_requested: function (slot_name) {
            console.log("Main: Load requested for slot:", slot_name);
            if (typeof game !== 'undefined' && game.saves.load_from_slot) {
                game.saves.load_from_slot(slot_name);
                load_game_panel.visible = false;
                mainWindow.menu_visible = false;
                mainWindow.game_started = true;
                mainWindow.game_paused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function () {
            Design.UiSound.back();
            load_game_panel.visible = false;
            mainWindow.return_to_main_menu();
        }
    }

    SettingsPanel {
        id: settingsPanel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                settingsPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onCancelled: function () {
            Design.UiSound.back();
            mainWindow.return_to_main_menu();
            settingsPanel.visible = false;
        }
    }

    ObjectivesPanel {
        id: objectivesPanel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                objectivesPanel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onClose_requested: function () {
            Design.UiSound.back();
            objectivesPanel.visible = false;
            if (typeof game !== 'undefined' && typeof game.setup.is_mission_match !== 'undefined' && game.setup.is_mission_match && mainWindow.game_started) {
                mainWindow.game_paused = false;
                gameViewItem.forceActiveFocus();
            } else {
                mainWindow.return_to_main_menu();
            }
        }
    }

    HelpPanel {
        id: help_panel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                help_panel.forceActiveFocus();
                Design.UiSound.panelOpen();
            } else {
                Design.UiSound.panelClose();
            }
            mainWindow.sync_audio_context();
        }
        onStart_tutorial_requested: mainWindow.start_tutorial()
        onClose_requested: {
            Design.UiSound.back();
            help_panel.visible = false;
            if (help_panel.from_menu || !mainWindow.game_started) {
                mainWindow.return_to_main_menu();
                return;
            }
            gameViewItem.forceActiveFocus();
        }
    }

    Item {
        id: commander_preview

        anchors.fill: parent
        z: 40
        visible: false

        readonly property QtObject stub: QtObject {
            property bool active: true
            property string message_id: "preview"
            property string speaker_name: "Marcus Claudius Marcellus"
            property string speaker_role: "Roman field commander"
            property string nation: "roman_republic"
            property string speaker_id: "roman_field_commander"
            property string pose: "dismissive"
            property string text: qsTr("A new standard in the valley, and nobody under it who has held a spear more than twice.")
            property real duration: 9.0
            property bool holds_outcome: false

            function dismiss() {
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Design.Theme.backgroundDeep
        }

        CommanderMessagePanel {
            anchors.centerIn: parent
            scale: 2.0
            source: commander_preview.stub
        }
    }

    SaveProgressOverlay {
        id: save_progress_overlay

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: hud.visible ? hud.bottom_panel_height : 0
        z: 30
    }

    ProfilingOverlay {
        id: profiling_overlay

        anchors.fill: parent
        z: 40
    }

    Item {
        id: edge_scroll_overlay

        readonly property real horz_threshold: EdgeScroll.horizontalZone(Design.A11y.edgeScrollSensitivity, Design.A11y.uiScale)
        readonly property real vert_threshold: EdgeScroll.verticalZone(Design.A11y.edgeScrollSensitivity, Design.A11y.uiScale)
        property real x_pos: -1
        property real y_pos: -1

        function in_hud_zone(x, y) {
            return hud.blocks_world_pointer(x, y);
        }

        anchors.fill: parent
        z: 0.5
        visible: !mainWindow.menu_visible && !mapSelect.visible && !missions_screen.visible
        enabled: visible

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            preventStealing: false
            onPositionChanged: function (mouse) {
                edge_scroll_overlay.x_pos = mouse.x;
                edge_scroll_overlay.y_pos = mouse.y;
                if (typeof game !== 'undefined' && game.orders.set_hover_at_screen) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.orders.set_hover_at_screen(mouse.x, mouse.y);
                    else
                        game.orders.set_hover_at_screen(-1, -1);
                }
                if (typeof game !== 'undefined' && game.placement.is_placing_formation && game.placement.on_formation_mouse_move) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.placement.on_formation_mouse_move(mouse.x, mouse.y);
                }
                if (typeof game !== 'undefined' && game.placement.is_placing_construction && game.placement.on_construction_mouse_move) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.placement.on_construction_mouse_move(mouse.x, mouse.y);
                }
            }
            onWheel: function (w) {
                if (typeof game !== 'undefined' && game.placement.construction_preview_rotatable && game.placement.construction_preview_rotatable()) {
                    var constructionDy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                    if (constructionDy !== 0 && game.placement.on_construction_scroll)
                        game.placement.on_construction_scroll(constructionDy);
                    w.accepted = true;
                    return;
                }
                if (typeof game !== 'undefined' && game.placement.is_placing_formation && game.placement.on_formation_scroll) {
                    var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                    if (dy !== 0)
                        game.placement.on_formation_scroll(dy);
                    w.accepted = true;
                    return;
                }
                w.accepted = false;
            }
            onEntered: function () {
                edge_scroll_timer.start();
                if (typeof game !== 'undefined' && game.orders.set_hover_at_screen) {
                    if (!edge_scroll_overlay.in_hud_zone(edge_scroll_overlay.x_pos, edge_scroll_overlay.y_pos))
                        game.orders.set_hover_at_screen(edge_scroll_overlay.x_pos, edge_scroll_overlay.y_pos);
                    else
                        game.orders.set_hover_at_screen(-1, -1);
                }
            }
            onExited: function () {
                edge_scroll_overlay.x_pos = -1;
                edge_scroll_overlay.y_pos = -1;
                if (typeof game !== 'undefined' && game.orders.set_hover_at_screen)
                    game.orders.set_hover_at_screen(-1, -1);
            }
        }

        Timer {
            id: edge_scroll_timer

            interval: 16
            repeat: true
            running: edge_scroll_overlay.enabled && edge_scroll_overlay.x_pos >= 0 && edge_scroll_overlay.y_pos >= 0
            onTriggered: {
                if (typeof game === 'undefined')
                    return;
                const w = edge_scroll_overlay.width;
                const h = edge_scroll_overlay.height;
                const x = edge_scroll_overlay.x_pos;
                const y = edge_scroll_overlay.y_pos;
                if (x < 0 || y < 0)
                    return;
                if (mainWindow.edge_scroll_disabled) {
                    if (game.orders.set_hover_at_screen)
                        game.orders.set_hover_at_screen(-1, -1);
                    return;
                }
                const over_hud = edge_scroll_overlay.in_hud_zone(x, y);
                if (game.orders.set_hover_at_screen)
                    game.orders.set_hover_at_screen(over_hud ? -1 : x, over_hud ? -1 : y);
                if (!Design.A11y.edgeScrollEnabled)
                    return;
                const step = EdgeScroll.vector(x, y, w, h, Design.A11y.edgeScrollSensitivity, Design.A11y.uiScale);
                if (step.x !== 0 || step.y !== 0)
                    game.camera.move(step.x, step.y);
            }
        }
    }

    Design.IronDialog {
        id: error_dialog

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 520)
        title: qsTr("Error")
        tone: "danger"
        message: game ? game.last_error : ""
        onPrimaryActivated: {
            if (game)
                game.clear_error();
        }
    }

    Connections {
        function onTutorial_finished() {
            Core.UiPreferences.tutorialCompleted = true;
        }

        target: (typeof game !== 'undefined' && game) ? game.tutorial : null
    }

    Connections {
        function onLast_error_changed() {
            if (game.last_error !== "" && !mainWindow.suppress_modals)
                error_dialog.open();
        }

        target: game
    }

    Connections {
        function onCurrent_mission_changed() {
            if (mainWindow.suppress_modals)
                return;
            if (typeof game !== 'undefined' && typeof game.setup.is_mission_match !== 'undefined' && game.setup.is_mission_match && !game.is_loading) {
                mainWindow.game_paused = true;
                objectivesPanel.visible = true;
            }
        }

        target: typeof game !== 'undefined' ? game.setup : null
    }

    Connections {
        function onPresented(entry) {
            Design.UiSound.notification();
        }

        target: Design.Notifications
    }

    Connections {
        function onPlayer_defeated(text, ally, owner_id) {
            if (!text || !mainWindow.game_started)
                return;
            Design.Notifications.push(ally ? "urgent" : "info", text, {
                    "channel": "player-defeated-" + owner_id,
                    "icon": ally ? Design.Icons.warning : Design.Icons.attack
                });
        }

        target: game
    }

    SystemVoice {
        engine: typeof game !== 'undefined' ? game : null
    }

    Connections {
        function onMission_announcement(text) {
            if (!text || !mainWindow.game_started)
                return;
            Design.Notifications.info(text, {
                    "channel": "mission-announcement",
                    "icon": Design.Icons.objective
                });
        }

        target: game
    }
}

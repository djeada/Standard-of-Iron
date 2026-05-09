import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import StandardOfIron 1.0

ApplicationWindow {
    id: mainWindow

    property alias game_view: gameViewItem
    property bool menu_visible: true
    property bool game_started: false
    property bool game_paused: false
    property bool edge_scroll_disabled: false
    property string mission_announcement_text: ""

    width: 1280
    height: 720
    visibility: Window.FullScreen
    visible: true
    title: qsTr("Standard of Iron - RTS Game")
    color: Theme.bg

    GameView {
        id: gameViewItem

        anchors.fill: parent
        z: 0
        focus: !mainWindow.menu_visible
        visible: game_started
    }

    HUD {
        id: hud

        anchors.fill: parent
        z: 1
        visible: !mainWindow.menu_visible && game_started
        onActiveFocusChanged: {
            if (activeFocus)
                gameViewItem.forceActiveFocus();

        }
        onPause_toggled: {
            mainWindow.game_paused = !mainWindow.game_paused;
            gameViewItem.set_paused(mainWindow.game_paused);
            gameViewItem.forceActiveFocus();
        }
        onSpeed_changed: function(speed) {
            gameViewItem.set_game_speed(speed);
            gameViewItem.forceActiveFocus();
        }
        onCommand_mode_changed: function(mode) {
            console.log("Main: Command mode changed to:", mode);
            if (typeof game !== 'undefined') {
                console.log("Main: Setting game.cursor_mode property to", mode);
                game.cursor_mode = mode;
            } else {
                console.log("Main: game is undefined");
            }
            gameViewItem.forceActiveFocus();
        }
        onRecruit_unit: function(unit_type) {
            if (typeof game !== 'undefined' && game.recruit_near_selected)
                game.recruit_near_selected(unit_type);

            gameViewItem.forceActiveFocus();
        }
        onReturn_to_main_menu_requested: {
            mainWindow.menu_visible = true;
        }
    }

    Rectangle {
        id: mission_announcement_toast

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 26
        z: 12
        visible: game_started && opacity > 0.01
        opacity: 0
        radius: 8
        color: "#cc2a1d12"
        border.color: Theme.thumbBr
        border.width: 1
        width: Math.min(parent.width * 0.7, 700)
        height: mission_announcement_label.implicitHeight + 22

        Text {
            id: mission_announcement_label

            anchors.centerIn: parent
            width: parent.width - 28
            text: mainWindow.mission_announcement_text
            color: Theme.textMain
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 220
            }

        }

    }

    Timer {
        id: mission_announcement_timer

        interval: 3600
        repeat: false
        onTriggered: mission_announcement_toast.opacity = 0
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
                    font.pixelSize: 36
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: qsTr("Press Space to resume")
                    color: Theme.textSubLite
                    font.pixelSize: 14
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
            if (visible) {
                mainMenu.forceActiveFocus();
                gameViewItem.focus = false;
            } else if (mainMenu.game_started) {
                gameViewItem.forceActiveFocus();
            }
        }
        onOpen_skirmish: function() {
            mapSelect.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_campaign: function() {
            campaign_screen.visible = true;
            mainWindow.menu_visible = false;
        }
        onSave_game: function() {
            if (mainWindow.game_started) {
                save_game_panel.visible = true;
                mainWindow.menu_visible = false;
            }
        }
        onLoad_save: function() {
            load_game_panel.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_settings: function() {
            settingsPanel.visible = true;
            mainWindow.menu_visible = false;
        }
        onOpen_objectives: function() {
            objectivesPanel.visible = true;
            mainWindow.menu_visible = false;
        }
        onExit_requested: function() {
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
                gameViewItem.focus = false;
            }
        }
        onMap_chosen: function(map_path, player_configs) {
            console.log("Main: onMap_chosen received", map_path, "with", player_configs.length, "player configs");
            if (typeof game !== 'undefined' && game.start_skirmish)
                game.start_skirmish(map_path, player_configs);

            mapSelect.visible = false;
            mainWindow.menu_visible = false;
            mainWindow.game_started = true;
            mainWindow.game_paused = false;
            gameViewItem.forceActiveFocus();
        }
        onCancelled: function() {
            mapSelect.visible = false;
            mainWindow.menu_visible = true;
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
                gameViewItem.focus = false;
            }
        }
        onMission_selected: function(campaign_id, mission_id) {
            console.log("Main: Campaign mission selected:", campaign_id + "/" + mission_id);
            if (typeof game !== 'undefined' && game.start_campaign_mission) {
                game.start_campaign_mission(campaign_id + "/" + mission_id);
                campaign_screen.visible = false;
                mainWindow.menu_visible = false;
                mainWindow.game_started = true;
                mainWindow.game_paused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function() {
            campaign_screen.visible = false;
            mainWindow.menu_visible = true;
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
                gameViewItem.focus = false;
            }
        }
        onSave_requested: function(slot_name) {
            console.log("Main: Save requested for slot:", slot_name);
            if (typeof game !== 'undefined' && game.save_gameToSlot)
                game.save_gameToSlot(slot_name);

            save_game_panel.visible = false;
            mainWindow.menu_visible = true;
        }
        onCancelled: function() {
            save_game_panel.visible = false;
            mainWindow.menu_visible = true;
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
                gameViewItem.focus = false;
            }
        }
        onLoad_requested: function(slot_name) {
            console.log("Main: Load requested for slot:", slot_name);
            if (typeof game !== 'undefined' && game.load_game_from_slot) {
                game.load_game_from_slot(slot_name);
                load_game_panel.visible = false;
                mainWindow.menu_visible = false;
                mainWindow.game_started = true;
                mainWindow.game_paused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function() {
            load_game_panel.visible = false;
            mainWindow.menu_visible = true;
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
                gameViewItem.focus = false;
            }
        }
        onCancelled: function() {
            settingsPanel.visible = false;
            mainWindow.menu_visible = true;
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
                gameViewItem.focus = false;
            }
        }
        onClose_requested: function() {
            objectivesPanel.visible = false;
            if (typeof game !== 'undefined' && typeof game.is_campaign_mission !== 'undefined' && game.is_campaign_mission && mainWindow.game_started) {
                mainWindow.game_paused = false;
                gameViewItem.set_paused(false);
                gameViewItem.forceActiveFocus();
            } else {
                mainWindow.menu_visible = true;
            }
        }
    }

    Item {
        id: edge_scroll_overlay

        property real horz_threshold: 12
        property real horz_max_speed: 0.15
        property real vert_threshold: 10
        property real vert_max_speed: 0.01
        property real x_pos: -1
        property real y_pos: -1
        property int vertical_shift: 6

        function in_hud_zone(x, y) {
            var topH = (typeof hud !== 'undefined' && hud && hud.topPanelHeight) ? hud.topPanelHeight : 0;
            var bottomH = (typeof hud !== 'undefined' && hud && hud.bottomPanelHeight) ? hud.bottomPanelHeight : 0;
            if (y < topH)
                return true;

            if (y > (height - bottomH))
                return true;

            return false;
        }

        anchors.fill: parent
        z: 2
        visible: !mainWindow.menu_visible && !mapSelect.visible
        enabled: visible

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            preventStealing: false
            onPositionChanged: function(mouse) {
                edge_scroll_overlay.x_pos = mouse.x;
                edge_scroll_overlay.y_pos = mouse.y;
                if (typeof game !== 'undefined' && game.set_hover_at_screen) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.set_hover_at_screen(mouse.x, mouse.y);
                    else
                        game.set_hover_at_screen(-1, -1);
                }
                if (typeof game !== 'undefined' && game.is_placing_formation && game.on_formation_mouse_move) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.on_formation_mouse_move(mouse.x, mouse.y);

                }
                if (typeof game !== 'undefined' && game.is_placing_construction && game.on_construction_mouse_move) {
                    if (!edge_scroll_overlay.in_hud_zone(mouse.x, mouse.y))
                        game.on_construction_mouse_move(mouse.x, mouse.y);

                }
            }
            onWheel: function(w) {
                if (typeof game !== 'undefined' && game.is_placing_formation && game.on_formation_scroll) {
                    var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                    if (dy !== 0)
                        game.on_formation_scroll(dy);

                    w.accepted = true;
                    return ;
                }
                w.accepted = false;
            }
            onEntered: function() {
                edge_scroll_timer.start();
                if (typeof game !== 'undefined' && game.set_hover_at_screen) {
                    if (!edge_scroll_overlay.in_hud_zone(edge_scroll_overlay.x_pos, edge_scroll_overlay.y_pos))
                        game.set_hover_at_screen(edge_scroll_overlay.x_pos, edge_scroll_overlay.y_pos);
                    else
                        game.set_hover_at_screen(-1, -1);
                }
            }
            onExited: function() {
                edge_scroll_timer.stop();
                edge_scroll_overlay.x_pos = -1;
                edge_scroll_overlay.y_pos = -1;
                if (typeof game !== 'undefined' && game.set_hover_at_screen)
                    game.set_hover_at_screen(-1, -1);

            }
        }

        Timer {
            id: edge_scroll_timer

            interval: 16
            repeat: true
            onTriggered: {
                if (typeof game === 'undefined')
                    return ;

                const w = edge_scroll_overlay.width;
                const h = edge_scroll_overlay.height;
                const x = edge_scroll_overlay.x_pos;
                const y = edge_scroll_overlay.y_pos;
                if (x < 0 || y < 0)
                    return ;

                if (mainWindow.edge_scroll_disabled) {
                    if (game.set_hover_at_screen)
                        game.set_hover_at_screen(-1, -1);

                    return ;
                }
                if (game.set_hover_at_screen)
                    game.set_hover_at_screen(x, y);

                const th = edge_scroll_overlay.horz_threshold;
                const tv = edge_scroll_overlay.vert_threshold;
                const clamp = function clamp(v, lo, hi) {
                    return Math.max(lo, Math.min(hi, v));
                };
                const dl = x;
                const dr = w - x;
                const dt = y;
                const db = h - y;
                const il = clamp(1 - dl / th, 0, 1);
                const ir = clamp(1 - dr / th, 0, 1);
                const iu = clamp(1 - dt / tv, 0, 1);
                const id = clamp(1 - db / tv, 0, 1);
                if (il === 0 && ir === 0 && iu === 0 && id === 0)
                    return ;

                const curveH = function curveH(a) {
                    return a * a;
                };
                const curveV = function curveV(a) {
                    return a * a * a;
                };
                const rawDx = (curveH(ir) - curveH(il)) * edge_scroll_overlay.horz_max_speed;
                const rawDz = (curveV(iu) - curveV(id)) * edge_scroll_overlay.vert_max_speed;
                const dx = rawDx / edge_scroll_overlay.horz_max_speed;
                const dz = rawDz / edge_scroll_overlay.vert_max_speed;
                if (dx !== 0 || dz !== 0)
                    game.camera_move(dx, dz);

            }
        }

    }

    Dialog {
        id: error_dialog

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 500)
        title: "Error"
        modal: true
        standardButtons: Dialog.Ok
        onAccepted: {
            if (game)
                game.clear_error();

        }

        contentItem: Rectangle {
            color: Theme.panelBase
            implicitHeight: error_text.implicitHeight + 40

            Text {
                id: error_text

                anchors.centerIn: parent
                width: parent.width - 40
                text: game ? game.last_error : ""
                color: Theme.accentBright
                wrapMode: Text.WordWrap
                font.pixelSize: 14
            }

        }

    }

    Connections {
        function onLast_error_changed() {
            if (game.last_error !== "")
                error_dialog.open();

        }

        target: game
    }

    Connections {
        function onCampaign_mission_changed() {
            if (typeof game !== 'undefined' && typeof game.is_campaign_mission !== 'undefined' && game.is_campaign_mission && !game.is_loading) {
                mainWindow.game_paused = true;
                gameViewItem.set_paused(true);
                objectivesPanel.visible = true;
            }
        }

        target: game
    }

    Connections {
        function onMission_announcement(text) {
            if (!text || !mainWindow.game_started)
                return ;

            mainWindow.mission_announcement_text = text;
            mission_announcement_toast.opacity = 1;
            mission_announcement_timer.restart();
        }

        target: game
    }

}

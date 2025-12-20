import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: mainWindow

    property alias gameView: gameViewItem
    property bool menuVisible: true
    property bool gameStarted: false
    property bool gamePaused: false
    property bool edgeScrollDisabled: false

    width: 1280
    height: 720
    visibility: Window.FullScreen
    visible: true
    title: qsTr("Standard of Iron - RTS Game")

    GameView {
        id: gameViewItem

        anchors.fill: parent
        z: 0
        focus: !mainWindow.menuVisible
        visible: gameStarted
    }

    HUD {
        id: hud

        anchors.fill: parent
        z: 1
        visible: !mainWindow.menuVisible && gameStarted
        onActiveFocusChanged: {
            if (activeFocus)
                gameViewItem.forceActiveFocus();

        }
        onPauseToggled: {
            mainWindow.gamePaused = !mainWindow.gamePaused;
            gameViewItem.setPaused(mainWindow.gamePaused);
            gameViewItem.forceActiveFocus();
        }
        onSpeedChanged: function(speed) {
            gameViewItem.setGameSpeed(speed);
            gameViewItem.forceActiveFocus();
        }
        onCommandModeChanged: function(mode) {
            console.log("Main: Command mode changed to:", mode);
            if (typeof game !== 'undefined') {
                console.log("Main: Setting game.cursor_mode property to", mode);
                game.cursor_mode = mode;
            } else {
                console.log("Main: game is undefined");
            }
            gameViewItem.forceActiveFocus();
        }
        onRecruit: function(unitType) {
            if (typeof game !== 'undefined' && game.recruit_near_selected)
                game.recruit_near_selected(unitType);

            gameViewItem.forceActiveFocus();
        }
        onReturnToMainMenuRequested: {
            mainWindow.menuVisible = true;
        }
    }

    Rectangle {
        id: pauseOverlay

        anchors.fill: parent
        z: 10
        visible: mainWindow.gamePaused && gameStarted
        color: "#80000000"

        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 150
            color: "#2c3e50"
            radius: 8
            border.color: "#34495e"
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: qsTr("PAUSED")
                    color: "#ecf0f1"
                    font.pixelSize: 36
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: qsTr("Press Space to resume")
                    color: "#bdc3c7"
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
        visible: mainWindow.menuVisible
        Component.onCompleted: {
            if (mainWindow.menuVisible)
                mainMenu.forceActiveFocus();

        }
        onVisibleChanged: {
            if (visible) {
                mainMenu.forceActiveFocus();
                gameViewItem.focus = false;
            } else if (gameStarted) {
                gameViewItem.forceActiveFocus();
            }
        }
        onOpenSkirmish: function() {
            mapSelect.visible = true;
            mainWindow.menuVisible = false;
        }
        onOpenCampaign: function() {
            campaign_screen.visible = true;
            mainWindow.menuVisible = false;
        }
        onSaveGame: function() {
            if (mainWindow.gameStarted) {
                saveGamePanel.visible = true;
                mainWindow.menuVisible = false;
            }
        }
        onLoadSave: function() {
            loadGamePanel.visible = true;
            mainWindow.menuVisible = false;
        }
        onOpenSettings: function() {
            settingsPanel.visible = true;
            mainWindow.menuVisible = false;
        }
        onOpenObjectives: function() {
            objectivesPanel.visible = true;
            mainWindow.menuVisible = false;
        }
        onExitRequested: function() {
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
        onMapChosen: function(map_path, playerConfigs) {
            console.log("Main: onMapChosen received", map_path, "with", playerConfigs.length, "player configs");
            if (typeof game !== 'undefined' && game.start_skirmish)
                game.start_skirmish(map_path, playerConfigs);

            mapSelect.visible = false;
            mainWindow.menuVisible = false;
            mainWindow.gameStarted = true;
            mainWindow.gamePaused = false;
            gameViewItem.forceActiveFocus();
        }
        onCancelled: function() {
            mapSelect.visible = false;
            mainWindow.menuVisible = true;
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
                mainWindow.menuVisible = false;
                mainWindow.gameStarted = true;
                mainWindow.gamePaused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function() {
            campaign_screen.visible = false;
            mainWindow.menuVisible = true;
        }
    }

    SaveGamePanel {
        id: saveGamePanel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                saveGamePanel.forceActiveFocus();
                gameViewItem.focus = false;
            }
        }
        onSaveRequested: function(slotName) {
            console.log("Main: Save requested for slot:", slotName);
            if (typeof game !== 'undefined' && game.save_gameToSlot)
                game.save_gameToSlot(slotName);

            saveGamePanel.visible = false;
            mainWindow.menuVisible = true;
        }
        onCancelled: function() {
            saveGamePanel.visible = false;
            mainWindow.menuVisible = true;
        }
    }

    LoadGamePanel {
        id: loadGamePanel

        anchors.fill: parent
        z: 22
        visible: false
        onVisibleChanged: {
            if (visible) {
                loadGamePanel.forceActiveFocus();
                gameViewItem.focus = false;
            }
        }
        onLoadRequested: function(slotName) {
            console.log("Main: Load requested for slot:", slotName);
            if (typeof game !== 'undefined' && game.load_game_from_slot) {
                game.load_game_from_slot(slotName);
                loadGamePanel.visible = false;
                mainWindow.menuVisible = false;
                mainWindow.gameStarted = true;
                mainWindow.gamePaused = false;
                gameViewItem.forceActiveFocus();
            }
        }
        onCancelled: function() {
            loadGamePanel.visible = false;
            mainWindow.menuVisible = true;
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
            mainWindow.menuVisible = true;
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
        onCloseRequested: function() {
            objectivesPanel.visible = false;
            mainWindow.menuVisible = true;
        }
    }

    Item {
        id: edgeScrollOverlay

        property real horzThreshold: 12
        property real horzMaxSpeed: 0.15
        property real vertThreshold: 10
        property real vertMaxSpeed: 0.01
        property real xPos: -1
        property real yPos: -1
        property int verticalShift: 6

        function inHudZone(x, y) {
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
        visible: !mainWindow.menuVisible && !mapSelect.visible
        enabled: visible

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            preventStealing: false
            onPositionChanged: function(mouse) {
                edgeScrollOverlay.xPos = mouse.x;
                edgeScrollOverlay.yPos = mouse.y;
                if (typeof game !== 'undefined' && game.set_hover_at_screen) {
                    if (!edgeScrollOverlay.inHudZone(mouse.x, mouse.y))
                        game.set_hover_at_screen(mouse.x, mouse.y);
                    else
                        game.set_hover_at_screen(-1, -1);
                }
                if (typeof game !== 'undefined' && game.is_placing_formation && game.on_formation_mouse_move) {
                    if (!edgeScrollOverlay.inHudZone(mouse.x, mouse.y))
                        game.on_formation_mouse_move(mouse.x, mouse.y);

                }
                if (typeof game !== 'undefined' && game.is_placing_construction && game.on_construction_mouse_move) {
                    if (!edgeScrollOverlay.inHudZone(mouse.x, mouse.y))
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
                edgeScrollTimer.start();
                if (typeof game !== 'undefined' && game.set_hover_at_screen) {
                    if (!edgeScrollOverlay.inHudZone(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos))
                        game.set_hover_at_screen(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos);
                    else
                        game.set_hover_at_screen(-1, -1);
                }
            }
            onExited: function() {
                edgeScrollTimer.stop();
                edgeScrollOverlay.xPos = -1;
                edgeScrollOverlay.yPos = -1;
                if (typeof game !== 'undefined' && game.set_hover_at_screen)
                    game.set_hover_at_screen(-1, -1);

            }
        }

        Timer {
            id: edgeScrollTimer

            interval: 16
            repeat: true
            onTriggered: {
                if (typeof game === 'undefined')
                    return ;

                const w = edgeScrollOverlay.width;
                const h = edgeScrollOverlay.height;
                const x = edgeScrollOverlay.xPos;
                const y = edgeScrollOverlay.yPos;
                if (x < 0 || y < 0)
                    return ;

                if (mainWindow.edgeScrollDisabled) {
                    if (game.set_hover_at_screen)
                        game.set_hover_at_screen(-1, -1);

                    return ;
                }
                if (game.set_hover_at_screen)
                    game.set_hover_at_screen(x, y);

                const th = edgeScrollOverlay.horzThreshold;
                const tv = edgeScrollOverlay.vertThreshold;
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
                const rawDx = (curveH(ir) - curveH(il)) * edgeScrollOverlay.horzMaxSpeed;
                const rawDz = (curveV(iu) - curveV(id)) * edgeScrollOverlay.vertMaxSpeed;
                const dx = rawDx / edgeScrollOverlay.horzMaxSpeed;
                const dz = rawDz / edgeScrollOverlay.vertMaxSpeed;
                if (dx !== 0 || dz !== 0)
                    game.camera_move(dx, dz);

            }
        }

    }

    Dialog {
        id: errorDialog

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
            color: "#2a2a2a"
            implicitHeight: errorText.implicitHeight + 40

            Text {
                id: errorText

                anchors.centerIn: parent
                width: parent.width - 40
                text: game ? game.last_error : ""
                color: "#ffcccc"
                wrapMode: Text.WordWrap
                font.pixelSize: 14
            }

        }

    }

    Connections {
        function onLast_error_changed() {
            if (game.last_error !== "")
                errorDialog.open();

        }

        target: game
    }

}

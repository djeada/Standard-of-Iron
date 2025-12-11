import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud

    property bool gameIsPaused: false
    property real currentSpeed: 1
    property string currentCommandMode: "normal"
    property int topPanelHeight: topPanel.height
    property int bottomPanelHeight: bottomPanel.height
    property int selectionTick: 0
    property bool hasMovableUnits: false

    signal pauseToggled()
    signal speedChanged(real speed)
    signal commandModeChanged(string mode)
    signal recruit(string unitType)
    signal returnToMainMenuRequested()
    signal hudBecameVisible()

    onVisibleChanged: {
        if (visible)
            hudBecameVisible();

    }

    Connections {
        function onSelected_units_changed() {
            selectionTick += 1;
            var hasTroops = false;
            if (typeof game !== 'undefined' && game.has_units_selected && game.has_selected_type) {
                var troopTypes = ["warrior", "archer", "swordsman", "spearman"];
                for (var i = 0; i < troopTypes.length; i++) {
                    if (game.has_selected_type(troopTypes[i])) {
                        hasTroops = true;
                        break;
                    }
                }
            }
            var actualMode = "normal";
            if (hasTroops && typeof game !== 'undefined' && game.get_selected_units_command_mode)
                actualMode = game.get_selected_units_command_mode();

            if (currentCommandMode !== actualMode) {
                currentCommandMode = actualMode;
                commandModeChanged(actualMode);
            }
            hasMovableUnits = hasTroops;
        }

        target: (typeof game !== 'undefined') ? game : null
    }

    Timer {
        id: productionRefresh

        interval: 100
        repeat: true
        running: true
        onTriggered: {
            selectionTick += 1;
            if (hasMovableUnits && typeof game !== 'undefined' && game.get_selected_units_command_mode) {
                var actualMode = game.get_selected_units_command_mode();
                if (currentCommandMode !== actualMode)
                    currentCommandMode = actualMode;

            }
        }
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
            gameIsPaused: hud.gameIsPaused
            currentSpeed: hud.currentSpeed
            onPauseToggled: {
                hud.gameIsPaused = !hud.gameIsPaused;
                hud.pauseToggled();
            }
            onSpeedChanged: function(s) {
                hud.currentSpeed = s;
                hud.speedChanged(s);
            }
        }

    }

    Item {
        id: bottomPanel

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(140, parent.height * 0.2)

        HUDBottom {
            id: hudBottom

            anchors.fill: parent
            currentCommandMode: hud.currentCommandMode
            selectionTick: hud.selectionTick
            hasMovableUnits: hud.hasMovableUnits
            onCommandModeChanged: function(m) {
                hud.currentCommandMode = m;
                hud.commandModeChanged(m);
            }
            onRecruit: function(t) {
                hud.recruit(t);
            }
        }

    }

    HUDVictory {
        id: hudVictory

        anchors.fill: parent
        onReturnToMainMenuRequested: {
            hud.returnToMainMenuRequested();
        }

        Connections {
            function onHudBecameVisible() {
                if (typeof game !== 'undefined' && game.victory_state === "")
                    hudVictory.forceHide();

            }

            target: hud
        }

    }

}

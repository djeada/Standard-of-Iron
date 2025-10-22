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
    property var minimapProvider: null

    signal pauseToggled()
    signal speedChanged(real speed)
    signal commandModeChanged(string mode)
    signal recruit(string unitType)
    signal returnToMainMenuRequested()
    signal hudBecameVisible()
    signal cameraMoveTo(real worldX, real worldZ)
    signal sendTroopsTo(real worldX, real worldZ)

    onVisibleChanged: {
        if (visible)
            hudBecameVisible();

    }

    Connections {
        function onSelectedUnitsChanged() {
            selectionTick += 1;
            var hasTroops = false;
            if (typeof game !== 'undefined' && game.hasUnitsSelected && game.hasSelectedType) {
                var troopTypes = ["warrior", "archer", "knight", "spearman"];
                for (var i = 0; i < troopTypes.length; i++) {
                    if (game.hasSelectedType(troopTypes[i])) {
                        hasTroops = true;
                        break;
                    }
                }
            }
            var actualMode = "normal";
            if (hasTroops && typeof game !== 'undefined' && game.getSelectedUnitsCommandMode)
                actualMode = game.getSelectedUnitsCommandMode();

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
            if (hasMovableUnits && typeof game !== 'undefined' && game.getSelectedUnitsCommandMode) {
                var actualMode = game.getSelectedUnitsCommandMode();
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
            minimapProvider: hud.minimapProvider
            onPauseToggled: {
                hud.gameIsPaused = !hud.gameIsPaused;
                hud.pauseToggled();
            }
            onSpeedChanged: function(s) {
                hud.currentSpeed = s;
                hud.speedChanged(s);
            }
            onMinimapDoubleClicked: {
                minimapOverlay.visible = true;
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
                if (typeof game !== 'undefined' && game.victoryState === "")
                    hudVictory.forceHide();

            }

            target: hud
        }

    }

    MinimapOverlay {
        id: minimapOverlay

        anchors.fill: parent
        minimapProvider: hud.minimapProvider
        visible: false

        onClosed: {
            minimapOverlay.visible = false;
        }

        onCameraMoveTo: function(worldX, worldZ) {
            hud.cameraMoveTo(worldX, worldZ);
        }

        onSendTroopsTo: function(worldX, worldZ) {
            hud.sendTroopsTo(worldX, worldZ);
        }
    }

}

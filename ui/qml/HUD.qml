import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud
    
    signal pauseToggled()
    signal speedChanged(real speed)
    signal commandModeChanged(string mode) // "normal", "attack", "guard", "patrol", "stop"
    signal recruit(string unitType)
    
    property bool gameIsPaused: false
    property real currentSpeed: 1.0
    property string currentCommandMode: "normal"
    
    // Expose panel heights for other overlays (e.g., edge scroll) to avoid stealing input over UI
    property int topPanelHeight: topPanel.height
    property int bottomPanelHeight: bottomPanel.height
    // Tick to refresh bindings when selection changes in engine
    property int selectionTick: 0

    Connections {
        target: (typeof game !== 'undefined') ? game : null
        function onSelectedUnitsChanged() { 
            selectionTick += 1
        }
    }

    // Periodic refresh to update production timers and counters while building
    Timer {
        id: productionRefresh
        interval: 100
        repeat: true
        running: true
        onTriggered: selectionTick += 1
    }
    
    // Top panel moved to HUDTop.qml. Keep id topPanel for backwards compatibility.
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
            onPauseToggled: { hud.gameIsPaused = !hud.gameIsPaused; hud.pauseToggled(); }
            onSpeedChanged: function(s) { hud.currentSpeed = s; hud.speedChanged(s); }
        }
    }
    
    // Bottom panel moved to HUDBottom.qml. Keep id bottomPanel for compatibility.
    Item {
        id: bottomPanel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(140, parent.height * 0.20)
        HUDBottom {
            id: hudBottom
            anchors.fill: parent
            currentCommandMode: hud.currentCommandMode
            selectionTick: hud.selectionTick
            onCommandModeChanged: function(m) { hud.currentCommandMode = m; hud.commandModeChanged(m); }
            onRecruit: function(t) { hud.recruit(t); }
        }
    }
    
    // Victory overlay moved to HUDVictory.qml
    HUDVictory { anchors.fill: parent }
}
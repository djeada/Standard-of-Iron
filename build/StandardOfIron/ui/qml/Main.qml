import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: mainWindow
    width: 1280
    height: 720
    visible: true
    title: "Standard of Iron - RTS Game"
    
    property alias gameView: gameViewItem
    
    // Main game view
    GameView {
        id: gameViewItem
        anchors.fill: parent
        z: 0
    }
    
    // HUD overlay
    HUD {
        id: hud
        anchors.fill: parent
        z: 1
        
        onPauseToggled: {
            // Handle pause/resume
            gameViewItem.setPaused(!gameViewItem.isPaused)
        }
        
        onSpeedChanged: function(speed) {
            gameViewItem.setGameSpeed(speed)
        }
        
        onUnitCommand: function(command) {
            gameViewItem.issueCommand(command)
        }
    }
}
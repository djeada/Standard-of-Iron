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
        focus: true
    }
    
    // HUD overlay
    HUD {
        id: hud
        anchors.fill: parent
        z: 1
        // Keep keyboard focus on the game view when interacting with HUD controls
        onActiveFocusChanged: if (activeFocus) gameViewItem.forceActiveFocus()
        
        onPauseToggled: {
            // Handle pause/resume
            gameViewItem.setPaused(!gameViewItem.isPaused)
            // Return focus to the game view for keyboard controls
            gameViewItem.forceActiveFocus()
        }
        
        onSpeedChanged: function(speed) {
            gameViewItem.setGameSpeed(speed)
            // Return focus to the game view for keyboard controls
            gameViewItem.forceActiveFocus()
        }
        
        onUnitCommand: function(command) {
            gameViewItem.issueCommand(command)
        }

        onRecruit: function(unitType) {
            if (typeof game !== 'undefined' && game.recruitNearSelected)
                game.recruitNearSelected(unitType)
            // Return focus to the game view for keyboard controls
            gameViewItem.forceActiveFocus()
        }
    }

    // Edge scroll overlay (hover-only) above HUD to ensure bottom edge works
    Item {
        id: edgeScrollOverlay
        anchors.fill: parent
        z: 2
        property real threshold: 80    // px from edge where scrolling starts
        property real maxSpeed: 1.2    // world units per tick at the very edge
        property real xPos: -1
        property real yPos: -1

        // Hover tracker that does not consume clicks
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            preventStealing: false
            onPositionChanged: function(mouse) {
                edgeScrollOverlay.xPos = mouse.x
                edgeScrollOverlay.yPos = mouse.y
                // Also feed hover to the engine since this overlay sits above the GL view
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(mouse.x, mouse.y)
                }
            }
            onEntered: function(mouse) {
                edgeScrollTimer.start()
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos)
                }
            }
            onExited: function(mouse) {
                edgeScrollTimer.stop()
                edgeScrollOverlay.xPos = -1
                edgeScrollOverlay.yPos = -1
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(-1, -1)
                }
            }
        }

        Timer {
            id: edgeScrollTimer
            interval: 16
            repeat: true
            onTriggered: {
                if (typeof game === 'undefined') return
                const w = edgeScrollOverlay.width
                const h = edgeScrollOverlay.height
                const x = edgeScrollOverlay.xPos
                const y = edgeScrollOverlay.yPos
                if (x < 0 || y < 0) return
                // Keep hover updated even if positionChanged throttles
                if (game.setHoverAtScreen) game.setHoverAtScreen(x, y)
                const t = edgeScrollOverlay.threshold
                const clamp = function(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
                // Distance from edges
                const dl = x
                const dr = w - x
                const dt = y
                const db = h - y
                // Normalized intensities (0..1)
                const il = clamp(1.0 - dl / t, 0, 1)
                const ir = clamp(1.0 - dr / t, 0, 1)
                const iu = clamp(1.0 - dt / t, 0, 1)
                const id = clamp(1.0 - db / t, 0, 1)
                if (il===0 && ir===0 && iu===0 && id===0) return
                // Apply gentle curve for smoother start
                const curve = function(a) { return a*a }
                const maxS = edgeScrollOverlay.maxSpeed
                const dx = (curve(ir) - curve(il)) * maxS
                const dz = (curve(iu) - curve(id)) * maxS
                if (dx !== 0 || dz !== 0) game.cameraMove(dx, dz)
            }
        }
    }
}
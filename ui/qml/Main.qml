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
        
        onCommandModeChanged: function(mode) {
            console.log("Main: Command mode changed to:", mode)
            // Set cursor mode in game engine via property (not method call)
            if (typeof game !== 'undefined') {
                console.log("Main: Setting game.cursorMode property to", mode)
                game.cursorMode = mode  // Set property directly, not setCursorMode()
            } else {
                console.log("Main: game is undefined")
            }
            gameViewItem.forceActiveFocus()
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
        // Horizontal edge-scroll sensitivity and speed
        property real horzThreshold: 80     // px from left/right edge where scroll begins
        property real horzMaxSpeed: 0.5     // world units per tick at the very edge (left/right)
        // Vertical edge-scroll is intentionally less sensitive and slower
        property real vertThreshold: 120    // px after dead-zone for top/bottom
        property real verticalDeadZone: 32  // no scroll within this many px past HUD bars
        property real vertMaxSpeed: 0.1     // world units per tick at the very edge (top/bottom)
        property real xPos: -1
        property real yPos: -1
        // Shift vertical edge-scroll away from HUD panels by this many pixels
    property int verticalShift: 6
        // Computed guard zones derived from HUD panel heights
        function inHudZone(x, y) {
            // Guard against missing HUD object
            var topH = (typeof hud !== 'undefined' && hud && hud.topPanelHeight) ? hud.topPanelHeight : 0
            var bottomH = (typeof hud !== 'undefined' && hud && hud.bottomPanelHeight) ? hud.bottomPanelHeight : 0
            if (y < topH) return true
            if (y > (height - bottomH)) return true
            return false
        }

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
                // Only feed hover to engine if not over HUD panels; otherwise clear hover
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    if (!edgeScrollOverlay.inHudZone(mouse.x, mouse.y)) {
                        game.setHoverAtScreen(mouse.x, mouse.y)
                    } else {
                        game.setHoverAtScreen(-1, -1)
                    }
                }
            }
            onEntered: function(mouse) {
                edgeScrollTimer.start()
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    if (!edgeScrollOverlay.inHudZone(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos)) {
                        game.setHoverAtScreen(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos)
                    } else {
                        game.setHoverAtScreen(-1, -1)
                    }
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
                // Skip camera movement and hover updates when over HUD panels
                if (edgeScrollOverlay.inHudZone(x, y)) {
                    if (game.setHoverAtScreen) game.setHoverAtScreen(-1, -1)
                    return
                }
                // Keep hover updated even if positionChanged throttles
                if (game.setHoverAtScreen) game.setHoverAtScreen(x, y)
                const th = edgeScrollOverlay.horzThreshold
                const tv = edgeScrollOverlay.vertThreshold
                const vdz = edgeScrollOverlay.verticalDeadZone
                const clamp = function(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
                // Distance from edges
                const dl = x
                const dr = w - x
                // Shift vertical edges inward to avoid overlapping HUD panels
                const topBar = (typeof hud !== 'undefined' && hud && hud.topPanelHeight) ? hud.topPanelHeight : 0
                const bottomBar = (typeof hud !== 'undefined' && hud && hud.bottomPanelHeight) ? hud.bottomPanelHeight : 0
                const topEdge = topBar + edgeScrollOverlay.verticalShift
                const bottomEdge = h - bottomBar - edgeScrollOverlay.verticalShift
                // Apply a vertical dead-zone beyond the HUD before scrolling starts
                const dt = Math.max(0, (y - topEdge) - vdz)
                const db = Math.max(0, (bottomEdge - y) - vdz)
                // Normalized intensities (0..1)
                const il = clamp(1.0 - dl / th, 0, 1)
                const ir = clamp(1.0 - dr / th, 0, 1)
                const iu = clamp(1.0 - dt / tv, 0, 1)
                const id = clamp(1.0 - db / tv, 0, 1)
                if (il===0 && ir===0 && iu===0 && id===0) return
                // Apply gentle curve for smoother start
                const curveH = function(a) { return a*a }
                // Vertical curve is gentler (cubic) to reduce early strength
                const curveV = function(a) { return a*a*a }
                const dx = (curveH(ir) - curveH(il)) * edgeScrollOverlay.horzMaxSpeed
                const dz = (curveV(iu) - curveV(id)) * edgeScrollOverlay.vertMaxSpeed
                if (dx !== 0 || dz !== 0) game.cameraMove(dx, dz)
            }
        }
    }
}

// main.qml
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
    property bool menuVisible: true

    // Main game view (hidden while main menu is visible)
    GameView {
        id: gameViewItem
        anchors.fill: parent
        z: 0
        focus: !mainWindow.menuVisible
        visible: !mainWindow.menuVisible
    }

    // HUD overlay
    HUD {
        id: hud
        anchors.fill: parent
        z: 1
        visible: !mainWindow.menuVisible
        // Keep keyboard focus on the game view when interacting with HUD controls
        onActiveFocusChanged: if (activeFocus) gameViewItem.forceActiveFocus()

        onPauseToggled: {
            gameViewItem.setPaused(!gameViewItem.isPaused)
            gameViewItem.forceActiveFocus()
        }

        onSpeedChanged: function(speed) {
            gameViewItem.setGameSpeed(speed)
            gameViewItem.forceActiveFocus()
        }

        onCommandModeChanged: function(mode) {
            console.log("Main: Command mode changed to:", mode)
            if (typeof game !== 'undefined') {
                console.log("Main: Setting game.cursorMode property to", mode)
                game.cursorMode = mode
            } else {
                console.log("Main: game is undefined")
            }
            gameViewItem.forceActiveFocus()
        }

        onRecruit: function(unitType) {
            if (typeof game !== 'undefined' && game.recruitNearSelected)
                game.recruitNearSelected(unitType)
            gameViewItem.forceActiveFocus()
        }
    }

    // Main Menu overlay
    MainMenu {
        id: mainMenu
        anchors.fill: parent
        z: 20
        visible: mainWindow.menuVisible

        Component.onCompleted: {
            if (mainWindow.menuVisible) mainMenu.forceActiveFocus()
        }

        onVisibleChanged: {
            if (visible) {
                mainMenu.forceActiveFocus()
                gameViewItem.focus = false
            }
        }

        onOpenSkirmish: function() {
            mapSelect.visible = true
            mainMenu.visible = false
        }
        onOpenSettings: function() {
            if (typeof game !== 'undefined' && game.openSettings) game.openSettings()
        }
        onLoadSave: function() {
            if (typeof game !== 'undefined' && game.loadSave) game.loadSave()
        }
        onExitRequested: function() {
            if (typeof game !== 'undefined' && game.exitGame) game.exitGame()
        }
    }

    MapSelect {
        id: mapSelect
        anchors.fill: parent
        z: 21
        visible: false

        onVisibleChanged: {
            if (visible) {
                mapSelect.forceActiveFocus()
                gameViewItem.focus = false
            }
        }

        onMapChosen: function(mapPath) {
            if (typeof game !== 'undefined' && game.startSkirmish) game.startSkirmish(mapPath)
            mapSelect.visible = false
            mainWindow.menuVisible = false
        }
        onCancelled: function() {
            mapSelect.visible = false
            mainMenu.visible = true
        }
    }

    // Edge scroll overlay (hover-only) above HUD to ensure bottom edge works
    // Make it completely invisible (not in the scene graph) when menus are showing,
    // so it cannot intercept any mouse events.
    Item {
        id: edgeScrollOverlay
        anchors.fill: parent
        z: 2
        visible: !mainWindow.menuVisible && !mapSelect.visible
        enabled: visible

        // Horizontal edge-scroll sensitivity and speed
        property real horzThreshold: 80
        property real horzMaxSpeed: 0.5
        // Vertical edge-scroll is intentionally less sensitive and slower
        property real vertThreshold: 120
        property real verticalDeadZone: 32
        property real vertMaxSpeed: 0.1
        property real xPos: -1
        property real yPos: -1
        // Shift vertical edge-scroll away from HUD panels by this many pixels
        property int verticalShift: 6

        // Computed guard zones derived from HUD panel heights
        function inHudZone(x, y) {
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
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    if (!edgeScrollOverlay.inHudZone(mouse.x, mouse.y)) {
                        game.setHoverAtScreen(mouse.x, mouse.y)
                    } else {
                        game.setHoverAtScreen(-1, -1)
                    }
                }
            }
            onEntered: function() {
                edgeScrollTimer.start()
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    if (!edgeScrollOverlay.inHudZone(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos)) {
                        game.setHoverAtScreen(edgeScrollOverlay.xPos, edgeScrollOverlay.yPos)
                    } else {
                        game.setHoverAtScreen(-1, -1)
                    }
                }
            }
            onExited: function() {
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

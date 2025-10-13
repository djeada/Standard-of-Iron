
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: mainWindow
    width: 1280
    height: 720
    
    visibility: Window.FullScreen
    visible: true
    title: "Standard of Iron - RTS Game"

    property alias gameView: gameViewItem
    property bool menuVisible: true
    property bool gameStarted: false  
    property bool gamePaused: false   
    
    
    property bool edgeScrollDisabled: false

    
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
        
        onActiveFocusChanged: if (activeFocus) gameViewItem.forceActiveFocus()

        onPauseToggled: {
            mainWindow.gamePaused = !mainWindow.gamePaused
            gameViewItem.setPaused(mainWindow.gamePaused)
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
                    text: "PAUSED"
                    color: "#ecf0f1"
                    font.pixelSize: 36
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: "Press Space to resume"
                    color: "#bdc3c7"
                    font.pixelSize: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    
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
            } else if (gameStarted) {
                
                gameViewItem.forceActiveFocus()
            }
        }

        onOpenSkirmish: function() {
            mapSelect.visible = true
            mainWindow.menuVisible = false  
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

        onMapChosen: function(mapPath, playerConfigs) {
            console.log("Main: onMapChosen received", mapPath, "with", playerConfigs.length, "player configs")
            if (typeof game !== 'undefined' && game.startSkirmish) {
                game.startSkirmish(mapPath, playerConfigs)
            }
            mapSelect.visible = false
            mainWindow.menuVisible = false
            mainWindow.gameStarted = true  
            mainWindow.gamePaused = false  
            gameViewItem.forceActiveFocus()
        }
        onCancelled: function() {
            mapSelect.visible = false
            mainWindow.menuVisible = true
        }
    }

    
    
    
    Item {
        id: edgeScrollOverlay
        anchors.fill: parent
        z: 2
        visible: !mainWindow.menuVisible && !mapSelect.visible
        enabled: visible

        
    
    
    property real horzThreshold: 120
    property real horzMaxSpeed: 0.15

    property real vertThreshold: 100
    property real verticalDeadZone: 45
    property real vertMaxSpeed: 0.01
        property real xPos: -1
        property real yPos: -1
        
        property int verticalShift: 6

        
        function inHudZone(x, y) {
            var topH = (typeof hud !== 'undefined' && hud && hud.topPanelHeight) ? hud.topPanelHeight : 0
            var bottomH = (typeof hud !== 'undefined' && hud && hud.bottomPanelHeight) ? hud.bottomPanelHeight : 0
            if (y < topH) return true
            if (y > (height - bottomH)) return true
            return false
        }

        
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
                
                if (edgeScrollOverlay.inHudZone(x, y) || mainWindow.edgeScrollDisabled) {
                    if (game.setHoverAtScreen) game.setHoverAtScreen(-1, -1)
                    return
                }
                
                if (game.setHoverAtScreen) game.setHoverAtScreen(x, y)
                const th = edgeScrollOverlay.horzThreshold
                const tv = edgeScrollOverlay.vertThreshold
                const vdz = edgeScrollOverlay.verticalDeadZone
                const clamp = function(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
                
                const dl = x
                const dr = w - x
                
                const topBar = (typeof hud !== 'undefined' && hud && hud.topPanelHeight) ? hud.topPanelHeight : 0
                const bottomBar = (typeof hud !== 'undefined' && hud && hud.bottomPanelHeight) ? hud.bottomPanelHeight : 0
                const topEdge = topBar + edgeScrollOverlay.verticalShift
                const bottomEdge = h - bottomBar - edgeScrollOverlay.verticalShift
                
                const dt = Math.max(0, (y - topEdge) - vdz)
                const db = Math.max(0, (bottomEdge - y) - vdz)
                
                const il = clamp(1.0 - dl / th, 0, 1)
                const ir = clamp(1.0 - dr / th, 0, 1)
                const iu = clamp(1.0 - dt / tv, 0, 1)
                const id = clamp(1.0 - db / tv, 0, 1)
                if (il===0 && ir===0 && iu===0 && id===0) return
                
                const curveH = function(a) { return a*a }
                
                const curveV = function(a) { return a*a*a }
                const rawDx = (curveH(ir) - curveH(il)) * edgeScrollOverlay.horzMaxSpeed
                const rawDz = (curveV(iu) - curveV(id)) * edgeScrollOverlay.vertMaxSpeed
                
                const dx = rawDx / edgeScrollOverlay.horzMaxSpeed
                const dz = rawDz / edgeScrollOverlay.vertMaxSpeed
                if (dx !== 0 || dz !== 0) game.cameraMove(dx, dz)
            }
        }
    }
}

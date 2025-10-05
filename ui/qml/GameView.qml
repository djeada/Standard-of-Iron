import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Item {
    id: gameView
    objectName: "GameView"
    
    property bool isPaused: false
    property real gameSpeed: 1.0
    property bool setRallyMode: false  // Rally point mode toggle
    
    signal mapClicked(real x, real y)
    signal unitSelected(int unitId)
    signal areaSelected(real x1, real y1, real x2, real y2)
    
    property string cursorMode: "normal"  // "normal", "attack", "guard", "patrol"
    
    function setPaused(paused) {
        isPaused = paused
        if (typeof game !== 'undefined' && game.setPaused)
            game.setPaused(paused)
    }
    
    function setGameSpeed(speed) {
        gameSpeed = speed
        if (typeof game !== 'undefined' && game.setGameSpeed)
            game.setGameSpeed(speed)
    }
    
    function issueCommand(command) {
        console.log("Command issued:", command)
        // Handle unit commands
    }
    
    // OpenGL rendering item inside scene graph
        GLView {
        id: renderArea
        anchors.fill: parent
        engine: game // GameEngine object exposed from C++
        focus: false
        
        // Update cursor mode when engine changes it
        Connections {
            target: game
            function onCursorModeChanged() {
                if (typeof game !== 'undefined' && game.cursorMode) {
                    gameView.cursorMode = game.cursorMode
                }
            }
        }
        
        // Sync initial cursor mode state
        Component.onCompleted: {
            if (typeof game !== 'undefined' && game.cursorMode) {
                gameView.cursorMode = game.cursorMode
            }
        }
        
        // Placeholder text (disabled by default to not cover GL)
        // Text {
        //     anchors.centerIn: parent
        //     text: "3D Game World\n(OpenGL Render Area)\n\nPress WASD to move camera\nMouse to look around\nScroll to zoom"
        //     color: "white"
        //     font.pointSize: 16
        //     horizontalAlignment: Text.AlignHCenter
        // }
        
        // Camera controls info overlay (offset below HUD top bar)
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
            anchors.topMargin: 70   // HUD top bar is 60px tall; keep some gap
            width: 200
            height: 160
            color: "#34495e"
            opacity: 0.8
            
            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                
                Text {
                    text: "Camera Controls:"
                    color: "white"
                    font.bold: true
                    font.pointSize: 10
                }
                Text {
                    text: "WASD - Move"
                    color: "white"
                    font.pointSize: 9
                }
                Text {
                    text: "Mouse - Look"
                    color: "white"
                    font.pointSize: 9
                }
                Text {
                    text: "Scroll - Zoom"
                    color: "white"
                    font.pointSize: 9
                }
                Text {
                    text: "Q/E - Rotate"
                    color: "white"
                    font.pointSize: 9
                }
                Text {
                    text: "R/F - Up/Down"
                    color: "white"
                    font.pointSize: 9
                }
                
                Rectangle {
                    width: parent.width - 16
                    height: 1
                    color: "#7f8c8d"
                    opacity: 0.5
                }
                
                Text {
                    text: "Cursor: " + gameView.cursorMode.toUpperCase()
                    color: gameView.cursorMode === "normal" ? "#bdc3c7" : "#3498db"
                    font.bold: gameView.cursorMode !== "normal"
                    font.pointSize: 9
                }
            }
        }
        
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            propagateComposedEvents: true
            preventStealing: true
            
            // Control cursor shape directly here
            cursorShape: (gameView.cursorMode === "normal") ? Qt.ArrowCursor : Qt.BlankCursor
            
            enabled: gameView.visible
            
            onEntered: {
                // Notify C++ that we're hovering over the game view
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(0, 0) // Will be updated by onPositionChanged
                }
            }
            
            onExited: {
                // Notify C++ that we've left the game view - show normal cursor
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(-1, -1)
                }
            }
            
            onPositionChanged: function(mouse) {
                // Position tracking now uses built-in mouseX/mouseY via direct binding
                // This handles selection box during drag
                if (isSelecting) {
                    var endX = mouse.x
                    var endY = mouse.y
                    
                    selectionBox.x = Math.min(startX, endX)
                    selectionBox.y = Math.min(startY, endY)
                    selectionBox.width = Math.abs(endX - startX)
                    selectionBox.height = Math.abs(endY - startY)
                } else {
                    if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                        game.setHoverAtScreen(mouse.x, mouse.y)
                    }
                }
            }
            onWheel: function(w) {
                // Mouse wheel: move camera up/down (RTS-style height adjust)
                // delta is in eighths of a degree; use angleDelta.y where available
                var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120)
                if (dy !== 0 && typeof game !== 'undefined' && game.cameraElevate) {
                    // Scale to world units
                    game.cameraElevate(-dy * 0.5)
                }
                w.accepted = true
            }
            
            property bool isSelecting: false
            property real startX: 0
            property real startY: 0
            
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    // Rally mode takes priority
                    if (gameView.setRallyMode) {
                        if (typeof game !== 'undefined' && game.setRallyAtScreen) {
                            game.setRallyAtScreen(mouse.x, mouse.y)
                        }
                        gameView.setRallyMode = false
                        return
                    }
                    
                    // Attack mode: issue attack command
                    if (gameView.cursorMode === "attack") {
                        if (typeof game !== 'undefined' && game.onAttackClick) {
                            game.onAttackClick(mouse.x, mouse.y)
                        }
                        return
                    }
                    
                    // Guard mode: issue guard command
                    if (gameView.cursorMode === "guard") {
                        // TODO: Implement guard command
                        return
                    }
                    
                    // Patrol mode: issue patrol command
                    if (gameView.cursorMode === "patrol") {
                        if (typeof game !== 'undefined' && game.onPatrolClick) {
                            game.onPatrolClick(mouse.x, mouse.y)
                        }
                        return
                    }
                    
                    // Normal mode: start selection drag
                    isSelecting = true
                    startX = mouse.x
                    startY = mouse.y
                    selectionBox.x = startX
                    selectionBox.y = startY
                    selectionBox.width = 0
                    selectionBox.height = 0
                    selectionBox.visible = true
                } else if (mouse.button === Qt.RightButton) {
                    // Right-click cancels rally mode, cursor modes, and deselects
                    if (gameView.setRallyMode) {
                        gameView.setRallyMode = false
                    }
                    if (typeof game !== 'undefined' && game.onRightClick) {
                        game.onRightClick(mouse.x, mouse.y)
                    }
                }
            }
            
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton && isSelecting) {
                    isSelecting = false
                    selectionBox.visible = false
                    
                    if (selectionBox.width > 5 && selectionBox.height > 5) {
                        // Area selection
                        areaSelected(selectionBox.x, selectionBox.y, 
                                   selectionBox.x + selectionBox.width,
                                   selectionBox.y + selectionBox.height)
                        if (typeof game !== 'undefined' && game.onAreaSelected) {
                            game.onAreaSelected(selectionBox.x, selectionBox.y,
                                                selectionBox.x + selectionBox.width,
                                                selectionBox.y + selectionBox.height,
                                                false)
                        }
                    } else {
                        // Point selection (unless rally was set in onPressed)
                        mapClicked(mouse.x, mouse.y)
                        if (typeof game !== 'undefined' && game.onClickSelect) {
                            game.onClickSelect(mouse.x, mouse.y, false)
                        }
                    }
                }
            }
        }

        // Selection box
        Rectangle {
            id: selectionBox
            visible: false
            border.color: "white"
            border.width: 1
            color: "transparent"
        }
    }
    
    // Custom cursor overlay - uses C++ global cursor tracking
    Item {
        id: customCursorContainer
        visible: gameView.cursorMode !== "normal"
        width: 32
        height: 32
        z: 999999
        
        // Bind to C++ global cursor position (QCursor::pos())
        x: (typeof game !== 'undefined' && game.globalCursorX) ? game.globalCursorX - 16 : 0
        y: (typeof game !== 'undefined' && game.globalCursorY) ? game.globalCursorY - 16 : 0
        
        // Attack cursor with pulsing animation
        Item {
            id: attackCursorContainer
            visible: gameView.cursorMode === "attack"
            anchors.fill: parent
            
            property real pulseScale: 1.0
            
            SequentialAnimation on pulseScale {
                running: attackCursorContainer.visible
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 1.2; duration: 400; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 1.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
            }
            
            Canvas {
                id: attackCursor
                anchors.fill: parent
                scale: attackCursorContainer.pulseScale
                transformOrigin: Item.Center
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    
                    // Red crosshair - brighter for better visibility
                    ctx.strokeStyle = "#ff4444"
                    ctx.lineWidth = 3
                    
                    // Vertical line
                    ctx.beginPath()
                    ctx.moveTo(16, 4)
                    ctx.lineTo(16, 28)
                    ctx.stroke()
                    
                    // Horizontal line
                    ctx.beginPath()
                    ctx.moveTo(4, 16)
                    ctx.lineTo(28, 16)
                    ctx.stroke()
                    
                    // Center dot - larger and brighter
                    ctx.fillStyle = "#ff2222"
                    ctx.beginPath()
                    ctx.arc(16, 16, 4, 0, Math.PI * 2)
                    ctx.fill()
                    
                    // Outer glow
                    ctx.strokeStyle = "rgba(255, 68, 68, 0.5)"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.arc(16, 16, 7, 0, Math.PI * 2)
                    ctx.stroke()
                    
                    // Corner brackets
                    ctx.strokeStyle = "#e74c3c"
                    ctx.lineWidth = 2
                    
                    // Top-left
                    ctx.beginPath()
                    ctx.moveTo(8, 12)
                    ctx.lineTo(8, 8)
                    ctx.lineTo(12, 8)
                    ctx.stroke()
                    
                    // Top-right
                    ctx.beginPath()
                    ctx.moveTo(20, 8)
                    ctx.lineTo(24, 8)
                    ctx.lineTo(24, 12)
                    ctx.stroke()
                    
                    // Bottom-left
                    ctx.beginPath()
                    ctx.moveTo(8, 20)
                    ctx.lineTo(8, 24)
                    ctx.lineTo(12, 24)
                    ctx.stroke()
                    
                    // Bottom-right
                    ctx.beginPath()
                    ctx.moveTo(20, 24)
                    ctx.lineTo(24, 24)
                    ctx.lineTo(24, 20)
                    ctx.stroke()
                }
                
                Component.onCompleted: requestPaint()
            }
        }
        
        // Guard cursor
        Canvas {
            id: guardCursor
            visible: gameView.cursorMode === "guard"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                // Blue shield
                ctx.fillStyle = "#3498db"
                ctx.strokeStyle = "#2980b9"
                ctx.lineWidth = 2
                
                // Shield shape
                ctx.beginPath()
                ctx.moveTo(16, 6)
                ctx.lineTo(24, 10)
                ctx.lineTo(24, 18)
                ctx.lineTo(16, 26)
                ctx.lineTo(8, 18)
                ctx.lineTo(8, 10)
                ctx.closePath()
                ctx.fill()
                ctx.stroke()
                
                // Shield emblem (checkmark)
                ctx.strokeStyle = "#ecf0f1"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(13, 16)
                ctx.lineTo(15, 18)
                ctx.lineTo(19, 12)
                ctx.stroke()
            }
            Component.onCompleted: requestPaint()
        }
        
        // Patrol cursor
        Canvas {
            id: patrolCursor
            visible: gameView.cursorMode === "patrol"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                // Green waypoint marker
                ctx.strokeStyle = "#27ae60"
                ctx.lineWidth = 2
                
                // Circular path
                ctx.beginPath()
                ctx.arc(16, 16, 10, 0, Math.PI * 2)
                ctx.stroke()
                
                // Direction arrows
                ctx.fillStyle = "#27ae60"
                
                // Right arrow
                ctx.beginPath()
                ctx.moveTo(26, 16)
                ctx.lineTo(22, 13)
                ctx.lineTo(22, 19)
                ctx.closePath()
                ctx.fill()
                
                // Left arrow
                ctx.beginPath()
                ctx.moveTo(6, 16)
                ctx.lineTo(10, 13)
                ctx.lineTo(10, 19)
                ctx.closePath()
                ctx.fill()
                
                // Center dot
                ctx.beginPath()
                ctx.arc(16, 16, 3, 0, Math.PI * 2)
                ctx.fill()
            }
            Component.onCompleted: requestPaint()
        }
    }
    
    // Edge scrolling handled by Main.qml overlay for smoother behavior and to bypass overlays
    
    // Keyboard handling
    Keys.onPressed: function(event) {
        if (typeof game === 'undefined') return
        var yawStep = event.modifiers & Qt.ShiftModifier ? 4 : 2
        var panStep = 0.6
        switch (event.key) {
            // ESC opens main menu (closing is handled by MainMenu itself when it has focus)
            case Qt.Key_Escape:
                if (typeof mainWindow !== 'undefined' && !mainWindow.menuVisible) {
                    mainWindow.menuVisible = true
                    event.accepted = true
                }
                break
            // Space toggles pause
            case Qt.Key_Space:
                if (typeof mainWindow !== 'undefined') {
                    mainWindow.gamePaused = !mainWindow.gamePaused
                    gameView.setPaused(mainWindow.gamePaused)
                    event.accepted = true
                }
                break
            // WASD pans the camera just like arrow keys
            case Qt.Key_W: game.cameraMove(0, panStep);  event.accepted = true; break
            case Qt.Key_S: game.cameraMove(0, -panStep); event.accepted = true; break
            case Qt.Key_A: game.cameraMove(-panStep, 0); event.accepted = true; break
            case Qt.Key_D: game.cameraMove(panStep, 0);  event.accepted = true; break
            // Arrow keys pan as well
            case Qt.Key_Up:    game.cameraMove(0, panStep);  event.accepted = true; break
            case Qt.Key_Down:  game.cameraMove(0, -panStep); event.accepted = true; break
            case Qt.Key_Left:  game.cameraMove(-panStep, 0); event.accepted = true; break
            case Qt.Key_Right: game.cameraMove(panStep, 0);  event.accepted = true; break
            // Yaw rotation on Q/E
            case Qt.Key_Q: game.cameraYaw(-yawStep); event.accepted = true; break
            case Qt.Key_E: game.cameraYaw(yawStep);  event.accepted = true; break
            // Elevation
            case Qt.Key_R: game.cameraElevate(0.5);  event.accepted = true; break
            case Qt.Key_F: game.cameraElevate(-0.5); event.accepted = true; break
        }
    }
    
    focus: true
}
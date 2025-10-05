import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Item {
    id: gameView
    objectName: "GameView"
    
    property bool isPaused: false
    property real gameSpeed: 1.0
    property bool setRallyMode: false  
    
    signal mapClicked(real x, real y)
    signal unitSelected(int unitId)
    signal areaSelected(real x1, real y1, real x2, real y2)
    
    property string cursorMode: "normal"  
    
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
        
    }
    
    
        GLView {
        id: renderArea
        anchors.fill: parent
        engine: game 
        focus: false
        
        
        Connections {
            target: game
            function onCursorModeChanged() {
                if (typeof game !== 'undefined' && game.cursorMode) {
                    gameView.cursorMode = game.cursorMode
                }
            }
        }
        
        
        Component.onCompleted: {
            if (typeof game !== 'undefined' && game.cursorMode) {
                gameView.cursorMode = game.cursorMode
            }
        }
        
        
        
        
        
        
        
        
        
        
        
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
            anchors.topMargin: 70   
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
            
            
            cursorShape: (gameView.cursorMode === "normal") ? Qt.ArrowCursor : Qt.BlankCursor
            
            enabled: gameView.visible
            
            onEntered: {
                
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(0, 0) 
                }
            }
            
            onExited: {
                
                if (typeof game !== 'undefined' && game.setHoverAtScreen) {
                    game.setHoverAtScreen(-1, -1)
                }
            }
            
            onPositionChanged: function(mouse) {
                
                
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
                
                
                var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120)
                if (dy !== 0 && typeof game !== 'undefined' && game.cameraElevate) {
                    
                    game.cameraElevate(-dy * 0.5)
                }
                w.accepted = true
            }
            
            property bool isSelecting: false
            property real startX: 0
            property real startY: 0
            
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    
                    if (gameView.setRallyMode) {
                        if (typeof game !== 'undefined' && game.setRallyAtScreen) {
                            game.setRallyAtScreen(mouse.x, mouse.y)
                        }
                        gameView.setRallyMode = false
                        return
                    }
                    
                    
                    if (gameView.cursorMode === "attack") {
                        if (typeof game !== 'undefined' && game.onAttackClick) {
                            game.onAttackClick(mouse.x, mouse.y)
                        }
                        return
                    }
                    
                    
                    if (gameView.cursorMode === "guard") {
                        
                        return
                    }
                    
                    
                    if (gameView.cursorMode === "patrol") {
                        if (typeof game !== 'undefined' && game.onPatrolClick) {
                            game.onPatrolClick(mouse.x, mouse.y)
                        }
                        return
                    }
                    
                    
                    isSelecting = true
                    startX = mouse.x
                    startY = mouse.y
                    selectionBox.x = startX
                    selectionBox.y = startY
                    selectionBox.width = 0
                    selectionBox.height = 0
                    selectionBox.visible = true
                } else if (mouse.button === Qt.RightButton) {
                    
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
                        
                        mapClicked(mouse.x, mouse.y)
                        if (typeof game !== 'undefined' && game.onClickSelect) {
                            game.onClickSelect(mouse.x, mouse.y, false)
                        }
                    }
                }
            }
        }

        
        Rectangle {
            id: selectionBox
            visible: false
            border.color: "white"
            border.width: 1
            color: "transparent"
        }
    }
    
    
    Item {
        id: customCursorContainer
        visible: gameView.cursorMode !== "normal"
        width: 32
        height: 32
        z: 999999
        
        
        x: (typeof game !== 'undefined' && game.globalCursorX) ? game.globalCursorX - 16 : 0
        y: (typeof game !== 'undefined' && game.globalCursorY) ? game.globalCursorY - 16 : 0
        
        
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
                    
                    
                    ctx.strokeStyle = "#ff4444"
                    ctx.lineWidth = 3
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(16, 4)
                    ctx.lineTo(16, 28)
                    ctx.stroke()
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(4, 16)
                    ctx.lineTo(28, 16)
                    ctx.stroke()
                    
                    
                    ctx.fillStyle = "#ff2222"
                    ctx.beginPath()
                    ctx.arc(16, 16, 4, 0, Math.PI * 2)
                    ctx.fill()
                    
                    
                    ctx.strokeStyle = "rgba(255, 68, 68, 0.5)"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.arc(16, 16, 7, 0, Math.PI * 2)
                    ctx.stroke()
                    
                    
                    ctx.strokeStyle = "#e74c3c"
                    ctx.lineWidth = 2
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(8, 12)
                    ctx.lineTo(8, 8)
                    ctx.lineTo(12, 8)
                    ctx.stroke()
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(20, 8)
                    ctx.lineTo(24, 8)
                    ctx.lineTo(24, 12)
                    ctx.stroke()
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(8, 20)
                    ctx.lineTo(8, 24)
                    ctx.lineTo(12, 24)
                    ctx.stroke()
                    
                    
                    ctx.beginPath()
                    ctx.moveTo(20, 24)
                    ctx.lineTo(24, 24)
                    ctx.lineTo(24, 20)
                    ctx.stroke()
                }
                
                Component.onCompleted: requestPaint()
            }
        }
        
        
        Canvas {
            id: guardCursor
            visible: gameView.cursorMode === "guard"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                
                ctx.fillStyle = "#3498db"
                ctx.strokeStyle = "#2980b9"
                ctx.lineWidth = 2
                
                
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
        
        
        Canvas {
            id: patrolCursor
            visible: gameView.cursorMode === "patrol"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                
                ctx.strokeStyle = "#27ae60"
                ctx.lineWidth = 2
                
                
                ctx.beginPath()
                ctx.arc(16, 16, 10, 0, Math.PI * 2)
                ctx.stroke()
                
                
                ctx.fillStyle = "#27ae60"
                
                
                ctx.beginPath()
                ctx.moveTo(26, 16)
                ctx.lineTo(22, 13)
                ctx.lineTo(22, 19)
                ctx.closePath()
                ctx.fill()
                
                
                ctx.beginPath()
                ctx.moveTo(6, 16)
                ctx.lineTo(10, 13)
                ctx.lineTo(10, 19)
                ctx.closePath()
                ctx.fill()
                
                
                ctx.beginPath()
                ctx.arc(16, 16, 3, 0, Math.PI * 2)
                ctx.fill()
            }
            Component.onCompleted: requestPaint()
        }
    }
    
    
    
    
    Keys.onPressed: function(event) {
        if (typeof game === 'undefined') return
        var yawStep = event.modifiers & Qt.ShiftModifier ? 4 : 2
        var panStep = 0.6
        switch (event.key) {
            
            case Qt.Key_Escape:
                if (typeof mainWindow !== 'undefined' && !mainWindow.menuVisible) {
                    mainWindow.menuVisible = true
                    event.accepted = true
                }
                break
            
            case Qt.Key_Space:
                if (typeof mainWindow !== 'undefined') {
                    mainWindow.gamePaused = !mainWindow.gamePaused
                    gameView.setPaused(mainWindow.gamePaused)
                    event.accepted = true
                }
                break
            
            case Qt.Key_W: game.cameraMove(0, panStep);  event.accepted = true; break
            case Qt.Key_S: game.cameraMove(0, -panStep); event.accepted = true; break
            case Qt.Key_A: game.cameraMove(-panStep, 0); event.accepted = true; break
            case Qt.Key_D: game.cameraMove(panStep, 0);  event.accepted = true; break
            
            case Qt.Key_Up:    game.cameraMove(0, panStep);  event.accepted = true; break
            case Qt.Key_Down:  game.cameraMove(0, -panStep); event.accepted = true; break
            case Qt.Key_Left:  game.cameraMove(-panStep, 0); event.accepted = true; break
            case Qt.Key_Right: game.cameraMove(panStep, 0);  event.accepted = true; break
            
            case Qt.Key_Q: game.cameraYaw(-yawStep); event.accepted = true; break
            case Qt.Key_E: game.cameraYaw(yawStep);  event.accepted = true; break
            
            case Qt.Key_R: game.cameraElevate(0.5);  event.accepted = true; break
            case Qt.Key_F: game.cameraElevate(-0.5); event.accepted = true; break
        }
    }
    
    focus: true
}
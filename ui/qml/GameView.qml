import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Item {
    id: gameView
    objectName: "GameView"
    
    property bool isPaused: false
    property real gameSpeed: 1.0
    
    signal mapClicked(real x, real y)
    signal unitSelected(int unitId)
    signal areaSelected(real x1, real y1, real x2, real y2)
    
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
            height: 120
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
            }
        }
        
        MouseArea {
            id: mouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            propagateComposedEvents: true
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
                    isSelecting = true
                    startX = mouse.x
                    startY = mouse.y
                    selectionBox.x = startX
                    selectionBox.y = startY
                    selectionBox.width = 0
                    selectionBox.height = 0
                    selectionBox.visible = true
                } else if (mouse.button === Qt.RightButton) {
                    if (typeof game !== 'undefined' && game.onRightClick) {
                        game.onRightClick(mouse.x, mouse.y)
                    }
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
                        // Point selection
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
    
    // Edge scrolling handled by Main.qml overlay for smoother behavior and to bypass overlays
    
    // Keyboard handling
    Keys.onPressed: function(event) {
        if (typeof game === 'undefined') return
        var yawStep = event.modifiers & Qt.ShiftModifier ? 4 : 2
        var panStep = 0.6
        switch (event.key) {
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
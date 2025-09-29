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
    }
    
    function setGameSpeed(speed) {
        gameSpeed = speed
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
        
        // Placeholder text (disabled by default to not cover GL)
        // Text {
        //     anchors.centerIn: parent
        //     text: "3D Game World\n(OpenGL Render Area)\n\nPress WASD to move camera\nMouse to look around\nScroll to zoom"
        //     color: "white"
        //     font.pointSize: 16
        //     horizontalAlignment: Text.AlignHCenter
        // }
        
        // Camera controls info overlays
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
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
                    } else {
                        // Point selection
                        mapClicked(mouse.x, mouse.y)
                        if (typeof game !== 'undefined' && game.onMapClicked) {
                            game.onMapClicked(mouse.x, mouse.y)
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
    
    // Edge scrolling areas
    MouseArea {
        id: leftEdge
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 10
        hoverEnabled: true
        
        onEntered: {
            scrollTimer.direction = "left"
            scrollTimer.start()
        }
        onExited: scrollTimer.stop()
    }
    
    MouseArea {
        id: rightEdge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 10
        hoverEnabled: true
        
        onEntered: {
            scrollTimer.direction = "right"
            scrollTimer.start()
        }
        onExited: scrollTimer.stop()
    }
    
    MouseArea {
        id: topEdge
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 10
        hoverEnabled: true
        
        onEntered: {
            scrollTimer.direction = "up"
            scrollTimer.start()
        }
        onExited: scrollTimer.stop()
    }
    
    MouseArea {
        id: bottomEdge
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 10
        hoverEnabled: true
        
        onEntered: {
            scrollTimer.direction = "down"
            scrollTimer.start()
        }
        onExited: scrollTimer.stop()
    }
    
    Timer {
        id: scrollTimer
        interval: 16 // ~60 FPS
        repeat: true
        
        property string direction: ""
        
        onTriggered: {
            // Handle camera movement based on direction
            console.log("Edge scroll:", direction)
        }
    }
    
    // Keyboard handling
    Keys.onPressed: function(event) {
        switch (event.key) {
            case Qt.Key_W:
                console.log("Move camera forward")
                break
            case Qt.Key_S:
                console.log("Move camera backward")
                break
            case Qt.Key_A:
                console.log("Move camera left")
                break
            case Qt.Key_D:
                console.log("Move camera right")
                break
            case Qt.Key_Q:
                console.log("Rotate camera left")
                break
            case Qt.Key_E:
                console.log("Rotate camera right")
                break
            case Qt.Key_R:
                console.log("Move camera up")
                break
            case Qt.Key_F:
                console.log("Move camera down")
                break
        }
    }
    
    focus: true
}
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: overlay
    
    property var minimapProvider: null
    visible: false
    
    signal closed()
    signal cameraMoveTo(real worldX, real worldZ)
    signal sendTroopsTo(real worldX, real worldZ)
    
    // Semi-transparent background
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.7
        
        MouseArea {
            anchors.fill: parent
            onClicked: overlay.closed()
        }
    }
    
    // Main minimap container
    Rectangle {
        id: mapContainer
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 800)
        height: Math.min(parent.height * 0.8, 600)
        color: "#0E1C1E"
        radius: 12
        border.width: 3
        border.color: "#3498db"
        
        // Header
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 50
            color: "#0a0f14"
            radius: 12
            
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.radius
                color: parent.color
            }
            
            Label {
                anchors.centerIn: parent
                text: qsTr("Strategic Map")
                color: "#eaf6ff"
                font.pixelSize: 18
                font.bold: true
            }
            
            Button {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 10
                width: 36
                height: 36
                text: "✕"
                font.pixelSize: 20
                focusPolicy: Qt.NoFocus
                
                background: Rectangle {
                    color: parent.pressed ? "#e74c3c" : parent.hovered ? "#c0392b" : "transparent"
                    radius: 18
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "#ecf0f1"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: parent.font
                }
                
                onClicked: overlay.closed()
            }
        }
        
        // Large minimap canvas
        Rectangle {
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.margins: 8
            color: "#0f1a22"
            radius: 8
            border.width: 1
            border.color: "#2c3e50"
            clip: true
            
            Canvas {
                id: largeCanvas
                anchors.fill: parent
                anchors.margins: 2
                
                onPaint: {
                    if (!minimapProvider) {
                        return;
                    }
                    
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    
                    // Draw terrain background
                    ctx.fillStyle = "#1a2f38";
                    ctx.fillRect(0, 0, width, height);
                    
                    // Draw grid
                    ctx.strokeStyle = "#0f2430";
                    ctx.lineWidth = 1;
                    var gridSize = 15;
                    for (var i = 0; i <= width; i += width / gridSize) {
                        ctx.beginPath();
                        ctx.moveTo(i, 0);
                        ctx.lineTo(i, height);
                        ctx.stroke();
                    }
                    for (var j = 0; j <= height; j += height / gridSize) {
                        ctx.beginPath();
                        ctx.moveTo(0, j);
                        ctx.lineTo(width, j);
                        ctx.stroke();
                    }
                    
                    // Draw buildings (larger)
                    var buildings = minimapProvider.buildings;
                    for (var i = 0; i < buildings.length; i++) {
                        var building = buildings[i];
                        var pos = minimapProvider.worldToMinimap(
                            building.x, building.z, width, height);
                        
                        ctx.fillStyle = building.color;
                        ctx.fillRect(pos.x - 6, pos.y - 6, 12, 12);
                        
                        // Draw border for buildings
                        ctx.strokeStyle = "#ffffff";
                        ctx.lineWidth = 1;
                        ctx.strokeRect(pos.x - 6, pos.y - 6, 12, 12);
                    }
                    
                    // Draw units (larger dots)
                    var units = minimapProvider.units;
                    for (var i = 0; i < units.length; i++) {
                        var unit = units[i];
                        var pos = minimapProvider.worldToMinimap(
                            unit.x, unit.z, width, height);
                        
                        ctx.fillStyle = unit.color;
                        ctx.beginPath();
                        ctx.arc(pos.x, pos.y, 4, 0, 2 * Math.PI);
                        ctx.fill();
                        
                        // Draw border for units
                        ctx.strokeStyle = "#ffffff";
                        ctx.lineWidth = 1;
                        ctx.stroke();
                    }
                    
                    // Draw viewport rectangle (more prominent)
                    var vp = minimapProvider.viewport;
                    var vpMin = minimapProvider.worldToMinimap(
                        vp.x, vp.y, width, height);
                    var vpMax = minimapProvider.worldToMinimap(
                        vp.x + vp.width, vp.y + vp.height, width, height);
                    
                    ctx.strokeStyle = "#3498db";
                    ctx.lineWidth = 3;
                    ctx.strokeRect(vpMin.x, vpMin.y, 
                                   vpMax.x - vpMin.x, vpMax.y - vpMin.y);
                    
                    // Fill viewport with semi-transparent highlight
                    ctx.fillStyle = "rgba(52, 152, 219, 0.2)";
                    ctx.fillRect(vpMin.x, vpMin.y, 
                                 vpMax.x - vpMin.x, vpMax.y - vpMin.y);
                }
                
                Connections {
                    target: minimapProvider
                    function onUnitsChanged() { largeCanvas.requestPaint(); }
                    function onBuildingsChanged() { largeCanvas.requestPaint(); }
                    function onViewportChanged() { largeCanvas.requestPaint(); }
                }
            }
            
            MouseArea {
                id: mapMouseArea
                anchors.fill: parent
                hoverEnabled: true
                
                property bool rightButton: false
                
                onDoubleClicked: function(mouse) {
                    overlay.closed();
                }
                
                onClicked: function(mouse) {
                    if (!minimapProvider) {
                        return;
                    }
                    
                    var worldPos = minimapProvider.minimapToWorld(
                        mouse.x - 2, mouse.y - 2,
                        width - 4, height - 4);
                    
                    // Left click = move camera
                    // Right click = send troops
                    if (mouse.button === Qt.RightButton) {
                        overlay.sendTroopsTo(worldPos.x, worldPos.y);
                    } else {
                        overlay.cameraMoveTo(worldPos.x, worldPos.y);
                    }
                }
                
                // Show cursor feedback
                cursorShape: Qt.CrossCursor
            }
            
            // No data label
            Label {
                anchors.centerIn: parent
                text: qsTr("Loading map data...")
                color: "#3f5362"
                font.pixelSize: 16
                font.bold: true
                visible: !minimapProvider
            }
        }
        
        // Footer with instructions
        Rectangle {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 40
            color: "#0a0f14"
            radius: 12
            
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.radius
                color: parent.color
            }
            
            Row {
                anchors.centerIn: parent
                spacing: 20
                
                Label {
                    text: "🖱️ Left Click: Move Camera"
                    color: "#86a7b6"
                    font.pixelSize: 12
                }
                
                Rectangle {
                    width: 2
                    height: 20
                    color: "#2c3e50"
                }
                
                Label {
                    text: "🖱️ Right Click: Send Troops"
                    color: "#86a7b6"
                    font.pixelSize: 12
                }
                
                Rectangle {
                    width: 2
                    height: 20
                    color: "#2c3e50"
                }
                
                Label {
                    text: "ESC / Double-Click: Close"
                    color: "#86a7b6"
                    font.pixelSize: 12
                }
            }
        }
    }
    
    // Update timer
    Timer {
        interval: 100
        running: overlay.visible && minimapProvider !== null
        repeat: true
        onTriggered: {
            if (minimapProvider) {
                minimapProvider.refresh();
            }
        }
    }
    
    // ESC key handler
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            overlay.closed();
            event.accepted = true;
        }
    }
    
    Component.onCompleted: {
        focus = true;
    }
    
    onVisibleChanged: {
        if (visible) {
            focus = true;
        }
    }
}

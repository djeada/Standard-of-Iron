import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: minimap

    property var minimapProvider: null
    property bool clickable: false
    
    signal doubleClicked()
    signal clicked(real worldX, real worldZ)

    Rectangle {
        id: background
        anchors.fill: parent
        color: "#0a0f14"
        radius: 8
        border.width: 2
        border.color: "#3498db"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 6
            color: "#0f1a22"
            clip: true

            Canvas {
                id: minimapCanvas
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

                    // Draw grid for visual reference
                    ctx.strokeStyle = "#0f2430";
                    ctx.lineWidth = 0.5;
                    var gridSize = 10;
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

                    // Draw buildings
                    var buildings = minimapProvider.buildings;
                    for (var i = 0; i < buildings.length; i++) {
                        var building = buildings[i];
                        var pos = minimapProvider.worldToMinimap(
                            building.x, building.z, width, height);
                        
                        ctx.fillStyle = building.color;
                        ctx.fillRect(pos.x - 3, pos.y - 3, 6, 6);
                    }

                    // Draw units as dots
                    var units = minimapProvider.units;
                    for (var i = 0; i < units.length; i++) {
                        var unit = units[i];
                        var pos = minimapProvider.worldToMinimap(
                            unit.x, unit.z, width, height);
                        
                        ctx.fillStyle = unit.color;
                        ctx.beginPath();
                        ctx.arc(pos.x, pos.y, 2, 0, 2 * Math.PI);
                        ctx.fill();
                    }

                    // Draw viewport rectangle
                    var vp = minimapProvider.viewport;
                    var vpMin = minimapProvider.worldToMinimap(
                        vp.x, vp.y, width, height);
                    var vpMax = minimapProvider.worldToMinimap(
                        vp.x + vp.width, vp.y + vp.height, width, height);
                    
                    ctx.strokeStyle = "#3498db";
                    ctx.lineWidth = 2;
                    ctx.strokeRect(vpMin.x, vpMin.y, 
                                   vpMax.x - vpMin.x, vpMax.y - vpMin.y);
                }

                Connections {
                    target: minimapProvider
                    function onUnitsChanged() { minimapCanvas.requestPaint(); }
                    function onBuildingsChanged() { minimapCanvas.requestPaint(); }
                    function onViewportChanged() { minimapCanvas.requestPaint(); }
                }
            }

            // Label when no data
            Label {
                anchors.centerIn: parent
                text: minimapProvider ? "" : qsTr("MINIMAP")
                color: "#3f5362"
                font.pixelSize: 11
                font.bold: true
                visible: !minimapProvider
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            enabled: minimap.clickable

            onDoubleClicked: {
                minimap.doubleClicked();
            }

            onClicked: function(mouse) {
                if (!minimapProvider || !minimap.clickable) {
                    return;
                }

                var worldPos = minimapProvider.minimapToWorld(
                    mouse.x - 4, mouse.y - 4,
                    width - 8, height - 8);
                
                minimap.clicked(worldPos.x, worldPos.y);
            }
        }
    }

    // Update timer for dynamic changes
    Timer {
        interval: 100
        running: minimapProvider !== null && visible
        repeat: true
        onTriggered: {
            if (minimapProvider) {
                minimapProvider.refresh();
            }
        }
    }
}

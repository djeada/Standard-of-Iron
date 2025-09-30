import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud
    
    signal pauseToggled()
    signal speedChanged(real speed)
    signal unitCommand(string command)
    signal recruit(string unitType)
    
    property bool gameIsPaused: false
    property real currentSpeed: 1.0
    // Tick to refresh bindings when selection changes in engine
    property int selectionTick: 0

    Connections {
        target: (typeof game !== 'undefined') ? game : null
        function onSelectedUnitsChanged() { selectionTick += 1 }
    }

    // Periodic refresh to update production timers and counters while building
    Timer {
        id: productionRefresh
        interval: 100
        repeat: true
        running: true
        onTriggered: selectionTick += 1
    }
    
    // Top panel
    Rectangle {
        id: topPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 1
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10
            
            // Pause/Play button
            Button {
                text: gameIsPaused ? "▶" : "⏸"
                font.pointSize: 16
                focusPolicy: Qt.NoFocus
                onClicked: {
                    gameIsPaused = !gameIsPaused
                    pauseToggled()
                }
            }
            
            Rectangle {
                width: 1
                height: parent.height * 0.6
                color: "#34495e"
            }
            
            // Speed controls
            Text {
                text: "Speed:"
                color: "white"
                font.pointSize: 12
            }
            
            Button {
                text: "0.5x"
                enabled: !gameIsPaused
                checkable: true
                checked: currentSpeed === 0.5 && !gameIsPaused
                focusPolicy: Qt.NoFocus
                onClicked: {
                    currentSpeed = 0.5
                    speedChanged(currentSpeed)
                }
            }
            
            Button {
                text: "1x"
                enabled: !gameIsPaused
                checkable: true
                checked: currentSpeed === 1.0 && !gameIsPaused
                focusPolicy: Qt.NoFocus
                onClicked: {
                    currentSpeed = 1.0
                    speedChanged(currentSpeed)
                }
            }
            
            Button {
                text: "2x"
                enabled: !gameIsPaused
                checkable: true
                checked: currentSpeed === 2.0 && !gameIsPaused
                focusPolicy: Qt.NoFocus
                onClicked: {
                    currentSpeed = 2.0
                    speedChanged(currentSpeed)
                }
            }
            
            Rectangle {
                width: 1
                height: parent.height * 0.6
                color: "#34495e"
            }
            
            // Resources display (placeholder)
            Text {
                text: "Resources: 1000 Gold, 500 Wood"
                color: "white"
                font.pointSize: 12
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // Minimap placeholder
            Rectangle {
                width: 120
                height: parent.height * 0.8
                color: "#1a252f"
                border.color: "#34495e"
                border.width: 1
                
                Text {
                    anchors.centerIn: parent
                    text: "Minimap"
                    color: "#7f8c8d"
                    font.pointSize: 10
                }
            }
        }
    }
    
    // Bottom panel - Selection and orders
    Rectangle {
        id: bottomPanel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 120
        color: "#2c3e50"
        border.color: "#34495e"
        border.width: 1
        // Allow edge-scroll MouseArea under it to still receive hover at the very edge
        // by adding a tiny pass-through strip
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 8
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            propagateComposedEvents: true
            onEntered: { if (typeof game !== 'undefined') {/* noop - GameView bottomEdge handles scroll */} }
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10
            
            // Unit selection panel
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: "#34495e"
                border.color: "#1a252f"
                border.width: 1
                
                Column {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4
                    
                    Text {
                        text: "Selected Units"
                        color: "white"
                        font.pointSize: 10
                        font.bold: true
                    }
                    
                    ScrollView {
                        width: parent.width
                        height: parent.height - 20
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                        
                        ListView {
                            id: selectedUnitsList
                            model: (typeof game !== 'undefined' && game.selectedUnitsModel) ? game.selectedUnitsModel : null
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick
                            
                            delegate: Rectangle {
                                width: selectedUnitsList.width
                                height: 25
                                color: "#1a252f"
                                
                                Row {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 4
                                    spacing: 4
                                    
                                    Text {
                                        text: (typeof name !== 'undefined') ? name : "Unit"
                                        color: "white"
                                        font.pointSize: 9
                                    }
                                    
                                    Rectangle {
                                        width: 40
                                        height: 8
                                        color: "#e74c3c"
                                        
                                        Rectangle {
                                            width: parent.width * (typeof healthRatio !== 'undefined' ? healthRatio : 0)
                                            height: parent.height
                                            color: "#27ae60"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Command buttons
            GridLayout {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 4
                columnSpacing: 4
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Move"
                    focusPolicy: Qt.NoFocus
                    onClicked: unitCommand("move")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Attack"
                    focusPolicy: Qt.NoFocus
                    onClicked: unitCommand("attack")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Stop"
                    focusPolicy: Qt.NoFocus
                    onClicked: unitCommand("stop")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Follow"
                    focusPolicy: Qt.NoFocus
                    checkable: true
                    onToggled: { if (typeof game !== 'undefined' && game.cameraFollowSelection) game.cameraFollowSelection(checked) }
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Patrol"
                    focusPolicy: Qt.NoFocus
                    onClicked: unitCommand("patrol")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Guard"
                    focusPolicy: Qt.NoFocus
                    onClicked: unitCommand("guard")
                }
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // Production panel (contextual: shows when a Barracks is selected)
            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: "#34495e"
                border.color: "#1a252f"
                border.width: 1
                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6
                    Text { id: prodHeader; text: "Production"; color: "white"; font.pointSize: 11; font.bold: true }
                    ScrollView {
                        id: prodScroll
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: prodHeader.bottom
                        anchors.bottom: parent.bottom
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                        Column {
                            width: prodScroll.width
                            spacing: 6
                            // Show recruit buttons only when a barracks is in selection
                            Repeater {
                                // Include selectionTick in binding so we refresh when selection changes
                                model: (selectionTick, (typeof game !== 'undefined' && game.hasSelectedType && game.hasSelectedType("barracks"))) ? 1 : 0
                                delegate: Column {
                                    spacing: 6
                                    property var prod: (selectionTick, (typeof game !== 'undefined' && game.getSelectedProductionState) ? game.getSelectedProductionState() : ({}))
                                    // Production button
                                    Button {
                                        id: recruitBtn
                                        text: "Recruit Archer"
                                        focusPolicy: Qt.NoFocus
                                        enabled: (function(){
                                            if (typeof prod === 'undefined' || !prod) return false
                                            if (!prod.hasBarracks) return false
                                            if (prod.inProgress) return false
                                            if (prod.producedCount >= prod.maxUnits) return false
                                            return true
                                        })()
                                        onClicked: recruit("archer")
                                        onPressed: selectionTick += 1
                                    }
                                    // Progress bar for build timer
                                    Rectangle {
                                        width: 180; height: 8; radius: 4
                                        color: "#1a252f"
                                        border.color: "#2c3e50"; border.width: 1
                                        visible: prod.inProgress
                                        Rectangle {
                                            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                                            height: parent.height
                                            width: parent.width * (prod.buildTime > 0 ? (1.0 - Math.max(0, prod.timeRemaining) / prod.buildTime) : 0)
                                            color: "#27ae60"
                                            radius: 4
                                        }
                                    }
                                    // Timer + population cap display
                                    Row {
                                        spacing: 8
                                        Text { text: prod.inProgress ? ("Time left: " + Math.max(0, prod.timeRemaining).toFixed(1) + "s") : ("Build time: " + (prod.buildTime || 0).toFixed(0) + "s"); color: "#bdc3c7"; font.pointSize: 9 }
                                        Text { text: (prod.producedCount || 0) + "/" + (prod.maxUnits || 0); color: "#bdc3c7"; font.pointSize: 9 }
                                    }
                                    // Cap reached message
                                    Text { text: (prod.producedCount >= prod.maxUnits) ? "Cap reached" : ""; color: "#e67e22"; font.pointSize: 9 }

                                    // Set Rally toggle: when active, next click in game view sets rally
                                    Row {
                                        spacing: 6
                                        Button {
                                            text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "Click map: set rally (Esc to cancel)" : "Set Rally"
                                            focusPolicy: Qt.NoFocus
                                            enabled: !!prod.hasBarracks
                                            onClicked: if (typeof gameView !== 'undefined') gameView.setRallyMode = !gameView.setRallyMode
                                        }
                                        Text { text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "Click on the map" : ""; color: "#bdc3c7"; font.pointSize: 9 }
                                    }
                                }
                            }
                            // Fallback info if no production building
                            Item { 
                                visible: (selectionTick, (typeof game === 'undefined' || !game.hasSelectedType || !game.hasSelectedType("barracks"))); 
                                Layout.fillWidth: true
                                Text { text: "No production"; color: "#7f8c8d"; anchors.horizontalCenter: parent.horizontalCenter; font.pointSize: 10 }
                            }
                        }
                    }
                }
            }
        }
    }
}
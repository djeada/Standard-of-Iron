import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud
    
    signal pauseToggled()
    signal speedChanged(real speed)
    signal unitCommand(string command)
    
    property bool gameIsPaused: false
    property real currentSpeed: 1.0
    
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
                onClicked: {
                    currentSpeed = 0.5
                    speedChanged(currentSpeed)
                }
            }
            
            Button {
                text: "1x"
                enabled: !gameIsPaused
                onClicked: {
                    currentSpeed = 1.0
                    speedChanged(currentSpeed)
                }
            }
            
            Button {
                text: "2x"
                enabled: !gameIsPaused
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
                        
                        ListView {
                            id: selectedUnitsList
                            model: ListModel {
                                ListElement { name: "Warrior"; health: 100; maxHealth: 100 }
                                ListElement { name: "Archer"; health: 75; maxHealth: 80 }
                            }
                            
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
                                        text: model.name
                                        color: "white"
                                        font.pointSize: 9
                                    }
                                    
                                    Rectangle {
                                        width: 40
                                        height: 8
                                        color: "#e74c3c"
                                        
                                        Rectangle {
                                            width: parent.width * (model.health / model.maxHealth)
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
                    onClicked: unitCommand("move")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Attack"
                    onClicked: unitCommand("attack")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Stop"
                    onClicked: unitCommand("stop")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Hold"
                    onClicked: unitCommand("hold")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Patrol"
                    onClicked: unitCommand("patrol")
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: "Guard"
                    onClicked: unitCommand("guard")
                }
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // Production panel (placeholder)
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: "#34495e"
                border.color: "#1a252f"
                border.width: 1
                
                Text {
                    anchors.centerIn: parent
                    text: "Building/Production"
                    color: "#7f8c8d"
                    font.pointSize: 10
                }
            }
        }
    }
}
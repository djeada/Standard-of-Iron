import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.UI 1.0

// ═══════════════════════════════════════════════════════════════
// PRODUCTION PANEL COMPONENT
// Displays production queue, timer, unit grid, and rally controls
// ═══════════════════════════════════════════════════════════════
Rectangle {
    id: productionPanel
    
    color: "#0f1419"
    border.color: "#3498db"
    border.width: 2
    radius: 6

    // Properties to be passed from parent
    property int selectionTick: 0
    property var gameInstance: null
    
    // Signals
    signal recruitUnit(string unitType)
    signal rallyModeToggled()
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        
        Column {
            width: productionPanel.width - 20  // Account for margins
            spacing: 8

            // ───────────────────────────────────────────────────────
            // 1️⃣ PRODUCTION STATUS (Queue + Timer Bar)
            // ───────────────────────────────────────────────────────
            Rectangle {
                width: parent.width
                height: productionContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: hasBarracks

                property bool hasBarracks: (productionPanel.selectionTick, 
                                           (productionPanel.gameInstance && 
                                            productionPanel.gameInstance.hasSelectedType && 
                                            productionPanel.gameInstance.hasSelectedType("barracks")))

                Column {
                    id: productionContent
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 10
                    width: parent.width - 16

                    property var prod: (productionPanel.selectionTick, 
                                       (productionPanel.gameInstance && productionPanel.gameInstance.getSelectedProductionState) 
                                       ? productionPanel.gameInstance.getSelectedProductionState() 
                                       : ({}))

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "PRODUCTION QUEUE"
                        color: "#3498db"
                        font.pointSize: 8
                        font.bold: true
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        // Queue icons
                        Repeater {
                            model: 5

                            Rectangle {
                                width: 36
                                height: 36
                                radius: 6
                                
                                property int queueTotal: (productionContent.prod.inProgress ? 1 : 0) + (productionContent.prod.queueSize || 0)
                                property bool isOccupied: index < queueTotal
                                property bool isProducing: index === 0 && productionContent.prod.inProgress
                                
                                color: isProducing ? "#27ae60" : (isOccupied ? "#2c3e50" : "#1a1a1a")
                                border.color: isProducing ? "#229954" : (isOccupied ? "#4a6572" : "#2a2a2a")
                                border.width: 2

                                // Pulsing animation for producing unit
                                SequentialAnimation on opacity {
                                    running: isProducing
                                    loops: Animation.Infinite

                                    NumberAnimation {
                                        from: 0.7
                                        to: 1.0
                                        duration: 800
                                    }
                                    NumberAnimation {
                                        from: 1.0
                                        to: 0.7
                                        duration: 800
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (!parent.isOccupied) {
                                            return "·";
                                        }
                                        
                                        var unitType;
                                        if (index === 0 && productionContent.prod.inProgress) {
                                            unitType = productionContent.prod.productType || "archer";
                                        } else {
                                            var queueIndex = productionContent.prod.inProgress ? index - 1 : index;
                                            if (productionContent.prod.productionQueue && productionContent.prod.productionQueue[queueIndex]) {
                                                unitType = productionContent.prod.productionQueue[queueIndex];
                                            } else {
                                                unitType = "archer";
                                            }
                                        }
                                        
                                        if (typeof Theme !== 'undefined' && Theme.unitIcons) {
                                            return Theme.unitIcons[unitType] || Theme.unitIcons["default"] || "👤";
                                        }
                                        return "🏹";
                                    }
                                    color: parent.isProducing ? "#ffffff" : (parent.isOccupied ? "#bdc3c7" : "#3a3a3a")
                                    font.pointSize: parent.isOccupied ? 16 : 20
                                    font.bold: parent.isProducing
                                }

                                // Slot number indicator
                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    text: (index + 1).toString()
                                    color: parent.isOccupied ? "#7f8c8d" : "#2a2a2a"
                                    font.pointSize: 7
                                    font.bold: true
                                }
                            }
                        }
                    }

                    // Queue status text
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        property int queueTotal: (productionContent.prod.inProgress ? 1 : 0) + (productionContent.prod.queueSize || 0)
                        text: queueTotal + " / 5"
                        color: queueTotal >= 5 ? "#e74c3c" : "#bdc3c7"
                        font.pointSize: 9
                        font.bold: queueTotal >= 5
                    }

                    // ───────────────────────────────────────────────────────
                    // Progress Bar (integrated in same area)
                    // ───────────────────────────────────────────────────────
                    Rectangle {
                        width: parent.width - 20
                        height: 20
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#0a0f14"
                        border.color: "#2c3e50"
                        border.width: 2
                        visible: productionContent.prod.inProgress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!productionContent.prod.inProgress || productionContent.prod.buildTime <= 0) {
                                    return 0;
                                }
                                var progress = 1 - (Math.max(0, productionContent.prod.timeRemaining) / productionContent.prod.buildTime);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#27ae60"
                            radius: 8

                            // Shimmer effect
                            SequentialAnimation on opacity {
                                running: parent.width > 0
                                loops: Animation.Infinite

                                NumberAnimation {
                                    from: 0.8
                                    to: 1.0
                                    duration: 600
                                }
                                NumberAnimation {
                                    from: 1.0
                                    to: 0.8
                                    duration: 600
                                }
                            }
                        }

                        // Time overlay
                        Text {
                            anchors.centerIn: parent
                            text: productionContent.prod.inProgress ? 
                                  Math.max(0, productionContent.prod.timeRemaining).toFixed(1) + "s" : 
                                  "Idle"
                            color: "#ecf0f1"
                            font.pointSize: 9
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#000000"
                        }
                    }

                    // Production count
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Units Produced: " + (productionContent.prod.producedCount || 0) + " / " + (productionContent.prod.maxUnits || 0)
                        color: (productionContent.prod.producedCount >= productionContent.prod.maxUnits) ? "#e74c3c" : "#bdc3c7"
                        font.pointSize: 8
                    }
                }
            }

            // ───────────────────────────────────────────────────────
            // 3️⃣ UNIT SELECTION GRID (MIDDLE SECTION)
            // ───────────────────────────────────────────────────────
            Rectangle {
                width: parent.width
                height: unitGridContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: hasBarracks

                property bool hasBarracks: (productionPanel.selectionTick, 
                                           (productionPanel.gameInstance && 
                                            productionPanel.gameInstance.hasSelectedType && 
                                            productionPanel.gameInstance.hasSelectedType("barracks")))

                Column {
                    id: unitGridContent
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8

                    property var prod: (productionPanel.selectionTick, 
                                       (productionPanel.gameInstance && productionPanel.gameInstance.getSelectedProductionState) 
                                       ? productionPanel.gameInstance.getSelectedProductionState() 
                                       : ({}))

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "RECRUIT UNITS"
                        color: "#3498db"
                        font.pointSize: 8
                        font.bold: true
                    }

                    // Unit grid (2 columns for future expansion)
                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8

                        // Archer unit button
                        Rectangle {
                            width: 110
                            height: 80
                            radius: 6
                            
                            property int queueTotal: (unitGridContent.prod.inProgress ? 1 : 0) + (unitGridContent.prod.queueSize || 0)
                            property bool isEnabled: unitGridContent.prod.hasBarracks && 
                                                    unitGridContent.prod.producedCount < unitGridContent.prod.maxUnits &&
                                                    queueTotal < 5
                            
                            color: isEnabled ? (archerMouseArea.containsMouse ? "#34495e" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? "#4a6572" : "#2a2a2a"
                            border.width: 2
                            opacity: isEnabled ? 1.0 : 0.5

                            Column {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: (typeof Theme !== 'undefined' && Theme.unitIcons) ? Theme.unitIcons["archer"] || "🏹" : "🏹"
                                    color: parent.parent.parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                    font.pointSize: 24
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Archer"
                                    color: parent.parent.parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                    font.pointSize: 10
                                    font.bold: true
                                }

                                Row {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: 4

                                    Text {
                                        text: "👥"
                                        color: parent.parent.parent.parent.isEnabled ? "#f39c12" : "#5a5a5a"
                                        font.pointSize: 9
                                    }

                                    Text {
                                        text: unitGridContent.prod.villagerCost || 1
                                        color: parent.parent.parent.parent.isEnabled ? "#f39c12" : "#5a5a5a"
                                        font.pointSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            MouseArea {
                                id: archerMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: parent.isEnabled
                                onClicked: productionPanel.recruitUnit("archer")
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor

                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? 
                                             "Recruit Archer\nCost: " + (unitGridContent.prod.villagerCost || 1) + " villagers\nBuild time: " + (unitGridContent.prod.buildTime || 0).toFixed(0) + "s" :
                                             (parent.queueTotal >= 5 ? "Queue is full (5/5)" : (unitGridContent.prod.producedCount >= unitGridContent.prod.maxUnits ? "Unit cap reached" : "Cannot recruit"))
                                ToolTip.delay: 300
                            }

                            // Click effect
                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
                                opacity: archerMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }
                        }

                        // Placeholder for future units (Warrior, Spearman, etc.)
                        Rectangle {
                            width: 110
                            height: 80
                            radius: 6
                            color: "#1a1a1a"
                            border.color: "#2a2a2a"
                            border.width: 2
                            opacity: 0.3

                            Column {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "🔒"
                                    color: "#3a3a3a"
                                    font.pointSize: 24
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Locked"
                                    color: "#3a3a3a"
                                    font.pointSize: 9
                                }
                            }
                        }
                    }
                }
            }

            // ───────────────────────────────────────────────────────
            // 4️⃣ RALLY POINT CONTROLS (BOTTOM SECTION)
            // ───────────────────────────────────────────────────────
            Rectangle {
                width: parent.width
                height: 1
                color: "#34495e"
                visible: hasBarracks

                property bool hasBarracks: (productionPanel.selectionTick, 
                                           (productionPanel.gameInstance && 
                                            productionPanel.gameInstance.hasSelectedType && 
                                            productionPanel.gameInstance.hasSelectedType("barracks")))
            }

            Rectangle {
                width: parent.width
                height: rallyContent.height + 12
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: hasBarracks

                property bool hasBarracks: (productionPanel.selectionTick, 
                                           (productionPanel.gameInstance && 
                                            productionPanel.gameInstance.hasSelectedType && 
                                            productionPanel.gameInstance.hasSelectedType("barracks")))

                Column {
                    id: rallyContent
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 6

                    property var prod: (productionPanel.selectionTick, 
                                       (productionPanel.gameInstance && productionPanel.gameInstance.getSelectedProductionState) 
                                       ? productionPanel.gameInstance.getSelectedProductionState() 
                                       : ({}))

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.parent.width - 20
                        height: 32
                        text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "📍 Click Map to Set Rally" : "📍 Set Rally Point"
                        focusPolicy: Qt.NoFocus
                        enabled: rallyContent.prod.hasBarracks
                        onClicked: productionPanel.rallyModeToggled()

                        background: Rectangle {
                            color: parent.enabled ? (parent.down ? "#16a085" : (parent.hovered ? "#1abc9c" : "#2c3e50")) : "#1a1a1a"
                            radius: 6
                            border.color: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "#1abc9c" : "#34495e"
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pointSize: 9
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#5a5a5a"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: "Set where newly recruited units will gather.\nRight-click to cancel."
                        ToolTip.delay: 500
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "Right-click to cancel" : ""
                        color: "#7f8c8d"
                        font.pointSize: 8
                        font.italic: true
                    }
                }
            }

            // Spacer to push "No production" message to center when no barracks selected
            Item {
                height: 20
                visible: !hasBarracksSelected

                property bool hasBarracksSelected: (productionPanel.selectionTick, 
                                                    (productionPanel.gameInstance && 
                                                     productionPanel.gameInstance.hasSelectedType && 
                                                     productionPanel.gameInstance.hasSelectedType("barracks")))
            }

            // No production message
            Item {
                visible: !hasBarracks
                width: parent.width
                height: 200

                property bool hasBarracks: (productionPanel.selectionTick, 
                                           (productionPanel.gameInstance && 
                                            productionPanel.gameInstance.hasSelectedType && 
                                            productionPanel.gameInstance.hasSelectedType("barracks")))

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🏰"
                        color: "#34495e"
                        font.pointSize: 32
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "No Barracks Selected"
                        color: "#7f8c8d"
                        font.pointSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Select a barracks to recruit units"
                        color: "#5a6c7d"
                        font.pointSize: 9
                    }
                }
            }
        } // End Column
    } // End ScrollView
}

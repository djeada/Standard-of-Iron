import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Item {
    id: hud
    
    signal pauseToggled()
    signal speedChanged(real speed)
    signal commandModeChanged(string mode) // "normal", "attack", "guard", "patrol", "stop"
    signal recruit(string unitType)
    
    property bool gameIsPaused: false
    property real currentSpeed: 1.0
    property string currentCommandMode: "normal"
    
    // Expose panel heights for other overlays (e.g., edge scroll) to avoid stealing input over UI
    property int topPanelHeight: topPanel.height
    property int bottomPanelHeight: bottomPanel.height
    // Tick to refresh bindings when selection changes in engine
    property int selectionTick: 0

    Connections {
        target: (typeof game !== 'undefined') ? game : null
        function onSelectedUnitsChanged() { 
            selectionTick += 1
        }
    }

    // Periodic refresh to update production timers and counters while building
    Timer {
        id: productionRefresh
        interval: 100
        repeat: true
        running: true
        onTriggered: selectionTick += 1
    }
    
    // Top panel - Responsive design
    Rectangle {
        id: topPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(50, parent.height * 0.08)
        color: "#1a1a1a"
        opacity: 0.95
        
        // Gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#2c3e50" }
                GradientStop { position: 1.0; color: "#1a252f" }
            }
            opacity: 0.8
        }
        
        // Border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: "#3498db" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 15
            
            // Pause/Play button
            Button {
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                text: gameIsPaused ? "▶" : "⏸"
                font.pointSize: 18
                font.bold: true
                focusPolicy: Qt.NoFocus
                background: Rectangle {
                    color: parent.pressed ? "#e74c3c" : (parent.hovered ? "#c0392b" : "#34495e")
                    radius: 4
                    border.color: "#2c3e50"
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "#ecf0f1"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    gameIsPaused = !gameIsPaused
                    pauseToggled()
                }
            }
            
            Rectangle {
                width: 2
                Layout.fillHeight: true
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: "#34495e" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
            
            // Speed controls
            Row {
                Layout.preferredWidth: 220
                spacing: 8
                
                Text {
                    text: "Speed:"
                    color: "#ecf0f1"
                    font.pointSize: 11
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                
                ButtonGroup {
                    id: speedGroup
                }
                
                Button {
                    width: 50
                    height: 32
                    text: "0.5x"
                    enabled: !gameIsPaused
                    checkable: true
                    checked: currentSpeed === 0.5 && !gameIsPaused
                    focusPolicy: Qt.NoFocus
                    ButtonGroup.group: speedGroup
                    background: Rectangle {
                        color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50")
                        radius: 4
                        border.color: parent.checked ? "#229954" : "#1a252f"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pointSize: 9
                        font.bold: true
                        color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        currentSpeed = 0.5
                        speedChanged(currentSpeed)
                    }
                }
                
                Button {
                    width: 50
                    height: 32
                    text: "1x"
                    enabled: !gameIsPaused
                    checkable: true
                    checked: currentSpeed === 1.0 && !gameIsPaused
                    focusPolicy: Qt.NoFocus
                    ButtonGroup.group: speedGroup
                    background: Rectangle {
                        color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50")
                        radius: 4
                        border.color: parent.checked ? "#229954" : "#1a252f"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pointSize: 9
                        font.bold: true
                        color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        currentSpeed = 1.0
                        speedChanged(currentSpeed)
                    }
                }
                
                Button {
                    width: 50
                    height: 32
                    text: "2x"
                    enabled: !gameIsPaused
                    checkable: true
                    checked: currentSpeed === 2.0 && !gameIsPaused
                    focusPolicy: Qt.NoFocus
                    ButtonGroup.group: speedGroup
                    background: Rectangle {
                        color: parent.checked ? "#27ae60" : (parent.hovered ? "#34495e" : "#2c3e50")
                        radius: 4
                        border.color: parent.checked ? "#229954" : "#1a252f"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pointSize: 9
                        font.bold: true
                        color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        currentSpeed = 2.0
                        speedChanged(currentSpeed)
                    }
                }
            }
            
            Rectangle {
                width: 2
                Layout.fillHeight: true
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: "#34495e" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
            
            // Camera controls
            Row {
                spacing: 8
                
                Text {
                    text: "Camera:"
                    color: "#ecf0f1"
                    font.pointSize: 11
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                
                Button {
                    width: 70
                    height: 32
                    text: "Follow"
                    checkable: true
                    checked: false
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        color: parent.checked ? "#3498db" : (parent.hovered ? "#34495e" : "#2c3e50")
                        radius: 4
                        border.color: parent.checked ? "#2980b9" : "#1a252f"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pointSize: 9
                        font.bold: true
                        color: "#ecf0f1"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onToggled: { 
                        if (typeof game !== 'undefined' && game.cameraFollowSelection) 
                            game.cameraFollowSelection(checked) 
                    }
                }
            }
            
            Rectangle {
                width: 2
                Layout.fillHeight: true
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: "#34495e" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
            
            // Troop count display
            Text {
                text: "🗡️ " + (typeof game !== 'undefined' ? game.playerTroopCount : 0) + " / " + (typeof game !== 'undefined' ? game.maxTroopsPerPlayer : 0)
                color: {
                    if (typeof game === 'undefined') return "#95a5a6"
                    var count = game.playerTroopCount
                    var max = game.maxTroopsPerPlayer
                    if (count >= max) return "#e74c3c"  // Red when at limit
                    if (count >= max * 0.8) return "#f39c12"  // Orange when near limit
                    return "#2ecc71"  // Green when plenty of room
                }
                font.pointSize: 11
                font.bold: true
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // Minimap placeholder
            Rectangle {
                Layout.preferredWidth: Math.min(140, parent.width * 0.12)
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                color: "#1a252f"
                border.width: 2
                border.color: "#3498db"
                radius: 4
                
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    color: "#0a0f14"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "MINIMAP"
                        color: "#34495e"
                        font.pointSize: 9
                        font.bold: true
                    }
                }
            }
        }
    }
    
    // Bottom panel - Selection and commands
    Rectangle {
        id: bottomPanel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(140, parent.height * 0.20)
        color: "#1a1a1a"
        opacity: 0.95
        
        // Gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a252f" }
                GradientStop { position: 1.0; color: "#2c3e50" }
            }
            opacity: 0.8
        }
        
        // Top border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: "#3498db" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12
            
            // Unit selection panel
            Rectangle {
                Layout.preferredWidth: Math.max(200, parent.width * 0.18)
                Layout.fillHeight: true
                color: "#0f1419"
                border.color: "#3498db"
                border.width: 2
                radius: 6
                
                Column {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6
                    
                    Rectangle {
                        width: parent.width
                        height: 25
                        color: "#1a252f"
                        radius: 4
                        
                        Text {
                            anchors.centerIn: parent
                            text: "SELECTED UNITS"
                            color: "#3498db"
                            font.pointSize: 10
                            font.bold: true
                        }
                    }
                    
                    ScrollView {
                        width: parent.width
                        height: parent.height - 35
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                        
                        ListView {
                            id: selectedUnitsList
                            model: (typeof game !== 'undefined' && game.selectedUnitsModel) ? game.selectedUnitsModel : null
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick
                            spacing: 3
                            
                            delegate: Rectangle {
                                width: selectedUnitsList.width - 10
                                height: 28
                                color: "#1a252f"
                                radius: 4
                                border.color: "#34495e"
                                border.width: 1
                                
                                Row {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 6
                                    spacing: 8
                                    
                                    Text {
                                        text: (typeof name !== 'undefined') ? name : "Unit"
                                        color: "#ecf0f1"
                                        font.pointSize: 9
                                        font.bold: true
                                        width: 80
                                    }
                                    
                                    // Health bar
                                    Rectangle {
                                        width: 60
                                        height: 12
                                        color: "#2c3e50"
                                        radius: 6
                                        border.color: "#1a252f"
                                        border.width: 1
                                        
                                        Rectangle {
                                            width: parent.width * (typeof healthRatio !== 'undefined' ? healthRatio : 0)
                                            height: parent.height
                                            color: {
                                                var ratio = (typeof healthRatio !== 'undefined' ? healthRatio : 0)
                                                if (ratio > 0.6) return "#27ae60"
                                                if (ratio > 0.3) return "#f39c12"
                                                return "#e74c3c"
                                            }
                                            radius: 6
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Command buttons
            Column {
                Layout.preferredWidth: Math.max(260, parent.width * 0.22)
                Layout.fillHeight: true
                spacing: 8
                
                // Command mode indicator
                Rectangle {
                    width: parent.width
                    height: 36
                    color: currentCommandMode === "normal" ? "#0f1419" : "#1a252f"
                    border.color: currentCommandMode === "normal" ? "#34495e" : "#3498db"
                    border.width: 2
                    radius: 6
                    
                    // Glow effect for active mode
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        color: "transparent"
                        border.color: "#3498db"
                        border.width: currentCommandMode !== "normal" ? 1 : 0
                        radius: 8
                        opacity: 0.4
                        visible: currentCommandMode !== "normal"
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: currentCommandMode === "normal" ? "◉ Normal Mode" :
                              currentCommandMode === "attack" ? "⚔️ ATTACK MODE - Click Enemy" :
                              currentCommandMode === "guard" ? "🛡️ GUARD MODE - Click Position" :
                              currentCommandMode === "patrol" ? "🚶 PATROL MODE - Set Waypoints" :
                              "⏹️ STOP COMMAND"
                        color: currentCommandMode === "normal" ? "#7f8c8d" : "#3498db"
                        font.pointSize: currentCommandMode === "normal" ? 10 : 11
                        font.bold: currentCommandMode !== "normal"
                    }
                }
                
                // Command buttons grid
                GridLayout {
                    width: parent.width
                    columns: 3
                    rowSpacing: 6
                    columnSpacing: 6
                    
                    // Helper function for button styling
                    function getButtonColor(btn, baseColor) {
                        if (btn.pressed) return Qt.darker(baseColor, 1.3)
                        if (btn.checked) return baseColor
                        if (btn.hovered) return Qt.lighter(baseColor, 1.2)
                        return "#2c3e50"
                    }
                    
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: "Attack"
                        focusPolicy: Qt.NoFocus
                        enabled: (typeof game !== 'undefined' && game.hasUnitsSelected) ? game.hasUnitsSelected : false
                        checkable: true
                        checked: currentCommandMode === "attack"
                        background: Rectangle {
                            color: parent.enabled ? (parent.checked ? "#e74c3c" : (parent.hovered ? "#c0392b" : "#34495e")) : "#1a252f"
                            radius: 6
                            border.color: parent.checked ? "#c0392b" : "#1a252f"
                            border.width: 2
                        }
                        contentItem: Text {
                            text: "⚔️\n" + parent.text
                            font.pointSize: 8
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            currentCommandMode = checked ? "attack" : "normal"
                            commandModeChanged(currentCommandMode)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: game.hasUnitsSelected ? "Attack enemy units or buildings.\nUnits will chase targets." : "Select units first"
                        ToolTip.delay: 500
                    }
                    
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: "Guard"
                        focusPolicy: Qt.NoFocus
                        enabled: (typeof game !== 'undefined' && game.hasUnitsSelected) ? game.hasUnitsSelected : false
                        checkable: true
                        checked: currentCommandMode === "guard"
                        background: Rectangle {
                            color: parent.enabled ? (parent.checked ? "#3498db" : (parent.hovered ? "#2980b9" : "#34495e")) : "#1a252f"
                            radius: 6
                            border.color: parent.checked ? "#2980b9" : "#1a252f"
                            border.width: 2
                        }
                        contentItem: Text {
                            text: "🛡️\n" + parent.text
                            font.pointSize: 8
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            currentCommandMode = checked ? "guard" : "normal"
                            commandModeChanged(currentCommandMode)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: game.hasUnitsSelected ? "Guard a position.\nUnits will defend from all sides." : "Select units first"
                        ToolTip.delay: 500
                    }
                    
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: "Patrol"
                        focusPolicy: Qt.NoFocus
                        enabled: (typeof game !== 'undefined' && game.hasUnitsSelected) ? game.hasUnitsSelected : false
                        checkable: true
                        checked: currentCommandMode === "patrol"
                        background: Rectangle {
                            color: parent.enabled ? (parent.checked ? "#27ae60" : (parent.hovered ? "#229954" : "#34495e")) : "#1a252f"
                            radius: 6
                            border.color: parent.checked ? "#229954" : "#1a252f"
                            border.width: 2
                        }
                        contentItem: Text {
                            text: "🚶\n" + parent.text
                            font.pointSize: 8
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            currentCommandMode = checked ? "patrol" : "normal"
                            commandModeChanged(currentCommandMode)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: game.hasUnitsSelected ? "Patrol between waypoints.\nClick start and end points." : "Select units first"
                        ToolTip.delay: 500
                    }
                    
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: "Stop"
                        focusPolicy: Qt.NoFocus
                        enabled: (typeof game !== 'undefined' && game.hasUnitsSelected) ? game.hasUnitsSelected : false
                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#d35400" : (parent.hovered ? "#e67e22" : "#34495e")) : "#1a252f"
                            radius: 6
                            border.color: parent.enabled ? "#d35400" : "#1a252f"
                            border.width: 2
                        }
                        contentItem: Text {
                            text: "⏹️\n" + parent.text
                            font.pointSize: 8
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            // Call C++ stop command to actually stop units
                            if (typeof game !== 'undefined' && game.onStopCommand) {
                                game.onStopCommand()
                            }
                            // Reset UI state
                            currentCommandMode = "normal"
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: game.hasUnitsSelected ? "Stop all actions immediately" : "Select units first"
                        ToolTip.delay: 500
                    }
                    
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        text: "Hold"
                        focusPolicy: Qt.NoFocus
                        enabled: (typeof game !== 'undefined' && game.hasUnitsSelected) ? game.hasUnitsSelected : false
                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#8e44ad" : (parent.hovered ? "#9b59b6" : "#34495e")) : "#1a252f"
                            radius: 6
                            border.color: parent.enabled ? "#8e44ad" : "#1a252f"
                            border.width: 2
                        }
                        contentItem: Text {
                            text: "📍\n" + parent.text
                            font.pointSize: 8
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            currentCommandMode = "hold"
                            commandModeChanged(currentCommandMode)
                            Qt.callLater(function() { currentCommandMode = "normal" })
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: game.hasUnitsSelected ? "Hold position and defend" : "Select units first"
                        ToolTip.delay: 500
                    }
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
    
    // Victory/Defeat overlay
    Rectangle {
        id: victoryOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        visible: (typeof game !== 'undefined' && game.victoryState !== "")
        z: 100
        
        Column {
            anchors.centerIn: parent
            spacing: 20
            
            Text {
                id: victoryText
                anchors.horizontalCenter: parent.horizontalCenter
                text: (typeof game !== 'undefined' && game.victoryState === "victory") ? "VICTORY!" : "DEFEAT"
                color: (typeof game !== 'undefined' && game.victoryState === "victory") ? "#27ae60" : "#e74c3c"
                font.pointSize: 48
                font.bold: true
            }
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (typeof game !== 'undefined' && game.victoryState === "victory") ? "Enemy barracks destroyed!" : "Your barracks was destroyed!"
                color: "white"
                font.pointSize: 18
            }
            
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Continue"
                font.pointSize: 14
                focusPolicy: Qt.NoFocus
                onClicked: {
                    // Close overlay (game continues)
                    victoryOverlay.visible = false
                }
            }
        }
    }
}
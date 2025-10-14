import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

RowLayout {
    id: bottomRoot

    property string currentCommandMode
    property int selectionTick
    property bool hasMovableUnits

    signal commandModeChanged(string mode)
    signal recruit(string unitType)

    anchors.fill: parent
    anchors.margins: 10
    spacing: 12

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(240, bottomPanel.width * 0.3)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
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
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: undefined
                anchors.bottom: parent.bottom
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
                                        var ratio = (typeof healthRatio !== 'undefined' ? healthRatio : 0);
                                        if (ratio > 0.6)
                                            return "#27ae60";

                                        if (ratio > 0.3)
                                            return "#f39c12";

                                        return "#e74c3c";
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

    Column {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(320, bottomPanel.width * 0.4)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        spacing: 8

        Rectangle {
            width: parent.width
            height: 36
            color: bottomRoot.currentCommandMode === "normal" ? "#0f1419" : (bottomRoot.currentCommandMode === "attack" ? "#8b1a1a" : "#1a252f")
            border.color: bottomRoot.currentCommandMode === "normal" ? "#34495e" : (bottomRoot.currentCommandMode === "attack" ? "#e74c3c" : "#3498db")
            border.width: 2
            radius: 6
            opacity: bottomRoot.hasMovableUnits ? 1 : 0.5

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: "transparent"
                border.color: bottomRoot.currentCommandMode === "attack" ? "#e74c3c" : "#3498db"
                border.width: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits ? 1 : 0
                radius: 8
                opacity: 0.4
                visible: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits
            }

            Text {
                anchors.centerIn: parent
                text: !bottomRoot.hasMovableUnits ? "◉ Select Troops for Commands" : (bottomRoot.currentCommandMode === "normal" ? "◉ Normal Mode" : bottomRoot.currentCommandMode === "attack" ? "⚔️ ATTACK MODE - Click Enemy" : bottomRoot.currentCommandMode === "guard" ? "🛡️ GUARD MODE - Click Position" : bottomRoot.currentCommandMode === "patrol" ? "🚶 PATROL MODE - Set Waypoints" : "⏹️ STOP COMMAND")
                color: !bottomRoot.hasMovableUnits ? "#5a6c7d" : (bottomRoot.currentCommandMode === "normal" ? "#7f8c8d" : (bottomRoot.currentCommandMode === "attack" ? "#ff6b6b" : "#3498db"))
                font.pointSize: bottomRoot.currentCommandMode === "normal" ? 10 : 11
                font.bold: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits
            }

            SequentialAnimation on opacity {
                running: bottomRoot.currentCommandMode === "attack" && bottomRoot.hasMovableUnits
                loops: Animation.Infinite

                NumberAnimation {
                    from: 0.8
                    to: 1
                    duration: 600
                }

                NumberAnimation {
                    from: 1
                    to: 0.8
                    duration: 600
                }

            }

        }

        GridLayout {
            function getButtonColor(btn, baseColor) {
                if (btn.pressed)
                    return Qt.darker(baseColor, 1.3);

                if (btn.checked)
                    return baseColor;

                if (btn.hovered)
                    return Qt.lighter(baseColor, 1.2);

                return "#2c3e50";
            }

            width: parent.width
            columns: 3
            rowSpacing: 6
            columnSpacing: 6

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Attack"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "attack" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "attack" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? "Attack enemy units or buildings.\nUnits will chase targets." : "Select troops first"
                ToolTip.delay: 500

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

            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Guard"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "guard" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "guard" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? "Guard a position.\nUnits will defend from all sides." : "Select troops first"
                ToolTip.delay: 500

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

            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Patrol"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "patrol" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "patrol" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? "Patrol between waypoints.\nClick start and end points." : "Select troops first"
                ToolTip.delay: 500

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

            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Stop"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                onClicked: {
                    if (typeof game !== 'undefined' && game.onStopCommand)
                        game.onStopCommand();

                    bottomRoot.commandModeChanged("normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? "Stop all actions immediately" : "Select troops first"
                ToolTip.delay: 500

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

            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Hold"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged("hold");
                    Qt.callLater(function() {
                        bottomRoot.commandModeChanged("normal");
                    });
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? "Hold position and defend" : "Select troops first"
                ToolTip.delay: 500

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

            }

        }

    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(240, bottomPanel.width * 0.3)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        color: "#34495e"
        border.color: "#1a252f"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text {
                id: prodHeader

                text: "Production"
                color: "white"
                font.pointSize: 11
                font.bold: true
            }

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

                    Repeater {
                        model: (bottomRoot.selectionTick, (typeof game !== 'undefined' && game.hasSelectedType && game.hasSelectedType("barracks"))) ? 1 : 0

                        delegate: Column {
                            property var prod: (bottomRoot.selectionTick, (typeof game !== 'undefined' && game.getSelectedProductionState) ? game.getSelectedProductionState() : ({
                            }))

                            spacing: 6

                            Button {
                                id: recruitBtn

                                text: "Recruit Archer (" + (prod.villagerCost || 1) + ")"
                                focusPolicy: Qt.NoFocus
                                enabled: (function() {
                                    if (typeof prod === 'undefined' || !prod)
                                        return false;

                                    if (!prod.hasBarracks)
                                        return false;

                                    if (prod.inProgress)
                                        return false;

                                    if (prod.producedCount >= prod.maxUnits)
                                        return false;

                                    return true;
                                })()
                                onClicked: bottomRoot.recruit("archer")
                                ToolTip.visible: hovered
                                ToolTip.text: "Recruit 1 archer unit\nCost: " + (prod.villagerCost || 1) + " villagers"
                                ToolTip.delay: 500
                            }

                            Rectangle {
                                width: 180
                                height: 8
                                radius: 4
                                color: "#1a252f"
                                border.color: "#2c3e50"
                                border.width: 1
                                visible: prod.inProgress

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: parent.height
                                    width: parent.width * (prod.buildTime > 0 ? (1 - Math.max(0, prod.timeRemaining) / prod.buildTime) : 0)
                                    color: "#27ae60"
                                    radius: 4
                                }

                            }

                            Row {
                                spacing: 8

                                Text {
                                    text: prod.inProgress ? ("Time left: " + Math.max(0, prod.timeRemaining).toFixed(1) + "s") : ("Build time: " + (prod.buildTime || 0).toFixed(0) + "s")
                                    color: "#bdc3c7"
                                    font.pointSize: 9
                                }

                                Text {
                                    text: (prod.producedCount || 0) + "/" + (prod.maxUnits || 0)
                                    color: "#bdc3c7"
                                    font.pointSize: 9
                                }

                            }

                            Text {
                                text: (prod.producedCount >= prod.maxUnits) ? "Cap reached" : ""
                                color: "#e67e22"
                                font.pointSize: 9
                            }

                            Row {
                                spacing: 6

                                Button {
                                    text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "Click map to set rally (right-click to cancel)" : "Set Rally"
                                    focusPolicy: Qt.NoFocus
                                    enabled: !!prod.hasBarracks
                                    onClicked: {
                                        if (typeof gameView !== 'undefined')
                                            gameView.setRallyMode = !gameView.setRallyMode;

                                    }
                                }

                                Text {
                                    text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "Click on the map" : ""
                                    color: "#bdc3c7"
                                    font.pointSize: 9
                                }

                            }

                        }

                    }

                    Item {
                        visible: (bottomRoot.selectionTick, (typeof game === 'undefined' || !game.hasSelectedType || !game.hasSelectedType("barracks")))
                        width: parent.width
                        height: 30

                        Text {
                            text: "No production"
                            color: "#7f8c8d"
                            anchors.centerIn: parent
                            font.pointSize: 10
                        }

                    }

                }

            }

        }

    }

}

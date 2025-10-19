import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.UI 1.0

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
                id: holdButton

                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: "Hold"
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                
                property bool isHoldActive: {
                    bottomRoot.selectionTick;
                    return (typeof game !== 'undefined' && game.anySelectedInHoldMode) ? game.anySelectedInHoldMode() : false;
                }
                onClicked: {
                    if (typeof game !== 'undefined' && game.onHoldCommand)
                        game.onHoldCommand();

                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? (isHoldActive ? "Exit hold mode (toggle)" : "Hold position and defend") : "Select troops first"
                ToolTip.delay: 500

                Connections {
                    function onHoldModeChanged(active) {
                        holdButton.isHoldActive = (typeof game !== 'undefined' && game.anySelectedInHoldMode) ? game.anySelectedInHoldMode() : false;
                    }

                    target: (typeof game !== 'undefined') ? game : null
                }

                background: Rectangle {
                    color: {
                        if (!parent.enabled)
                            return "#1a252f";

                        if (parent.isHoldActive)
                            return "#8e44ad";

                        if (parent.pressed)
                            return "#8e44ad";

                        if (parent.hovered)
                            return "#9b59b6";

                        return "#34495e";
                    }
                    radius: 6
                    border.color: parent.enabled ? (parent.isHoldActive ? "#d35400" : "#8e44ad") : "#1a252f"
                    border.width: parent.isHoldActive ? 3 : 2
                }

                contentItem: Text {
                    text: (parent.isHoldActive ? "✓ " : "") + "📍\n" + parent.text
                    font.pointSize: 8
                    font.bold: true
                    color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

            }

        }

    }

    ProductionPanel {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(280, bottomPanel.width * 0.35)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        selectionTick: bottomRoot.selectionTick
        gameInstance: (typeof game !== 'undefined') ? game : null
        onRecruitUnit: function(unitType) {
            bottomRoot.recruit(unitType);
        }
        onRallyModeToggled: {
            if (typeof gameView !== 'undefined')
                gameView.setRallyMode = !gameView.setRallyMode;

        }
    }

}

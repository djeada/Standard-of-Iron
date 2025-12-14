import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

RowLayout {
    id: bottomRoot

    property string currentCommandMode
    property int selectionTick
    property bool hasMovableUnits

    signal commandModeChanged(string mode)
    signal recruit(string unitType)

    function unitIconSource(unitType, nationKey) {
        if (typeof StyleGuide === "undefined" || !StyleGuide.unitIconSources || !unitType)
            return "";

        var sources = StyleGuide.unitIconSources[unitType];
        if (!sources)
            sources = StyleGuide.unitIconSources["default"];

        var key = (nationKey && sources[nationKey]) ? nationKey : "default";
        return sources[key] || "";
    }

    function unitIconEmoji(unitType) {
        if (typeof StyleGuide !== "undefined" && StyleGuide.unitIcons)
            return StyleGuide.unitIcons[unitType] || StyleGuide.unitIcons["default"] || "👤";

        return "👤";
    }

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

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                color: "#1a252f"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: qsTr("SELECTED UNITS")
                    color: "#3498db"
                    font.pointSize: 10
                    font.bold: true
                }

            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListView {
                    id: selectedUnitsList

                    model: (typeof game !== 'undefined' && game.selected_units_model) ? game.selected_units_model : null
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    spacing: 3

                    delegate: Rectangle {
                        id: selectedUnitItem

                        property bool isHovered: false
                        property string unitType: (typeof name !== "undefined") ? name : ""
                        property string nationKey: (typeof nation !== "undefined" && nation !== "") ? nation : "default"

                        width: selectedUnitsList.width - 10
                        height: 28
                        color: isHovered ? "#243346" : "#1a252f"
                        radius: 4
                        border.color: isHovered ? "#4aa3ff" : "#34495e"
                        border.width: isHovered ? 2 : 1

                        MouseArea {
                            id: selectedUnitMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: false
                            cursorShape: Qt.PointingHandCursor
                            onEntered: selectedUnitItem.isHovered = true
                            onExited: selectedUnitItem.isHovered = false
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.LeftButton && typeof game !== 'undefined' && game.select_unit_by_id && typeof unit_id !== 'undefined')
                                    game.select_unit_by_id(unit_id);

                            }
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 6
                            spacing: 8

                            Item {
                                width: 32
                                height: 24

                                Image {
                                    id: selectedUnitIcon

                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    fillMode: Image.PreserveAspectFit
                                    source: bottomRoot.unitIconSource(selectedUnitItem.unitType, selectedUnitItem.nationKey)
                                    visible: source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: bottomRoot.unitIconEmoji(selectedUnitItem.unitType)
                                    color: "#ecf0f1"
                                    font.pixelSize: 16
                                    visible: selectedUnitIcon.source === ""
                                }

                            }

                            Rectangle {
                                width: 60
                                height: 12
                                color: "#2c3e50"
                                radius: 6
                                border.color: "#1a252f"
                                border.width: 1

                                Rectangle {
                                    width: parent.width * (typeof health_ratio !== 'undefined' ? health_ratio : 0)
                                    height: parent.height
                                    color: {
                                        var ratio = (typeof health_ratio !== 'undefined' ? health_ratio : 0);
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
                text: !bottomRoot.hasMovableUnits ? qsTr("◉ Select Troops for Commands") : (bottomRoot.currentCommandMode === "normal" ? qsTr("◉ Normal Mode") : bottomRoot.currentCommandMode === "attack" ? qsTr("⚔️ ATTACK MODE - Click Enemy") : bottomRoot.currentCommandMode === "guard" ? qsTr("🛡️ GUARD MODE - Click Position") : bottomRoot.currentCommandMode === "patrol" ? qsTr("🚶 PATROL MODE - Set Waypoints") : qsTr("⏹️ STOP COMMAND"))
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
                text: qsTr("Attack")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "attack" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "attack" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? qsTr("Attack enemy units or buildings.\nUnits will chase targets.") : qsTr("Select troops first")
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
                text: qsTr("Guard")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "guard" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "guard" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? qsTr("Guard a position.\nUnits will defend from all sides.") : qsTr("Select troops first")
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
                text: qsTr("Patrol")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                checkable: true
                checked: bottomRoot.currentCommandMode === "patrol" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "patrol" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? qsTr("Patrol between waypoints.\nClick start and end points.") : qsTr("Select troops first")
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
                text: qsTr("Stop")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_stop_command)
                        game.on_stop_command();

                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? qsTr("Stop all actions immediately") : qsTr("Select troops first")
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

                property bool isHoldActive: {
                    bottomRoot.selectionTick;
                    return (typeof game !== 'undefined' && game.any_selected_in_hold_mode) ? game.any_selected_in_hold_mode() : false;
                }

                Layout.fillWidth: true
                Layout.preferredHeight: 38
                text: qsTr("Hold")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_hold_command)
                        game.on_hold_command();

                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.hasMovableUnits ? (isHoldActive ? qsTr("Exit hold mode (toggle)") : qsTr("Hold position and defend")) : qsTr("Select troops first")
                ToolTip.delay: 500

                Connections {
                    function onHold_mode_changed(active) {
                        holdButton.isHoldActive = (typeof game !== 'undefined' && game.any_selected_in_hold_mode) ? game.any_selected_in_hold_mode() : false;
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

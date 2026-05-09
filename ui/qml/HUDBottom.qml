import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

RowLayout {
    id: bottomRoot

    property string currentCommandMode
    property int selectionTick
    property bool hasMovableUnits
    readonly property var hs: StyleGuide.historical
    property var modeAvailability: ({
        "canAttack": true,
        "canGuard": true,
        "canHold": true,
        "canPatrol": true,
        "canHeal": true,
        "canBuild": true
    })

    signal commandModeChanged(string mode)
    signal recruit(string unitType)

    function updateModeAvailability() {
        if (typeof game !== 'undefined' && game.get_selected_units_mode_availability)
            modeAvailability = game.get_selected_units_mode_availability();

    }

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

    function unitTypeKeyFromDisplayName(displayName) {
        if (!displayName)
            return "";

        var s = displayName.toString().trim().toLowerCase();
        s = s.replace(/[^a-z0-9]+/g, "_");
        s = s.replace(/^_+|_+$/g, "");
        return s;
    }

    function commandIcon(filename) {
        if (typeof StyleGuide === "undefined" || !filename)
            return "";

        return StyleGuide.iconPath(filename);
    }

    function hasSelectedUnit(type) {
        if (typeof game !== 'undefined' && game.has_selected_type)
            return game.has_selected_type(type);

        return false;
    }

    function commandBannerText() {
        if (!hasMovableUnits)
            return qsTr("No troops selected");

        if (currentCommandMode === "normal")
            return qsTr("Orders ready");

        var orderText = qsTr("Stop command");
        if (currentCommandMode === "attack")
            orderText = qsTr("Attack order");
        else if (currentCommandMode === "guard")
            orderText = qsTr("Guard order");
        else if (currentCommandMode === "patrol")
            orderText = qsTr("Patrol order");
        else if (currentCommandMode === "heal")
            orderText = qsTr("Medic order");
        else if (currentCommandMode === "build")
            orderText = qsTr("Engineer order");

        return orderText;
    }

    function healthColor(ratio) {
        if (ratio > 0.6)
            return "#7F9A5F";

        if (ratio > 0.3)
            return hs.bronze;

        return hs.waxHover;
    }

    function staminaColor(running) {
        return running ? hs.bronze : "#6F8E8C";
    }

    function shouldPulseCommandBanner() {
        return hasMovableUnits && (currentCommandMode === "attack" || currentCommandMode === "guard" || currentCommandMode === "patrol");
    }

    Component.onCompleted: updateModeAvailability()
    onSelectionTickChanged: updateModeAvailability()
    anchors.fill: parent
    anchors.margins: 10
    spacing: 12

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(240, bottomPanel.width * 0.3)
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignTop
        color: hs.parchmentDark
        border.color: hs.bronze
        border.width: 2
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 25
                color: hs.parchmentLight
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: qsTr("SELECTED UNITS")
                    color: hs.bronze
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
                        property string unitDisplayName: (typeof name !== "undefined") ? name : ""
                        property string unitTypeKey: (typeof unit_type !== "undefined" && unit_type !== "") ? unit_type : bottomRoot.unitTypeKeyFromDisplayName(unitDisplayName)
                        property string nationKey: (typeof nation !== "undefined" && nation !== "") ? nation : "default"

                        width: selectedUnitsList.width - 10
                        height: 36
                        color: isHovered ? hs.parchmentLight : hs.parchmentDark
                        radius: 4
                        border.color: isHovered ? hs.bronze : hs.bronzeDeep
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
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8
                            height: 28

                            Item {
                                width: 32
                                height: 28

                                Image {
                                    id: selectedUnitIcon

                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    fillMode: Image.PreserveAspectFit
                                    source: bottomRoot.unitIconSource(selectedUnitItem.unitTypeKey, selectedUnitItem.nationKey)
                                    visible: source !== "" && status !== Image.Error
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: bottomRoot.unitIconEmoji(selectedUnitItem.unitTypeKey)
                                    color: Theme.textMain
                                    font.pixelSize: 16
                                    visible: selectedUnitIcon.source === "" || selectedUnitIcon.status === Image.Error
                                }

                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Rectangle {
                                    width: 60
                                    height: 10
                                    color: "#15100C"
                                    radius: 5
                                    border.color: hs.bronzeDeep
                                    border.width: 1

                                    Rectangle {
                                        width: parent.width * (typeof health_ratio !== 'undefined' ? health_ratio : 0)
                                        height: parent.height
                                        color: {
                                            var ratio = (typeof health_ratio !== 'undefined' ? health_ratio : 0);
                                            return bottomRoot.healthColor(ratio);
                                        }
                                        radius: 5
                                    }

                                }

                                Rectangle {
                                    width: 60
                                    height: 6
                                    color: "#15100C"
                                    radius: 3
                                    border.color: hs.bronzeDeep
                                    border.width: 1
                                    visible: (typeof can_run !== 'undefined') ? can_run : false

                                    Rectangle {
                                        width: parent.width * (typeof stamina_ratio !== 'undefined' ? stamina_ratio : 1)
                                        height: parent.height
                                        color: {
                                            var running = (typeof is_running !== 'undefined') ? is_running : false;
                                            return bottomRoot.staminaColor(running);
                                        }
                                        radius: 3
                                    }

                                }

                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: (typeof name !== 'undefined' ? name : selectedUnitItem.unitDisplayName)
                                color: Theme.textMain
                                font.pointSize: 8
                                font.bold: false
                                elide: Text.ElideRight
                                width: parent.width - 108
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
            color: bottomRoot.currentCommandMode === "normal" ? hs.parchmentDark : (bottomRoot.currentCommandMode === "attack" ? hs.bannerAttack : hs.bannerNeutral)
            border.color: bottomRoot.currentCommandMode === "normal" ? hs.bronzeDeep : hs.bronze
            border.width: 2
            radius: 6
            opacity: bottomRoot.hasMovableUnits ? 1 : 0.5

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: "transparent"
                border.color: hs.bronze
                border.width: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits ? 1 : 0
                radius: 8
                opacity: 0.4
                visible: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits
            }

            Text {
                anchors.centerIn: parent
                text: bottomRoot.commandBannerText()
                color: !bottomRoot.hasMovableUnits ? Theme.textDim : Theme.textMain
                font.pointSize: bottomRoot.currentCommandMode === "normal" ? 10 : 11
                font.bold: bottomRoot.currentCommandMode !== "normal" && bottomRoot.hasMovableUnits
            }

            SequentialAnimation on opacity {
                running: bottomRoot.shouldPulseCommandBanner()
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
            id: cmdGrid

            property int cmdIconSize: 42

            function getButtonColor(btn, baseColor) {
                if (btn.pressed)
                    return Qt.darker(baseColor, 1.3);

                if (btn.checked)
                    return baseColor;

                if (btn.hovered)
                    return Qt.lighter(baseColor, 1.2);

                return hs.parchmentLight;
            }

            width: parent.width
            columns: 4
            rowSpacing: 6
            columnSpacing: 6

            Button {
                id: attackButton

                property bool modeAvailable: bottomRoot.modeAvailability.canAttack !== false

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Attack")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable
                checkable: true
                checked: bottomRoot.currentCommandMode === "attack" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "attack" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Attack not available for selected units (e.g. healers)") : (bottomRoot.hasMovableUnits ? qsTr("Attack enemy units or buildings.\nUnits will chase targets.") : qsTr("Select troops first"))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.checked ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("attack_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: attackButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: attackButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: guardButton

                property bool modeAvailable: bottomRoot.modeAvailability.canGuard !== false

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Guard")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable
                checkable: true
                checked: bottomRoot.currentCommandMode === "guard" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "guard" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Guard not available for selected units") : (bottomRoot.hasMovableUnits ? qsTr("Guard a position.\nUnits will defend from all sides.") : qsTr("Select troops first"))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.checked ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("defend_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: guardButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: guardButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: patrolButton

                property bool modeAvailable: bottomRoot.modeAvailability.canPatrol !== false

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Patrol")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable
                checkable: true
                checked: bottomRoot.currentCommandMode === "patrol" && bottomRoot.hasMovableUnits
                onClicked: {
                    bottomRoot.commandModeChanged(checked ? "patrol" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Patrol not available for selected units") : (bottomRoot.hasMovableUnits ? qsTr("Patrol between waypoints.\nClick start and end points.") : qsTr("Select troops first"))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.checked ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("patrol_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: patrolButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: patrolButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: healButton

                property bool modeAvailable: bottomRoot.modeAvailability.canHeal !== false
                property bool hasHealerSelected: {
                    bottomRoot.selectionTick;
                    return bottomRoot.hasSelectedUnit("healer");
                }

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Heal")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable && hasHealerSelected
                checkable: true
                checked: bottomRoot.currentCommandMode === "heal" && bottomRoot.hasMovableUnits
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_heal_command)
                        game.on_heal_command();

                    bottomRoot.commandModeChanged(checked ? "heal" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Heal not available for selected units") : (!hasHealerSelected ? qsTr("Select healer units") : qsTr("Heal allies in range"))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.checked ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("heal_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: healButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: healButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: stopButton

                Layout.fillWidth: true
                Layout.preferredHeight: 48
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
                    color: parent.enabled ? (parent.pressed ? hs.waxDark : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.enabled ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: "\u25a0"
                        font.pointSize: 18
                        font.bold: true
                        color: stopButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        text: stopButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: stopButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: buildButton

                property bool modeAvailable: bottomRoot.modeAvailability.canBuild !== false
                property bool hasBuilderSelected: {
                    bottomRoot.selectionTick;
                    return bottomRoot.hasSelectedUnit("builder");
                }

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Build")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable && hasBuilderSelected
                checkable: true
                checked: bottomRoot.currentCommandMode === "build" && bottomRoot.hasMovableUnits
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_build_command)
                        game.on_build_command();

                    bottomRoot.commandModeChanged(checked ? "build" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Build not available for selected units") : (!hasBuilderSelected ? qsTr("Select builder units") : qsTr("Build structures or place foundations"))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (parent.hovered ? hs.waxHover : hs.parchmentLight)) : hs.parchmentDark
                    radius: 6
                    border.color: parent.checked ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("build_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: buildButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: buildButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: holdButton

                property int activeStateTick: 0
                property string holdModeState: {
                    bottomRoot.selectionTick;
                    activeStateTick;
                    return (typeof game !== 'undefined' && game.get_selected_units_toggle_state) ? game.get_selected_units_toggle_state("hold") : "none";
                }
                readonly property bool isHoldActive: holdModeState === "all"
                readonly property bool isHoldMixed: holdModeState === "mixed"
                property bool modeAvailable: bottomRoot.modeAvailability.canHold !== false

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Hold")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && modeAvailable
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_hold_command)
                        game.on_hold_command();

                }
                ToolTip.visible: hovered
                ToolTip.text: !modeAvailable ? qsTr("Hold not available for selected units") : (bottomRoot.hasMovableUnits ? (isHoldActive ? qsTr("Exit hold mode (toggle)") : (isHoldMixed ? qsTr("Some selected troops are already holding. Click to apply hold to all eligible selected troops.") : qsTr("Hold position and defend"))) : qsTr("Select troops first"))
                ToolTip.delay: 500

                Connections {
                    function onHold_mode_changed(active) {
                        holdButton.activeStateTick += 1;
                    }

                    target: (typeof game !== 'undefined') ? game : null
                }

                background: Rectangle {
                    color: {
                        if (!parent.enabled)
                            return hs.parchmentDark;

                        if (parent.isHoldActive)
                            return hs.wax;

                        if (parent.isHoldMixed)
                            return hs.waxHover;

                        if (parent.pressed)
                            return hs.waxDark;

                        if (parent.hovered)
                            return hs.bannerNeutral;

                        return hs.parchmentLight;
                    }
                    radius: 6
                    border.color: parent.enabled ? ((parent.isHoldActive || parent.isHoldMixed) ? hs.bronze : hs.bronzeDeep) : hs.bronzeDeep
                    border.width: (parent.isHoldActive || parent.isHoldMixed) ? 2 : 1
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("hold_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (holdButton.isHoldActive ? qsTr("Active ") : (holdButton.isHoldMixed ? qsTr("Mixed ") : "")) + holdButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: holdButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

                }

            }

            Button {
                id: formationButton

                property int activeStateTick: 0
                property string formationModeState: {
                    bottomRoot.selectionTick;
                    activeStateTick;
                    return (typeof game !== 'undefined' && game.get_selected_units_toggle_state) ? game.get_selected_units_toggle_state("formation") : "none";
                }
                readonly property bool isFormationActive: formationModeState === "all"
                readonly property bool isFormationMixed: formationModeState === "mixed"
                property int selectedCount: {
                    bottomRoot.selectionTick;
                    return (typeof game !== 'undefined' && game.selected_units_model) ? game.selected_units_model.rowCount() : 0;
                }

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Formation")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.hasMovableUnits && selectedCount > 1
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_formation_command)
                        game.on_formation_command();

                }
                ToolTip.visible: hovered
                ToolTip.text: {
                    if (!bottomRoot.hasMovableUnits)
                        return qsTr("Select troops first");

                    if (selectedCount <= 1)
                        return qsTr("Select multiple units to use formation");

                    if (isFormationActive)
                        return qsTr("Exit formation mode (toggle)");

                    if (isFormationMixed)
                        return qsTr("Some selected troops are already in formation mode. Click to apply formation mode to the full eligible selection.");

                    return qsTr("Arrange units in tactical formation");
                }
                ToolTip.delay: 500

                Connections {
                    function onFormation_mode_changed(active) {
                        formationButton.activeStateTick += 1;
                    }

                    target: (typeof game !== 'undefined') ? game : null
                }

                background: Rectangle {
                    color: {
                        if (!parent.enabled)
                            return hs.parchmentDark;

                        if (parent.isFormationActive)
                            return "#6F8E8C";

                        if (parent.isFormationMixed)
                            return "#82A4A1";

                        if (parent.pressed)
                            return "#5F7F83";

                        if (parent.hovered)
                            return "#82A4A1";

                        return hs.parchmentLight;
                    }
                    radius: 6
                    border.color: parent.enabled ? ((parent.isFormationActive || parent.isFormationMixed) ? hs.bronze : hs.bronzeDeep) : hs.bronzeDeep
                    border.width: (parent.isFormationActive || parent.isFormationMixed) ? 2 : 1
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmdIconSize
                        height: cmdGrid.cmdIconSize
                        source: bottomRoot.commandIcon("formation_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (formationButton.isFormationActive ? qsTr("Active ") : (formationButton.isFormationMixed ? qsTr("Mixed ") : "")) + formationButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: formationButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }

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
        onBuildTower: {
            if (typeof game !== 'undefined' && game.start_building_placement)
                game.start_building_placement("defense_tower");

        }
        onBuilderConstruction: function(itemType) {
            if (typeof game !== 'undefined' && game.start_builder_construction)
                game.start_builder_construction(itemType);

        }
    }

}

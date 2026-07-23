import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

RowLayout {
    id: bottomRoot

    property string current_command_mode
    property int selection_tick
    property bool has_movable_units
    readonly property var hs: StyleGuide.historical
    property var action_states: ({})

    signal command_mode_changed(string mode)
    signal recruit_unit(string unit_type)

    function update_action_states() {
        if (typeof game !== 'undefined' && game.get_hud_action_states)
            action_states = game.get_hud_action_states();
        else
            action_states = ({});
    }

    function unit_icon_source(unitType, nation_key) {
        if (typeof StyleGuide === "undefined" || !StyleGuide.unit_icon_sources || !unitType)
            return "";
        var sources = StyleGuide.unit_icon_sources[unitType];
        if (!sources)
            sources = StyleGuide.unit_icon_sources["default"];
        var key = (nation_key && sources[nation_key]) ? nation_key : "default";
        return sources[key] || "";
    }

    function unit_icon_emoji(unitType) {
        if (typeof StyleGuide !== "undefined" && StyleGuide.unit_icons)
            return StyleGuide.unit_icons[unitType] || StyleGuide.unit_icons["default"] || "👤";
        return "👤";
    }

    function unit_type_key_from_display_name(display_name) {
        if (!display_name)
            return "";
        var s = display_name.toString().trim().toLowerCase();
        s = s.replace(/[^a-z0-9]+/g, "_");
        s = s.replace(/^_+|_+$/g, "");
        return s;
    }

    function command_icon(filename) {
        if (typeof StyleGuide === "undefined" || !filename)
            return "";
        return StyleGuide.icon_path(filename);
    }

    function action_state(actionId) {
        var state = action_states ? action_states[actionId] : null;
        return state || {
            "enabled": false,
            "active": false,
            "mixed": false,
            "placing": false,
            "passive": false,
            "eligibleCount": 0
        };
    }

    function command_banner_text() {
        if (!has_movable_units)
            return qsTr("No troops selected");
        if (current_command_mode === "normal")
            return qsTr("Orders ready");
        var orderText = qsTr("Stop command");
        if (current_command_mode === "attack")
            orderText = qsTr("Attack order");
        else if (current_command_mode === "guard")
            orderText = qsTr("Guard order");
        else if (current_command_mode === "patrol")
            orderText = qsTr("Patrol order");
        else if (current_command_mode === "heal")
            orderText = qsTr("Medic order");
        else if (current_command_mode === "build")
            orderText = qsTr("Engineer order");
        else if (current_command_mode === "collect")
            orderText = qsTr("Collection order");
        else if (current_command_mode === "deliver")
            orderText = qsTr("Barracks delivery");
        else if (current_command_mode === "formation")
            orderText = qsTr("Formation order");
        else if (current_command_mode === "rally")
            orderText = qsTr("Rally order");
        return orderText;
    }

    function health_color(ratio) {
        if (ratio > 0.6)
            return Theme.accent;
        if (ratio > 0.3)
            return hs.bronze;
        return hs.waxHover;
    }

    function stamina_color(running) {
        return running ? hs.bronze : Theme.textSubLite;
    }

    function should_pulse_command_banner() {
        return has_movable_units && (current_command_mode === "attack" || current_command_mode === "guard" || current_command_mode === "patrol" || current_command_mode === "collect" || current_command_mode === "deliver");
    }

    Component.onCompleted: update_action_states()
    onSelection_tickChanged: update_action_states()
    anchors.fill: parent
    anchors.margins: 10
    spacing: 12

    Connections {
        function onCursor_mode_changed() {
            bottomRoot.update_action_states();
        }

        function onPlacing_construction_changed() {
            bottomRoot.update_action_states();
        }

        function onPlacing_formation_changed() {
            bottomRoot.update_action_states();
        }

        target: (typeof game !== 'undefined') ? game : null
    }

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

                        property bool is_hovered: false
                        property string unit_display_name: (typeof name !== "undefined") ? name : ""
                        property string unit_type_key: (typeof unit_type !== "undefined" && unit_type !== "") ? unit_type : bottomRoot.unit_type_key_from_display_name(unit_display_name)
                        property string nation_key: (typeof nation !== "undefined" && nation !== "") ? nation : "default"

                        width: selectedUnitsList.width - 10
                        height: 36
                        color: is_hovered ? hs.parchmentLight : hs.parchmentDark
                        radius: 4
                        border.color: is_hovered ? hs.bronze : hs.bronzeDeep
                        border.width: is_hovered ? 2 : 1

                        MouseArea {
                            id: selectedUnitMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: false
                            cursorShape: Qt.PointingHandCursor
                            onEntered: selectedUnitItem.is_hovered = true
                            onExited: selectedUnitItem.is_hovered = false
                            onClicked: function (mouse) {
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
                                    source: bottomRoot.unit_icon_source(selectedUnitItem.unit_type_key, selectedUnitItem.nation_key)
                                    visible: source !== "" && status !== Image.Error
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: bottomRoot.unit_icon_emoji(selectedUnitItem.unit_type_key)
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
                                    color: Theme.bgShade
                                    radius: 5
                                    border.color: hs.bronzeDeep
                                    border.width: 1

                                    Rectangle {
                                        width: parent.width * (typeof health_ratio !== 'undefined' ? health_ratio : 0)
                                        height: parent.height
                                        color: {
                                            var ratio = (typeof health_ratio !== 'undefined' ? health_ratio : 0);
                                            return bottomRoot.health_color(ratio);
                                        }
                                        radius: 5
                                    }
                                }

                                Rectangle {
                                    width: 60
                                    height: 6
                                    color: Theme.bgShade
                                    radius: 3
                                    border.color: hs.bronzeDeep
                                    border.width: 1
                                    visible: (typeof can_run !== 'undefined') ? can_run : false

                                    Rectangle {
                                        width: parent.width * (typeof stamina_ratio !== 'undefined' ? stamina_ratio : 1)
                                        height: parent.height
                                        color: {
                                            var running = (typeof is_running !== 'undefined') ? is_running : false;
                                            return bottomRoot.stamina_color(running);
                                        }
                                        radius: 3
                                    }
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: (typeof name !== 'undefined' ? name : selectedUnitItem.unit_display_name)
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
            color: bottomRoot.current_command_mode === "normal" ? hs.parchmentDark : (bottomRoot.current_command_mode === "attack" ? hs.bannerAttack : hs.bannerNeutral)
            border.color: bottomRoot.current_command_mode === "normal" ? hs.bronzeDeep : hs.bronze
            border.width: 2
            radius: 6
            opacity: bottomRoot.has_movable_units ? 1 : 0.5

            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: "transparent"
                border.color: hs.bronze
                border.width: bottomRoot.current_command_mode !== "normal" && bottomRoot.has_movable_units ? 1 : 0
                radius: 8
                opacity: 0.4
                visible: bottomRoot.current_command_mode !== "normal" && bottomRoot.has_movable_units
            }

            Text {
                anchors.centerIn: parent
                text: bottomRoot.command_banner_text()
                color: !bottomRoot.has_movable_units ? Theme.textDim : Theme.textMain
                font.pointSize: bottomRoot.current_command_mode === "normal" ? 10 : 11
                font.bold: bottomRoot.current_command_mode !== "normal" && bottomRoot.has_movable_units
            }

            SequentialAnimation on opacity  {
                running: bottomRoot.should_pulse_command_banner()
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

            property int cmd_icon_size: 42

            function get_button_color(btn, baseColor) {
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

                property var action_state: bottomRoot.action_state("attack")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Attack")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: bottomRoot.current_command_mode === "attack" && bottomRoot.has_movable_units
                onClicked: {
                    bottomRoot.command_mode_changed(checked ? "attack" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Attack not available for the current selection") : qsTr("Attack enemy units or buildings.\nOnly eligible selected troops will receive the order."))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (attackButton.action_state.mixed ? hs.waxHover : (parent.hovered ? hs.waxHover : hs.parchmentLight))) : hs.parchmentDark
                    radius: 6
                    border.color: (parent.checked || attackButton.action_state.mixed) ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("attack_mode.png")
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

                property var action_state: bottomRoot.action_state("guard")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Guard")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: bottomRoot.current_command_mode === "guard" && bottomRoot.has_movable_units
                onClicked: {
                    bottomRoot.command_mode_changed(checked ? "guard" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Guard not available for the current selection") : qsTr("Guard a position.\nOnly guard-capable selected troops will receive the order."))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (guardButton.action_state.mixed ? hs.waxHover : (parent.hovered ? hs.waxHover : hs.parchmentLight))) : hs.parchmentDark
                    radius: 6
                    border.color: (parent.checked || guardButton.action_state.mixed) ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("defend_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (guardButton.action_state.active ? qsTr("Active ") : (guardButton.action_state.mixed ? qsTr("Mixed ") : "")) + guardButton.text
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

                property var action_state: bottomRoot.action_state("patrol")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Patrol")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: bottomRoot.current_command_mode === "patrol" && bottomRoot.has_movable_units
                onClicked: {
                    bottomRoot.command_mode_changed(checked ? "patrol" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Patrol not available for the current selection") : qsTr("Patrol between waypoints.\nOnly patrol-capable selected troops will receive the order."))
                ToolTip.delay: 500

                background: Rectangle {
                    color: parent.enabled ? (parent.checked ? hs.wax : (patrolButton.action_state.mixed ? hs.waxHover : (parent.hovered ? hs.waxHover : hs.parchmentLight))) : hs.parchmentDark
                    radius: 6
                    border.color: (parent.checked || patrolButton.action_state.mixed) ? hs.bronze : hs.bronzeDeep
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("patrol_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (patrolButton.action_state.active ? qsTr("Active ") : (patrolButton.action_state.mixed ? qsTr("Mixed ") : "")) + patrolButton.text
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

                property var action_state: bottomRoot.action_state("heal")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Heal")
                focusPolicy: Qt.NoFocus
                enabled: false
                checkable: true
                checked: bottomRoot.current_command_mode === "heal" && bottomRoot.has_movable_units
                ToolTip.visible: hovered
                ToolTip.text: action_state.passive ? qsTr("Healers automatically heal nearby allies.\nThis is passive, not a manual order.") : qsTr("Select healer units")
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
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("heal_mode.png")
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
                enabled: bottomRoot.has_movable_units
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_stop_command)
                        game.on_stop_command();
                }
                ToolTip.visible: hovered
                ToolTip.text: bottomRoot.has_movable_units ? qsTr("Stop all actions immediately") : qsTr("Select troops first")
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
                id: deliverButton

                property var action_state: bottomRoot.action_state("deliver")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Deliver")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: bottomRoot.current_command_mode === "deliver" && bottomRoot.has_movable_units
                onClicked: {
                    bottomRoot.command_mode_changed(checked ? "deliver" : "normal");
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Deliver is only available to civilians") : qsTr("Click a friendly barracks.\nOnly selected civilians will receive the order."))
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
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("deliver_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: deliverButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: deliverButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Button {
                id: collectButton

                property var action_state: bottomRoot.action_state("collect")
                readonly property bool is_collect_active: action_state.placing

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Collect")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: is_collect_active && bottomRoot.has_movable_units
                onClicked: {
                    if (typeof game === 'undefined')
                        return;
                    if (is_collect_active) {
                        if (game.on_construction_cancel)
                            game.on_construction_cancel();
                        bottomRoot.command_mode_changed("normal");
                        return;
                    }
                    if (game.start_builder_construction)
                        game.start_builder_construction("collect");
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Collect is only available to builders") : (is_collect_active ? qsTr("Click a tree, boulder, or iron ore deposit.\nRight-click to cancel.") : qsTr("Only selected builders will receive the collect order.")))
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
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("collect_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: collectButton.enabled ? 1 : 0.45
                        visible: source !== ""
                    }

                    Text {
                        text: collectButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: collectButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Button {
                id: buildButton

                property var action_state: bottomRoot.action_state("build")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Build")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                checkable: true
                checked: bottomRoot.current_command_mode === "build" && bottomRoot.has_movable_units
                onClicked: {
                    if (typeof game === 'undefined')
                        return;
                    if (game.is_placing_construction && game.on_construction_cancel) {
                        game.on_construction_cancel();
                        return;
                    }
                    if (bottomRoot.current_command_mode === "build") {
                        bottomRoot.command_mode_changed("normal");
                        return;
                    }
                    if (game.on_build_command)
                        game.on_build_command();
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Build is only available to builders") : qsTr("Open builder orders and place structures.\nOnly selected builders will receive build orders."))
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
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("build_mode.png")
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

                property var action_state: bottomRoot.action_state("hold")
                readonly property bool is_hold_active: action_state.active
                readonly property bool is_hold_mixed: action_state.mixed

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Hold")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_hold_command)
                        game.on_hold_command();
                }
                ToolTip.visible: hovered
                ToolTip.text: !bottomRoot.has_movable_units ? qsTr("Select troops first") : (!action_state.enabled ? qsTr("Hold not available for the current selection") : (is_hold_active ? qsTr("Exit hold mode (toggle)") : (is_hold_mixed ? qsTr("Some selected troops are already holding. Click to apply hold to all eligible selected troops.") : qsTr("Hold position and defend"))))
                ToolTip.delay: 500

                background: Rectangle {
                    color: {
                        if (!parent.enabled)
                            return hs.parchmentDark;
                        if (parent.is_hold_active)
                            return hs.wax;
                        if (parent.is_hold_mixed)
                            return hs.waxHover;
                        if (parent.pressed)
                            return hs.waxDark;
                        if (parent.hovered)
                            return hs.bannerNeutral;
                        return hs.parchmentLight;
                    }
                    radius: 6
                    border.color: parent.enabled ? ((parent.is_hold_active || parent.is_hold_mixed) ? hs.bronze : hs.bronzeDeep) : hs.bronzeDeep
                    border.width: (parent.is_hold_active || parent.is_hold_mixed) ? 2 : 1
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("hold_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (holdButton.is_hold_active ? qsTr("Active ") : (holdButton.is_hold_mixed ? qsTr("Mixed ") : "")) + holdButton.text
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

                property var action_state: bottomRoot.action_state("formation")
                readonly property bool is_formation_active: action_state.active
                readonly property bool is_formation_mixed: action_state.mixed

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Formation")
                focusPolicy: Qt.NoFocus
                enabled: bottomRoot.has_movable_units && action_state.enabled
                onClicked: {
                    if (typeof game !== 'undefined' && game.on_formation_command)
                        game.on_formation_command();
                }
                ToolTip.visible: hovered
                ToolTip.text: {
                    if (!bottomRoot.has_movable_units)
                        return qsTr("Select troops first");
                    if (!action_state.enabled)
                        return qsTr("Select multiple units to use formation");
                    if (is_formation_active)
                        return qsTr("Exit formation mode (toggle)");
                    if (is_formation_mixed)
                        return qsTr("Some selected troops are already in formation mode. Click to apply formation mode to the full eligible selection.");
                    return qsTr("Arrange units in tactical formation");
                }
                ToolTip.delay: 500

                background: Rectangle {
                    color: {
                        if (!parent.enabled)
                            return hs.parchmentDark;
                        if (parent.is_formation_active)
                            return hs.bannerNeutral;
                        if (parent.is_formation_mixed)
                            return hs.parchmentLight;
                        if (parent.pressed)
                            return hs.bronzeDeep;
                        if (parent.hovered)
                            return hs.parchmentLight;
                        return hs.parchmentLight;
                    }
                    radius: 6
                    border.color: parent.enabled ? ((parent.is_formation_active || parent.is_formation_mixed) ? hs.bronze : hs.bronzeDeep) : hs.bronzeDeep
                    border.width: (parent.is_formation_active || parent.is_formation_mixed) ? 2 : 1
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("formation_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: source !== ""
                    }

                    Text {
                        text: (formationButton.is_formation_active ? qsTr("Active ") : (formationButton.is_formation_mixed ? qsTr("Mixed ") : "")) + formationButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: formationButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Button {
                id: rallyButton

                property var action_state: bottomRoot.action_state("rally")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("Rally")
                focusPolicy: Qt.NoFocus
                enabled: action_state.enabled
                ToolTip.visible: hovered
                ToolTip.text: action_state.enabled ? qsTr("Commander plants a rally flag at a chosen position.\nAll troops will march to the flag once it is placed.") : qsTr("Select a commander to use rally")
                ToolTip.delay: 500
                onClicked: {
                    if (typeof game !== 'undefined' && game.begin_commander_flag_rally)
                        game.begin_commander_flag_rally();
                }

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

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("rally_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: rallyButton.enabled ? 1 : 0.45
                        visible: source !== ""
                    }

                    Text {
                        text: rallyButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: rallyButton.enabled ? Theme.textMain : Theme.textDim
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Button {
                id: auraButton

                property var action_state: bottomRoot.action_state("aura")

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: action_state.active ? qsTr("Aura Active") : qsTr("Command Aura")
                focusPolicy: Qt.NoFocus
                enabled: action_state.enabled
                ToolTip.visible: hovered
                ToolTip.text: action_state.active ? qsTr("Nearby troops are empowered until the aura expires.") : (action_state.enabled ? qsTr("Temporarily empower nearby troops. A glow marks every affected soldier.") : qsTr("Select a ready commander to activate the aura"))
                ToolTip.delay: 500
                onClicked: {
                    if (typeof game !== 'undefined' && game.commander_trigger_aura) {
                        game.commander_trigger_aura();
                        bottomRoot.update_action_states();
                    }
                }

                background: Rectangle {
                    color: parent.enabled ? (auraButton.action_state.active ? hs.wax : (parent.pressed ? hs.waxDark : (parent.hovered ? hs.waxHover : hs.parchmentLight))) : hs.parchmentDark
                    radius: 6
                    border.color: auraButton.action_state.active ? Theme.accent : (parent.enabled ? hs.bronze : hs.bronzeDeep)
                    border.width: 2
                }

                contentItem: Row {
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        width: cmdGrid.cmd_icon_size
                        height: cmdGrid.cmd_icon_size
                        source: bottomRoot.command_icon("aura_mode.png")
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: auraButton.enabled || auraButton.action_state.active ? 1 : 0.45
                    }

                    Text {
                        text: auraButton.text
                        font.pointSize: 11
                        font.bold: true
                        color: auraButton.enabled || auraButton.action_state.active ? Theme.textMain : Theme.textDim
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
        selection_tick: bottomRoot.selection_tick
        game_instance: (typeof game !== 'undefined') ? game : null
        onRecruit_unit: function (unit_type) {
            bottomRoot.recruit_unit(unit_type);
        }
        onRally_mode_toggled: {
            if (typeof game === 'undefined' || typeof gameView === 'undefined')
                return;
            if (gameView.cursor_mode === "place_barracks_rally") {
                if (game.cancel_barracks_rally_placement)
                    game.cancel_barracks_rally_placement();
            } else if (game.begin_barracks_rally_placement) {
                game.begin_barracks_rally_placement();
            }
        }
        onBuild_tower: {
            if (typeof game !== 'undefined' && game.start_building_placement)
                game.start_building_placement("defense_tower");
        }
        onBuilder_construction: function (item_type) {
            if (typeof game !== 'undefined' && game.start_builder_construction)
                game.start_builder_construction(item_type);
        }
    }
}

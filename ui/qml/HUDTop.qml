import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: topRoot

    property bool game_is_paused: false
    property real current_speed: 1

    readonly property bool compact: width < 1000
    readonly property bool ultraCompact: width < 620
    readonly property var speedOptions: GameSpeeds.options

    readonly property real minimapLegendHeight: fogLegend.visible ? Design.Metrics.space4 + fogLegend.implicitHeight + Design.Metrics.space8 : 0

    property bool camera_legend_visible: false

    signal pause_toggled
    signal speed_changed(real speed)
    signal help_requested
    signal camera_legend_toggled

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function speed_label(speed) {
        return GameSpeeds.label(speed);
    }

    function speed_index(speed) {
        return GameSpeeds.index_of(speed);
    }

    function speed_labels() {
        return GameSpeeds.labels();
    }

    property string primaryObjective: ""
    property bool primaryObjectiveCountsWaves: false

    readonly property bool missionStaged: game_ready() && game.mission && game.mission.staged

    readonly property string primaryObjectiveText: {
        if (missionStaged) {
            if (game.mission.active_index < 0)
                return qsTr("All objectives complete");
            var staged = game.mission.active_title;
            if (game.mission.active_required > 1)
                staged += qsTr(" (%1/%2)").arg(game.mission.active_progress).arg(game.mission.active_required);
            return staged;
        }
        if (primaryObjective === "")
            return "";
        if (!primaryObjectiveCountsWaves || !game_ready() || !game.waves || !game.waves.active)
            return primaryObjective;
        return primaryObjective + qsTr(" (%1/%2)").arg(game.waves.cleared_phases).arg(game.waves.total_phases);
    }

    function refresh_primary_objective() {
        if (!game_ready() || !game.setup.current_mission_objectives) {
            primaryObjective = "";
            primaryObjectiveCountsWaves = false;
            return;
        }
        var objectives = game.setup.current_mission_objectives();
        var conditions = objectives && objectives.victory_conditions ? objectives.victory_conditions : [];
        primaryObjective = conditions.length > 0 ? (conditions[0].description || "") : "";
        primaryObjectiveCountsWaves = conditions.length > 0 && conditions[0].type === "survive_waves";
    }

    Component.onCompleted: refresh_primary_objective()

    Connections {
        function onCurrent_mission_changed() {
            topRoot.refresh_primary_objective();
        }

        target: topRoot.game_ready() ? game.setup : null
    }

    function resource_amount(kind) {
        if (!game_ready() || !game.selected_player_state || !game.selected_player_state.resources)
            return 0;
        return game.selected_player_state.resources[kind] || 0;
    }

    readonly property var economy: game_ready() && game.economy ? game.economy : null

    readonly property var resource_entries: {
        var entries = economy && economy.resources ? economy.resources : [];
        var shown = [];
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].relevant !== false)
                shown.push(entries[i]);
        }
        if (shown.length > 0)
            return shown;
        var fallback = [];
        for (var k = 0; k < EconomyGuide.resourceOrder.length; ++k) {
            var key = EconomyGuide.resourceOrder[k];
            fallback.push({
                    "key": key,
                    "amount": resource_amount(key)
                });
        }
        return fallback;
    }

    function resource_status(entry) {
        if (!entry || !entry.shortfall || entry.shortfall <= 0)
            return "default";
        return "warning";
    }

    signal economy_help_requested

    function population() {
        return game_ready() && game.selected_player_state ? (game.selected_player_state.population || 0) : 0;
    }

    function population_cap() {
        return game_ready() && game.selected_player_state ? (game.selected_player_state.population_cap || 0) : 0;
    }

    function population_status() {
        var cap = population_cap();
        if (cap <= 0)
            return "disabled";
        var count = population();
        if (count >= cap)
            return "danger";
        if (count >= cap * 0.8)
            return "warning";
        return "default";
    }

    function owner_count(type) {
        var owners = game_ready() ? (game.owner_info || []) : [];
        var count = 0;
        for (var i = 0; i < owners.length; ++i) {
            if (owners[i].type === type)
                count++;
        }
        return count;
    }

    function owners_tooltip() {
        var owners = game_ready() ? (game.owner_info || []) : [];
        var lines = [];
        for (var i = 0; i < owners.length; ++i) {
            var line = owners[i].id + ": " + owners[i].name + " (" + owners[i].type + ")";
            if (owners[i].isLocal)
                line += qsTr(" [You]");
            lines.push(line);
        }
        return lines.join("\n");
    }

    Design.IronPanel {
        id: bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.max(Design.Metrics.controlHeight + Design.Metrics.space24, Design.Metrics.space24 * 3)
        raised: true

        border.color: Design.FactionTheme.activeFaction === "" ? Design.Theme.borderStrong : Design.FactionTheme.accentDeep

        RowLayout {
            anchors.fill: parent
            spacing: Design.Metrics.space12

            RowLayout {
                spacing: Design.Metrics.space8
                Layout.alignment: Qt.AlignVCenter

                Design.IronIconButton {
                    iconText: topRoot.game_is_paused ? Design.Icons.play : Design.Icons.pause
                    tooltip: topRoot.game_is_paused ? qsTr("Resume") : qsTr("Pause")
                    onClicked: topRoot.pause_toggled()
                }

                Design.IronDivider {
                    vertical: true
                    Layout.fillHeight: true
                    visible: !topRoot.ultraCompact
                }

                Repeater {
                    model: topRoot.speedOptions

                    delegate: Design.IronButton {
                        required property var modelData

                        visible: !topRoot.compact
                        Layout.preferredWidth: Design.Metrics.space24 * 2
                        implicitWidth: Design.Metrics.space24 * 2
                        text: topRoot.speed_label(modelData)
                        tone: topRoot.current_speed === modelData ? "primary" : "secondary"
                        accessibleName: qsTr("Battle speed %1").arg(topRoot.speed_label(modelData))
                        onClicked: topRoot.speed_changed(modelData)
                    }
                }

                Design.IronDropdown {
                    visible: topRoot.compact
                    Layout.preferredWidth: Design.Metrics.space24 * 4
                    model: topRoot.speed_labels()
                    currentIndex: topRoot.speed_index(topRoot.current_speed)
                    accessibleName: qsTr("Battle speed")
                    onActivated: function (index) {
                        topRoot.speed_changed(topRoot.speedOptions[index]);
                    }
                }

                Design.IronDivider {
                    vertical: true
                    Layout.fillHeight: true
                    visible: !topRoot.ultraCompact
                }

                Design.IronIconButton {
                    iconText: Design.Icons.follow
                    tooltip: qsTr("Follow the selection with the camera")
                    checkable: true
                    tone: checked ? "primary" : "secondary"
                    onToggled: {
                        if (topRoot.game_ready() && game.camera.follow_selection)
                            game.camera.follow_selection(checked);
                    }
                }

                Design.IronIconButton {
                    iconText: Design.Icons.reset
                    tooltip: qsTr("Return the camera to your camp (%1)").arg(InputBindings.display_shortcut_for("rts.camera_reset"))
                    onClicked: {
                        if (topRoot.game_ready() && game.camera.reset)
                            game.camera.reset();
                    }
                }

                Design.IronIconButton {
                    iconText: Design.Icons.objective
                    tooltip: topRoot.camera_legend_visible ? qsTr("Hide the camera controls") : qsTr("Show how to move the camera")
                    accessibleName: qsTr("Camera controls")
                    checkable: true
                    checked: topRoot.camera_legend_visible
                    tone: checked ? "primary" : "secondary"
                    onToggled: topRoot.camera_legend_toggled()
                }

                Design.IronIconButton {
                    iconText: "?"
                    tooltip: qsTr("Open the field manual (help)")
                    onClicked: topRoot.help_requested()
                }
            }

            Item {
                id: objectiveZone

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Row {
                    id: objectiveRow

                    anchors.centerIn: parent
                    spacing: Design.Metrics.space8
                    width: Math.min(objectiveGlyph.implicitWidth + spacing + objectiveText.implicitWidth, objectiveZone.width)
                    visible: topRoot.primaryObjectiveText !== "" && !(topRoot.game_ready() && game.is_spectator_mode)

                    Text {
                        id: objectiveGlyph

                        anchors.verticalCenter: parent.verticalCenter
                        text: Design.Icons.objective
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.body
                    }

                    Text {
                        id: objectiveText

                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, Math.max(0, objectiveRow.parent.width - objectiveGlyph.width - objectiveRow.spacing))
                        text: topRoot.primaryObjectiveText
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.label
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: objectiveFocusArea

                    anchors.fill: objectiveRow
                    enabled: objectiveRow.visible && topRoot.missionStaged && topRoot.game_ready() && game.mission.active_has_target
                    hoverEnabled: enabled
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                    ToolTip.visible: containsMouse
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: topRoot.missionStaged ? (game.mission.active_hint !== "" ? game.mission.active_hint + "\n" + qsTr("Click to look at the objective.") : qsTr("Click to look at the objective.")) : ""

                    onClicked: game.mission.focus_active_stage()
                }

                Design.IronBadge {
                    anchors.centerIn: parent
                    visible: topRoot.game_ready() && game.is_spectator_mode
                    tone: Design.Theme.warning
                    text: Design.Icons.spectator + " " + qsTr("SPECTATOR")

                    ToolTip.visible: spectatorHover.hovered
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: qsTr("Watching a CPU-only match. You cannot issue commands.")

                    HoverHandler {
                        id: spectatorHover
                    }
                }
            }

            RowLayout {
                spacing: Design.Metrics.space12
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: Design.Metrics.space8

                Repeater {
                    model: topRoot.resource_entries

                    delegate: Design.IronResourceCounter {
                        required property var modelData

                        iconSource: Design.Icons.resource(modelData.key)
                        iconText: Design.Icons.resourceGlyph(modelData.key)
                        label: EconomyGuide.resource_label(modelData.key)
                        amount: modelData.amount || 0
                        status: topRoot.resource_status(modelData)
                        tooltipText: EconomyGuide.resource_tooltip(modelData)
                        compact: topRoot.compact
                    }
                }

                Design.IronIconButton {
                    iconText: Design.Icons.build
                    tooltip: qsTr("Resource and building guide")
                    onClicked: topRoot.economy_help_requested()
                }

                Design.IronDivider {
                    vertical: true
                    Layout.fillHeight: true
                    visible: !topRoot.compact
                }

                Design.IronResourceCounter {
                    iconSource: Design.Icons.status("population")
                    label: qsTr("Population")
                    amountText: topRoot.population() + " / " + topRoot.population_cap()
                    status: topRoot.population_status()
                    compact: topRoot.compact
                }

                Design.IronDivider {
                    vertical: true
                    Layout.fillHeight: true
                    visible: !topRoot.compact
                }

                RowLayout {
                    visible: !topRoot.compact
                    spacing: Design.Metrics.space8

                    Design.IronResourceCounter {
                        iconSource: Design.Icons.status("human")
                        label: qsTr("Human players")
                        amount: topRoot.owner_count("Player")
                        compact: true
                    }

                    Design.IronResourceCounter {
                        iconSource: Design.Icons.status("ai")
                        label: qsTr("AI players")
                        amount: topRoot.owner_count("AI")
                        compact: true
                    }

                    ToolTip.visible: ownersHover.hovered
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: topRoot.owners_tooltip()

                    HoverHandler {
                        id: ownersHover
                    }
                }

                Design.IronResourceCounter {
                    iconSource: Design.Icons.status("defeated")
                    label: qsTr("Enemies defeated")
                    amount: topRoot.game_ready() ? game.enemy_troops_defeated : 0
                    compact: topRoot.compact
                }
            }
        }
    }

    Design.IronPanel {
        id: minimap

        visible: !topRoot.ultraCompact
        width: Design.Metrics.space24 * 8
        height: width
        anchors.right: parent.right

        anchors.top: bar.bottom
        anchors.rightMargin: Design.Metrics.hudZoneMargin
        anchors.topMargin: Design.Metrics.space8
        z: 100
        raised: true
        accessibleName: qsTr("Minimap")

        Image {
            id: minimapImage

            property int image_version: 0

            function bump_version() {
                minimapImage.image_version++;
            }

            anchors.fill: parent
            source: image_version > 0 ? "image://minimap/v" + image_version : ""
            fillMode: Image.PreserveAspectFit
            smooth: true
            cache: false
            asynchronous: false

            Connections {
                function onImage_changed() {
                    Qt.callLater(minimapImage.bump_version);
                }

                target: topRoot.game_ready() ? game.minimap : null
            }

            Text {
                anchors.centerIn: parent
                visible: parent.status !== Image.Ready
                text: qsTr("MINIMAP")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                font.weight: Design.Typography.medium
            }

            Repeater {
                id: waveAlerts

                model: (topRoot.game_ready() && game.waves) ? game.waves.alerts : []

                delegate: Item {
                    required property var modelData

                    readonly property real paintedW: minimapImage.paintedWidth
                    readonly property real paintedH: minimapImage.paintedHeight

                    visible: paintedW > 0 && paintedH > 0
                    x: ((minimapImage.width - paintedW) / 2) + (modelData.nx || 0) * paintedW
                    y: ((minimapImage.height - paintedH) / 2) + (modelData.ny || 0) * paintedH
                    z: 10

                    Rectangle {
                        anchors.centerIn: parent
                        width: Design.Metrics.space8
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: Design.Metrics.borderThin
                        border.color: Design.Theme.danger

                        SequentialAnimation on scale  {
                            running: !Design.A11y.reducedMotion
                            loops: Animation.Infinite

                            NumberAnimation {
                                from: 0.7
                                to: 2.2
                                duration: 900
                                easing.type: Easing.OutQuad
                            }

                            NumberAnimation {
                                to: 0.7
                                duration: 0
                            }
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: Design.Metrics.space4
                        height: width
                        radius: width / 2
                        color: Design.Theme.danger
                    }
                }
            }

            Repeater {
                id: objectivePins

                model: (topRoot.game_ready() && game.mission) ? game.mission.markers : []

                delegate: Item {
                    id: objectivePin

                    required property var modelData

                    readonly property real paintedW: minimapImage.paintedWidth
                    readonly property real paintedH: minimapImage.paintedHeight
                    readonly property bool current: modelData.active !== false

                    property real pulse: 1

                    SequentialAnimation on pulse  {
                        running: objectivePin.current && !Design.A11y.reducedMotion
                        loops: Animation.Infinite

                        NumberAnimation {
                            from: 1
                            to: 0.25
                            duration: 1100
                            easing.type: Easing.InOutQuad
                        }

                        NumberAnimation {
                            to: 1
                            duration: 1100
                            easing.type: Easing.InOutQuad
                        }
                    }

                    visible: paintedW > 0 && paintedH > 0
                    opacity: objectivePin.current ? 1 : 0.6
                    x: ((minimapImage.width - paintedW) / 2) + (modelData.nx || 0) * paintedW
                    y: ((minimapImage.height - paintedH) / 2) + (modelData.ny || 0) * paintedH
                    z: objectivePin.current ? 12 : 11

                    Rectangle {
                        anchors.centerIn: parent
                        width: objectivePin.current ? Design.Metrics.space12 : Design.Metrics.space8
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: Design.Metrics.borderThin
                        border.color: Design.Theme.accent
                        opacity: objectivePin.current ? objectivePin.pulse : 1
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: objectivePin.current ? Design.Metrics.space4 : Design.Metrics.space2
                        height: width
                        radius: width / 2
                        color: Design.Theme.accent
                    }
                }
            }

            MouseArea {
                id: minimapMouse

                ToolTip.visible: containsMouse
                ToolTip.delay: Design.Metrics.tooltipDelay
                ToolTip.text: qsTr("Left click moves the camera, right click orders selected troops.\nVisible: currently scouted, enemies shown.\nExplored: seen before, terrain only.\nUnseen: never scouted.")

                function dispatch(button, x, y) {
                    if (!topRoot.game_ready())
                        return;
                    var paintedW = minimapImage.paintedWidth;
                    var paintedH = minimapImage.paintedHeight;
                    if (paintedW === 0 || paintedH === 0)
                        return;
                    var imgX = x - (minimapImage.width - paintedW) / 2;
                    var imgY = y - (minimapImage.height - paintedH) / 2;
                    if (imgX < 0 || imgX >= paintedW || imgY < 0 || imgY >= paintedH)
                        return;
                    if (button === Qt.LeftButton)
                        game.minimap.on_left_click(imgX, imgY, paintedW, paintedH);
                    else
                        game.minimap.on_right_click(imgX, imgY, paintedW, paintedH);
                }

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                cursorShape: containsMouse ? Qt.PointingHandCursor : Qt.ArrowCursor
                onPressed: function (mouse) {
                    minimapMouse.dispatch(mouse.button, mouse.x, mouse.y);
                }
                onPositionChanged: function (mouse) {
                    if (mouse.buttons & Qt.LeftButton)
                        minimapMouse.dispatch(Qt.LeftButton, mouse.x, mouse.y);
                }
            }
        }
    }

    Flow {
        id: fogLegend

        readonly property color visibleSwatch: "#ff5e754c"
        readonly property color exploredSwatch: "#ff524f4c"
        readonly property color unseenSwatch: "#ff2d261e"

        visible: minimap.visible
        anchors.right: minimap.right
        anchors.top: minimap.bottom
        anchors.topMargin: Design.Metrics.space4
        spacing: Design.Metrics.space8
        z: 100

        Repeater {
            model: [{
                    "swatch": fogLegend.visibleSwatch,
                    "label": qsTr("Visible")
                }, {
                    "swatch": fogLegend.exploredSwatch,
                    "label": qsTr("Explored")
                }, {
                    "swatch": fogLegend.unseenSwatch,
                    "label": qsTr("Unseen")
                }]

            delegate: Row {
                required property var modelData

                spacing: Design.Metrics.space2

                Rectangle {
                    width: Design.Metrics.space8
                    height: width
                    anchors.verticalCenter: parent.verticalCenter
                    color: parent.modelData.swatch
                    border.width: Design.Metrics.borderThin
                    border.color: Design.Theme.borderSubtle
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.modelData.label
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                    style: Text.Outline
                    styleColor: Design.Theme.backgroundDeep
                }
            }
        }
    }
}

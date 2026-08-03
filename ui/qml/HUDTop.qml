import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: topRoot

    property bool game_is_paused: false
    property real current_speed: 1

    readonly property bool compact: width < 900
    readonly property bool ultraCompact: width < 620
    readonly property var speedOptions: [0.5, 1, 2]

    signal pause_toggled
    signal speed_changed(real speed)

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    property string primaryObjective: ""
    property bool primaryObjectiveCountsWaves: false

    readonly property string primaryObjectiveText: {
        if (primaryObjective === "")
            return "";
        if (!primaryObjectiveCountsWaves || !game_ready() || !game.waves || !game.waves.active)
            return primaryObjective;
        return primaryObjective + qsTr(" (%1/%2)").arg(game.waves.cleared_phases).arg(game.waves.total_phases);
    }

    function refresh_primary_objective() {
        if (!game_ready() || !game.get_current_mission_objectives) {
            primaryObjective = "";
            primaryObjectiveCountsWaves = false;
            return;
        }
        var objectives = game.get_current_mission_objectives();
        var conditions = objectives && objectives.victory_conditions ? objectives.victory_conditions : [];
        primaryObjective = conditions.length > 0 ? (conditions[0].description || "") : "";
        primaryObjectiveCountsWaves = conditions.length > 0 && conditions[0].type === "survive_waves";
    }

    Component.onCompleted: refresh_primary_objective()

    Connections {
        function onCampaign_mission_changed() {
            topRoot.refresh_primary_objective();
        }

        target: topRoot.game_ready() ? game : null
    }

    function resource_amount(kind) {
        if (!game_ready() || !game.selected_player_state || !game.selected_player_state.resources)
            return 0;
        return game.selected_player_state.resources[kind] || 0;
    }

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
                        text: modelData + "×"
                        tone: (!topRoot.game_is_paused && topRoot.current_speed === modelData) ? "primary" : "secondary"
                        blocked: topRoot.game_is_paused
                        disabledReason: qsTr("Resume the battle to change speed")
                        onClicked: topRoot.speed_changed(modelData)
                    }
                }

                Design.IronDropdown {
                    visible: topRoot.compact
                    Layout.preferredWidth: Design.Metrics.space24 * 4
                    model: ["0.5×", "1×", "2×"]
                    currentIndex: topRoot.current_speed === 0.5 ? 0 : topRoot.current_speed === 1 ? 1 : 2
                    blocked: topRoot.game_is_paused
                    disabledReason: qsTr("Resume the battle to change speed")
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
                        if (topRoot.game_ready() && game.camera_follow_selection)
                            game.camera_follow_selection(checked);
                    }
                }

                Design.IronIconButton {
                    iconText: Design.Icons.reset
                    tooltip: qsTr("Reset the camera")
                    onClicked: {
                        if (topRoot.game_ready() && game.reset_camera)
                            game.reset_camera();
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Row {
                    anchors.centerIn: parent
                    spacing: Design.Metrics.space8
                    visible: topRoot.primaryObjective !== "" && !(topRoot.game_ready() && game.is_spectator_mode)

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Design.Icons.objective
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.body
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: topRoot.primaryObjectiveText
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.label
                        elide: Text.ElideRight
                    }
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
                    model: ["gold", "wood", "stone", "iron"]

                    delegate: Design.IronResourceCounter {
                        required property var modelData

                        iconSource: Design.Icons.resource(modelData)
                        label: modelData
                        amount: topRoot.resource_amount(modelData)
                        compact: topRoot.compact
                    }
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
                function onMinimap_image_changed() {
                    Qt.callLater(minimapImage.bump_version);
                }

                target: topRoot.game_ready() ? game : null
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

            MouseArea {
                id: minimapMouse

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
                        game.on_minimap_left_click(imgX, imgY, paintedW, paintedH);
                    else
                        game.on_minimap_right_click(imgX, imgY, paintedW, paintedH);
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
}

import QtQuick 2.15
import QtQuick.Layouts 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root
    anchors.fill: parent

    property real bottomInset: 0
    property var status: ({})
    property var engine: null

    readonly property real uiScale: Math.max(0.75, Math.min(2.0, height / 1080))
    function scaled(value) {
        return Math.round(value * root.uiScale);
    }

    property real pulsePhase

    NumberAnimation on pulsePhase  {
        running: root.visible
        from: 0.0
        to: 1.0
        duration: 1000
        loops: Animation.Infinite
    }

    readonly property real slowPulse: 0.5 + 0.5 * Math.sin(pulsePhase * 2 * Math.PI)

    function status_value(key, fallback) {
        if (!status || status[key] === undefined || status[key] === null) {
            return fallback;
        }
        return status[key];
    }

    function cooldown_ratio(remainingKey, totalKey) {
        var total = Number(status_value(totalKey, 0.0));
        if (total <= 0.0) {
            return 0.0;
        }
        return Math.max(0.0, Math.min(1.0, Number(status_value(remainingKey, 0.0)) / total));
    }

    function attack_sweep_rotation(direction) {
        switch (Number(direction)) {
        case 0:
            return -28;
        case 1:
            return 28;
        case 2:
            return -90;
        case 3:
            return 0;
        case 4:
            return 90;
        default:
            return 0;
        }
    }

    Timer {
        interval: 33
        repeat: true
        running: root.visible && root.engine === null && typeof game !== 'undefined' && game.get_controlled_commander_status
        onTriggered: root.status = game.get_controlled_commander_status()
    }

    property bool focusProjected: false
    property real focusScreenX: 0
    property real focusScreenY: 0
    property real focusScreenHeight: 0

    function refresh_focus_projection() {
        var provider = root.engine !== null ? root.engine : (typeof game !== 'undefined' ? game : null);
        if (provider === null || !provider.rpg_project_world || root.status_value("focus_marker_valid", false) !== true) {
            root.focusProjected = false;
            return;
        }
        var wx = Number(root.status_value("focus_marker_x", 0));
        var wy = Number(root.status_value("focus_marker_y", 0));
        var wz = Number(root.status_value("focus_marker_z", 0));
        var chest = provider.rpg_project_world(wx, wy + 1.15, wz);
        var feet = provider.rpg_project_world(wx, wy, wz);
        if (!chest || !chest.valid || !feet || !feet.valid) {
            root.focusProjected = false;
            return;
        }
        root.focusProjected = true;
        root.focusScreenX = chest.x;
        root.focusScreenY = chest.y;
        root.focusScreenHeight = Math.abs(feet.y - chest.y) * (1.8 / 1.15);
    }

    Timer {
        interval: 16
        repeat: true
        running: root.visible
        onTriggered: root.refresh_focus_projection()
    }

    Rectangle {
        id: damageVignette
        anchors.fill: parent
        color: "transparent"
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#cc8b0000"
                }
                GradientStop {
                    position: 0.35
                    color: "#00000000"
                }
                GradientStop {
                    position: 0.65
                    color: "#00000000"
                }
                GradientStop {
                    position: 1.0
                    color: "#cc8b0000"
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            rotation: 90
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#aa6b0000"
                }
                GradientStop {
                    position: 0.30
                    color: "#00000000"
                }
                GradientStop {
                    position: 0.70
                    color: "#00000000"
                }
                GradientStop {
                    position: 1.0
                    color: "#aa6b0000"
                }
            }
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutQuad
            }
        }
    }

    Rectangle {
        id: lowHealthPulse
        anchors.fill: parent
        color: "transparent"
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#60ff0000"
                }
                GradientStop {
                    position: 0.45
                    color: "#00000000"
                }
                GradientStop {
                    position: 0.55
                    color: "#00000000"
                }
                GradientStop {
                    position: 1.0
                    color: "#60ff0000"
                }
            }
        }

        SequentialAnimation on opacity  {
            running: Design.A11y.screenEffectIntensity > 0.0 && Number(root.status_value("health_ratio", 1.0)) < 0.30 && Number(root.status_value("health_ratio", 1.0)) > 0.0
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.0
                to: 0.55 * Design.A11y.screenEffectIntensity
                duration: 600
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: 0.55 * Design.A11y.screenEffectIntensity
                to: 0.0
                duration: 600
                easing.type: Easing.InOutSine
            }
        }
    }

    Rectangle {
        id: guardGlow
        anchors.fill: parent
        color: "transparent"
        visible: root.status_value("guard_active", false) === true && Design.A11y.screenEffectIntensity > 0.0
        opacity: visible ? 0.45 * Design.A11y.screenEffectIntensity : 0.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: root.scaled(12)
            color: "transparent"
            border.width: Math.max(2, root.scaled(4))
            border.color: "#3388ccff"
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: 120
            }
        }
    }

    Item {
        id: perfectGuardFlash
        anchors.centerIn: parent
        width: root.scaled(190)
        height: width
        opacity: root.status_value("perfect_guard_active", false) === true ? 0.25 : 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 5
            border.color: "#ffffff"
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: 60
            }
        }
    }

    Item {
        id: combatEntryFlash
        anchors.centerIn: parent
        width: root.scaled(230)
        height: width
        property color accentColor: "#8bdcff"
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 4
            border.color: combatEntryFlash.accentColor
        }
    }

    Item {
        id: combatFrame
        anchors.fill: parent
        visible: root.status_value("focus_marker_locked", false) === true || root.status_value("is_attacking", false) === true
        opacity: visible ? (0.22 + Math.min(0.16, Number(root.status_value("combo_step", 0)) * 0.04)) : 0.0

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(2, root.scaled(4))
            height: root.scaled(180)
            radius: width / 2
            color: root.status_value("finisher_ready", false) === true ? "#d6ffd36b" : "#8abfe8ff"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(2, root.scaled(4))
            height: root.scaled(180)
            radius: width / 2
            color: root.status_value("finisher_ready", false) === true ? "#d6ffd36b" : "#8abfe8ff"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -root.scaled(54)
            width: root.scaled(92)
            height: Math.max(1, root.scaled(2))
            radius: 1
            rotation: -10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.scaled(54)
            width: root.scaled(92)
            height: Math.max(1, root.scaled(2))
            radius: 1
            rotation: 10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -root.scaled(54)
            width: root.scaled(92)
            height: Math.max(1, root.scaled(2))
            radius: 1
            rotation: 10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(24)
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.scaled(54)
            width: root.scaled(92)
            height: Math.max(1, root.scaled(2))
            radius: 1
            rotation: -10
            color: "#88f6f3e7"
        }
    }

    Item {
        id: attackSweep
        anchors.centerIn: parent
        width: root.scaled(240)
        height: width
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.centerIn: parent
            width: root.scaled(164)
            height: root.scaled(6)
            radius: height / 2
            rotation: root.attack_sweep_rotation(root.status_value("attack_direction", 0))
            color: "#d7ffd28a"
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.scaled(108)
            height: Math.max(1, root.scaled(2))
            radius: height / 2
            rotation: root.attack_sweep_rotation(root.status_value("attack_direction", 0)) + 90
            color: "#99ffffff"
        }
    }

    Item {
        id: dodgeTrail
        anchors.fill: parent
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: root.scaled(72)
            anchors.verticalCenter: parent.verticalCenter
            width: root.scaled(180)
            height: root.scaled(6)
            radius: height / 2
            rotation: -20
            color: "#88b8fff6"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(72)
            anchors.verticalCenter: parent.verticalCenter
            width: root.scaled(180)
            height: root.scaled(6)
            radius: height / 2
            rotation: 20
            color: "#88b8fff6"
        }
    }

    Item {
        id: guardBreakShock
        anchors.centerIn: parent
        width: root.scaled(260)
        height: width
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 6
            border.color: "#ff6a36"
        }
    }

    Item {
        id: lockBrackets

        readonly property bool lockedOn: root.status_value("focus_marker_locked", false) === true
        readonly property int armLength: root.scaled(lockedOn ? 20 : 15)
        readonly property int armThickness: Math.max(2, root.scaled(3))
        readonly property color armColor: lockedOn ? "#ffe6a8" : "#cfefff"

        width: Math.max(root.scaled(38), Math.min(root.scaled(220), root.focusScreenHeight * (lockedOn ? 0.78 : 0.66)))
        height: width
        x: root.focusProjected ? root.focusScreenX - width / 2 : (parent.width - width) / 2
        y: root.focusProjected ? root.focusScreenY - height / 2 : (parent.height - height) / 2
        visible: root.focusProjected && root.status_value("focus_marker_valid", false) === true
        opacity: visible ? (lockedOn ? 0.95 : 0.7) : 0.0

        Behavior on width  {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutQuad
            }
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: 120
            }
        }

        Repeater {
            model: [{
                    "hAnchor": "left",
                    "vAnchor": "top"
                }, {
                    "hAnchor": "right",
                    "vAnchor": "top"
                }, {
                    "hAnchor": "left",
                    "vAnchor": "bottom"
                }, {
                    "hAnchor": "right",
                    "vAnchor": "bottom"
                }]

            delegate: Item {
                width: lockBrackets.armLength
                height: lockBrackets.armLength
                anchors.left: modelData.hAnchor === "left" ? parent.left : undefined
                anchors.right: modelData.hAnchor === "right" ? parent.right : undefined
                anchors.top: modelData.vAnchor === "top" ? parent.top : undefined
                anchors.bottom: modelData.vAnchor === "bottom" ? parent.bottom : undefined

                Rectangle {
                    width: parent.width
                    height: lockBrackets.armThickness
                    radius: height / 2
                    color: lockBrackets.armColor
                    anchors.top: modelData.vAnchor === "top" ? parent.top : undefined
                    anchors.bottom: modelData.vAnchor === "bottom" ? parent.bottom : undefined
                }

                Rectangle {
                    width: lockBrackets.armThickness
                    height: parent.height
                    radius: width / 2
                    color: lockBrackets.armColor
                    anchors.left: modelData.hAnchor === "left" ? parent.left : undefined
                    anchors.right: modelData.hAnchor === "right" ? parent.right : undefined
                }
            }
        }
    }

    Item {
        id: crosshair
        objectName: "rpgCrosshair"
        width: root.scaled(58)
        height: root.scaled(58)
        anchors.centerIn: parent
        opacity: root.status_value("guard_active", false) === true ? 0.4 : 0.96

        property int comboStep: Number(root.status_value("combo_step", 0))
        property bool finisherReady: root.status_value("finisher_ready", false) === true
        property bool punishActive: root.status_value("punish_active", false) === true
        property bool lockedOn: root.status_value("focus_marker_locked", false) === true
        property bool targetInRange: root.status_value("aim_candidate_in_range", false) === true
        property color crossColor: finisherReady ? "#ffe07a" : (punishActive ? "#ff9952" : (targetInRange ? "#52f4ff" : (lockedOn ? "#bfe8ff" : "#f3efe6")))
        property real crossSize: finisherReady ? 1.18 : (targetInRange ? 1.12 : (comboStep >= 2 ? 1.08 : 1.0))

        scale: crossSize

        Behavior on scale  {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutBack
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.scaled(34)
            height: width
            radius: width / 2
            color: "#18000000"
            border.width: 1
            border.color: "#30ffffff"
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(3, root.scaled(7))
            height: width
            radius: width / 2
            color: crosshair.crossColor
        }

        Rectangle {
            width: Math.max(2, root.scaled(2))
            height: root.scaled(16)
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.verticalCenter
            anchors.bottomMargin: root.scaled(7)
        }

        Rectangle {
            width: Math.max(2, root.scaled(2))
            height: root.scaled(16)
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            anchors.topMargin: root.scaled(7)
        }

        Rectangle {
            width: root.scaled(16)
            height: Math.max(2, root.scaled(2))
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: root.scaled(7)
        }

        Rectangle {
            width: root.scaled(16)
            height: Math.max(2, root.scaled(2))
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: root.scaled(7)
        }
    }

    readonly property int abilityTileSize: scaled(60)
    readonly property int abilityRowWidth: abilityTileSize * 3 + scaled(24)

    readonly property int abilityBandWidth: abilityRowWidth + scaled(28) + scaled(12)

    Item {
        id: hudBarsRow
        objectName: "rpgHudBarsRow"
        anchors.bottom: parent.bottom

        anchors.bottomMargin: root.bottomInset + root.scaled(20)
        anchors.horizontalCenter: parent.horizontalCenter

        width: Math.max(root.scaled(180), Math.min(root.scaled(460), root.width - 2 * root.abilityBandWidth))
        height: root.scaled(34)

        RowLayout {
            anchors.fill: parent
            spacing: root.scaled(8)

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: "#d20a0b10"
                    border.color: "#55cc3333"
                    border.width: 1
                }

                Rectangle {
                    id: healthDrain
                    property real drainRatio: Number(root.status_value("health_ratio", 1.0))
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - 6) * drainRatio
                    height: parent.height - 6
                    radius: 4
                    color: "#66442222"

                    Behavior on width  {
                        NumberAnimation {
                            duration: 600
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                Rectangle {
                    id: healthFill
                    property real hpRatio: Number(root.status_value("health_ratio", 1.0))
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - 6) * hpRatio
                    height: parent.height - 6
                    radius: 4
                    color: "#cc3333"

                    Behavior on width  {
                        NumberAnimation {
                            duration: 180
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("HP %1/%2").arg(Number(root.status_value("health", 0))).arg(Number(root.status_value("max_health", 100)))
                    color: "#ffffff"
                    font.pixelSize: root.scaled(12)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#88000000"
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: "#d20a0b10"
                    border.color: "#443a9e3a"
                    border.width: 1
                }

                Rectangle {
                    id: staminaDrain
                    property real stamRatio: Number(root.status_value("stamina_ratio", 1.0))
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - 6) * stamRatio
                    height: parent.height - 6
                    radius: 4
                    color: "#662e8b2e"

                    Behavior on width  {
                        NumberAnimation {
                            duration: 600
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                Rectangle {
                    id: staminaFill
                    property real stamRatio: Number(root.status_value("stamina_ratio", 1.0))
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - 6) * stamRatio
                    height: parent.height - 6
                    radius: 4
                    color: "#3a9e3a"
                    opacity: stamRatio < 0.20 ? (0.45 + 0.55 * root.slowPulse) : 1.0

                    Behavior on width  {
                        NumberAnimation {
                            duration: 140
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("STM %1%").arg(Number(root.status_value("stamina_ratio", 1) * 100).toFixed(0))
                    color: "#cff7ffff"
                    font.pixelSize: root.scaled(12)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#66000000"
                }
            }
        }
    }

    Row {
        id: comboIndicator
        objectName: "rpgComboIndicator"

        property int combo: Number(root.status_value("combo_step", 0))
        property bool finisherReady: root.status_value("finisher_ready", false) === true

        anchors.bottom: postureBar.top
        anchors.bottomMargin: root.scaled(6)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: root.scaled(6)
        visible: combo > 0
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 200
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: comboIndicator.finisherReady ? qsTr("FINISHER") : qsTr("COMBO")
            color: comboIndicator.finisherReady ? "#ffdd00" : "#99ffffff"
            font.pixelSize: root.scaled(9)
            font.bold: true
            font.letterSpacing: 1.2
            style: Text.Outline
            styleColor: "#88000000"
        }

        Repeater {
            model: 4

            delegate: Rectangle {
                readonly property bool lit: index < comboIndicator.combo
                readonly property bool isFinisher: index === 3

                anchors.verticalCenter: parent.verticalCenter
                width: root.scaled(lit ? 16 : 11)
                height: root.scaled(6)
                radius: height / 2
                color: lit ? (isFinisher ? "#ffdd00" : "#8bdcff") : "#44ffffff"
                opacity: lit && isFinisher ? (0.55 + 0.45 * root.slowPulse) : 1.0

                Behavior on width  {
                    NumberAnimation {
                        duration: 140
                        easing.type: Easing.OutBack
                    }
                }

                Behavior on color  {
                    ColorAnimation {
                        duration: 120
                    }
                }
            }
        }
    }

    Item {
        id: postureBar
        objectName: "rpgPostureBar"

        anchors.bottom: hudBarsRow.top
        anchors.bottomMargin: root.scaled(6)
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(root.scaled(216), hudBarsRow.width)
        height: root.scaled(14)
        visible: Number(root.status_value("posture_ratio", 0.0)) > 0.05
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 200
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "#900a0a0d"
            border.color: "#44ffd18a"
            border.width: 1
        }

        Rectangle {
            property real postureRatio: Number(root.status_value("posture_ratio", 0.0))
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 4) * postureRatio
            height: parent.height - 4
            radius: 3
            color: postureRatio > 0.75 ? "#ccff5533" : (postureRatio > 0.45 ? "#ccff9b2e" : "#cce7d347")

            Behavior on width  {
                NumberAnimation {
                    duration: 120
                }
            }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("POSTURE")
            color: "#99ffffff"
            font.pixelSize: root.scaled(8)
            font.bold: true
            font.letterSpacing: 1.0
        }
    }

    Item {
        id: guardBreakWarning
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -root.scaled(120)
        visible: root.status_value("guard_broken", false) === true
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 150
            }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("GUARD BROKEN")
            color: "#ff4444"
            font.pixelSize: root.scaled(22)
            font.bold: true
            font.letterSpacing: 2.0
            style: Text.Outline
            styleColor: "#cc000000"
        }
    }

    Item {
        id: punishIndicator
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -root.scaled(90)
        visible: root.status_value("punish_active", false) === true
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 100
            }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("\u26A1 PUNISH \u26A1")
            color: "#ffcc00"
            font.pixelSize: root.scaled(18)
            font.bold: true
            style: Text.Outline
            styleColor: "#88000000"
        }
    }

    Row {
        id: abilityCooldowns
        objectName: "rpgAbilityCooldowns"
        anchors.right: parent.right
        anchors.rightMargin: root.scaled(28)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomInset + root.scaled(20)
        spacing: root.scaled(12)

        Repeater {
            model: [{
                    "name": qsTr("BASH"),
                    "key": "F",
                    "cdKey": "shield_bash_cooldown_remaining",
                    "totalKey": "shield_bash_cooldown",
                    "readyKey": "shield_bash_ready"
                }, {
                    "name": qsTr("RUSH"),
                    "key": "1",
                    "cdKey": "vanguard_rush_cooldown_remaining",
                    "totalKey": "vanguard_rush_cooldown",
                    "readyKey": "vanguard_rush_ready"
                }, {
                    "name": qsTr("WIND"),
                    "key": "2",
                    "cdKey": "second_wind_cooldown_remaining",
                    "totalKey": "second_wind_cooldown",
                    "readyKey": "second_wind_ready"
                }]

            delegate: Item {
                width: root.abilityTileSize
                height: root.abilityTileSize

                property bool isReady: root.status_value(modelData.readyKey, true) === true
                property real cdRatio: root.cooldown_ratio(modelData.cdKey, modelData.totalKey)

                Rectangle {
                    anchors.fill: parent
                    radius: root.scaled(12)
                    color: parent.isReady ? "#5a1a3d22" : "#5a281312"
                    border.width: 2
                    border.color: parent.isReady ? "#a0ffe0a6" : "#888c6d4e"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.scaled(5)
                    radius: root.scaled(9)
                    color: "transparent"
                    border.width: 1
                    border.color: parent.isReady ? "#35ffffff" : "#22000000"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.scaled(5)
                    radius: root.scaled(9)
                    color: "#33000000"
                    clip: true
                    visible: !parent.isReady

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * parent.parent.cdRatio
                        color: "#7a4a3a30"
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: root.scaled(6)
                    anchors.rightMargin: root.scaled(6)
                    radius: width / 2
                    color: parent.isReady ? "#d6f8e6a0" : "#88796c58"
                    width: root.scaled(18)
                    height: width

                    Text {
                        anchors.centerIn: parent
                        text: modelData.key
                        color: "#1a120b"
                        font.pixelSize: root.scaled(10)
                        font.bold: true
                    }
                }

                Column {
                    id: abilityLabels
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: root.scaled(4)
                    spacing: root.scaled(2)

                    readonly property bool ready: root.status_value(modelData.readyKey, true) === true

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.name
                        color: abilityLabels.ready ? "#eeffeeee" : "#88aaaaaa"
                        font.pixelSize: root.scaled(11)
                        font.bold: true
                        font.letterSpacing: 0.8
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: abilityLabels.ready ? qsTr("READY") : Math.ceil(Number(root.status_value(modelData.cdKey, 0.0))).toString()
                        color: abilityLabels.ready ? "#d2ffe7cb" : "#d2f5c88f"
                        font.pixelSize: root.scaled(9)
                        font.bold: true
                        font.letterSpacing: 0.7
                    }
                }
            }
        }
    }

    property real _prevHealth: -1.0
    property bool _prevAttacking: false
    property bool _prevGuardBroken: false
    property bool _prevPerfectGuard: false
    property bool _prevDodge: false
    property bool _prevLockedTarget: false
    onStatusChanged: {
        var hp = Number(status_value("health_ratio", 1.0));
        var attacking = root.status_value("is_attacking", false) === true;
        var guardBroken = root.status_value("guard_broken", false) === true;
        var perfectGuard = root.status_value("perfect_guard_active", false) === true;
        var dodgeActive = root.status_value("dodge_active", false) === true;
        var hasLockedTarget = root.status_value("focus_marker_locked", false) === true;
        if (_prevHealth >= 0.0 && hp < _prevHealth) {
            var damage_severity = Math.min(1.0, (_prevHealth - hp) * 3.0);
            damageVignette.opacity = damage_severity * 0.8 * Design.A11y.screenEffectIntensity;
            damageDecay.restart();
        }
        if (attacking && !_prevAttacking) {
            attackSweep.opacity = 0.85;
            attackSweepDecay.restart();
        }
        if (perfectGuard && !_prevPerfectGuard) {
            combatEntryFlash.accentColor = "#bce7ff";
            combatEntryFlash.opacity = 0.24;
            combatEntryDecay.restart();
        }
        if (guardBroken && !_prevGuardBroken) {
            guardBreakShock.opacity = 0.26;
            guardBreakDecay.restart();
        }
        if (dodgeActive && !_prevDodge) {
            dodgeTrail.opacity = 0.42;
            dodgeTrailDecay.restart();
        }
        if (hasLockedTarget && !_prevLockedTarget) {
            combatEntryFlash.accentColor = "#8ad6ff";
            combatEntryFlash.opacity = 0.14;
            combatEntryDecay.restart();
        }
        _prevHealth = hp;
        _prevAttacking = attacking;
        _prevGuardBroken = guardBroken;
        _prevPerfectGuard = perfectGuard;
        _prevDodge = dodgeActive;
        _prevLockedTarget = hasLockedTarget;
    }

    Timer {
        id: damageDecay
        interval: 16
        repeat: true
        onTriggered: {
            damageVignette.opacity = Math.max(0.0, damageVignette.opacity - 0.04);
            if (damageVignette.opacity <= 0.0) {
                damageDecay.stop();
            }
        }
    }

    Timer {
        id: combatEntryDecay
        interval: 16
        repeat: true
        onTriggered: {
            combatEntryFlash.opacity = Math.max(0.0, combatEntryFlash.opacity - 0.025);
            if (combatEntryFlash.opacity <= 0.0) {
                combatEntryDecay.stop();
            }
        }
    }

    Timer {
        id: attackSweepDecay
        interval: 16
        repeat: true
        onTriggered: {
            attackSweep.opacity = Math.max(0.0, attackSweep.opacity - 0.05);
            if (attackSweep.opacity <= 0.0) {
                attackSweepDecay.stop();
            }
        }
    }

    Timer {
        id: dodgeTrailDecay
        interval: 16
        repeat: true
        onTriggered: {
            dodgeTrail.opacity = Math.max(0.0, dodgeTrail.opacity - 0.04);
            if (dodgeTrail.opacity <= 0.0) {
                dodgeTrailDecay.stop();
            }
        }
    }

    Timer {
        id: guardBreakDecay
        interval: 16
        repeat: true
        onTriggered: {
            guardBreakShock.opacity = Math.max(0.0, guardBreakShock.opacity - 0.03);
            if (guardBreakShock.opacity <= 0.0) {
                guardBreakDecay.stop();
            }
        }
    }
}

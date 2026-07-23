import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    property real bottomInset: 0
    property var status: ({})

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
        interval: 80
        repeat: true
        running: root.visible && typeof game !== 'undefined' && game.get_controlled_commander_status
        onTriggered: root.status = game.get_controlled_commander_status()
    }

    Component.onCompleted: {
        if (typeof game !== 'undefined' && game.get_controlled_commander_status) {
            status = game.get_controlled_commander_status();
        }
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
            running: Number(root.status_value("health_ratio", 1.0)) < 0.30 && Number(root.status_value("health_ratio", 1.0)) > 0.0
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.0
                to: 0.55
                duration: 600
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: 0.55
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
        visible: root.status_value("guard_active", false) === true
        opacity: visible ? 0.6 : 0.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 12
            color: "transparent"
            border.width: 6
            border.color: "#6688ccff"
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
        width: 190
        height: 190
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
        width: 230
        height: 230
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
        visible: root.status_value("locked_target_name", "") !== "" || root.status_value("is_attacking", false) === true || root.status_value("guard_active", false) === true
        opacity: visible ? (0.22 + Math.min(0.16, Number(root.status_value("combo_step", 0)) * 0.04)) : 0.0

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: 4
            height: 180
            radius: 2
            color: root.status_value("finisher_ready", false) === true ? "#d6ffd36b" : "#8abfe8ff"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            width: 4
            height: 180
            radius: 2
            color: root.status_value("finisher_ready", false) === true ? "#d6ffd36b" : "#8abfe8ff"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -54
            width: 92
            height: 2
            radius: 1
            rotation: -10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 54
            width: 92
            height: 2
            radius: 1
            rotation: 10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -54
            width: 92
            height: 2
            radius: 1
            rotation: 10
            color: "#88f6f3e7"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 54
            width: 92
            height: 2
            radius: 1
            rotation: -10
            color: "#88f6f3e7"
        }
    }

    Item {
        id: attackSweep
        anchors.centerIn: parent
        width: 240
        height: 240
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.centerIn: parent
            width: 164
            height: 6
            radius: 3
            rotation: root.attack_sweep_rotation(root.status_value("attack_direction", 0))
            color: "#d7ffd28a"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 108
            height: 2
            radius: 1
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
            anchors.leftMargin: 72
            anchors.verticalCenter: parent.verticalCenter
            width: 180
            height: 6
            radius: 3
            rotation: -20
            color: "#88b8fff6"
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 72
            anchors.verticalCenter: parent.verticalCenter
            width: 180
            height: 6
            radius: 3
            rotation: 20
            color: "#88b8fff6"
        }
    }

    Item {
        id: guardBreakShock
        anchors.centerIn: parent
        width: 260
        height: 260
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
        id: punishPulseRing
        anchors.centerIn: parent
        width: 136
        height: 136
        visible: root.status_value("punish_active", false) === true
        opacity: visible ? 0.7 : 0.0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: "#99ffc84a"
        }

        SequentialAnimation on scale  {
            running: punishPulseRing.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.86
                to: 1.05
                duration: 320
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                from: 1.05
                to: 0.94
                duration: 420
                easing.type: Easing.InOutQuad
            }
        }
    }

    Item {
        id: lockBrackets
        anchors.centerIn: parent
        width: 92
        height: 92
        visible: root.status_value("locked_target_name", "") !== ""
        opacity: visible ? 0.92 : 0.0

        Repeater {
            model: [{
                    "x": 0,
                    "y": 0,
                    "hAnchor": "left",
                    "vAnchor": "top"
                }, {
                    "x": lockBrackets.width - 18,
                    "y": 0,
                    "hAnchor": "right",
                    "vAnchor": "top"
                }, {
                    "x": 0,
                    "y": lockBrackets.height - 18,
                    "hAnchor": "left",
                    "vAnchor": "bottom"
                }, {
                    "x": lockBrackets.width - 18,
                    "y": lockBrackets.height - 18,
                    "hAnchor": "right",
                    "vAnchor": "bottom"
                }]

            delegate: Item {
                x: modelData.x
                y: modelData.y
                width: 18
                height: 18

                Rectangle {
                    width: 18
                    height: 3
                    radius: 1.5
                    color: "#d7d9f2ff"
                    anchors.top: modelData.vAnchor === "top" ? parent.top : undefined
                    anchors.bottom: modelData.vAnchor === "bottom" ? parent.bottom : undefined
                }

                Rectangle {
                    width: 3
                    height: 18
                    radius: 1.5
                    color: "#d7d9f2ff"
                    anchors.left: modelData.hAnchor === "left" ? parent.left : undefined
                    anchors.right: modelData.hAnchor === "right" ? parent.right : undefined
                }
            }
        }
    }

    Item {
        id: finisherBurst
        anchors.centerIn: parent
        width: 84
        height: 84
        visible: root.status_value("finisher_ready", false) === true
        opacity: visible ? 0.9 : 0.0
        property real spin: 0

        NumberAnimation on spin  {
            running: finisherBurst.visible
            from: 0
            to: 360
            duration: 2600
            loops: Animation.Infinite
        }

        Rectangle {
            anchors.centerIn: parent
            width: 58
            height: 58
            radius: 14
            rotation: finisherBurst.spin
            color: "transparent"
            border.width: 2
            border.color: "#d0ffd24a"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 42
            height: 42
            radius: 12
            rotation: -finisherBurst.spin * 0.7
            color: "transparent"
            border.width: 1
            border.color: "#88fff0aa"
        }
    }

    Item {
        id: crosshair
        width: 58
        height: 58
        anchors.centerIn: parent
        opacity: root.status_value("guard_active", false) === true ? 0.4 : 0.96

        property int comboStep: Number(root.status_value("combo_step", 0))
        property bool finisherReady: root.status_value("finisher_ready", false) === true
        property bool punishActive: root.status_value("punish_active", false) === true
        property bool lockedOn: root.status_value("locked_target_name", "") !== ""
        property color crossColor: finisherReady ? "#ffe07a" : (punishActive ? "#ff9952" : (lockedOn ? "#bfe8ff" : "#f3efe6"))
        property real crossSize: finisherReady ? 1.18 : (comboStep >= 2 ? 1.08 : 1.0)

        scale: crossSize

        Behavior on scale  {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutBack
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 34
            height: 34
            radius: 17
            color: "#18000000"
            border.width: 1
            border.color: "#30ffffff"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 7
            radius: 3.5
            color: crosshair.crossColor
        }

        Rectangle {
            width: 2
            height: 16
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.verticalCenter
            anchors.bottomMargin: 7
        }

        Rectangle {
            width: 2
            height: 16
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            anchors.topMargin: 7
        }

        Rectangle {
            width: 16
            height: 2
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: 7
        }

        Rectangle {
            width: 16
            height: 2
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: 7
        }
    }

    Item {
        id: healthBarContainer
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomInset + 28
        anchors.horizontalCenter: parent.horizontalCenter
        width: 352
        height: 40

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "#d20a0b10"
            border.color: "#55f0c58a"
            border.width: 1

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.height * 0.5
                radius: parent.radius
                color: "#18ffffff"
            }
        }

        Rectangle {
            id: healthDrain
            property real drainRatio: Number(root.status_value("health_ratio", 1.0))
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 8) * drainRatio
            height: parent.height - 8
            radius: 6
            color: "#885a0a0a"

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
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 8) * hpRatio
            height: parent.height - 8
            radius: 6
            color: hpRatio > 0.55 ? "#de2ecc6c" : (hpRatio > 0.28 ? "#decf9732" : "#dedc4a32")

            Behavior on width  {
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutQuad
                }
            }
            Behavior on color  {
                ColorAnimation {
                    duration: 300
                }
            }
        }

        Text {
            anchors.centerIn: parent
            text: "HP  " + Number(root.status_value("health", 0)) + " / " + Number(root.status_value("max_health", 100))
            color: "#ffffff"
            font.pixelSize: 14
            font.bold: true
            style: Text.Outline
            styleColor: "#88000000"
        }
    }

    Item {
        id: staminaBarContainer
        anchors.top: healthBarContainer.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 260
        height: 16

        Rectangle {
            anchors.fill: parent
            radius: 5
            color: "#b30a0a0d"
            border.color: "#44bfe7d9"
            border.width: 1
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
            color: stamRatio > 0.30 ? "#cc3ad7a0" : "#cccf9732"
            opacity: stamRatio < 0.20 ? (0.5 + 0.5 * Math.sin(Date.now() * 0.008)) : 1.0

            Behavior on width  {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutQuad
                }
            }
            Behavior on color  {
                ColorAnimation {
                    duration: 250
                }
            }
        }

        Text {
            anchors.centerIn: parent
            text: "STAMINA"
            color: "#cff7ffff"
            font.pixelSize: 9
            font.bold: true
            font.letterSpacing: 1.5
            style: Text.Outline
            styleColor: "#66000000"
            visible: staminaBarContainer.width > 120
        }
    }

    Item {
        id: comboIndicator
        anchors.right: parent.right
        anchors.rightMargin: 80
        anchors.verticalCenter: parent.verticalCenter
        width: 80
        height: 80
        visible: Number(root.status_value("combo_step", 0)) > 0
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 200
            }
        }

        property int combo: Number(root.status_value("combo_step", 0))
        property bool finisherReady: root.status_value("finisher_ready", false) === true

        Text {
            anchors.centerIn: parent
            text: comboIndicator.combo + "x"
            color: comboIndicator.finisherReady ? "#ffdd00" : "#ffffff"
            font.pixelSize: comboIndicator.finisherReady ? 38 : 30
            font.bold: true
            style: Text.Outline
            styleColor: comboIndicator.finisherReady ? "#cc884400" : "#88000000"
        }

        Text {
            anchors.top: parent.verticalCenter
            anchors.topMargin: 18
            anchors.horizontalCenter: parent.horizontalCenter
            text: comboIndicator.finisherReady ? "FINISHER!" : "COMBO"
            color: comboIndicator.finisherReady ? "#ffdd00" : "#aaffffff"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1.2
            visible: comboIndicator.combo > 0
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            radius: 12
            color: "transparent"
            border.width: 3
            border.color: "#88ffdd00"
            visible: comboIndicator.finisherReady
            opacity: 0.5 + 0.5 * Math.sin(Date.now() * 0.006)
        }
    }

    Item {
        id: postureBar
        anchors.bottom: healthBarContainer.top
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 216
        height: 12
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
            text: "POSTURE"
            color: "#99ffffff"
            font.pixelSize: 7
            font.bold: true
            font.letterSpacing: 1.0
        }
    }

    Item {
        id: guardBreakWarning
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -120
        visible: root.status_value("guard_broken", false) === true
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 150
            }
        }

        Text {
            anchors.centerIn: parent
            text: "GUARD BROKEN"
            color: "#ff4444"
            font.pixelSize: 22
            font.bold: true
            font.letterSpacing: 2.0
            style: Text.Outline
            styleColor: "#cc000000"
        }
    }

    Item {
        id: punishIndicator
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -90
        visible: root.status_value("punish_active", false) === true
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 100
            }
        }

        Text {
            anchors.centerIn: parent
            text: "\u26A1 PUNISH \u26A1"
            color: "#ffcc00"
            font.pixelSize: 18
            font.bold: true
            style: Text.Outline
            styleColor: "#88000000"
        }
    }

    Row {
        id: abilityCooldowns
        anchors.right: parent.right
        anchors.rightMargin: 28
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomInset + 20
        spacing: 12

        Repeater {
            model: [{
                    "name": "BASH",
                    "key": "F",
                    "cdKey": "shield_bash_cooldown_remaining",
                    "totalKey": "shield_bash_cooldown",
                    "readyKey": "shield_bash_ready"
                }, {
                    "name": "RUSH",
                    "key": "1",
                    "cdKey": "vanguard_rush_cooldown_remaining",
                    "totalKey": "vanguard_rush_cooldown",
                    "readyKey": "vanguard_rush_ready"
                }, {
                    "name": "HEAL",
                    "key": "2",
                    "cdKey": "second_wind_cooldown_remaining",
                    "totalKey": "second_wind_cooldown",
                    "readyKey": "second_wind_ready"
                }]

            delegate: Item {
                width: 64
                height: 64

                property bool isReady: root.status_value(modelData.readyKey, true) === true
                property real cdRatio: root.cooldown_ratio(modelData.cdKey, modelData.totalKey)

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: parent.isReady ? "#5a1a3d22" : "#5a281312"
                    border.width: 2
                    border.color: parent.isReady ? "#a0ffe0a6" : "#888c6d4e"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 9
                    color: "transparent"
                    border.width: 1
                    border.color: parent.isReady ? "#35ffffff" : "#22000000"
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 9
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
                    anchors.topMargin: 6
                    anchors.rightMargin: 6
                    radius: 8
                    color: parent.isReady ? "#d6f8e6a0" : "#88796c58"
                    width: 18
                    height: 18

                    Text {
                        anchors.centerIn: parent
                        text: modelData.key
                        color: "#1a120b"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 11
                    text: modelData.name
                    color: parent.isReady ? "#eeffeeee" : "#88aaaaaa"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 22
                    text: parent.isReady ? "READY" : Math.ceil(Number(root.status_value(modelData.cdKey, 0.0))).toString()
                    color: parent.isReady ? "#d2ffe7cb" : "#d2f5c88f"
                    font.pixelSize: 8
                    font.bold: true
                    font.letterSpacing: 0.7
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
        var hasLockedTarget = root.status_value("locked_target_name", "") !== "";
        if (_prevHealth >= 0.0 && hp < _prevHealth) {
            var damage_severity = Math.min(1.0, (_prevHealth - hp) * 3.0);
            damageVignette.opacity = damage_severity * 0.8;
            damageDecay.restart();
        }
        if (attacking && !_prevAttacking) {
            attackSweep.opacity = 0.85;
            attackSweepDecay.restart();
            combatEntryFlash.accentColor = hasLockedTarget ? "#79cfff" : "#ffb260";
            combatEntryFlash.opacity = 0.18;
            combatEntryDecay.restart();
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

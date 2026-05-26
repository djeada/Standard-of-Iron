import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

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

    // ─── Full-screen damage vignette ───
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

    // ─── Low health danger pulse ───
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

    // ─── Guard active edge glow ───
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

    // ─── Perfect guard flash ───
    Rectangle {
        id: perfectGuardFlash
        anchors.fill: parent
        color: "#ffffff"
        opacity: root.status_value("perfect_guard_active", false) === true ? 0.25 : 0.0
        visible: opacity > 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 60
            }
        }
    }

    // ─── Crosshair ───
    Item {
        id: crosshair
        width: 48
        height: 48
        anchors.centerIn: parent
        opacity: root.status_value("guard_active", false) === true ? 0.35 : 0.90

        // Combo-dependent crosshair color
        property int comboStep: Number(root.status_value("combo_step", 0))
        property color crossColor: comboStep >= 3 ? "#ffcc00" : (comboStep >= 2 ? "#ff8844" : "#e0e0e0")
        property real crossSize: comboStep >= 3 ? 1.2 : (comboStep >= 2 ? 1.1 : 1.0)

        scale: crossSize

        Behavior on scale  {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutBack
            }
        }

        // Center dot
        Rectangle {
            width: 5
            height: 5
            radius: 2.5
            color: crosshair.crossColor
            anchors.centerIn: parent
        }

        // Top tick
        Rectangle {
            width: 2
            height: 12
            color: crosshair.crossColor
            opacity: 0.85
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.verticalCenter
            anchors.bottomMargin: 6
        }

        // Bottom tick
        Rectangle {
            width: 2
            height: 12
            color: crosshair.crossColor
            opacity: 0.85
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            anchors.topMargin: 6
        }

        // Left tick
        Rectangle {
            width: 12
            height: 2
            color: crosshair.crossColor
            opacity: 0.85
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: 6
        }

        // Right tick
        Rectangle {
            width: 12
            height: 2
            color: crosshair.crossColor
            opacity: 0.85
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: 6
        }
    }

    // ─── Health bar (bottom center) ───
    Item {
        id: healthBarContainer
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 72
        anchors.horizontalCenter: parent.horizontalCenter
        width: 320
        height: 36

        // Background
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: "#cc0a0a0a"
            border.color: "#55888888"
            border.width: 1
        }

        // Delayed drain bar (shows recent damage)
        Rectangle {
            id: healthDrain
            property real drainRatio: Number(root.status_value("health_ratio", 1.0))
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 8) * drainRatio
            height: parent.height - 8
            radius: 4
            color: "#884a0000"

            Behavior on width  {
                NumberAnimation {
                    duration: 600
                    easing.type: Easing.OutQuad
                }
            }
        }

        // Health fill
        Rectangle {
            id: healthFill
            property real hpRatio: Number(root.status_value("health_ratio", 1.0))
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 8) * hpRatio
            height: parent.height - 8
            radius: 4
            color: hpRatio > 0.55 ? "#dd22aa33" : (hpRatio > 0.28 ? "#ddcc8800" : "#ddcc2200")

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

        // Health text
        Text {
            anchors.centerIn: parent
            text: Number(root.status_value("health", 0)) + " / " + Number(root.status_value("max_health", 100))
            color: "#ffffff"
            font.pixelSize: 13
            font.bold: true
            style: Text.Outline
            styleColor: "#88000000"
        }
    }

    // ─── Stamina bar (below health) ───
    Item {
        id: staminaBarContainer
        anchors.top: healthBarContainer.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 240
        height: 14

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "#aa0a0a0a"
            border.color: "#44888888"
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
            radius: 3
            color: stamRatio > 0.30 ? "#cc44bb66" : "#cccc6622"
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
            color: "#aaffffff"
            font.pixelSize: 8
            font.bold: true
            font.letterSpacing: 1.5
            style: Text.Outline
            styleColor: "#66000000"
            visible: staminaBarContainer.width > 120
        }
    }

    // ─── Combo counter (right of crosshair) ───
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

        // Finisher pulse glow
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

    // ─── Posture bar (above health bar when taking damage) ───
    Item {
        id: postureBar
        anchors.bottom: healthBarContainer.top
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 200
        height: 10
        visible: Number(root.status_value("posture_ratio", 0.0)) > 0.05
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity  {
            NumberAnimation {
                duration: 200
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: "#880a0a0a"
            border.color: "#44aaaaaa"
            border.width: 1
        }

        Rectangle {
            property real postureRatio: Number(root.status_value("posture_ratio", 0.0))
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - 4) * postureRatio
            height: parent.height - 4
            radius: 2
            color: postureRatio > 0.75 ? "#ccff3300" : (postureRatio > 0.45 ? "#ccff8800" : "#ccffcc00")

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

    // ─── Guard break warning ───
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

    // ─── Punish window indicator ───
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

    // ─── Ability cooldown indicators (bottom-right) ───
    Row {
        id: abilityCooldowns
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        spacing: 12

        Repeater {
            model: [{
                    "name": "BASH",
                    "cdKey": "shield_bash_cooldown_remaining",
                    "totalKey": "shield_bash_cooldown",
                    "readyKey": "shield_bash_ready"
                }, {
                    "name": "RUSH",
                    "cdKey": "vanguard_rush_cooldown_remaining",
                    "totalKey": "vanguard_rush_cooldown",
                    "readyKey": "vanguard_rush_ready"
                }, {
                    "name": "HEAL",
                    "cdKey": "second_wind_cooldown_remaining",
                    "totalKey": "second_wind_cooldown",
                    "readyKey": "second_wind_ready"
                }]

            delegate: Item {
                width: 52
                height: 52

                property bool isReady: root.status_value(modelData.readyKey, true) === true
                property real cdRatio: root.cooldown_ratio(modelData.cdKey, modelData.totalKey)

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: parent.isReady ? "#44228844" : "#44220000"
                    border.width: 2
                    border.color: parent.isReady ? "#88aaffaa" : "#88886644"
                }

                // Cooldown sweep
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: 6
                    color: "#44000000"
                    clip: true
                    visible: !parent.isReady

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * parent.parent.cdRatio
                        color: "#66444444"
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: modelData.name
                    color: parent.isReady ? "#eeffeeee" : "#88aaaaaa"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.5
                }
            }
        }
    }

    // ─── Damage vignette trigger from health changes ───
    property real _prevHealth: -1.0
    onStatusChanged: {
        var hp = Number(status_value("health_ratio", 1.0));
        if (_prevHealth >= 0.0 && hp < _prevHealth) {
            var damage_severity = Math.min(1.0, (_prevHealth - hp) * 3.0);
            damageVignette.opacity = damage_severity * 0.8;
            damageDecay.restart();
        }
        _prevHealth = hp;
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
}

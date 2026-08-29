import QtQuick 2.15
import QtQuick.Layouts 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root
    anchors.fill: parent

    property real bottomInset: 0
    property real topInset: 0
    property var status: ({})
    property var projector: null

    readonly property real resolutionScale: Math.max(0.75, Math.min(2.0, height / 1080))
    readonly property real uiScale: Math.max(0.75, Math.min(4.0, root.resolutionScale * Design.A11y.uiScale))

    function scaled(value) {
        return Math.round(value * root.uiScale);
    }

    function fontSize(rung) {
        return Math.max(Design.Typography.minimumSize, Math.round(rung * root.resolutionScale));
    }

    function shade(base, alpha) {
        return Qt.rgba(base.r, base.g, base.b, alpha);
    }

    readonly property color ember: Design.Theme.danger
    readonly property color bronze: Design.Theme.accent
    readonly property color bronzeBright: Design.Theme.focus
    readonly property color bone: Design.Theme.parchment
    readonly property color iron: Design.Theme.backgroundDeep
    readonly property color steel: Qt.rgba(0.63, 0.69, 0.75, 1.0)

    property real pulsePhase

    NumberAnimation on pulsePhase  {
        running: root.visible && Design.Motion.allowAmbientLoops
        from: 0.0
        to: 1.0
        duration: 1000
        loops: Animation.Infinite
    }

    readonly property real slowPulse: 0.5 + 0.5 * Math.sin(pulsePhase * 2 * Math.PI)

    readonly property bool bowStance: String(status_value("weapon_stance", "melee")) === "bow"
    readonly property bool bowDrawing: bowStance && status_value("bow_drawing", false) === true
    readonly property bool bowFullDraw: bowStance && status_value("bow_full_draw", false) === true
    readonly property bool bowStrained: bowDrawing && status_value("bow_hold_strained", false) === true
    readonly property real bowDrawProgress: Math.max(0.0, Math.min(1.0, Number(status_value("bow_draw_progress", 0.0))))

    readonly property real verticalFovDegrees: Math.max(20.0, Math.min(110.0, Number(status_value("camera_fov_degrees", 68.0))))

    readonly property real focalPixels: (root.height * 0.5) / Math.tan(verticalFovDegrees * Math.PI / 360.0)

    readonly property real bowSpreadPixels: bowStance ? Math.min(root.height * 0.24, focalPixels * Math.tan(Math.min(28.0, Number(status_value("bow_spread_degrees", 0.0))) * Math.PI / 180.0)) : 0.0

    readonly property real healthRatio: Math.max(0.0, Math.min(1.0, Number(status_value("health_ratio", 1.0))))
    readonly property real staminaRatio: Math.max(0.0, Math.min(1.0, Number(status_value("stamina_ratio", 1.0))))
    readonly property real postureRatio: Math.max(0.0, Math.min(1.0, Number(status_value("posture_ratio", 0.0))))
    readonly property int comboStep: Number(status_value("combo_step", 0))
    readonly property bool finisherReady: status_value("finisher_ready", false) === true
    readonly property bool punishActive: status_value("punish_active", false) === true
    readonly property bool guardBroken: status_value("guard_broken", false) === true
    readonly property bool lockedOn: status_value("focus_marker_locked", false) === true
    readonly property bool skirmishContext: String(status_value("fight_context", "none")) === "skirmish"

    readonly property real targetRatio: Math.max(0.0, Math.min(1.0, Number(status_value("focus_target_hp_ratio", 0.0))))
    readonly property bool targetStaggered: status_value("focus_target_staggered", false) === true
    readonly property bool targetGuardBroken: status_value("focus_target_guard_broken", false) === true
    readonly property bool targetPunishable: targetStaggered || targetGuardBroken

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

    function vitality_color(ratio) {
        if (ratio <= 0.3) {
            return root.ember;
        }
        if (ratio <= 0.6) {
            return Qt.tint(root.ember, root.shade(Design.Theme.warning, 0.35));
        }
        return root.ember;
    }

    Timer {
        interval: 33
        repeat: true
        running: root.visible && typeof game !== 'undefined' && game.commander.status
        onTriggered: root.status = game.commander.status()
    }

    property bool focusProjected: false
    property real focusScreenX: 0
    property real focusScreenY: 0
    property real focusScreenHeight: 0

    function refresh_focus_projection() {
        if (root.projector === null || !root.projector.ready || root.status_value("focus_marker_valid", false) !== true) {
            root.focusProjected = false;
            return;
        }
        var wx = Number(root.status_value("focus_marker_x", 0));
        var wy = Number(root.status_value("focus_marker_y", 0));
        var wz = Number(root.status_value("focus_marker_z", 0));
        var chest = root.projector.project(wx, wy + 1.15, wz);
        var feet = root.projector.project(wx, wy, wz);
        if (chest === null || feet === null) {
            root.focusProjected = false;
            return;
        }
        root.focusProjected = true;
        root.focusScreenX = chest.x;
        root.focusScreenY = chest.y;
        root.focusScreenHeight = Math.abs(feet.y - chest.y) * (1.8 / 1.15);
    }

    Connections {
        target: root.visible ? root.projector : null

        function onTickChanged() {
            root.refresh_focus_projection();
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
                    color: root.shade(root.ember, 0.62)
                }
                GradientStop {
                    position: 0.35
                    color: "transparent"
                }
                GradientStop {
                    position: 0.65
                    color: "transparent"
                }
                GradientStop {
                    position: 1.0
                    color: root.shade(root.ember, 0.62)
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            rotation: 90
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: root.shade(root.ember, 0.42)
                }
                GradientStop {
                    position: 0.3
                    color: "transparent"
                }
                GradientStop {
                    position: 0.7
                    color: "transparent"
                }
                GradientStop {
                    position: 1.0
                    color: root.shade(root.ember, 0.42)
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
                    color: root.shade(root.ember, 0.42)
                }
                GradientStop {
                    position: 0.45
                    color: "transparent"
                }
                GradientStop {
                    position: 0.55
                    color: "transparent"
                }
                GradientStop {
                    position: 1.0
                    color: root.shade(root.ember, 0.42)
                }
            }
        }

        SequentialAnimation on opacity  {
            running: Design.A11y.screenEffectIntensity > 0.0 && root.healthRatio < 0.3 && root.healthRatio > 0.0
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
            border.color: root.shade(root.steel, 0.24)
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
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
            border.width: Math.max(2, root.scaled(5))
            border.color: root.bone
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
        property color accentColor: root.steel
        opacity: 0.0
        visible: opacity > 0.0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, root.scaled(4))
            border.color: combatEntryFlash.accentColor
        }
    }

    Item {
        id: combatFrame
        anchors.fill: parent
        visible: root.lockedOn
        opacity: visible ? (0.55 + Math.min(0.3, root.comboStep * 0.08)) : 0.0

        readonly property color railColor: root.finisherReady ? root.bronzeBright : root.bronze

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.normal
            }
        }

        Repeater {
            model: [-1, 1]

            delegate: Rectangle {
                required property int modelData

                anchors.left: modelData < 0 ? parent.left : undefined
                anchors.right: modelData > 0 ? parent.right : undefined
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: root.scaled(96)
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: modelData < 0 ? root.shade(combatFrame.railColor, 0.3) : "transparent"
                    }
                    GradientStop {
                        position: 1.0
                        color: modelData < 0 ? "transparent" : root.shade(combatFrame.railColor, 0.3)
                    }
                }
            }
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
            height: Math.max(2, root.scaled(6))
            radius: height / 2
            rotation: root.attack_sweep_rotation(root.status_value("attack_direction", 0))
            color: root.shade(root.bone, 0.82)
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.scaled(108)
            height: Math.max(1, root.scaled(2))
            radius: height / 2
            rotation: root.attack_sweep_rotation(root.status_value("attack_direction", 0)) + 90
            color: root.shade(root.bronzeBright, 0.6)
        }
    }

    Item {
        id: dodgeTrail
        anchors.fill: parent
        opacity: 0.0
        visible: opacity > 0.0

        Repeater {
            model: [-1, 1]

            delegate: Rectangle {
                required property int modelData

                anchors.left: modelData < 0 ? parent.left : undefined
                anchors.right: modelData > 0 ? parent.right : undefined
                anchors.leftMargin: root.scaled(72)
                anchors.rightMargin: root.scaled(72)
                anchors.verticalCenter: parent.verticalCenter
                width: root.scaled(180)
                height: Math.max(2, root.scaled(6))
                radius: height / 2
                rotation: modelData * 20
                color: root.shade(root.steel, 0.55)
            }
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
            border.width: Math.max(2, root.scaled(6))
            border.color: root.ember
        }
    }

    Repeater {
        model: [{
                "side": "left",
                "name": "rpgLeftThreatPip",
                "flag": "threat_left"
            }, {
                "side": "right",
                "name": "rpgRightThreatPip",
                "flag": "threat_right"
            }]

        delegate: Item {
            required property var modelData

            objectName: modelData.name
            anchors.left: modelData.side === "left" ? parent.left : undefined
            anchors.right: modelData.side === "right" ? parent.right : undefined
            anchors.leftMargin: root.scaled(22)
            anchors.rightMargin: root.scaled(22)
            anchors.verticalCenter: parent.verticalCenter
            width: root.scaled(14)
            height: root.scaled(46)
            visible: root.skirmishContext && root.status_value(modelData.flag, false) === true
            opacity: visible ? 0.55 + 0.35 * root.slowPulse : 0.0

            Canvas {
                anchors.fill: parent
                readonly property bool pointsRight: modelData.side === "right"
                readonly property color pipColor: root.bronze

                onPipColorChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.beginPath();
                    if (pointsRight) {
                        ctx.moveTo(0, 0);
                        ctx.lineTo(width, height / 2);
                        ctx.lineTo(0, height);
                    } else {
                        ctx.moveTo(width, 0);
                        ctx.lineTo(0, height / 2);
                        ctx.lineTo(width, height);
                    }
                    ctx.closePath();
                    ctx.fillStyle = pipColor;
                    ctx.fill();
                }
            }
        }
    }

    Item {
        id: lockBrackets

        readonly property int armLength: root.scaled(root.lockedOn ? 20 : 15)
        readonly property int armThickness: Math.max(2, root.scaled(3))
        readonly property color armColor: root.lockedOn ? root.bronzeBright : root.shade(root.bone, 0.8)

        width: Math.max(root.scaled(38), Math.min(root.scaled(220), root.focusScreenHeight * (root.lockedOn ? 0.78 : 0.66)))
        height: width
        x: root.focusProjected ? root.focusScreenX - width / 2 : (parent.width - width) / 2
        y: root.focusProjected ? root.focusScreenY - height / 2 : (parent.height - height) / 2
        visible: root.focusProjected && root.status_value("focus_marker_valid", false) === true
        opacity: visible ? (root.lockedOn ? 0.95 : 0.7) : 0.0

        Behavior on width  {
            NumberAnimation {
                duration: Design.Motion.fast
                easing.type: Easing.OutQuad
            }
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
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
                required property var modelData

                width: lockBrackets.armLength
                height: lockBrackets.armLength
                anchors.left: modelData.hAnchor === "left" ? lockBrackets.left : undefined
                anchors.right: modelData.hAnchor === "right" ? lockBrackets.right : undefined
                anchors.top: modelData.vAnchor === "top" ? lockBrackets.top : undefined
                anchors.bottom: modelData.vAnchor === "bottom" ? lockBrackets.bottom : undefined

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

        width: root.scaled(54)
        height: width
        anchors.centerIn: parent
        opacity: root.status_value("guard_active", false) === true ? 0.4 : 0.96

        readonly property bool targetInRange: root.status_value("aim_candidate_in_range", false) === true
        readonly property color crossColor: (root.bowFullDraw || root.finisherReady) ? root.bronzeBright : (root.punishActive ? root.ember : (targetInRange || root.lockedOn ? root.bronze : root.shade(root.bone, 0.9)))
        readonly property real crossSize: root.finisherReady ? 1.16 : (targetInRange ? 1.1 : (root.comboStep >= 2 ? 1.06 : 1.0))
        readonly property int tickThickness: Math.max(2, root.scaled(3))
        readonly property int tickLength: root.scaled(15)
        property real tickGap: root.scaled(7) + root.bowSpreadPixels

        scale: crossSize

        Behavior on tickGap  {
            NumberAnimation {
                duration: 90
                easing.type: Easing.OutQuad
            }
        }

        Behavior on scale  {
            NumberAnimation {
                duration: Design.Motion.fast
                easing.type: Easing.OutBack
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.scaled(32)
            height: width
            radius: width / 2
            color: root.shade(root.iron, 0.18)
            border.width: Design.Metrics.borderThin
            border.color: root.shade(crosshair.crossColor, 0.28)
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.max(3, root.scaled(6))
            height: width
            radius: width / 2
            color: crosshair.crossColor
        }

        Rectangle {
            width: crosshair.tickThickness
            height: crosshair.tickLength
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: crosshair.horizontalCenter
            anchors.bottom: crosshair.verticalCenter
            anchors.bottomMargin: crosshair.tickGap
        }

        Rectangle {
            width: crosshair.tickThickness
            height: crosshair.tickLength
            color: crosshair.crossColor
            opacity: 0.9
            anchors.horizontalCenter: crosshair.horizontalCenter
            anchors.top: crosshair.verticalCenter
            anchors.topMargin: crosshair.tickGap
        }

        Rectangle {
            width: crosshair.tickLength
            height: crosshair.tickThickness
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: crosshair.verticalCenter
            anchors.right: crosshair.horizontalCenter
            anchors.rightMargin: crosshair.tickGap
        }

        Rectangle {
            width: crosshair.tickLength
            height: crosshair.tickThickness
            color: crosshair.crossColor
            opacity: 0.9
            anchors.verticalCenter: crosshair.verticalCenter
            anchors.left: crosshair.horizontalCenter
            anchors.leftMargin: crosshair.tickGap
        }
    }

    Canvas {
        id: drawRing
        objectName: "rpgBowDrawRing"
        anchors.centerIn: parent
        width: root.scaled(84)
        height: width
        visible: root.bowDrawing
        opacity: visible ? 1.0 : 0.0
        antialiasing: true

        readonly property real progress: root.bowDrawProgress
        readonly property color ringColor: root.bowStrained ? root.ember : (root.bowFullDraw ? root.bronzeBright : root.shade(root.bone, 0.85))

        onProgressChanged: requestPaint()
        onRingColorChanged: requestPaint()
        onVisibleChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            var cx = width / 2;
            var cy = height / 2;
            var radius = width / 2 - root.scaled(4);
            var start = -Math.PI / 2;
            ctx.lineWidth = Math.max(2, root.scaled(3));
            ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.35);
            ctx.beginPath();
            ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
            ctx.stroke();
            if (progress <= 0.0)
                return;
            ctx.strokeStyle = ringColor;
            ctx.lineCap = "round";
            ctx.beginPath();
            ctx.arc(cx, cy, radius, start, start + progress * 2 * Math.PI);
            ctx.stroke();
        }

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + root.scaled(10)
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Design.Metrics.borderFocus
            border.color: root.bronzeBright
            visible: root.bowFullDraw
            opacity: 0.35 + 0.35 * root.slowPulse
        }
    }

    Item {
        id: combatCallout
        objectName: "rpgCombatCallout"

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter
        anchors.bottomMargin: root.scaled(78)
        width: calloutLabel.implicitWidth + root.scaled(34)
        height: calloutLabel.implicitHeight + root.scaled(12)
        visible: root.guardBroken || root.punishActive || root.finisherReady
        opacity: visible ? 1.0 : 0.0

        readonly property color toneColor: root.guardBroken ? root.ember : (root.punishActive ? Design.Theme.warning : root.bronzeBright)

        Behavior on opacity  {
            NumberAnimation {
                duration: Design.Motion.fast
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: Design.Metrics.radiusSmall
            color: root.shade(root.iron, 0.62)
            border.width: Design.Metrics.borderThin
            border.color: root.shade(combatCallout.toneColor, 0.55)
        }

        Text {
            id: calloutLabel
            anchors.centerIn: parent
            text: root.guardBroken ? qsTr("GUARD BROKEN") : (root.punishActive ? qsTr("PUNISH") : qsTr("FINISHER"))
            color: combatCallout.toneColor
            font.family: Design.Typography.family
            font.pixelSize: root.fontSize(Design.Typography.label)
            font.weight: Design.Typography.bold
            font.letterSpacing: Design.Typography.trackingWide
            style: Text.Outline
            styleColor: root.shade(root.iron, 0.8)
        }
    }

    Item {
        id: hudBand
        objectName: "rpgHudBand"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomInset + root.scaled(20)
        height: Math.max(vitalsPlate.height, Math.max(abilityColumn.height, targetPlate.height))

        readonly property int edgeMargin: root.scaled(24)

        Item {
            id: vitalsPlate
            objectName: "rpgVitalsPlate"

            anchors.left: parent.left
            anchors.leftMargin: hudBand.edgeMargin
            anchors.bottom: parent.bottom
            width: Math.min(root.scaled(330), root.width * 0.34)
            height: vitalsColumn.implicitHeight + 2 * root.scaled(11)

            Rectangle {
                anchors.fill: parent
                anchors.margins: -root.scaled(3)
                radius: Design.Metrics.radiusLarge
                color: root.shade(root.iron, 0.35)
            }

            Rectangle {
                anchors.fill: parent
                radius: Design.Metrics.radiusMedium
                border.width: Design.Metrics.borderThin
                border.color: root.shade(root.bronze, 0.75)
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: root.shade(Design.Theme.panelLeather, 0.94)
                    }
                    GradientStop {
                        position: 1.0
                        color: root.shade(root.iron, 0.96)
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Math.max(2, root.scaled(3))
                radius: Design.Metrics.radiusSmall
                color: "transparent"
                border.width: Design.Metrics.borderThin
                border.color: root.shade(root.bone, 0.1)
            }

            Repeater {
                model: [{
                        "h": "left",
                        "v": "top"
                    }, {
                        "h": "right",
                        "v": "top"
                    }, {
                        "h": "left",
                        "v": "bottom"
                    }, {
                        "h": "right",
                        "v": "bottom"
                    }]

                delegate: Rectangle {
                    required property var modelData

                    width: Math.max(2, root.scaled(3))
                    height: width
                    radius: width / 2
                    color: root.shade(root.bronze, 0.8)
                    anchors.left: modelData.h === "left" ? vitalsPlate.left : undefined
                    anchors.right: modelData.h === "right" ? vitalsPlate.right : undefined
                    anchors.top: modelData.v === "top" ? vitalsPlate.top : undefined
                    anchors.bottom: modelData.v === "bottom" ? vitalsPlate.bottom : undefined
                    anchors.margins: root.scaled(6)
                }
            }

            ColumnLayout {
                id: vitalsColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.scaled(11)
                spacing: root.scaled(6)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.scaled(8)

                    Row {
                        id: comboIndicator
                        objectName: "rpgComboIndicator"

                        Layout.alignment: Qt.AlignVCenter
                        spacing: root.scaled(4)
                        opacity: root.comboStep > 0 ? 1.0 : 0.25

                        Behavior on opacity  {
                            NumberAnimation {
                                duration: Design.Motion.normal
                            }
                        }

                        Repeater {
                            model: 4

                            delegate: Rectangle {
                                required property int index

                                readonly property bool lit: index < root.comboStep
                                readonly property bool isFinisher: index === 3

                                anchors.verticalCenter: comboIndicator.verticalCenter
                                width: root.scaled(7)
                                height: width
                                rotation: 45
                                radius: 1
                                color: lit ? (isFinisher ? root.bronzeBright : root.bronze) : root.shade(root.bone, 0.16)
                                opacity: lit && isFinisher ? (0.6 + 0.4 * root.slowPulse) : 1.0

                                Behavior on color  {
                                    ColorAnimation {
                                        duration: Design.Motion.fast
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: String(root.status_value("name", ""))
                        color: root.shade(root.bone, 0.88)
                        elide: Text.ElideRight
                        font.family: Design.Typography.titleFamily
                        font.pixelSize: root.fontSize(Design.Typography.label)
                        font.letterSpacing: Design.Typography.trackingTitle
                        font.capitalization: Font.AllUppercase
                        font.hintingPreference: Design.Typography.titleHinting
                        font.kerning: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("HP %1/%2").arg(Number(root.status_value("health", 0))).arg(Number(root.status_value("max_health", 0)))
                        color: root.shade(root.bone, 0.96)
                        font.family: Design.Typography.family
                        font.pixelSize: root.fontSize(Design.Typography.label)
                        font.weight: Design.Typography.bold
                    }
                }

                RpgMeter {
                    objectName: "rpgHealthMeter"

                    Layout.fillWidth: true
                    Layout.preferredHeight: root.scaled(19)
                    value: root.healthRatio
                    ghostValue: root.healthRatio
                    ghostDuration: Design.Motion.reducedMotion ? 0 : 620
                    fillColor: Qt.darker(root.ember, 1.18)
                    frameColor: root.bronze
                    frameOpacity: 0.7
                    segments: 4
                }

                RpgMeter {
                    Layout.fillWidth: true
                    Layout.maximumWidth: vitalsColumn.width * 0.84
                    Layout.preferredHeight: root.scaled(10)
                    value: root.staminaRatio
                    fillColor: Qt.darker(Design.Theme.success, 1.35)
                    frameColor: root.bronze
                    frameOpacity: 0.4
                    crest: false
                    starved: root.staminaRatio < 0.2
                }

                Item {
                    id: postureBar
                    objectName: "rpgPostureBar"

                    Layout.fillWidth: true
                    Layout.maximumWidth: vitalsColumn.width * 0.66
                    Layout.preferredHeight: root.scaled(8)
                    opacity: root.postureRatio > 0.05 ? 1.0 : 0.0

                    Behavior on opacity  {
                        NumberAnimation {
                            duration: Design.Motion.normal
                        }
                    }

                    RpgMeter {
                        anchors.fill: parent
                        value: root.postureRatio
                        fillColor: root.postureRatio > 0.75 ? root.ember : Design.Theme.warning
                        frameColor: Design.Theme.warning
                        frameOpacity: 0.35
                        crest: false
                        segments: 6
                    }
                }
            }
        }

        Item {
            id: targetPlate
            objectName: "rpgTargetPlate"

            readonly property int sideWidth: Math.max(vitalsPlate.width, abilityColumn.width)
            readonly property int available: root.width - 2 * (sideWidth + hudBand.edgeMargin + root.scaled(16))

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: Math.max(0, Math.min(root.scaled(330), available))
            height: targetColumn.implicitHeight + 2 * root.scaled(10)
            visible: Number(root.status_value("focus_target_max_hp", 0)) > 0 && width >= root.scaled(150)
            opacity: visible ? 1.0 : 0.0

            Behavior on opacity  {
                NumberAnimation {
                    duration: Design.Motion.fast
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -root.scaled(3)
                radius: Design.Metrics.radiusLarge
                color: root.shade(root.iron, 0.35)
            }

            Rectangle {
                anchors.fill: parent
                radius: Design.Metrics.radiusMedium
                border.width: Design.Metrics.borderThin
                border.color: root.shade(root.lockedOn ? root.bronze : root.bone, root.lockedOn ? 0.7 : 0.24)
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: root.shade(Design.Theme.panelLeather, 0.9)
                    }
                    GradientStop {
                        position: 1.0
                        color: root.shade(root.iron, 0.94)
                    }
                }
            }

            ColumnLayout {
                id: targetColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.scaled(10)
                spacing: root.scaled(5)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.scaled(6)

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: root.scaled(8)
                        Layout.preferredHeight: root.scaled(8)
                        rotation: 45
                        radius: 1
                        color: root.lockedOn ? root.bronzeBright : root.shade(root.bone, 0.45)
                    }

                    Text {
                        text: qsTr("TARGET")
                        color: root.shade(root.bone, 0.5)
                        font.family: Design.Typography.family
                        font.pixelSize: root.fontSize(Design.Typography.caption)
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Text {
                        Layout.fillWidth: true
                        text: String(root.status_value("focus_target_name", ""))
                        color: root.shade(root.bone, 0.95)
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        font.family: Design.Typography.titleFamily
                        font.pixelSize: root.fontSize(Design.Typography.label)
                        font.letterSpacing: Design.Typography.trackingTitle
                        font.capitalization: Font.AllUppercase
                        font.hintingPreference: Design.Typography.titleHinting
                        font.kerning: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.scaled(6)

                    RpgMeter {
                        objectName: "rpgTargetMeter"

                        Layout.fillWidth: true
                        Layout.preferredHeight: root.scaled(12)
                        value: root.targetRatio
                        fillColor: root.targetPunishable ? root.bronzeBright : root.shade(root.bone, 0.78)
                        frameColor: root.bone
                        frameOpacity: 0.25
                        crest: false
                    }

                    Text {
                        Layout.preferredWidth: root.scaled(38)
                        text: qsTr("%1%").arg(Math.round(root.targetRatio * 100))
                        color: root.shade(root.bone, 0.8)
                        horizontalAlignment: Text.AlignRight
                        font.family: Design.Typography.family
                        font.pixelSize: root.fontSize(Design.Typography.label)
                        font.weight: Design.Typography.bold
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.targetGuardBroken ? qsTr("GUARD BROKEN") : qsTr("STAGGERED")
                    color: Design.Theme.warning
                    elide: Text.ElideRight
                    visible: root.targetPunishable
                    font.family: Design.Typography.family
                    font.pixelSize: root.fontSize(Design.Typography.caption)
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }
            }
        }

        Column {
            id: abilityColumn

            anchors.right: parent.right
            anchors.rightMargin: hudBand.edgeMargin
            anchors.bottom: parent.bottom
            spacing: root.scaled(7)

            readonly property int tileSize: root.scaled(70)

            Rectangle {
                id: weaponStanceChip
                objectName: "rpgWeaponStanceChip"

                anchors.right: parent.right
                width: stanceLabel.implicitWidth + root.scaled(18)
                height: root.scaled(22)
                radius: Design.Metrics.radiusSmall
                color: root.shade(root.iron, 0.9)
                border.width: Design.Metrics.borderThin
                border.color: root.shade(root.bronze, 0.6)
                visible: root.status_value("can_switch_weapon", false) === true

                Text {
                    id: stanceLabel
                    anchors.centerIn: parent
                    text: root.bowStance ? qsTr("BOW  ·  X") : qsTr("BLADE  ·  X")
                    color: root.shade(root.bone, 0.85)
                    font.family: Design.Typography.family
                    font.pixelSize: root.fontSize(Design.Typography.caption)
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }
            }

            Row {
                id: abilityCooldowns
                objectName: "rpgAbilityCooldowns"

                anchors.right: parent.right
                spacing: root.scaled(9)

                Repeater {
                    model: [{
                            "name": qsTr("SPECIAL"),
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
                        id: abilityTile
                        required property var modelData

                        readonly property bool isReady: root.status_value(abilityTile.modelData.readyKey, true) === true
                        readonly property real cdRatio: root.cooldown_ratio(abilityTile.modelData.cdKey, abilityTile.modelData.totalKey)

                        width: Math.max(abilityColumn.tileSize, tileLabels.implicitWidth + root.scaled(22))
                        height: Math.max(abilityColumn.tileSize, tileLabels.implicitHeight + keycap.height + root.scaled(20))

                        Rectangle {
                            id: tileFace
                            anchors.fill: parent
                            radius: Design.Metrics.radiusMedium
                            border.width: Design.Metrics.borderThin
                            border.color: abilityTile.isReady ? root.shade(root.bronze, 0.95) : root.shade(root.bone, 0.24)
                            clip: true
                            gradient: Gradient {
                                GradientStop {
                                    position: 0.0
                                    color: root.shade(Design.Theme.panelLeather, 0.94)
                                }
                                GradientStop {
                                    position: 1.0
                                    color: root.shade(root.iron, 0.96)
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                width: parent.width * (1.0 - abilityTile.cdRatio)
                                height: Math.max(2, root.scaled(4))
                                visible: !abilityTile.isReady
                                color: root.shade(root.bronze, 0.8)

                                Behavior on width  {
                                    NumberAnimation {
                                        duration: Design.Motion.fast
                                    }
                                }
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: Design.Motion.normal
                                }
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: Math.max(2, root.scaled(4))
                            radius: Design.Metrics.radiusSmall
                            color: "transparent"
                            border.width: Design.Metrics.borderThin
                            border.color: abilityTile.isReady ? root.shade(root.bone, 0.16) : "transparent"
                        }

                        Rectangle {
                            id: keycap
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: root.scaled(6)
                            anchors.rightMargin: root.scaled(6)
                            width: root.scaled(17)
                            height: width
                            radius: Design.Metrics.radiusSmall
                            color: abilityTile.isReady ? root.shade(root.bronze, 0.95) : root.shade(root.bone, 0.26)

                            Text {
                                anchors.centerIn: parent
                                text: abilityTile.modelData.key
                                color: abilityTile.isReady ? root.iron : root.shade(root.bone, 0.6)
                                font.family: Design.Typography.family
                                font.pixelSize: root.fontSize(Design.Typography.caption)
                                font.weight: Design.Typography.bold
                            }
                        }

                        Column {
                            id: tileLabels
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: root.scaled(9)
                            spacing: root.scaled(2)

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: abilityTile.modelData.name
                                color: abilityTile.isReady ? root.shade(root.bone, 0.96) : root.shade(root.bone, 0.62)
                                font.family: Design.Typography.family
                                font.pixelSize: root.fontSize(Design.Typography.label)
                                font.weight: Design.Typography.bold
                                font.letterSpacing: Design.Typography.trackingTitle
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: abilityTile.isReady ? qsTr("READY") : Math.ceil(Number(root.status_value(abilityTile.modelData.cdKey, 0.0))).toString()
                                color: abilityTile.isReady ? root.shade(root.bronze, 0.95) : root.shade(root.bone, 0.72)
                                font.family: Design.Typography.family
                                font.pixelSize: root.fontSize(Design.Typography.caption)
                                font.weight: Design.Typography.medium
                                font.letterSpacing: Design.Typography.trackingTitle
                            }
                        }
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
        var hp = root.healthRatio;
        var attacking = root.status_value("is_attacking", false) === true;
        var guardBroken = root.guardBroken;
        var perfectGuard = root.status_value("perfect_guard_active", false) === true;
        var dodgeActive = root.status_value("dodge_active", false) === true;
        var hasLockedTarget = root.lockedOn;
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
            combatEntryFlash.accentColor = root.bone;
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
            combatEntryFlash.accentColor = root.bronze;
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

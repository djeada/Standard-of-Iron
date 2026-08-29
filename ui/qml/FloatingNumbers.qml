import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    anchors.fill: parent

    property var source: null
    property var projector: null
    property bool combatEnabled: true
    property bool economyEnabled: true

    readonly property bool reducedMotion: Design.A11y.reducedMotion
    readonly property int maxTicks: reducedMotion ? 12 : 24
    readonly property int maxBursts: 48
    readonly property int activeTicks: tickLayer.children.length
    readonly property int activeBursts: burstLayer.children.length
    readonly property var resourceKeys: ["gold", "food", "wood", "stone", "iron"]

    Accessible.ignored: true

    function resource_key(index) {
        return index >= 0 && index < root.resourceKeys.length ? root.resourceKeys[index] : "";
    }

    function resource_glyph(index) {
        var key = root.resource_key(index);
        return key.length > 0 ? Design.Icons.resourceGlyph(key) : "";
    }

    function signed(amount) {
        return (amount > 0 ? "+" : "") + amount.toString();
    }

    function is_important_tick(ev) {
        return !!ev.killingBlow || !!ev.focused || Number(ev.severity || 0.0) >= 0.25;
    }

    function is_important_burst(ev) {
        return !!ev.killingBlow || Number(ev.severity || 0.0) >= 0.66;
    }

    function shows_damage(ev, burst) {
        if (Design.A11y.damageNumberMode === "off")
            return false;
        if (Design.A11y.damageNumberMode === "important")
            return burst ? root.is_important_burst(ev) : root.is_important_tick(ev);
        return true;
    }

    function accepts(ev, burst) {
        if (ev.kind === "damage" || ev.kind === "heal")
            return root.combatEnabled && root.shows_damage(ev, burst);
        return root.economyEnabled;
    }

    function accent_for(ev) {
        if (ev.kind === "resource")
            return Number(ev.amount) < 0 ? "#e4a05d" : "#8fe0a8";
        if (ev.kind === "reserve")
            return "#9db8ff";
        if (ev.kind === "heal")
            return "#8fe0a8";
        if (ev.killingBlow)
            return ev.incoming ? "#ff8a6a" : "#ffe08a";
        return ev.incoming ? "#ff6b5a" : "#ffd36b";
    }

    function body_for(ev) {
        if (ev.kind === "resource" || ev.kind === "reserve")
            return root.signed(Number(ev.amount));
        var magnitude = Math.abs(Number(ev.amount));
        return (ev.incoming ? "-" : "") + magnitude.toString();
    }

    Component {
        id: damageTick

        Item {
            id: tick

            required property int amount
            required property int hits
            required property real worldX
            required property real worldY
            required property real worldZ
            required property real severity
            required property int lane
            required property bool killingBlow
            required property bool incoming
            required property bool focused
            required property string kind
            required property int resource
            required property int pairedResource
            required property int pairedAmount

            readonly property var event: ({
                    "kind": kind,
                    "amount": amount,
                    "killingBlow": killingBlow,
                    "incoming": incoming,
                    "severity": severity
                })

            readonly property var projection: {
                if (root.projector === null)
                    return null;
                root.projector.tick;
                return root.projector.project(worldX, worldY, worldZ);
            }

            property real progress: 0.0
            readonly property real weight: Math.max(0.0, Math.min(1.0, severity * 1.4))
            readonly property real rise: root.reducedMotion ? 6 : (34 + weight * 22 + (killingBlow ? 12 : 0))
            readonly property real drift: root.reducedMotion ? 0 : lane * 9
            readonly property color accent: root.accent_for(tick.event)
            readonly property real fontSize: Design.Typography.display((focused ? 17 : 14) + (killingBlow ? 4 : 0) + weight * 3)

            width: label.implicitWidth + 14
            height: label.implicitHeight + 6
            visible: projection !== null
            x: (projection !== null ? projection.x : 0) - width / 2 + drift * progress
            y: (projection !== null ? root.projector.clamped_y(projection.y, height) : 0) - 18 - rise * progress
            opacity: progress < 0.55 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.55) / 0.45))
            scale: root.reducedMotion ? 1.0 : (progress < 0.12 ? 0.8 + progress * 2.2 : Math.max(0.92, 1.06 - progress * 0.14))

            SequentialAnimation {
                running: true

                NumberAnimation {
                    target: tick
                    property: "progress"
                    from: 0.0
                    to: 1.0
                    duration: root.reducedMotion ? 700 : (tick.killingBlow ? 1050 : 850)
                    easing.type: Easing.OutCubic
                }

                ScriptAction {
                    script: tick.destroy()
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: tick.killingBlow ? "#e0200a04" : "#c8100c08"
                border.width: tick.focused || tick.killingBlow ? 2 : 1
                border.color: tick.accent
            }

            Row {
                id: label

                anchors.centerIn: parent
                spacing: 3

                Text {
                    visible: tick.killingBlow
                    anchors.verticalCenter: parent.verticalCenter
                    text: "☠"
                    color: tick.accent
                    font.pixelSize: tick.fontSize
                    font.bold: true
                }

                Text {
                    visible: tick.resource >= 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.resource_glyph(tick.resource)
                    color: tick.accent
                    font.pixelSize: tick.fontSize
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.body_for(tick.event)
                    color: tick.incoming && tick.kind === "damage" ? "#ffe6df" : "#fff6d6"
                    font.pixelSize: tick.fontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: tick.accent
                }

                Text {
                    visible: tick.kind === "reserve"
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("POP")
                    color: tick.accent
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 5)
                    font.bold: true
                }

                Text {
                    visible: tick.pairedResource >= 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: "→ " + root.resource_glyph(tick.pairedResource) + root.signed(tick.pairedAmount)
                    color: "#8fe0a8"
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 2)
                    font.bold: true
                }

                Text {
                    visible: tick.hits > 1 && tick.kind === "damage"
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×" + tick.hits
                    color: tick.accent
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 5)
                    font.bold: true
                }
            }
        }
    }

    Component {
        id: damageBurst

        Item {
            id: burst

            required property int amount
            required property real worldX
            required property real worldY
            required property real worldZ
            required property real severityRatio
            required property int lane
            required property bool killingBlow

            readonly property var projection: {
                if (root.projector === null)
                    return null;
                root.projector.tick;
                return root.projector.project(worldX, worldY, worldZ);
            }

            property real progress: 0.0
            property real textScale: 0.74

            readonly property real severity: {
                var base = severityRatio > 0.0 ? Math.max(0.0, Math.min(1.0, severityRatio * 0.95)) : (amount >= 60 ? 0.90 : (amount >= 35 ? 0.66 : (amount >= 20 ? 0.48 : 0.30)));
                return killingBlow ? Math.max(0.85, base) : base;
            }
            readonly property real driftX: (lane * 16) + (lane === 0 ? 0 : (lane > 0 ? 10 : -10)) * (0.45 + severity)
            readonly property real riseDistance: 98 + severity * 34 + Math.abs(lane) * 10
            readonly property real ringSize: 76 + severity * 44
            readonly property color accentColor: killingBlow ? "#ffd36b" : (severity >= 0.72 ? "#ff784e" : (severity >= 0.46 ? "#ffae42" : "#ffffff"))
            readonly property color sparkColor: killingBlow ? "#fff6c2" : (severity >= 0.72 ? "#ffd0b0" : "#ffe4ad")

            width: 260
            height: 220
            visible: projection !== null
            x: (projection !== null ? projection.x : 0) - width / 2 + driftX * Math.pow(progress, 0.8)
            y: (projection !== null ? root.projector.clamped_y(projection.y, 60) : 0) - 140 - Math.abs(lane) * 8 - riseDistance * progress
            opacity: progress < 0.56 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.56) / 0.44))

            SequentialAnimation {
                running: true

                ParallelAnimation {
                    NumberAnimation {
                        target: burst
                        property: "progress"
                        from: 0.0
                        to: 1.0
                        duration: burst.killingBlow ? 780 : 680
                        easing.type: Easing.OutCubic
                    }

                    SequentialAnimation {
                        NumberAnimation {
                            target: burst
                            property: "textScale"
                            from: 0.74
                            to: burst.killingBlow ? 1.28 : 1.18
                            duration: 110
                            easing.type: Easing.OutBack
                        }

                        NumberAnimation {
                            target: burst
                            property: "textScale"
                            to: 1.0
                            duration: 180
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                ScriptAction {
                    script: burst.destroy()
                }
            }

            Item {
                id: burstCore

                anchors.centerIn: parent
                width: burst.ringSize * 1.8
                height: burst.ringSize * 1.8
                scale: 0.82 + burst.progress * 0.42

                Rectangle {
                    width: burst.ringSize * 0.90
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    color: burst.accentColor
                    opacity: (burst.killingBlow ? 0.26 : 0.18) * (1.0 - burst.progress)
                    scale: 0.85 + burst.progress * 0.65
                }

                Rectangle {
                    width: burst.ringSize
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    color: "transparent"
                    border.width: burst.killingBlow ? 4 : 3
                    border.color: burst.accentColor
                    opacity: 0.95 * (1.0 - burst.progress)
                    scale: 0.74 + burst.progress * 1.18
                }

                Rectangle {
                    width: burst.ringSize * 0.72
                    height: width
                    radius: width / 2
                    anchors.centerIn: parent
                    color: "transparent"
                    border.width: 2
                    border.color: burst.sparkColor
                    opacity: 0.72 * Math.max(0.0, 1.0 - burst.progress * 1.25)
                    scale: 0.92 + burst.progress * 0.58
                }

                Repeater {
                    model: burst.killingBlow ? 8 : 6

                    delegate: Item {
                        required property int index

                        width: burstCore.width
                        height: burstCore.height
                        anchors.centerIn: parent
                        rotation: (360 / (burst.killingBlow ? 8 : 6)) * index + burst.lane * 9

                        Rectangle {
                            width: 18 + burst.severity * 24
                            height: burst.killingBlow ? 5 : 4
                            radius: height / 2
                            x: burstCore.width / 2 + 14 + burst.severity * 12 + burst.progress * 16
                            y: burstCore.height / 2 - height / 2
                            color: parent.index % 2 === 0 ? burst.accentColor : burst.sparkColor
                            opacity: 0.92 * Math.max(0.0, 1.0 - burst.progress * 1.18)
                        }
                    }
                }
            }

            Item {
                id: labelContainer

                x: burst.width / 2 - width / 2
                y: 30
                width: Math.max(110, damageText.implicitWidth + 34)
                height: Math.max(52, damageText.implicitHeight + 18)
                scale: burst.textScale

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: burst.killingBlow ? "#d92b0900" : "#c4000000"
                    border.width: burst.killingBlow ? 2 : 1
                    border.color: burst.killingBlow ? "#ccffd36b" : "#88ffffff"
                    opacity: 0.92
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: height / 2
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, burst.killingBlow ? 0.36 : 0.22)
                }

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 2
                    text: burst.amount.toString()
                    color: "#000000"
                    opacity: 0.82
                    font.pixelSize: Design.Typography.display(burst.killingBlow ? 42 : (burst.severity >= 0.72 ? 36 : 30))
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#66000000"
                }

                Text {
                    id: damageText

                    anchors.centerIn: parent
                    text: burst.amount.toString()
                    color: burst.killingBlow ? "#fff3bf" : (burst.severity >= 0.72 ? "#ffebe3" : (burst.severity >= 0.46 ? "#fff0c6" : "#ffffff"))
                    font.pixelSize: Design.Typography.display(burst.killingBlow ? 42 : (burst.severity >= 0.72 ? 36 : 30))
                    font.bold: true
                    font.letterSpacing: burst.severity >= 0.72 ? 0.6 : 0.2
                    style: Text.Outline
                    styleColor: burst.killingBlow ? "#ff7420" : (burst.severity >= 0.72 ? "#d93d16" : "#000000")
                }
            }
        }
    }

    Item {
        id: tickLayer

        anchors.fill: parent
    }

    Item {
        id: burstLayer

        anchors.fill: parent
    }

    Timer {
        interval: 16
        running: root.source !== null && root.projector !== null && root.visible
        repeat: true
        onTriggered: {
            if (root.source === null)
                return;
            var events = root.source.pop_feedback_ticks();
            for (var i = 0; i < events.length; ++i) {
                var ev = events[i];
                var burst = ev.style === "burst";
                if (!root.accepts(ev, burst))
                    continue;
                if (burst) {
                    if (burstLayer.children.length >= root.maxBursts)
                        continue;
                    damageBurst.createObject(burstLayer, {
                            "amount": Math.abs(Number(ev.amount || 0)),
                            "worldX": Number(ev.x || 0.0),
                            "worldY": Number(ev.y || 0.0),
                            "worldZ": Number(ev.z || 0.0),
                            "severityRatio": Number(ev.severity || 0.0),
                            "lane": Number(ev.lane || 0),
                            "killingBlow": !!ev.killingBlow
                        });
                    continue;
                }
                if (tickLayer.children.length >= root.maxTicks)
                    continue;
                damageTick.createObject(tickLayer, {
                        "amount": Number(ev.amount || 0),
                        "hits": Number(ev.hits || 1),
                        "worldX": Number(ev.x || 0.0),
                        "worldY": Number(ev.y || 0.0),
                        "worldZ": Number(ev.z || 0.0),
                        "severity": Number(ev.severity || 0.0),
                        "lane": Number(ev.lane || 0),
                        "killingBlow": !!ev.killingBlow,
                        "incoming": !!ev.incoming,
                        "focused": !!ev.focused,
                        "kind": String(ev.kind || "damage"),
                        "resource": Number(ev.resource === undefined ? -1 : ev.resource),
                        "pairedResource": Number(ev.pairedResource === undefined ? -1 : ev.pairedResource),
                        "pairedAmount": Number(ev.pairedAmount || 0)
                    });
            }
        }
    }
}

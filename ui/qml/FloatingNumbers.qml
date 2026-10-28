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
    readonly property int maxTicks: reducedMotion ? 12 : 18
    readonly property int maxBursts: 24
    readonly property int maxTicksPerAnchor: 2
    readonly property int maxBurstsPerAnchor: 1
    readonly property int activeTicks: tickLayer.children.length
    readonly property int activeBursts: burstLayer.children.length
    readonly property var resourceKeys: ["gold", "food", "wood", "stone", "iron"]
    property var activeByAnchor: new Map()

    Accessible.ignored: true

    function anchor_key(ev, burst) {
        var anchor = Number(ev.anchor || 0);
        if (anchor <= 0)
            return "";
        return (burst ? "b" : "t") + anchor;
    }

    function anchor_count(key) {
        return Number(root.activeByAnchor.get(key) || 0);
    }

    function anchor_busy(key, limit) {
        if (key.length === 0)
            return false;
        return root.anchor_count(key) >= limit;
    }

    function acquire_anchor(key) {
        if (key.length === 0)
            return;
        root.activeByAnchor.set(key, root.anchor_count(key) + 1);
    }

    function release_anchor(key) {
        if (key.length === 0)
            return;
        var left = root.anchor_count(key) - 1;
        if (left <= 0)
            root.activeByAnchor.delete(key);
        else
            root.activeByAnchor.set(key, left);
    }

    function resource_key(index) {
        return index >= 0 && index < root.resourceKeys.length ? root.resourceKeys[index] : "";
    }

    function resource_glyph(index) {
        var key = root.resource_key(index);
        return key.length > 0 ? Design.Icons.resourceGlyph(key) : "";
    }

    readonly property real playfieldTop: root.projector !== null ? root.projector.topInset : 0
    readonly property real playfieldBottom: root.height - (root.projector !== null ? root.projector.bottomInset : 0)

    function view_x(x, w) {
        return Math.max(4, Math.min(root.width - w - 4, x));
    }

    function view_y(y, h) {
        var low = root.playfieldTop + 4;
        var high = root.playfieldBottom - h - 4;
        if (high <= low)
            return y;
        return Math.max(low, Math.min(high, y));
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

    readonly property var resourceAccents: ({
            "gold": "#f5c400",
            "food": "#ff9a1f",
            "wood": "#b8742c",
            "stone": "#8d9bab",
            "iron": "#4f9be8"
        })

    function accent_for(ev) {
        if (ev.kind === "resource") {
            if (Number(ev.amount) < 0)
                return "#c2463c";
            return root.resourceAccents[root.resource_key(Number(ev.resource === undefined ? -1 : ev.resource))] || "#3fbf5f";
        }
        if (ev.kind === "reserve")
            return Number(ev.amount) < 0 ? "#7a5bd6" : "#9b7bff";
        if (ev.kind === "heal")
            return "#1fbf6a";
        if (ev.killingBlow)
            return ev.incoming ? "#b5122e" : "#ffb000";
        return ev.incoming ? "#e5322d" : "#f07f13";
    }

    function body_for(ev) {
        if (ev.kind === "resource" || ev.kind === "reserve")
            return root.signed(Number(ev.amount));
        var magnitude = Math.abs(Number(ev.amount));
        if (ev.kind === "heal")
            return "+" + magnitude.toString();
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
            required property string anchorKey

            readonly property var event: ({
                    "kind": kind,
                    "amount": amount,
                    "killingBlow": killingBlow,
                    "incoming": incoming,
                    "severity": severity,
                    "resource": resource
                })

            readonly property var projection: {
                if (root.projector === null)
                    return null;
                root.projector.tick;
                return root.projector.project(worldX, worldY, worldZ);
            }

            property real progress: 0.0
            readonly property real weight: Math.max(0.0, Math.min(1.0, severity * 1.4))
            readonly property real rise: root.reducedMotion ? 6 : (46 + weight * 28 + (killingBlow ? 16 : 0))
            readonly property real drift: root.reducedMotion ? 0 : lane * 12
            readonly property color accent: root.accent_for(tick.event)
            readonly property real fontSize: Design.Typography.display((focused ? 24 : 21) + (killingBlow ? 5 : 0) + weight * 4)
            readonly property real pop: root.reducedMotion ? 1.0 : (progress < 0.08 ? 0.55 + progress * 8.0 : (progress < 0.2 ? 1.18 - (progress - 0.08) * 1.5 : 1.0))

            width: label.implicitWidth + 26
            height: label.implicitHeight + 10
            visible: projection !== null
            x: root.view_x((projection !== null ? projection.x : 0) - width / 2 + drift * progress, width)
            y: root.view_y((projection !== null ? root.projector.clamped_y(projection.y, height) : 0) - 22 - rise * progress, height)
            opacity: progress < 0.62 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.62) / 0.38))
            scale: pop

            SequentialAnimation {
                running: true

                NumberAnimation {
                    target: tick
                    property: "progress"
                    from: 0.0
                    to: 1.0
                    duration: root.reducedMotion ? 800 : (tick.killingBlow ? 1350 : 1150)
                    easing.type: Easing.OutCubic
                }

                ScriptAction {
                    script: {
                        root.release_anchor(tick.anchorKey);
                        tick.destroy();
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: -3
                radius: height / 2
                color: "#73000000"
            }

            Rectangle {
                id: pill

                anchors.fill: parent
                radius: height / 2
                border.width: tick.focused || tick.killingBlow ? 3 : 2
                border.color: Qt.lighter(tick.accent, 1.55)
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: Qt.lighter(tick.accent, 1.22)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.darker(tick.accent, 1.45)
                    }
                }
            }

            Rectangle {
                x: pill.height * 0.32
                y: 3
                width: pill.width - pill.height * 0.64
                height: pill.height * 0.36
                radius: height / 2
                color: Qt.rgba(1.0, 1.0, 1.0, 0.26)
            }

            Row {
                id: label

                anchors.centerIn: parent
                spacing: 3

                Text {
                    visible: tick.killingBlow
                    anchors.verticalCenter: parent.verticalCenter
                    text: "☠"
                    color: "#ffffff"
                    font.pixelSize: tick.fontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#a6000000"
                }

                Text {
                    visible: tick.resource >= 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.resource_glyph(tick.resource)
                    color: "#ffffff"
                    font.pixelSize: tick.fontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#a6000000"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.body_for(tick.event)
                    color: "#ffffff"
                    font.pixelSize: tick.fontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#b3000000"
                }

                Text {
                    visible: tick.kind === "reserve"
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("POP")
                    color: "#ffffff"
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 6)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#a6000000"
                }

                Text {
                    visible: tick.pairedResource >= 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: "→ " + root.resource_glyph(tick.pairedResource) + root.signed(tick.pairedAmount)
                    color: "#f2ffe8"
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 3)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#a6000000"
                }

                Text {
                    visible: tick.hits > 1 && tick.kind === "damage"
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×" + tick.hits
                    color: "#ffffff"
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 6)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#a6000000"
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
            required property string anchorKey

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
            readonly property color pillColor: killingBlow ? "#f0a000" : (severity >= 0.72 ? "#e0401f" : (severity >= 0.46 ? "#ee7a10" : "#d98a1c"))

            width: 260
            height: 220
            visible: projection !== null
            x: root.view_x((projection !== null ? projection.x : 0) - width / 2 + driftX * Math.pow(progress, 0.8), width)
            y: root.view_y((projection !== null ? root.projector.clamped_y(projection.y, 60) : 0) - 140 - Math.abs(lane) * 8 - riseDistance * progress + labelContainer.y, labelContainer.height) - labelContainer.y
            opacity: progress < 0.68 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.68) / 0.32))

            SequentialAnimation {
                running: true

                ParallelAnimation {
                    NumberAnimation {
                        target: burst
                        property: "progress"
                        from: 0.0
                        to: 1.0
                        duration: burst.killingBlow ? 1250 : 1050
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
                    script: {
                        root.release_anchor(burst.anchorKey);
                        burst.destroy();
                    }
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
                    anchors.topMargin: 4
                    anchors.bottomMargin: -4
                    radius: height / 2
                    color: "#80000000"
                }

                Rectangle {
                    id: burstPill

                    anchors.fill: parent
                    radius: height / 2
                    border.width: burst.killingBlow ? 3 : 2
                    border.color: Qt.lighter(burst.pillColor, 1.6)
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Qt.lighter(burst.pillColor, 1.25)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.darker(burst.pillColor, 1.5)
                        }
                    }
                }

                Rectangle {
                    x: burstPill.height * 0.32
                    y: 4
                    width: burstPill.width - burstPill.height * 0.64
                    height: burstPill.height * 0.36
                    radius: height / 2
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.28)
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
                    color: "#ffffff"
                    font.pixelSize: Design.Typography.display(burst.killingBlow ? 42 : (burst.severity >= 0.72 ? 36 : 30))
                    font.bold: true
                    font.letterSpacing: burst.severity >= 0.72 ? 0.6 : 0.2
                    style: Text.Outline
                    styleColor: "#b3000000"
                }
            }
        }
    }

    Item {
        id: playfield

        objectName: "floatingNumbersPlayfield"
        x: 0
        y: root.playfieldTop
        width: root.width
        height: Math.max(0, root.playfieldBottom - root.playfieldTop)
        clip: true

        Item {
            id: tickLayer

            y: -playfield.y
            width: root.width
            height: root.height
        }

        Item {
            id: burstLayer

            y: -playfield.y
            width: root.width
            height: root.height
        }
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
                var key = root.anchor_key(ev, burst);
                if (burst) {
                    if (burstLayer.children.length >= root.maxBursts)
                        continue;
                    if (!ev.killingBlow && root.anchor_busy(key, root.maxBurstsPerAnchor))
                        continue;
                    root.acquire_anchor(key);
                    damageBurst.createObject(burstLayer, {
                            "amount": Math.abs(Number(ev.amount || 0)),
                            "worldX": Number(ev.x || 0.0),
                            "worldY": Number(ev.y || 0.0),
                            "worldZ": Number(ev.z || 0.0),
                            "severityRatio": Number(ev.severity || 0.0),
                            "lane": Number(ev.lane || 0),
                            "killingBlow": !!ev.killingBlow,
                            "anchorKey": key
                        });
                    continue;
                }
                if (tickLayer.children.length >= root.maxTicks)
                    continue;
                if (!ev.killingBlow && root.anchor_busy(key, root.maxTicksPerAnchor))
                    continue;
                root.acquire_anchor(key);
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
                        "pairedAmount": Number(ev.pairedAmount || 0),
                        "anchorKey": key
                    });
            }
        }
    }
}

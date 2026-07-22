import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    property var engine: null
    readonly property int maxActiveBursts: 48

    function clamp01(value) {
        return Math.max(0.0, Math.min(1.0, value));
    }

    function severityForEvent(ev) {
        var ratio = Number(ev.damageRatio || 0.0);
        var rawDamage = Number(ev.damage || 0);
        var base = ratio > 0.0 ? clamp01(ratio * 0.95) : (rawDamage >= 60 ? 0.90 : (rawDamage >= 35 ? 0.66 : (rawDamage >= 20 ? 0.48 : 0.30)));
        return ev.killingBlow ? Math.max(0.85, base) : base;
    }

    function canSpawnBurst() {
        return effectLayer.children.length < maxActiveBursts;
    }

    Component {
        id: damageBurst

        Item {
            id: burst
            required property int dmg
            required property real worldX
            required property real worldY
            required property real worldZ
            required property real damageRatio
            required property int lane
            required property bool killingBlow

            property real progress: 0.0
            property real severity: root.severityForEvent({
                    "damage": dmg,
                    "damageRatio": damageRatio,
                    "killingBlow": killingBlow
                })
            property real baseX: 0.0
            property real baseY: 0.0
            property bool projected: false
            property real driftX: (lane * 16) + (lane === 0 ? 0 : (lane > 0 ? 10 : -10)) * (0.45 + severity)
            property real riseDistance: 98 + severity * 34 + Math.abs(lane) * 10
            property real ringSize: 76 + severity * 44
            property real textScale: 0.74
            property color accentColor: killingBlow ? "#ffd36b" : (severity >= 0.72 ? "#ff784e" : (severity >= 0.46 ? "#ffae42" : "#ffffff"))
            property color sparkColor: killingBlow ? "#fff6c2" : (severity >= 0.72 ? "#ffd0b0" : "#ffe4ad")

            width: 260
            height: 220
            visible: projected
            x: baseX - width / 2 + driftX * Math.pow(progress, 0.8)
            y: baseY - 140 - Math.abs(lane) * 8 - riseDistance * progress
            opacity: progress < 0.56 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.56) / 0.44))

            function refreshProjection() {
                if (root.engine === null) {
                    projected = false;
                    return;
                }
                var proj = root.engine.rpg_project_world(worldX, worldY, worldZ);
                projected = !!proj.valid;
                if (!projected)
                    return;
                baseX = proj.x;
                baseY = proj.y;
            }

            Component.onCompleted: refreshProjection()

            Timer {
                interval: 16
                running: true
                repeat: true
                onTriggered: burst.refreshProjection()
            }

            SequentialAnimation {
                running: true

                ParallelAnimation {
                    NumberAnimation {
                        target: burst
                        property: "progress"
                        from: 0.0
                        to: 1.0
                        duration: killingBlow ? 780 : 680
                        easing.type: Easing.OutCubic
                    }

                    SequentialAnimation {
                        NumberAnimation {
                            target: burst
                            property: "textScale"
                            from: 0.74
                            to: killingBlow ? 1.28 : 1.18
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
                            color: index % 2 === 0 ? burst.accentColor : burst.sparkColor
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
                    text: burst.dmg.toString()
                    color: "#000000"
                    opacity: 0.82
                    font.pixelSize: burst.killingBlow ? 42 : (burst.severity >= 0.72 ? 36 : 30)
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#66000000"
                }

                Text {
                    id: damageText
                    anchors.centerIn: parent
                    text: burst.dmg.toString()
                    color: burst.killingBlow ? "#fff3bf" : (burst.severity >= 0.72 ? "#ffebe3" : (burst.severity >= 0.46 ? "#fff0c6" : "#ffffff"))
                    font.pixelSize: burst.killingBlow ? 42 : (burst.severity >= 0.72 ? 36 : 30)
                    font.bold: true
                    font.letterSpacing: burst.severity >= 0.72 ? 0.6 : 0.2
                    style: Text.Outline
                    styleColor: burst.killingBlow ? "#ff7420" : (burst.severity >= 0.72 ? "#d93d16" : "#000000")
                }
            }
        }
    }

    Item {
        id: effectLayer
        anchors.fill: parent
    }

    Timer {
        interval: 16
        running: root.engine !== null
        repeat: true
        onTriggered: {
            if (root.engine === null)
                return;
            var events = root.engine.pop_rpg_damage_events();
            if (!root.visible)
                return;
            for (var i = 0; i < events.length; ++i) {
                if (!root.canSpawnBurst())
                    break;
                var ev = events[i];
                damageBurst.createObject(effectLayer, {
                        "dmg": Number(ev.damage || 0),
                        "worldX": Number(ev.x || 0.0),
                        "worldY": Number(ev.y || 0.0),
                        "worldZ": Number(ev.z || 0.0),
                        "damageRatio": Number(ev.damageRatio || 0.0),
                        "lane": Number(ev.lane || 0),
                        "killingBlow": !!ev.killingBlow
                    });
            }
        }
    }
}

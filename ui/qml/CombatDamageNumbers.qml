import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    anchors.fill: parent

    property var engine: null
    readonly property var activity: engine !== null && engine.activity ? engine.activity : null
    property bool reducedMotion: Design.A11y.reducedMotion
    readonly property int maxActiveBursts: reducedMotion ? 12 : 24

    function canSpawnBurst() {
        return effectLayer.children.length < maxActiveBursts;
    }

    function accentFor(ev) {
        if (ev.killingBlow)
            return ev.incoming ? "#ff8a6a" : "#ffe08a";
        if (ev.incoming)
            return "#ff6b5a";
        return "#ffd36b";
    }

    Component {
        id: damageTick

        Item {
            id: tick

            required property int dmg
            required property int hits
            required property real worldX
            required property real worldY
            required property real worldZ
            required property real damageRatio
            required property int lane
            required property bool killingBlow
            required property bool incoming
            required property bool focused

            property real progress: 0.0
            property real baseX: 0.0
            property real baseY: 0.0
            property bool projected: false
            property real severity: Math.max(0.0, Math.min(1.0, damageRatio * 1.4))
            property real rise: root.reducedMotion ? 6 : (34 + severity * 22 + (killingBlow ? 12 : 0))
            property real drift: root.reducedMotion ? 0 : lane * 9
            property color accent: root.accentFor({
                    "killingBlow": killingBlow,
                    "incoming": incoming
                })
            property real fontSize: Design.Typography.display((focused ? 17 : 14) + (killingBlow ? 4 : 0) + severity * 3)

            width: label.implicitWidth + 14
            height: label.implicitHeight + 6
            visible: projected
            x: baseX - width / 2 + drift * progress
            y: baseY - 18 - rise * progress
            opacity: progress < 0.55 ? 1.0 : Math.max(0.0, 1.0 - ((progress - 0.55) / 0.45))
            scale: root.reducedMotion ? 1.0 : (progress < 0.12 ? 0.8 + progress * 2.2 : Math.max(0.92, 1.06 - progress * 0.14))

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

            SequentialAnimation {
                running: true

                NumberAnimation {
                    target: tick
                    property: "progress"
                    from: 0.0
                    to: 1.0
                    duration: root.reducedMotion ? 700 : (killingBlow ? 1050 : 850)
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
                    anchors.verticalCenter: parent.verticalCenter
                    text: (tick.incoming ? "-" : "") + tick.dmg.toString()
                    color: tick.incoming ? "#ffe6df" : "#fff6d6"
                    font.pixelSize: tick.fontSize
                    font.bold: true
                    style: Text.Outline
                    styleColor: tick.accent
                }

                Text {
                    visible: tick.hits > 1
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×" + tick.hits
                    color: tick.accent
                    font.pixelSize: Math.max(Design.Typography.minimumSize, tick.fontSize - 5)
                    font.bold: true
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
        running: root.engine !== null && root.visible && effectLayer.children.length > 0
        repeat: true
        onTriggered: {
            for (var i = 0; i < effectLayer.children.length; ++i) {
                var tick = effectLayer.children[i];
                if (tick && tick.refreshProjection)
                    tick.refreshProjection();
            }
        }
    }

    Timer {
        interval: 33
        running: root.activity !== null && root.visible
        repeat: true
        onTriggered: {
            if (root.activity === null)
                return;
            var events = root.activity.pop_combat_damage_events();
            for (var i = 0; i < events.length; ++i) {
                if (!root.canSpawnBurst())
                    break;
                var ev = events[i];
                damageTick.createObject(effectLayer, {
                        "dmg": Number(ev.damage || 0),
                        "hits": Number(ev.hits || 1),
                        "worldX": Number(ev.x || 0.0),
                        "worldY": Number(ev.y || 0.0),
                        "worldZ": Number(ev.z || 0.0),
                        "damageRatio": Number(ev.damageRatio || 0.0),
                        "lane": Number(ev.lane || 0),
                        "killingBlow": !!ev.killingBlow,
                        "incoming": !!ev.incoming,
                        "focused": !!ev.focused
                    });
            }
        }
    }
}

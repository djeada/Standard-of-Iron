import QtQuick 2.15



Item {
    id: root
    anchors.fill: parent

    property var engine: null

    
    Component {
        id: damageLabel

        Text {
            id: label
            required property int dmg
            required property real screenX
            required property real screenY

            x: screenX - width / 2
            y: screenY
            text: dmg.toString()
            color: dmg >= 50 ? "#ff4444"
                 : dmg >= 20 ? "#ffaa22"
                             : "#ffffff"
            font.pixelSize: dmg >= 50 ? 22 : 16
            font.bold: true
            style: Text.Outline
            styleColor: "#000000"
            opacity: 1.0

            SequentialAnimation {
                id: anim
                running: true

                ParallelAnimation {
                    NumberAnimation { target: label; property: "y"; to: label.y - 60; duration: 900; easing.type: Easing.OutCubic }
                    NumberAnimation { target: label; property: "opacity"; from: 1.0; to: 0.0; duration: 900; easing.type: Easing.InQuad }
                }

                ScriptAction { script: label.destroy() }
            }
        }
    }

    
    Timer {
        interval: 50
        running: root.engine !== null
        repeat: true
        onTriggered: {
            if (root.engine === null) return
            var events = root.engine.pop_rpg_damage_events()
            for (var i = 0; i < events.length; ++i) {
                var ev = events[i]
                var proj = root.engine.rpg_project_world(ev.x, ev.y, ev.z)
                if (!proj.valid) continue
                damageLabel.createObject(root, {
                    dmg: ev.damage,
                    screenX: proj.x,
                    screenY: proj.y
                })
            }
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

RowLayout {
    id: bottomRoot

    readonly property var hs: StyleGuide.historical
    property var commander_status: default_status()

    function default_status() {
        return {
            "has_commander": false,
            "id": 0,
            "name": qsTr("Commander"),
            "nation": "",
            "alive": false,
            "health": 0,
            "max_health": 0,
            "health_ratio": 0.0,
            "stamina_ratio": 1.0,
            "is_running": false,
            "can_run": false,
            "rally_cooldown": 0.0,
            "rally_cooldown_remaining": 0.0,
            "rally_feedback_time": 0.0,
            "rally_ready": false,
            "aura_active": false,
            "combo_step": 0,
            "posture": 0.0,
            "posture_max": 100.0,
            "posture_ratio": 0.0,
            "punish_window_remaining": 0.0,
            "punish_active": false,
            "perfect_guard_remaining": 0.0,
            "perfect_guard_active": false,
            "guard_break_remaining": 0.0,
            "guard_broken": false,
            "finisher_ready": false,
            "camera_mode": "Chase",
            "shield_bash_cooldown": 3.0,
            "shield_bash_cooldown_remaining": 0.0,
            "shield_bash_ready": true,
            "vanguard_rush_cooldown": 4.5,
            "vanguard_rush_cooldown_remaining": 0.0,
            "vanguard_rush_ready": true,
            "second_wind_cooldown": 8.0,
            "second_wind_cooldown_remaining": 0.0,
            "second_wind_ready": true
        };
    }

    function refresh_status() {
        if (typeof game !== 'undefined' && game.get_controlled_commander_status) {
            commander_status = game.get_controlled_commander_status();
            return;
        }
        commander_status = default_status();
    }

    function status_value(key, fallback) {
        return commander_status && commander_status[key] !== undefined ? commander_status[key] : fallback;
    }

    function ratio_color(ratio) {
        if (ratio > 0.6)
            return Theme.accent;
        if (ratio > 0.3)
            return hs.bronze;
        return hs.waxHover;
    }

    function rally_progress() {
        var cooldown = Number(status_value("rally_cooldown", 0));
        var remaining = Number(status_value("rally_cooldown_remaining", 0));
        if (cooldown <= 0)
            return 1;
        return Math.max(0, Math.min(1, 1 - remaining / cooldown));
    }

    function rally_label() {
        if (!status_value("has_commander", false))
            return qsTr("Commander unavailable");
        if (status_value("rally_ready", false))
            return qsTr("Rally ready");
        return qsTr("Recovering %1s").arg(Number(status_value("rally_cooldown_remaining", 0)).toFixed(1));
    }

    function cooldown_progress(cooldownKey, remainingKey) {
        var cooldown = Math.max(0.001, Number(status_value(cooldownKey, 0)));
        var remaining = Number(status_value(remainingKey, 0));
        return Math.max(0, Math.min(1, 1 - remaining / cooldown));
    }

    function combat_abilities() {
        return [{
                "title": qsTr("Vanguard Rush"),
                "key": "1",
                "description": qsTr("Explode forward into the focused enemy and open a punish."),
                "readyKey": "vanguard_rush_ready",
                "cooldownKey": "vanguard_rush_cooldown",
                "remainingKey": "vanguard_rush_cooldown_remaining"
            }, {
                "title": qsTr("Second Wind"),
                "key": "2",
                "description": qsTr("Recover posture and stamina, then steady yourself for the next exchange."),
                "readyKey": "second_wind_ready",
                "cooldownKey": "second_wind_cooldown",
                "remainingKey": "second_wind_cooldown_remaining"
            }, {
                "title": qsTr("Shield Bash"),
                "key": "F",
                "description": qsTr("Guard-gated stagger burst that punishes enemies crowding your front."),
                "readyKey": "shield_bash_ready",
                "cooldownKey": "shield_bash_cooldown",
                "remainingKey": "shield_bash_cooldown_remaining"
            }];
    }

    function stance_label() {
        if (!status_value("has_commander", false))
            return qsTr("Disconnected");
        if (status_value("guard_broken", false))
            return qsTr("Guard broken");
        if (status_value("punish_active", false))
            return qsTr("Punish window");
        if (status_value("is_running", false))
            return qsTr("Sprinting");
        return qsTr("Steady");
    }

    Component.onCompleted: refresh_status()
    anchors.fill: parent
    anchors.margins: 10
    spacing: 12

    Timer {
        interval: 100
        repeat: true
        running: bottomRoot.visible
        onTriggered: bottomRoot.refresh_status()
    }

    Connections {
        function onControl_mode_changed() {
            bottomRoot.refresh_status();
        }

        target: (typeof game !== 'undefined') ? game : null
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(260, bottomRoot.width * 0.3)
        Layout.fillHeight: true
        color: hs.parchmentDark
        border.color: hs.bronze
        border.width: 2
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: qsTr("COMMANDER")
                        color: hs.bronze
                        font.pointSize: 10
                        font.bold: true
                    }

                    Text {
                        text: bottomRoot.status_value("name", qsTr("Commander"))
                        color: Theme.textMain
                        font.pointSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: bottomRoot.status_value("nation", "") !== "" ? bottomRoot.status_value("nation", "") : qsTr("Frontline command")
                        color: Theme.textSubLite
                        font.pointSize: 9
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    radius: 12
                    color: bottomRoot.status_value("aura_active", false) ? hs.wax : hs.parchmentLight
                    border.color: hs.bronzeDeep
                    border.width: 1
                    implicitWidth: auraText.implicitWidth + 18
                    implicitHeight: auraText.implicitHeight + 8

                    Text {
                        id: auraText

                        anchors.centerIn: parent
                        text: bottomRoot.status_value("aura_active", false) ? qsTr("Aura online") : qsTr("Aura offline")
                        color: Theme.textMain
                        font.pointSize: 8
                        font.bold: true
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Health %1 / %2").arg(bottomRoot.status_value("health", 0)).arg(bottomRoot.status_value("max_health", 0))
                    color: Theme.textMain
                    font.pointSize: 9
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    color: Theme.bgShade
                    radius: 8
                    border.color: hs.bronzeDeep
                    border.width: 1

                    Rectangle {
                        width: parent.width * Number(bottomRoot.status_value("health_ratio", 0))
                        height: parent.height
                        radius: 8
                        color: bottomRoot.ratio_color(Number(bottomRoot.status_value("health_ratio", 0)))
                    }
                }

                Text {
                    text: bottomRoot.status_value("can_run", false) ? qsTr("Stamina") : qsTr("Stamina stable")
                    color: Theme.textMain
                    font.pointSize: 9
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 12
                    color: Theme.bgShade
                    radius: 6
                    border.color: hs.bronzeDeep
                    border.width: 1
                    opacity: bottomRoot.status_value("can_run", false) ? 1 : 0.45

                    Rectangle {
                        width: parent.width * Number(bottomRoot.status_value("stamina_ratio", 1))
                        height: parent.height
                        radius: 6
                        color: bottomRoot.status_value("is_running", false) ? hs.bronze : Theme.textSubLite
                    }
                }

                Text {
                    text: qsTr("Posture")
                    color: Theme.textMain
                    font.pointSize: 9
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    color: Theme.bgShade
                    radius: 5
                    border.color: hs.bronzeDeep
                    border.width: 1

                    Rectangle {
                        width: parent.width * Number(bottomRoot.status_value("posture_ratio", 0))
                        height: parent.height
                        radius: 5
                        color: bottomRoot.status_value("guard_broken", false) ? hs.waxHover : (bottomRoot.status_value("perfect_guard_active", false) ? Theme.accent : hs.bronze)
                    }
                }
            }

            Text {
                text: qsTr("State: %1").arg(bottomRoot.stance_label())
                color: Theme.textSubLite
                font.pointSize: 9
            }

            Text {
                text: bottomRoot.status_value("perfect_guard_active", false) ? qsTr("Perfect guard live") : (bottomRoot.status_value("finisher_ready", false) ? qsTr("Finisher primed") : qsTr("Camera: %1").arg(bottomRoot.status_value("camera_mode", qsTr("Chase"))))
                color: Theme.textSubLite
                font.pointSize: 9
                wrapMode: Text.WordWrap
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(300, bottomRoot.width * 0.36)
        Layout.fillHeight: true
        color: hs.parchmentDark
        border.color: hs.bronze
        border.width: 2
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text {
                text: qsTr("ABILITIES")
                color: hs.bronze
                font.pointSize: 10
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: hs.parchmentLight
                radius: 6
                border.color: bottomRoot.status_value("rally_ready", false) ? hs.bronze : hs.bronzeDeep
                border.width: 2

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Rally")
                            color: Theme.textMain
                            font.pointSize: 12
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            radius: 10
                            color: bottomRoot.status_value("rally_ready", false) ? hs.wax : hs.parchmentDark
                            implicitWidth: rallyStateText.implicitWidth + 16
                            implicitHeight: rallyStateText.implicitHeight + 8

                            Text {
                                id: rallyStateText

                                anchors.centerIn: parent
                                text: bottomRoot.rally_label()
                                color: Theme.textMain
                                font.pointSize: 8
                                font.bold: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 10
                        color: Theme.bgShade
                        radius: 5
                        border.color: hs.bronzeDeep
                        border.width: 1

                        Rectangle {
                            width: parent.width * bottomRoot.rally_progress()
                            height: parent.height
                            radius: 5
                            color: bottomRoot.status_value("rally_ready", false) ? Theme.accent : hs.bronze
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Restore nearby wavering troops and keep the line steady.")
                        color: Theme.textSubLite
                        wrapMode: Text.WordWrap
                        font.pointSize: 9
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        text: bottomRoot.status_value("rally_ready", false) ? qsTr("Trigger Rally [R]") : qsTr("Rally Cooling Down")
                        enabled: bottomRoot.status_value("has_commander", false) && bottomRoot.status_value("rally_ready", false)
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            if (typeof game !== 'undefined' && game.commander_trigger_rally)
                                game.commander_trigger_rally();
                        }

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? hs.waxDark : (parent.hovered ? hs.waxHover : hs.wax)) : hs.parchmentDark
                            radius: 6
                            border.color: parent.enabled ? hs.bronze : hs.bronzeDeep
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? Theme.textMain : Theme.textDim
                            font.pointSize: 10
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    RowLayout {
                        spacing: 6
                        visible: bottomRoot.status_value("has_commander", false)

                        Text {
                            text: qsTr("Combo")
                            color: hs.bronze
                            font.pointSize: 9
                        }

                        Repeater {
                            model: 4
                            Item {
                                width: 18
                                height: 18

                                property bool lit: index < bottomRoot.status_value("combo_step", 0)
                                property bool is_finisher: index === 3

                                Rectangle {
                                    id: combo_dot
                                    anchors.centerIn: parent
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: parent.lit ? (parent.is_finisher ? "#ff8800" : Theme.accent) : hs.parchmentDark
                                    border.color: parent.lit ? (parent.is_finisher ? "#ffcc44" : Theme.accent) : hs.bronze
                                    border.width: parent.lit ? 2 : 1

                                    Behavior on color  {
                                        ColorAnimation {
                                            duration: 120
                                        }
                                    }

                                    property real pop_scale: 1.0
                                    transform: Scale {
                                        origin.x: combo_dot.width / 2
                                        origin.y: combo_dot.height / 2
                                        xScale: combo_dot.pop_scale
                                        yScale: combo_dot.pop_scale
                                    }

                                    onColorChanged: {
                                        if (parent.lit) {
                                            pop_scale = 1.55;
                                            pop_anim.restart();
                                        }
                                    }

                                    NumberAnimation {
                                        id: pop_anim
                                        target: combo_dot
                                        property: "pop_scale"
                                        from: 1.55
                                        to: 1.0
                                        duration: 220
                                        easing.type: Easing.OutBack
                                    }
                                }
                            }
                        }
                    }

                    Repeater {
                        model: bottomRoot.combat_abilities()

                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            visible: bottomRoot.status_value("has_commander", false)
                            spacing: 4

                            Text {
                                text: qsTr("%1 [%2]").arg(modelData["title"]).arg(modelData["key"])
                                color: bottomRoot.status_value(modelData["readyKey"], true) ? hs.bronze : hs.bronzeDeep
                                font.pointSize: 9
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 8
                                radius: 4
                                color: hs.parchmentDark
                                border.color: hs.bronze
                                border.width: 1

                                Rectangle {
                                    width: parent.width * bottomRoot.cooldown_progress(modelData["cooldownKey"], modelData["remainingKey"])
                                    height: parent.height
                                    radius: 4
                                    color: bottomRoot.status_value(modelData["readyKey"], true) ? Theme.accent : hs.bronze
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData["description"]
                                color: Theme.textSubLite
                                wrapMode: Text.WordWrap
                                font.pointSize: 8
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: Math.max(240, bottomRoot.width * 0.24)
        Layout.fillHeight: true
        color: hs.parchmentDark
        border.color: hs.bronze
        border.width: 2
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Text {
                text: qsTr("FPV CONTROLS")
                color: hs.bronze
                font.pointSize: 10
                font.bold: true
            }

            Text {
                text: qsTr("[LMB] Strike")
                color: Theme.textMain
                font.pointSize: 10
            }

            Text {
                text: qsTr("[RMB] Guard")
                color: Theme.textMain
                font.pointSize: 10
            }

            Text {
                text: qsTr("[WASD] Move  [Shift] Sprint")
                color: Theme.textMain
                font.pointSize: 10
            }

            Text {
                text: qsTr("[1] Rush  [2] Second Wind  [F] Bash")
                color: Theme.textMain
                font.pointSize: 10
                wrapMode: Text.WordWrap
            }

            Text {
                text: qsTr("[R] Rally  [C] Camera  [Enter] Return to RTS")
                color: Theme.textMain
                font.pointSize: 10
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillHeight: true
            }

            Text {
                text: bottomRoot.status_value("alive", false) ? qsTr("First-person command engaged") : qsTr("Commander unavailable")
                color: Theme.textSubLite
                font.pointSize: 9
                wrapMode: Text.WordWrap
            }
        }
    }
}

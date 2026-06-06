import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property string target_name: ""
    property int target_hp: 0
    property int target_max_hp: 0
    property real target_hp_ratio: 0.0
    property bool target_staggered: false
    property bool target_guard_broken: false

    property real displayed_ratio: target_hp_ratio

    property int _prev_hp: 0
    property int _prev_max_hp: 0
    property real _flash_opacity: 0.0

    implicitWidth: 400
    implicitHeight: 64

    opacity: target_max_hp > 0 ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity  {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutQuad
        }
    }

    Behavior on displayed_ratio  {
        NumberAnimation {
            duration: 280
            easing.type: Easing.OutCubic
        }
    }

    onTarget_hp_ratioChanged: {
        displayed_ratio = target_hp_ratio;
    }

    onTarget_hpChanged: {
        if (target_max_hp === _prev_max_hp && target_max_hp > 0 && target_hp < _prev_hp && _prev_hp > 0) {
            _flash_opacity = 1.0;
            flash_decay.restart();
        }
        _prev_hp = target_hp;
    }

    onTarget_max_hpChanged: {
        if (target_max_hp !== _prev_max_hp) {
            _flash_opacity = 0.0;
            _prev_max_hp = target_max_hp;
            _prev_hp = target_hp;
            flash_decay.stop();
        }
    }

    Timer {
        id: flash_decay
        interval: 16
        repeat: true
        onTriggered: {
            root._flash_opacity = Math.max(0.0, root._flash_opacity - 0.06);
            if (root._flash_opacity <= 0.0) {
                flash_decay.stop();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#e01a1008"
        radius: 10
        border.color: root.target_staggered ? "#ffff8800" : (root.target_guard_broken ? "#ffff2200" : Theme.border)
        border.width: root.target_staggered || root.target_guard_broken ? 3 : 2

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: "#ffee3300"
            border.width: 4
            opacity: root._flash_opacity
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: root.target_name
                    color: Theme.textMain
                    font.pointSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: staggerLabel.implicitWidth + 12
                    height: 18
                    radius: 4
                    color: root.target_staggered ? "#88ff8800" : "#88ff2200"
                    visible: root.target_staggered || root.target_guard_broken
                    border.width: 1
                    border.color: root.target_staggered ? "#ccffaa00" : "#ccff4400"

                    Text {
                        id: staggerLabel
                        anchors.centerIn: parent
                        text: root.target_guard_broken ? "BROKEN" : "STAGGERED"
                        color: "#ffffff"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                }

                Text {
                    text: root.target_hp + " / " + root.target_max_hp
                    color: "#ccffffff"
                    font.pointSize: 9
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#66000000"
                }
            }

            Item {
                Layout.fillWidth: true
                height: 16

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "#55000000"
                    border.color: "#66cc4400"
                    border.width: 1
                }

                Rectangle {
                    width: parent.width * root.target_hp_ratio
                    height: parent.height
                    radius: 8
                    color: "#99cc2200"
                    Behavior on width  {
                        NumberAnimation {
                            duration: 500
                            easing.type: Easing.OutQuad
                        }
                    }
                }

                Rectangle {
                    id: hp_fill
                    width: parent.width * root.displayed_ratio
                    height: parent.height
                    radius: 8
                    color: root.displayed_ratio > 0.55 ? "#cc1a7a22" : (root.displayed_ratio > 0.28 ? "#ccbb6600" : "#cccc1a00")
                    Behavior on color  {
                        ColorAnimation {
                            duration: 350
                        }
                    }
                }

                Repeater {
                    model: Math.max(0, Math.floor(root.target_max_hp / 50) - 1)
                    Rectangle {
                        x: parent.width * ((index + 1) * 50 / root.target_max_hp) - 1
                        width: 1
                        height: parent.height
                        color: "#33ffffff"
                    }
                }
            }
        }
    }
}

import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property string target_name: ""
    property int target_hp: 0
    property int target_max_hp: 0
    property real target_hp_ratio: 0.0

    property real displayed_ratio: target_hp_ratio

    property int _prev_hp: 0
    property int _prev_max_hp: 0
    property real _flash_opacity: 0.0

    implicitWidth: 360
    implicitHeight: 54

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
            root._flash_opacity = Math.max(0.0, root._flash_opacity - 0.07);
            if (root._flash_opacity <= 0.0) {
                flash_decay.stop();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#d91a1008"
        radius: 8
        border.color: Theme.border
        border.width: 2

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: "#ffcc2200"
            border.width: 3
            opacity: root._flash_opacity
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: root.target_name
                color: Theme.textMain
                font.pointSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Item {
                Layout.fillWidth: true
                height: 14

                Rectangle {
                    anchors.fill: parent
                    radius: 7
                    color: "#44000000"
                    border.color: "#55cc4400"
                    border.width: 1
                }

                Rectangle {
                    width: parent.width * root.target_hp_ratio
                    height: parent.height
                    radius: 7
                    color: "#88cc2200"
                }

                Rectangle {
                    id: hp_fill
                    width: parent.width * root.displayed_ratio
                    height: parent.height
                    radius: 7
                    color: root.displayed_ratio > 0.55 ? "#cc1a7a22" : (root.displayed_ratio > 0.28 ? "#ccbb6600" : "#cccc1a00")
                    Behavior on color  {
                        ColorAnimation {
                            duration: 350
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: root.target_hp + " / " + root.target_max_hp
                    color: "white"
                    font.pointSize: 8
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#88000000"
                }
            }
        }
    }
}

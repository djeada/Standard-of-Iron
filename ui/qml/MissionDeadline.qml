import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: deadlineRoot

    readonly property var mission: (typeof game !== 'undefined' && game && game.mission) ? game.mission : null
    readonly property real remaining: deadlineRoot.mission ? deadlineRoot.mission.seconds_until_deadline : -1
    readonly property bool has_deadline: deadlineRoot.remaining >= 0
    readonly property bool urgent: deadlineRoot.has_deadline && deadlineRoot.remaining <= 60

    function format_countdown(seconds) {
        var total = Math.max(0, Math.ceil(seconds));
        var minutes = Math.floor(total / 60);
        var rest = total % 60;
        return minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    visible: has_deadline
    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight

    Design.IronPanel {
        id: panel

        raised: true
        implicitWidth: Math.max(200, content.implicitWidth + Design.Metrics.space24)
        implicitHeight: content.implicitHeight + Design.Metrics.space16
        border.color: deadlineRoot.urgent ? Design.Theme.danger : Design.Theme.borderStrong

        SequentialAnimation on opacity  {
            running: deadlineRoot.visible && deadlineRoot.urgent && !Design.A11y.reducedMotion
            loops: Animation.Infinite

            NumberAnimation {
                to: 0.68
                duration: 520
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                to: 1
                duration: 520
                easing.type: Easing.InOutQuad
            }

            onStopped: panel.opacity = 1
        }

        ColumnLayout {
            id: content

            anchors.centerIn: parent
            spacing: Design.Metrics.space4

            RowLayout {
                spacing: Design.Metrics.space8

                Text {
                    text: Design.Icons.deadline
                    color: deadlineRoot.urgent ? Design.Theme.danger : Design.Theme.accent
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                }

                Text {
                    text: qsTr("Time left")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    font.weight: Design.Typography.medium
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: deadlineRoot.format_countdown(deadlineRoot.remaining)
                    color: deadlineRoot.urgent ? Design.Theme.danger : Design.Theme.accent
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    font.weight: Design.Typography.medium
                }
            }

            Text {
                Layout.fillWidth: true
                text: deadlineRoot.urgent ? qsTr("The hour is on you") : qsTr("Mission clock")
                color: deadlineRoot.urgent ? Design.Theme.danger : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }
        }
    }
}

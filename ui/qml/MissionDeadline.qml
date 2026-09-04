import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: deadlineRoot

    readonly property var mission: (typeof game !== 'undefined' && game && game.mission) ? game.mission : null
    readonly property real remaining: deadlineRoot.mission ? deadlineRoot.mission.seconds_until_deadline : -1
    readonly property bool has_deadline: deadlineRoot.remaining >= 0
    readonly property bool urgent: deadlineRoot.has_deadline && deadlineRoot.remaining <= 60
    readonly property bool caution: deadlineRoot.has_deadline && deadlineRoot.remaining <= 180
    readonly property color tone: deadlineRoot.urgent ? Design.Theme.danger : (deadlineRoot.caution ? Design.Theme.warning : Design.Theme.accent)
    readonly property string phase_label: deadlineRoot.urgent ? qsTr("FINAL MINUTE") : (deadlineRoot.caution ? qsTr("CLOSING WINDOW") : qsTr("MISSION CLOCK"))

    function format_countdown(seconds) {
        var total = Math.max(0, Math.ceil(seconds));
        var minutes = Math.floor(total / 60);
        var rest = total % 60;
        return minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    visible: has_deadline
    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight
    Accessible.role: deadlineRoot.urgent ? Accessible.AlertMessage : Accessible.StaticText
    Accessible.name: qsTr("Time left: %1").arg(deadlineRoot.format_countdown(deadlineRoot.remaining))
    Accessible.description: deadlineRoot.phase_label

    Design.IronPanel {
        id: panel

        raised: true
        contentPadding: 0
        implicitWidth: Math.max(Design.A11y.scaled(236), content.implicitWidth + Design.Metrics.space24)
        implicitHeight: Math.max(Design.Metrics.controlHeight + Design.Metrics.space16, content.implicitHeight + Design.Metrics.space16)
        border.color: deadlineRoot.tone
        border.width: deadlineRoot.urgent ? Design.Metrics.borderFocus : Design.Metrics.borderThin

        Rectangle {
            anchors.fill: parent
            radius: panel.radius
            color: Qt.rgba(deadlineRoot.tone.r, deadlineRoot.tone.g, deadlineRoot.tone.b, Design.Theme.highContrast ? 0.15 : 0.07)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Design.Metrics.space4
            radius: width / 2
            color: deadlineRoot.tone

            SequentialAnimation on opacity  {
                running: deadlineRoot.visible && deadlineRoot.urgent && Design.Motion.allowAmbientLoops
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1
                    to: 0.32
                    duration: Design.Motion.cinematic
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    from: 0.32
                    to: 1
                    duration: Design.Motion.cinematic
                    easing.type: Easing.InOutQuad
                }
            }
        }

        RowLayout {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Design.Metrics.space12
            anchors.rightMargin: Design.Metrics.space12
            spacing: Design.Metrics.space8

            Rectangle {
                Layout.preferredWidth: Design.Metrics.commandButtonSize - Design.Metrics.space8
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignVCenter
                radius: width / 2
                color: Qt.rgba(deadlineRoot.tone.r, deadlineRoot.tone.g, deadlineRoot.tone.b, 0.12)
                border.color: deadlineRoot.tone
                border.width: Design.Metrics.borderThin

                Text {
                    anchors.centerIn: parent
                    text: Design.Icons.deadline
                    color: deadlineRoot.tone
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.bodyLarge
                    font.weight: Design.Typography.bold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("TIME LEFT")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }

                Text {
                    Layout.fillWidth: true
                    text: deadlineRoot.phase_label
                    color: deadlineRoot.tone
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                    font.letterSpacing: Design.Typography.trackingNormal
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: deadlineRoot.format_countdown(deadlineRoot.remaining)
                color: deadlineRoot.tone
                font.family: Design.Typography.titleFamily
                font.pixelSize: Design.Typography.heading
                font.weight: Design.Typography.bold
                font.hintingPreference: Design.Typography.titleHinting
                font.letterSpacing: Design.Typography.trackingTitle
            }
        }

        Behavior on border.color  {
            ColorAnimation {
                duration: Design.Motion.normal
            }
        }

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.normal
            }
        }

        SequentialAnimation on opacity  {
            running: deadlineRoot.visible && deadlineRoot.urgent && !Design.A11y.reducedMotion
            loops: 1

            NumberAnimation {
                from: 0.82
                to: 1
                duration: Design.Motion.deliberate
                easing.type: Design.Motion.emphasizedEasing
            }
        }
    }
}

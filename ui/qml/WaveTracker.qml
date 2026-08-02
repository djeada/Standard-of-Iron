import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: waveRoot

    readonly property var waves: (typeof game !== 'undefined' && game && game.waves) ? game.waves : null
    readonly property bool has_waves: waves !== null && waves.active && waves.total_phases > 0
    readonly property string wave_state: waves ? waves.state : "idle"
    readonly property real countdown: waves ? waves.seconds_until_next : -1
    readonly property bool counting_down: countdown >= 0 && wave_state !== "complete"

    function format_countdown(seconds) {
        if (seconds < 0)
            return "";
        var total = Math.max(0, Math.ceil(seconds));
        var minutes = Math.floor(total / 60);
        var rest = total % 60;
        return minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    function state_tone() {
        switch (waveRoot.wave_state) {
        case "complete":
            return Design.Theme.success;
        case "incoming":
            return Design.Theme.danger;
        case "engaged":
            return Design.Theme.warning;
        default:
            return Design.Theme.accent;
        }
    }

    function state_label() {
        switch (waveRoot.wave_state) {
        case "complete":
            return qsTr("All waves broken");
        case "incoming":
            return qsTr("Incoming");
        case "engaged":
            return qsTr("Engaged");
        case "lull":
            return qsTr("Next wave");
        default:
            return "";
        }
    }

    visible: has_waves
    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight

    Design.IronPanel {
        id: panel

        raised: true
        implicitWidth: Math.max(200, content.implicitWidth + Design.Metrics.space24)
        implicitHeight: content.implicitHeight + Design.Metrics.space16
        border.color: waveRoot.wave_state === "incoming" ? Design.Theme.danger : Design.Theme.borderStrong

        SequentialAnimation on opacity  {
            running: waveRoot.visible && waveRoot.wave_state === "incoming" && !Design.A11y.reducedMotion
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
                    text: Design.Icons.objective
                    color: waveRoot.state_tone()
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                }

                Text {
                    text: waveRoot.waves ? qsTr("Wave %1 / %2").arg(Math.max(1, waveRoot.waves.current_phase)).arg(waveRoot.waves.total_phases) : ""
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    font.weight: Design.Typography.medium
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    visible: waveRoot.counting_down
                    text: waveRoot.format_countdown(waveRoot.countdown)
                    color: waveRoot.state_tone()
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    font.weight: Design.Typography.medium
                }
            }

            RowLayout {
                spacing: Design.Metrics.space8

                Text {
                    text: waveRoot.state_label()
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    visible: waveRoot.wave_state === "engaged" && waveRoot.waves && waveRoot.waves.live_enemies > 0
                    text: waveRoot.waves ? qsTr("%1 left").arg(waveRoot.waves.live_enemies) : ""
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }
            }

            Design.IronProgressBar {
                Layout.fillWidth: true
                Layout.preferredWidth: 180
                from: 0
                to: waveRoot.waves ? Math.max(1, waveRoot.waves.total_phases) : 1
                value: waveRoot.waves ? waveRoot.waves.cleared_phases : 0
                fillColor: waveRoot.state_tone()
            }
        }
    }
}

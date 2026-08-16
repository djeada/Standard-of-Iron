import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Design.IronPanel {
    id: root

    property var economy: null

    readonly property var state: root.economy && root.economy.coach ? root.economy.coach : ({})
    readonly property string step: root.state.step ? root.state.step : ""
    readonly property int stepIndex: root.state.step_index !== undefined ? root.state.step_index : 0
    readonly property int stepCount: root.state.step_count !== undefined ? root.state.step_count : 0

    signal help_requested

    implicitWidth: Design.Metrics.space24 * 15
    implicitHeight: layout.implicitHeight + Design.Metrics.space16
    raised: true
    accessibleName: qsTr("Economy prompts")

    ColumnLayout {
        id: layout

        anchors.fill: parent
        spacing: Design.Metrics.space4

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.Metrics.space8

            Text {
                text: Design.Icons.objective
                color: Design.Theme.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
            }

            Text {
                Layout.fillWidth: true
                text: EconomyGuide.coach_title(root.step)
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.bold
                elide: Text.ElideRight
            }

            Design.IronIconButton {
                iconText: Design.Icons.close
                tooltip: qsTr("Stop showing these prompts")
                onClicked: {
                    if (root.economy && root.economy.dismiss_coach)
                        root.economy.dismiss_coach();
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: EconomyGuide.coach_body(root.step, root.state)
            color: Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.Metrics.space8

            Repeater {
                model: root.state.steps ? root.state.steps : []

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    width: Design.Metrics.space8
                    height: Design.Metrics.space8
                    radius: height / 2
                    color: modelData.done ? Design.Theme.success : (index === root.stepIndex ? Design.Theme.accent : Design.Theme.borderStrong)
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Design.IronButton {
                text: qsTr("How it works")
                onClicked: root.help_requested()
            }
        }
    }
}

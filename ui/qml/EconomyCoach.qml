import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

HintCard {
    id: root

    property var economy: null

    readonly property var state: root.economy && root.economy.coach ? root.economy.coach : ({})
    readonly property string step: root.state.step ? root.state.step : ""
    readonly property int stepIndex: root.state.step_index !== undefined ? root.state.step_index : 0
    readonly property int stepCount: root.state.step_count !== undefined ? root.state.step_count : 0

    signal help_requested

    hintId: "economy_coach"
    title: EconomyGuide.coach_title(root.step)
    iconText: Design.Icons.objective
    closeTooltip: qsTr("Hide these prompts")

    implicitWidth: Design.Metrics.space24 * 15

    readonly property bool coach_ready: !!root.economy && root.economy.coach_visible === true

    function sync() {
        if (!root.economy)
            return;
        root.economy.coach_enabled = Core.UiHints.enabled[root.hintId] === true;
        if (root.coach_ready)
            Core.UiHints.show(root.hintId);
    }

    onCoach_readyChanged: sync()

    Component.onCompleted: sync()

    Connections {
        function onChanged() {
            root.sync();
        }

        target: Core.UiHints
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

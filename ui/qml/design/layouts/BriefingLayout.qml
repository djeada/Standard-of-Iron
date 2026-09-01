import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Item {
    id: root

    property string title: ""
    property string summary: ""
    property string factionId: ""
    property var victoryConditions: []
    property var defeatConditions: []
    property var optionalObjectives: []
    property var stages: []
    property bool stagesAreVictoryConditions: false
    property string victoryMode: "any"

    readonly property bool hasObjectives: stages.length > 0 || victoryConditions.length > 0 || defeatConditions.length > 0 || optionalObjectives.length > 0
    readonly property bool requiresAllVictoryConditions: victoryMode === "all"
    readonly property string victoryHeading: root.headingForVictoryCount(victoryConditions.length)
    readonly property string stagesHeading: root.stagesAreVictoryConditions ? root.headingForVictoryCount(stages.length) : qsTr("Mission Steps — in order")

    function headingForVictoryCount(count) {
        if (count <= 1)
            return qsTr("Victory Conditions");
        return root.requiresAllVictoryConditions ? qsTr("Victory Conditions — complete all") : qsTr("Victory Conditions — complete any");
    }

    implicitWidth: Design.Metrics.space24 * 20
    implicitHeight: Design.Metrics.space24 * 16

    ScrollView {
        id: scroller

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Column {
            width: scroller.availableWidth
            spacing: Design.Metrics.space16

            Row {
                width: parent.width
                spacing: Design.Metrics.space12
                visible: root.title !== ""

                Text {
                    id: factionGlyph

                    width: Math.max(Design.Metrics.iconMedium, implicitWidth)
                    anchors.verticalCenter: parent.verticalCenter
                    text: Design.FactionTheme.glyphFor(root.factionId)
                    color: Design.FactionTheme.accentFor(root.factionId)
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.heading
                }

                Column {
                    width: parent.width - factionGlyph.width - Design.Metrics.space12
                    spacing: Design.Metrics.space4

                    Text {
                        width: parent.width
                        text: root.title
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.displayFamily
                        font.pixelSize: Design.Typography.heading
                        font.weight: Design.Typography.bold
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        visible: root.summary !== ""
                        text: root.summary
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.body
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Design.IronDivider {
                width: parent.width
                visible: root.title !== "" && root.hasObjectives
            }

            Column {
                width: scroller.availableWidth
                spacing: Design.Metrics.space4
                visible: root.stages.length > 0

                Text {
                    text: root.stagesHeading
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                }

                Repeater {
                    model: root.stages

                    delegate: Design.IronObjectiveRow {
                        id: stageRow

                        required property var modelData

                        readonly property int stageRequired: modelData.required === undefined ? 1 : modelData.required
                        readonly property int stageProgress: modelData.progress === undefined ? 0 : modelData.progress

                        width: scroller.availableWidth
                        objectiveText: modelData.title || modelData.description || qsTr("Next step")
                        objectiveState: modelData.complete ? "complete" : (modelData.active ? "current" : "pending")
                        detail: {
                            var parts = [];
                            if (modelData.description && modelData.description !== modelData.title)
                                parts.push(modelData.description);
                            if (stageRequired > 1)
                                parts.push(qsTr("%1 of %2").arg(stageProgress).arg(stageRequired));
                            return parts.join(" · ");
                        }
                        progress: stageRequired > 1 ? stageProgress / stageRequired : -1
                    }
                }
            }

            Design.IronDivider {
                width: parent.width
                visible: root.stages.length > 0 && ((!root.stagesAreVictoryConditions && root.victoryConditions.length > 0) || root.defeatConditions.length > 0)
            }

            Repeater {
                model: [{
                        "heading": root.victoryHeading,
                        "state": "active",
                        "entries": root.stagesAreVictoryConditions ? [] : root.victoryConditions,
                        "fallback": qsTr("Complete the objective")
                    }, {
                        "heading": qsTr("Defeat Conditions"),
                        "state": "failed",
                        "entries": root.defeatConditions,
                        "fallback": qsTr("Avoid this condition")
                    }, {
                        "heading": qsTr("Optional Objectives"),
                        "state": "optional",
                        "entries": root.optionalObjectives,
                        "fallback": qsTr("Optional objective")
                    }]

                delegate: Column {
                    id: section

                    required property var modelData

                    width: scroller.availableWidth
                    spacing: Design.Metrics.space4
                    visible: section.modelData.entries && section.modelData.entries.length > 0

                    Text {
                        text: section.modelData.heading
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                    }

                    Repeater {
                        model: section.modelData.entries

                        delegate: Design.IronObjectiveRow {
                            required property var modelData

                            width: section.width
                            objectiveText: modelData.description || section.modelData.fallback

                            objectiveState: modelData.state || section.modelData.state
                            detail: modelData.detail || ""
                            progress: modelData.progress === undefined ? -1 : modelData.progress
                        }
                    }
                }
            }

            Text {
                width: parent.width
                visible: !root.hasObjectives
                text: qsTr("No mission objectives available.\nThis may be a skirmish, or objectives have not been configured.")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}

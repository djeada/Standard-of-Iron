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

    readonly property bool hasObjectives: victoryConditions.length > 0 || defeatConditions.length > 0 || optionalObjectives.length > 0

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
                    width: Design.Metrics.iconMedium
                    anchors.verticalCenter: parent.verticalCenter
                    text: Design.FactionTheme.glyphFor(root.factionId)
                    color: Design.FactionTheme.accentFor(root.factionId)
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.heading
                }

                Column {
                    width: parent.width - Design.Metrics.iconMedium - Design.Metrics.space12
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

            Repeater {
                model: [{
                        "heading": qsTr("Victory Conditions"),
                        "state": "active",
                        "entries": root.victoryConditions,
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

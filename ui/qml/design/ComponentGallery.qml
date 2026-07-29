import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "." as Design

ScrollView {
    id: root

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: Design.Metrics.space16

        Item {
            Layout.fillWidth: true
            Layout.topMargin: Design.Metrics.space16
            Layout.leftMargin: Design.Metrics.space16
            Layout.rightMargin: Design.Metrics.space16
            implicitHeight: heading.implicitHeight

            Column {
                id: heading

                width: parent.width
                spacing: Design.Metrics.space4

                Text {
                    text: qsTr("Iron and Ember")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.title
                    font.weight: Design.Typography.bold
                }

                Text {
                    text: qsTr("Scale %1×  ·  %2  ·  colour vision: %3").arg(Design.A11y.uiScale.toFixed(2)).arg(Design.A11y.reducedMotion ? qsTr("reduced motion") : qsTr("full motion")).arg(Design.A11y.colorVisionMode)
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Design.Metrics.space16
            Layout.rightMargin: Design.Metrics.space16
            Layout.bottomMargin: Design.Metrics.space16
            spacing: Design.Metrics.space16

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: Design.Metrics.space16

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: buttons.implicitHeight + Design.Metrics.space24

                    Column {
                        id: buttons

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Buttons")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Flow {
                            width: parent.width
                            spacing: Design.Metrics.space8

                            Design.IronButton {
                                text: qsTr("Primary")
                                tone: "primary"
                            }
                            Design.IronButton {
                                text: qsTr("Secondary")
                            }
                            Design.IronButton {
                                text: qsTr("Destructive")
                                tone: "destructive"
                            }
                            Design.IronButton {
                                text: qsTr("Disabled")
                                enabled: false
                                disabledReason: qsTr("Unavailable in this state")
                            }
                            Design.IronIconButton {
                                iconText: Design.Icons.search
                                tooltip: qsTr("Search")
                            }
                        }

                        Flow {
                            width: parent.width
                            spacing: Design.Metrics.space8

                            Design.IronBadge {
                                text: qsTr("Selected")
                            }
                            Design.IronHotkeyLabel {
                                text: "Ctrl+S"
                            }
                        }
                    }
                }

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: orders.implicitHeight + Design.Metrics.space24

                    Column {
                        id: orders

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Orders")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Flow {
                            width: parent.width
                            spacing: Design.Metrics.space4

                            Design.IronCommandButton {
                                actionId: "attack"
                                label: qsTr("Attack")
                                hotkey: "A"
                            }
                            Design.IronCommandButton {
                                actionId: "guard"
                                label: qsTr("Guard")
                                hotkey: "G"
                                active: true
                            }
                            Design.IronCommandButton {
                                actionId: "patrol"
                                label: qsTr("Patrol")
                                hotkey: "P"
                                eligibleCount: 8
                                activeCount: 3
                            }
                            Design.IronCommandButton {
                                actionId: "rally"
                                label: qsTr("Rally")
                                hotkey: "R"
                                placing: true
                            }
                            Design.IronCommandButton {
                                actionId: "build"
                                label: qsTr("Build")
                                enabled: false
                                disabledReason: qsTr("Build is only available to builders")
                            }
                        }
                    }
                }

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: factions.implicitHeight + Design.Metrics.space24

                    Column {
                        id: factions

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Faction skins")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Repeater {
                            model: ["roman_republic", "carthage", "iron_sepulcher"]

                            delegate: Row {
                                id: factionRow
                                required property string modelData

                                width: factions.width
                                spacing: Design.Metrics.space8

                                Rectangle {
                                    width: Design.Metrics.space4
                                    height: crest.implicitHeight
                                    color: Design.FactionTheme.accentFor(factionRow.modelData)
                                }

                                Column {
                                    id: crest

                                    spacing: Design.Metrics.space2

                                    Text {
                                        text: Design.FactionTheme.glyphFor(factionRow.modelData) + "  " + Design.FactionTheme.nameFor(factionRow.modelData)
                                        color: Design.FactionTheme.accentFor(factionRow.modelData)
                                        font.family: Design.Typography.displayFamily
                                        font.pixelSize: Design.Typography.label
                                        font.weight: Design.Typography.medium
                                    }

                                    Text {
                                        text: Design.FactionTheme.describe(factionRow.modelData).motto
                                        color: Design.Theme.textSecondary
                                        font.family: Design.Typography.family
                                        font.pixelSize: Design.Typography.caption
                                    }
                                }
                            }
                        }
                    }
                }

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: selection.implicitHeight + Design.Metrics.space24

                    Column {
                        id: selection

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Selection")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Design.IronUnitCard {
                            width: parent.width
                            unitName: qsTr("Roman Spearmen ×24")
                            subtitle: qsTr("Defense formation")
                            health: 0.78
                        }

                        Design.IronSelectionSummary {
                            width: parent.width
                            unitCount: 64
                            groups: [{
                                    "typeKey": "spearman",
                                    "name": qsTr("Spearmen"),
                                    "nation": "roman_republic",
                                    "count": 40,
                                    "woundedCount": 7,
                                    "health": 0.82,
                                    "stamina": 0.65
                                }, {
                                    "typeKey": "archer",
                                    "name": qsTr("Archers"),
                                    "nation": "roman_republic",
                                    "count": 24,
                                    "woundedCount": 0,
                                    "health": 1,
                                    "stamina": 0.9
                                }]
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: Design.Metrics.space16

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: inputs.implicitHeight + Design.Metrics.space24

                    Column {
                        id: inputs

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Inputs")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Design.IronSearchField {
                            width: parent.width
                        }

                        Design.IronDropdown {
                            width: parent.width
                            model: [qsTr("Balanced"), qsTr("Aggressive"), qsTr("Defensive")]
                        }

                        Design.IronSlider {
                            width: parent.width
                            value: 0.62
                        }

                        Design.IronProgressBar {
                            width: parent.width
                            value: 0.72
                        }

                        Design.IronCheckBox {
                            text: qsTr("Reduced motion")
                            description: qsTr("Collapses transitions to an instant state change")
                            checked: Design.A11y.reducedMotion
                            enabled: false
                        }
                    }
                }

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: objectives.implicitHeight + Design.Metrics.space24

                    Column {
                        id: objectives

                        width: parent.width
                        spacing: Design.Metrics.space4

                        Text {
                            text: qsTr("Objectives")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Design.IronObjectiveRow {
                            width: parent.width
                            objectiveText: qsTr("Capture the eastern barracks")
                        }
                        Design.IronObjectiveRow {
                            width: parent.width
                            objectiveState: "complete"
                            objectiveText: qsTr("Hold the ford until dusk")
                        }
                        Design.IronObjectiveRow {
                            width: parent.width
                            objectiveState: "failed"
                            objectiveText: qsTr("Do not lose the commander")
                        }
                        Design.IronObjectiveRow {
                            width: parent.width
                            objectiveState: "optional"
                            objectiveText: qsTr("Take no losses")
                            progress: 0.45
                        }
                    }
                }

                Design.IronPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    implicitHeight: alerts.implicitHeight + Design.Metrics.space24

                    Column {
                        id: alerts

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            text: qsTr("Notification priorities")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Design.IronNotification {
                            width: parent.width
                            priority: "critical"
                            message: qsTr("Commander under attack")
                            detail: qsTr("Eastern approach, 3 hostiles")
                        }
                        Design.IronNotification {
                            width: parent.width
                            priority: "urgent"
                            message: qsTr("Barracks destroyed")
                        }
                        Design.IronNotification {
                            width: parent.width
                            priority: "info"
                            message: qsTr("Spearmen ready")
                            count: 3
                        }
                        Design.IronNotification {
                            width: parent.width
                            priority: "ambient"
                            message: qsTr("Supply route restored")
                        }
                    }
                }
            }
        }
    }
}

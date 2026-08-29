import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var economy: null

    readonly property var help: root.economy && root.economy.help ? root.economy.help : ({})
    readonly property var resourceEntries: root.economy && root.economy.resources ? root.economy.resources : []
    readonly property var buildings: root.help.buildings ? root.help.buildings : []
    readonly property var units: root.help.units ? root.help.units : []

    signal close_requested

    function count(key) {
        return root.help[key] !== undefined ? root.help[key] : 0;
    }

    function builder_line() {
        var builders = root.count("builder_count");
        if (builders <= 0)
            return qsTr("You have no builders. Recruit one at a barracks before you can gather or build.");
        return qsTr("Builders: %1 (%2 idle). Placing a structure sends every selected builder to the site.").arg(builders).arg(root.count("idle_builder_count"));
    }

    function manpower_line() {
        var cap = root.count("manpower_cap");
        var used = root.count("manpower");
        if (cap <= 0)
            return qsTr("Recruiting draws on the reserve held by each barracks.");
        return qsTr("Manpower %1 / %2. A Home raises civilians for %3 food each; walking a civilian into a barracks adds %4 reserve. Farms ripen every %5s and sheep yield %6 food.").arg(used).arg(cap).arg(root.count("civilian_food_cost")).arg(root.count("civilian_delivery_grant")).arg(root.count("farm_cycle_seconds")).arg(root.count("sheep_yield"));
    }

    function blocked_reason(entry, kind) {
        if (!entry)
            return "";
        if (entry.prerequisite_met === false) {
            if (entry.prerequisite === "home")
                return qsTr("Needs a home");
            return kind === "unit" ? qsTr("Needs a barracks") : qsTr("Needs a builder");
        }
        if (kind === "unit" && entry.reserve_met === false)
            return qsTr("Not enough reserve at the barracks");
        if (kind === "unit" && entry.manpower_met === false)
            return qsTr("Manpower limit reached");
        if (entry.affordable === false)
            return qsTr("Missing %1").arg(EconomyGuide.missing_summary(entry.missing));
        return "";
    }

    anchors.fill: parent
    z: 40
    focus: visible
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.close_requested();
            event.accepted = true;
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.close_requested()
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.scrim
    }

    Design.IronPanel {
        id: container

        width: Math.min(parent.width * 0.72, Design.Metrics.space24 * 36)
        height: Math.min(parent.height * 0.82, Design.Metrics.space24 * 28)
        anchors.centerIn: parent
        raised: true
        accessibleName: qsTr("Economy guide")

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Design.Metrics.space8

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Resources and Building")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.title
                    font.weight: Design.Typography.bold
                }

                Design.IronIconButton {
                    iconText: Design.Icons.close
                    tooltip: qsTr("Close the economy guide")
                    onClicked: root.close_requested()
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Gather with builders, spend what they bring on structures, recruit from a barracks, and keep both running.")
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                wrapMode: Text.WordWrap
            }

            Design.IronDivider {
                Layout.fillWidth: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: container.width - Design.Metrics.space24 * 2
                    spacing: Design.Metrics.space12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("WHAT EACH RESOURCE IS FOR")
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Repeater {
                        model: root.resourceEntries

                        delegate: RowLayout {
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: Design.Metrics.space8

                            ColumnLayout {
                                Layout.preferredWidth: Design.Metrics.space24 * 5
                                spacing: 0

                                Design.IronResourceCounter {
                                    iconSource: Design.Icons.resource(modelData.key)
                                    iconText: Design.Icons.resourceGlyph(modelData.key)
                                    label: EconomyGuide.resource_label(modelData.key)
                                    amount: modelData.amount || 0
                                    tooltipText: EconomyGuide.resource_tooltip(modelData)
                                }

                                Text {
                                    text: EconomyGuide.resource_label(modelData.key)
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    font.weight: Design.Typography.medium
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    text: EconomyGuide.resource_source(modelData.key)
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: modelData.used_by && modelData.used_by.length > 0
                                    text: qsTr("Spent on: %1").arg(EconomyGuide.item_labels(modelData.used_by, 6))
                                    color: Design.Theme.textPrimary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: EconomyGuide.gather_state_line(modelData) !== ""
                                    text: EconomyGuide.gather_state_line(modelData)
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                }
                            }
                        }
                    }

                    Design.IronDivider {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("WHAT A BUILDER CAN RAISE")
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.builder_line()
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: root.buildings

                        delegate: ColumnLayout {
                            required property var modelData

                            readonly property string reason: root.blocked_reason(modelData, "building")

                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("%1 — %2 · %3s").arg(EconomyGuide.item_label(modelData.item_type)).arg(EconomyGuide.cost_summary(modelData.resource_costs)).arg(Math.round(modelData.build_time || 0))
                                color: modelData.affordable ? Design.Theme.textPrimary : Design.Theme.textDisabled
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.label
                                font.weight: Design.Typography.medium
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                text: EconomyGuide.item_purpose(modelData.item_type)
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: parent.reason !== ""
                                text: parent.reason
                                color: Design.Theme.warning
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Design.IronDivider {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("WHAT A BARRACKS CAN RECRUIT")
                        color: Design.Theme.accent
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.manpower_line()
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: root.units

                        delegate: ColumnLayout {
                            required property var modelData

                            readonly property string reason: root.blocked_reason(modelData, "unit")

                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("%1 — %2 reserve, %3 · %4s").arg(modelData.display_name || EconomyGuide.item_label(modelData.unit_type)).arg(modelData.cost || 0).arg(EconomyGuide.cost_summary(modelData.resource_costs)).arg(Math.round(modelData.build_time || 0))
                                color: modelData.affordable ? Design.Theme.textPrimary : Design.Theme.textDisabled
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.label
                                font.weight: Design.Typography.medium
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: parent.reason !== ""
                                text: parent.reason
                                color: Design.Theme.warning
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Design.IronButton {
                    text: qsTr("Back to the battle")
                    tone: "primary"
                    onClicked: root.close_requested()
                }
            }
        }
    }
}

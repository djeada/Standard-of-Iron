import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

Design.IronPanel {
    id: root

    property var convoy: ({})
    property int engagedPhase: 0
    property int totalPhases: 0

    readonly property var wagons: convoy.wagons || []
    readonly property bool active: wagons.length > 0
    readonly property bool holding: convoy.holding === true
    readonly property bool hasReliefEta: convoy.relief_eta !== undefined

    objectName: "convoyTracker"
    accessibleName: qsTr("Supply convoy")
    implicitWidth: Design.A11y.scaled(256)
    implicitHeight: content.implicitHeight + Design.Metrics.space24
    raised: true
    visible: active

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Design.Metrics.space12
        spacing: Design.Metrics.space4

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.Metrics.space8

            Text {
                text: Design.Icons.deliver
                color: Design.Theme.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Convoy")
                elide: Text.ElideRight
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
                font.weight: Design.Typography.bold
            }

            Text {
                text: qsTr("%1 delivered").arg(root.convoy.delivered || 0)
                color: Design.Theme.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.holding ? qsTr("Ambush at %1").arg(root.convoy.destination || "") : qsTr("Onward to %1").arg(root.convoy.destination || "")
            elide: Text.ElideRight
            color: root.holding ? Design.Theme.danger : Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.medium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Design.Metrics.space4
            color: Design.Theme.backgroundDeep
            radius: height / 2

            Rectangle {
                anchors.left: parent.left
                height: parent.height
                width: parent.width * Math.max(0, Math.min(1, Number(root.convoy.fraction) || 0))
                radius: parent.radius
                color: root.holding ? Design.Theme.danger : Design.Theme.accent
            }
        }

        Repeater {
            model: root.wagons

            delegate: RowLayout {
                id: wagon

                required property var modelData
                readonly property bool safe: modelData.state === "safe"
                readonly property bool lost: modelData.state === "lost"
                readonly property bool hit: modelData.under_fire === true && !safe && !lost
                readonly property real health: Math.max(0, Math.min(1, Number(modelData.health) || 0))
                readonly property color conditionColor: safe ? Design.Theme.success : (hit || lost || health <= 0.33 ? Design.Theme.danger : Design.Theme.accent)

                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Text {
                    Layout.fillWidth: true
                    text: wagon.modelData.name
                    elide: Text.ElideRight
                    color: wagon.lost ? Design.Theme.textSecondary : Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }

                Rectangle {
                    Layout.preferredWidth: Design.A11y.scaled(52)
                    Layout.preferredHeight: Design.Metrics.space4
                    visible: !wagon.safe && !wagon.lost
                    color: Design.Theme.backgroundDeep
                    radius: height / 2

                    Rectangle {
                        anchors.left: parent.left
                        width: parent.width * wagon.health
                        height: parent.height
                        radius: parent.radius
                        color: wagon.conditionColor
                    }
                }

                Text {
                    text: wagon.safe ? qsTr("Safe") : (wagon.lost ? qsTr("Lost") : (wagon.hit ? qsTr("Hit") : qsTr("%1%").arg(Math.ceil(wagon.health * 100))))
                    color: wagon.conditionColor
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.hasReliefEta || (root.holding && root.totalPhases > 0)
            text: root.hasReliefEta ? qsTr("%1 in %2 s").arg(root.convoy.relief_name).arg(root.convoy.relief_eta) : qsTr("Ambush %1 of %2").arg(root.engagedPhase).arg(root.totalPhases)
            elide: Text.ElideRight
            color: root.hasReliefEta ? Design.Theme.success : Design.Theme.textSecondary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property var model: null

    property var groups: []
    property int unitCount: 0

    property int detailCap: 12

    readonly property bool empty: unitCount <= 0
    readonly property bool singleUnit: unitCount === 1
    readonly property bool squad: unitCount > 1 && unitCount <= detailCap
    readonly property bool army: unitCount > detailCap

    signal unitActivated(var unitId)

    accessibleName: qsTr("Selected units")
    Accessible.description: header.text

    function healthColor(ratio) {
        if (ratio > 0.6)
            return Design.Theme.success;
        if (ratio > 0.3)
            return Design.Theme.warning;
        return Design.Theme.danger;
    }

    function iconFor(typeKey, nation, name) {
        return Design.Icons.unit(typeKey !== "" ? typeKey : Design.Icons.typeKeyFromName(name), nation);
    }

    implicitWidth: Design.Metrics.space24 * 10
    implicitHeight: body.implicitHeight + Design.Metrics.space24

    Column {
        id: body

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.Metrics.space8

        Item {
            width: parent.width
            height: header.implicitHeight

            Text {
                id: header

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: root.empty ? qsTr("No units selected") : root.singleUnit ? qsTr("1 unit selected") : qsTr("%1 units selected").arg(root.unitCount)
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                font.weight: Design.Typography.medium
            }

            Design.IronBadge {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: root.groups.length > 1
                text: qsTr("%1 types").arg(root.groups.length)
            }
        }

        Loader {
            width: parent.width
            active: root.empty
            visible: active
            sourceComponent: emptyView
        }

        Loader {
            width: parent.width
            active: root.singleUnit && root.groups.length > 0
            visible: active
            sourceComponent: singleUnitView
        }

        Loader {
            width: parent.width
            active: root.squad
            visible: active
            sourceComponent: chipView
        }

        Loader {
            width: parent.width
            active: root.army
            visible: active
            sourceComponent: rosterView
        }
    }

    Component {
        id: emptyView

        Row {
            spacing: Design.Metrics.space8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Design.Icons.commander
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.heading
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Drag a box over your troops, or click a unit")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }
        }
    }

    Component {
        id: singleUnitView

        Row {
            spacing: Design.Metrics.space8

            Image {
                width: Design.Metrics.iconMedium
                height: width
                fillMode: Image.PreserveAspectFit
                source: root.iconFor(root.groups[0].typeKey, root.groups[0].nation, root.groups[0].name)
            }

            Column {
                spacing: Design.Metrics.space2

                Text {
                    text: root.groups[0].name
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.medium
                }

                Design.IronProgressBar {
                    width: Design.Metrics.space24 * 5
                    value: root.groups[0].health
                    fillColor: root.healthColor(value)
                }

                Text {

                    text: Math.round(root.groups[0].health * 100) + "%"
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }
            }
        }
    }

    Component {
        id: chipView

        Flow {
            spacing: Design.Metrics.space4

            Repeater {
                model: root.model

                delegate: Item {
                    id: chip

                    required property var model

                    width: Design.Metrics.iconMedium + Design.Metrics.space8
                    height: Design.Metrics.iconMedium + Design.Metrics.space8

                    Accessible.role: Accessible.Button
                    Accessible.name: chip.model.name
                    Accessible.description: Math.round(chip.model.health_ratio * 100) + "%"

                    Rectangle {
                        anchors.fill: parent
                        radius: Design.Metrics.radiusSmall
                        color: chipMouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundDeep
                        border.width: Design.Metrics.borderThin
                        border.color: root.healthColor(chip.model.health_ratio)
                    }

                    Image {
                        anchors.centerIn: parent
                        width: Design.Metrics.iconSmall
                        height: width
                        fillMode: Image.PreserveAspectFit
                        source: root.iconFor(chip.model.unit_type, chip.model.nation, chip.model.name)
                    }

                    Item {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Design.Metrics.borderThin
                        height: Design.Metrics.space2

                        Rectangle {
                            width: parent.width * chip.model.health_ratio
                            height: parent.height
                            color: root.healthColor(chip.model.health_ratio)
                        }
                    }

                    MouseArea {
                        id: chipMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.unitActivated(chip.model.unit_id)
                    }

                    ToolTip.visible: chipMouse.containsMouse
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: chip.model.name + " — " + Math.round(chip.model.health_ratio * 100) + "%"
                }
            }
        }
    }

    Component {
        id: rosterView

        Column {
            spacing: Design.Metrics.space2

            Repeater {
                model: root.groups

                delegate: Row {
                    id: rosterRow

                    required property var modelData

                    spacing: Design.Metrics.space8

                    Accessible.role: Accessible.StaticText
                    Accessible.name: rosterRow.modelData.name + " ×" + rosterRow.modelData.count

                    Image {
                        width: Design.Metrics.iconSmall
                        height: width
                        anchors.verticalCenter: parent.verticalCenter
                        fillMode: Image.PreserveAspectFit
                        source: root.iconFor(rosterRow.modelData.typeKey, rosterRow.modelData.nation, rosterRow.modelData.name)
                    }

                    Text {
                        width: Design.Metrics.space24 * 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: rosterRow.modelData.name
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×" + rosterRow.modelData.count
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                    }

                    Design.IronProgressBar {
                        width: Design.Metrics.space24 * 3
                        anchors.verticalCenter: parent.verticalCenter
                        value: rosterRow.modelData.health
                        fillColor: root.healthColor(value)
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: rosterRow.modelData.woundedCount > 0
                        text: qsTr("%1 hurt").arg(rosterRow.modelData.woundedCount)
                        color: Design.Theme.warning
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                    }
                }
            }
        }
    }
}

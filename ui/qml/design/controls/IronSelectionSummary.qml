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
    readonly property bool groupedSquad: squad && groups.length >= 4

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
        spacing: Design.Metrics.space12

        Item {
            width: parent.width
            height: Math.max(headerBlock.implicitHeight, selectionCountBadge.implicitHeight)

            Column {
                id: headerBlock

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    id: header

                    text: root.empty ? qsTr("SELECTION") : root.singleUnit ? qsTr("SELECTED UNIT") : qsTr("SELECTED FORCE")
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                    font.letterSpacing: Design.Typography.trackingWide
                }

                Text {
                    visible: !root.empty
                    text: root.singleUnit && root.groups.length > 0 ? root.groups[0].name : qsTr("%1 soldiers ready").arg(root.unitCount)
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.label
                    font.weight: Design.Typography.bold
                }
            }

            Design.IronBadge {
                id: selectionCountBadge

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.empty
                tone: Design.Theme.accent
                text: root.singleUnit ? qsTr("1 unit") : root.groups.length === 1 ? qsTr("%1 units").arg(root.unitCount) : qsTr("%1 units  ·  %2 types").arg(root.unitCount).arg(root.groups.length)
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
            active: root.squad && !root.groupedSquad
            visible: active
            sourceComponent: chipView
        }

        Loader {
            width: parent.width
            active: root.army || root.groupedSquad
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

        Rectangle {
            width: parent.width
            height: Design.Metrics.space24 * 3
            radius: Design.Metrics.radiusMedium
            color: Design.Theme.backgroundDeep
            border.width: Design.Metrics.borderThin
            border.color: Design.Theme.borderSubtle

            Rectangle {
                id: singlePortrait

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: Design.Metrics.space8
                width: height
                radius: Design.Metrics.radiusSmall
                color: Design.Theme.panelLeather
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.borderStrong

                Image {
                    anchors.fill: parent
                    anchors.margins: Design.Metrics.space4
                    fillMode: Image.PreserveAspectFit
                    source: root.iconFor(root.groups[0].typeKey, root.groups[0].nation, root.groups[0].name)
                    smooth: true
                    mipmap: true
                }
            }

            Column {
                anchors.left: singlePortrait.right
                anchors.leftMargin: Design.Metrics.space12
                anchors.right: healthPercent.left
                anchors.rightMargin: Design.Metrics.space12
                anchors.verticalCenter: parent.verticalCenter
                spacing: Design.Metrics.space2

                Text {
                    width: parent.width
                    text: qsTr("Ready for orders")
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    elide: Text.ElideRight
                }

                Design.IronProgressBar {
                    width: parent.width
                    value: root.groups[0].health
                    fillColor: root.healthColor(value)
                }
            }

            Text {
                id: healthPercent

                anchors.right: parent.right
                anchors.rightMargin: Design.Metrics.space12
                anchors.verticalCenter: parent.verticalCenter
                text: Math.round(root.groups[0].health * 100) + "%"
                color: root.healthColor(root.groups[0].health)
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.bold
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

                    width: Design.Metrics.space24 * 2
                    height: Design.Metrics.space24 * 2

                    Accessible.role: Accessible.Button
                    Accessible.name: chip.model.name
                    Accessible.description: Math.round(chip.model.health_ratio * 100) + "%"

                    Rectangle {
                        anchors.fill: parent
                        radius: Design.Metrics.radiusSmall
                        color: chipMouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundDeep
                        border.width: chipMouse.containsMouse ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                        border.color: chipMouse.containsMouse ? Design.Theme.selection : Design.Theme.borderSubtle
                    }

                    Image {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -Design.Metrics.space2
                        width: Design.Metrics.iconMedium
                        height: width
                        fillMode: Image.PreserveAspectFit
                        source: root.iconFor(chip.model.unit_type, chip.model.nation, chip.model.name)
                        smooth: true
                        mipmap: true
                    }

                    Item {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Design.Metrics.space4
                        height: Design.Metrics.space4

                        Rectangle {
                            width: parent.width * chip.model.health_ratio
                            height: parent.height
                            radius: height / 2
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

        Flow {
            id: groupFlow

            width: parent.width
            spacing: Design.Metrics.space8

            readonly property int columnCount: root.groups.length > 8 ? 4 : Math.min(4, Math.max(1, root.groups.length))
            readonly property real cardWidth: Math.floor((width - spacing * (columnCount - 1)) / columnCount)
            readonly property real cardHeight: root.groups.length > 8 ? Design.Metrics.space24 * 2 : Design.Metrics.space24 * 3

            Repeater {
                model: root.groups

                delegate: Rectangle {
                    id: groupCard

                    required property var modelData

                    width: groupFlow.cardWidth
                    height: groupFlow.cardHeight
                    radius: Design.Metrics.radiusMedium
                    color: Design.Theme.backgroundDeep
                    border.width: Design.Metrics.borderThin
                    border.color: groupCard.modelData.woundedCount > 0 ? root.healthColor(groupCard.modelData.health) : Design.Theme.borderSubtle

                    Accessible.role: Accessible.StaticText
                    Accessible.name: groupCard.modelData.name + " ×" + groupCard.modelData.count
                    Accessible.description: Math.round(groupCard.modelData.health * 100) + "%"

                    Rectangle {
                        id: portraitFrame

                        anchors.left: parent.left
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(Design.Metrics.space24 * 2, parent.height - Design.Metrics.space12)
                        height: width
                        radius: Design.Metrics.radiusSmall
                        color: Design.Theme.panelLeather
                        border.width: Design.Metrics.borderThin
                        border.color: Design.Theme.borderStrong

                        Image {
                            anchors.fill: parent
                            anchors.margins: Design.Metrics.space4
                            fillMode: Image.PreserveAspectFit
                            source: root.iconFor(groupCard.modelData.typeKey, groupCard.modelData.nation, groupCard.modelData.name)
                            smooth: true
                            mipmap: true
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: -Design.Metrics.space4
                            anchors.top: parent.top
                            anchors.topMargin: -Design.Metrics.space4
                            width: Math.max(Design.Metrics.space16, groupCount.implicitWidth + Design.Metrics.space8)
                            height: Design.Metrics.space16
                            radius: height / 2
                            color: Design.Theme.panelLeather
                            border.width: Design.Metrics.borderThin
                            border.color: Design.Theme.accent

                            Text {
                                id: groupCount

                                anchors.centerIn: parent
                                text: "×" + groupCard.modelData.count
                                color: Design.Theme.accent
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                            }
                        }
                    }

                    Text {
                        id: groupName

                        anchors.left: portraitFrame.right
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space8
                        anchors.top: parent.top
                        anchors.topMargin: Design.Metrics.space8
                        text: groupCard.modelData.name
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.medium
                        elide: Text.ElideRight
                    }

                    Design.IronProgressBar {
                        id: groupHealth

                        anchors.left: portraitFrame.right
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space8
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Design.Metrics.space8
                        value: groupCard.modelData.health
                        fillColor: root.healthColor(value)
                    }

                    Text {
                        anchors.left: portraitFrame.right
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.bottom: groupHealth.top
                        anchors.bottomMargin: Design.Metrics.space2
                        visible: groupCard.modelData.woundedCount > 0 && groupCard.height >= Design.Metrics.space24 * 3
                        text: qsTr("%1 wounded").arg(groupCard.modelData.woundedCount)
                        color: Design.Theme.warning
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                    }
                }
            }
        }
    }
}

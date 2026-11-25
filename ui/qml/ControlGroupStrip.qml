import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: strip

    objectName: "controlGroupStrip"

    property bool gate: true
    property var sizes: []
    readonly property var held: {
        var rows = [];
        for (var group = 1; group < strip.sizes.length; ++group) {
            if (strip.sizes[group] > 0)
                rows.push({
                        "group": group,
                        "count": strip.sizes[group]
                    });
        }
        return rows;
    }
    readonly property bool game_ready: typeof game !== 'undefined' && game !== null && game.orders !== undefined && game.orders.control_group_sizes !== undefined

    function refresh() {
        if (!strip.game_ready)
            return;
        var next = game.orders.control_group_sizes();
        if (JSON.stringify(next) !== JSON.stringify(strip.sizes))
            strip.sizes = next;
    }

    visible: gate && held.length > 0
    implicitWidth: chips.implicitWidth + Design.Metrics.space12
    implicitHeight: chips.implicitHeight + Design.Metrics.space8
    width: implicitWidth
    height: implicitHeight
    radius: Design.Metrics.radiusSmall
    color: Qt.rgba(Design.Theme.backgroundDeep.r, Design.Theme.backgroundDeep.g, Design.Theme.backgroundDeep.b, 0.82)
    border.width: Design.Metrics.borderThin
    border.color: Design.Theme.borderStrong
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Control groups")

    Timer {
        interval: 250
        repeat: true
        running: strip.gate
        triggeredOnStart: true
        onTriggered: strip.refresh()
    }

    Row {
        id: chips

        anchors.centerIn: parent
        spacing: Design.Metrics.space4

        Repeater {
            model: strip.held

            delegate: Rectangle {
                id: chip

                required property var modelData

                objectName: "controlGroupChip_" + modelData.group
                width: chipLabel.implicitWidth + Design.Metrics.space12
                height: chipLabel.implicitHeight + Design.Metrics.space4
                radius: Design.Metrics.radiusSmall
                color: chipArea.containsMouse ? Design.Theme.panelLeather : "transparent"
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.accent

                Text {
                    id: chipLabel

                    anchors.centerIn: parent
                    text: qsTr("%1  ·  %2").arg(chip.modelData.group).arg(chip.modelData.count)
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                }

                MouseArea {
                    id: chipArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: containsMouse
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: qsTr("Control group %1 holds %n unit(s). Click, or press %1, to select it.", "", chip.modelData.count).arg(chip.modelData.group)
                    onClicked: {
                        if (strip.game_ready && game.orders.recall_control_group)
                            game.orders.recall_control_group(chip.modelData.group, false);
                    }
                }
            }
        }
    }
}

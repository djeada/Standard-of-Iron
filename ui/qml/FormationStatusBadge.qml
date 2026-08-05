import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: statusBadge

    property var status: ({})

    readonly property int content_width: Design.A11y.scaled(220)

    readonly property bool has_formation: status.active === true
    readonly property real cohesion: status.cohesion !== undefined ? status.cohesion : 0
    readonly property string phase: status.phase !== undefined ? status.phase : ""
    readonly property int member_count: status.member_count !== undefined ? status.member_count : 0
    readonly property int blocked_slots: status.blocked_slots !== undefined ? status.blocked_slots : 0
    readonly property bool mixed_groups: status.mixed_groups === true

    readonly property color phase_tone: {
        switch (phase) {
        case "formed":
            return Design.Theme.success;
        case "disrupted":
            return Design.Theme.danger;
        case "breaking":
            return Design.Theme.danger;
        }
        return Design.Theme.warning;
    }

    readonly property string phase_label: {
        switch (phase) {
        case "formed":
            return qsTr("Formed");
        case "disrupted":
            return qsTr("Disrupted");
        case "breaking":
            return qsTr("Breaking");
        }
        return qsTr("Forming up");
    }

    readonly property string phase_hint: {
        switch (phase) {
        case "formed":
            return qsTr("The line is holding its shape and takes reduced damage.");
        case "disrupted":
            return qsTr("The line has come apart and is taking extra damage. Re-issue the formation order to reform.");
        case "breaking":
            return qsTr("The formation is breaking up.");
        }
        return qsTr("Units are still moving into their slots.");
    }

    function game_ready() {
        return typeof game !== 'undefined' && game.placement !== undefined;
    }

    function refresh() {
        if (game_ready())
            status = game.placement.selected_formation_status;
    }

    readonly property bool any_selected: typeof game !== 'undefined' && game.has_units_selected

    visible: has_formation && any_selected
    implicitWidth: card.implicitWidth
    implicitHeight: card.implicitHeight

    onAny_selectedChanged: refresh()

    Component.onCompleted: refresh()

    Connections {
        function onFormation_options_changed() {
            statusBadge.refresh();
        }

        ignoreUnknownSignals: true
        target: statusBadge.game_ready() ? game.placement : null
    }

    Timer {
        interval: 350
        repeat: true
        running: statusBadge.any_selected

        onTriggered: statusBadge.refresh()
    }

    Rectangle {
        id: card

        border.color: statusBadge.phase_tone
        border.width: Design.Metrics.borderThin
        color: Design.Theme.panelIron
        implicitHeight: layout.implicitHeight + Design.Metrics.space8 * 2
        implicitWidth: statusBadge.content_width + Design.Metrics.space8 * 2
        radius: Design.Metrics.radiusMedium

        ToolTip.delay: Design.Metrics.tooltipDelay
        ToolTip.text: statusBadge.phase_hint
        ToolTip.visible: hoverArea.containsMouse

        MouseArea {
            id: hoverArea

            acceptedButtons: Qt.NoButton
            anchors.fill: parent
            hoverEnabled: true
        }

        ColumnLayout {
            id: layout

            spacing: Design.Metrics.space4
            width: statusBadge.content_width
            x: Design.Metrics.space8
            y: Design.Metrics.space8

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Text {
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                    text: statusBadge.status.intent_display_name !== undefined ? statusBadge.status.intent_display_name : ""
                }

                Text {
                    Layout.fillWidth: true
                    color: Design.Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: statusBadge.status.doctrine_display_name !== undefined ? statusBadge.status.doctrine_display_name : ""
                }

                Design.IronBadge {
                    text: statusBadge.phase_label
                    tone: statusBadge.phase_tone
                }
            }

            Design.IronProgressBar {
                Layout.fillWidth: true
                fillColor: statusBadge.phase_tone
                from: 0
                to: 1
                value: statusBadge.cohesion
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space8

                Text {
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    text: qsTr("Cohesion %1%").arg(Math.round(statusBadge.cohesion * 100))
                }

                Text {
                    Layout.fillWidth: true
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("%1 units").arg(statusBadge.member_count)
                }
            }

            Text {
                Layout.fillWidth: true
                color: Design.Theme.warning
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                text: statusBadge.mixed_groups ? qsTr("Selection spans several formations.") : qsTr("%1 slot(s) blocked by terrain.").arg(statusBadge.blocked_slots)
                visible: statusBadge.mixed_groups || statusBadge.blocked_slots > 0
                wrapMode: Text.WordWrap
            }
        }
    }
}

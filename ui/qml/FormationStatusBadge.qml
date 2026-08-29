import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

HintCard {
    id: statusBadge

    property var status: ({})

    readonly property int content_width: Design.A11y.scaled(300)

    readonly property bool has_formation: status.active === true
    readonly property real cohesion: status.cohesion !== undefined ? status.cohesion : 0
    readonly property string phase: status.phase !== undefined ? status.phase : ""
    readonly property int member_count: status.member_count !== undefined ? status.member_count : 0
    readonly property int blocked_slots: status.blocked_slots !== undefined ? status.blocked_slots : 0
    readonly property bool mixed_groups: status.mixed_groups === true

    readonly property bool any_selected: typeof game !== 'undefined' && game.has_units_selected

    readonly property color phase_tone: {
        switch (phase) {
        case "formed":
        case "arrived":
            return Design.Theme.success;
        case "disrupted":
            return Design.Theme.danger;
        }
        return Design.Theme.warning;
    }

    readonly property string phase_label: {
        switch (phase) {
        case "formed":
            return qsTr("Formed");
        case "arrived":
            return qsTr("In position");
        case "disrupted":
            return qsTr("Disrupted");
        case "opening":
            return qsTr("Opening ranks");
        case "traversing":
            return qsTr("Filing through");
        }
        return qsTr("Forming up");
    }

    readonly property string phase_hint: {
        switch (phase) {
        case "formed":
            return qsTr("The line is holding its shape and takes reduced damage.");
        case "arrived":
            return qsTr("The line has reached its ground and is holding it.");
        case "disrupted":
            return qsTr("The line has come apart and is taking extra damage. Re-issue the formation order to reform.");
        case "opening":
            return qsTr("The ranks are opening to let the line through.");
        case "traversing":
            return qsTr("The line is filing through a narrow crossing.");
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

    hintId: "formation_readout"
    title: status.intent_display_name !== undefined ? status.intent_display_name : qsTr("Formation")
    closeTooltip: qsTr("Hide this readout")
    accent: phase_tone
    hoverTooltip: phase_hint
    gate: has_formation && any_selected

    implicitWidth: content_width + Design.Metrics.space12 * 2

    border.color: phase_tone

    Component.onCompleted: refresh()

    onAny_selectedChanged: refresh()

    Connections {
        function onFormation_options_changed() {
            statusBadge.refresh();
        }

        function onFormation_deployed(unit_count) {
            if (unit_count < 2)
                return;
            statusBadge.refresh();
            Core.UiHints.show(statusBadge.hintId);
        }

        ignoreUnknownSignals: true
        target: statusBadge.game_ready() ? game.placement : null
    }

    Timer {
        interval: 350
        repeat: true
        running: statusBadge.visible

        onTriggered: statusBadge.refresh()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Design.Metrics.space8

        Text {
            Layout.fillWidth: true
            color: Design.Theme.textSecondary
            elide: Text.ElideRight
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Math.round(Design.Typography.caption * 0.8)
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

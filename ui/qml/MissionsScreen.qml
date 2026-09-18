import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Item {
    id: root

    property var missions: []
    property int selected_index: -1

    readonly property var selected: (selected_index >= 0 && selected_index < missions.length) ? missions[selected_index] : null
    readonly property var selected_objective_lines: objective_lines(selected)
    readonly property bool compact: width < Design.A11y.scaled(1080)
    readonly property bool shallow: height < Design.A11y.scaled(720)
    readonly property int frame_margin: Math.max(Design.Metrics.space12, Math.min(Design.Metrics.space32, Math.round(Math.min(width, height) * 0.032)))
    readonly property color selected_tone: (selected && selected.completed) ? Design.Theme.success : Design.Theme.accent
    readonly property int completed_count: {
        var count = 0;
        for (var i = 0; i < missions.length; i++) {
            if (missions[i] && missions[i].completed)
                count++;
        }
        return count;
    }

    signal mission_chosen(string file_path)
    signal cancelled

    function refresh_missions() {
        if (typeof game === "undefined" || !game.setup)
            return;
        if (game.setup.load_missions)
            game.setup.load_missions();
        missions = game.setup.missions || [];
        if (missions.length === 0)
            selected_index = -1;
        else if (selected_index < 0 || selected_index >= missions.length)
            selected_index = 0;
    }

    function condition_lines(conditions) {
        if (!conditions)
            return [];
        var lines = [];
        for (var i = 0; i < conditions.length; i++) {
            if (conditions[i] && conditions[i].description)
                lines.push(String(conditions[i].description));
        }
        return lines;
    }

    function objective_lines(mission) {
        return condition_lines(mission ? mission.objectives : null);
    }

    function bonus_lines(mission) {
        return condition_lines(mission ? mission.optional_objectives : null);
    }

    function failure_lines(mission) {
        return condition_lines(mission ? mission.defeat_conditions : null);
    }

    function starting_force_text(mission) {
        if (!mission || !mission.starting_force || mission.starting_force.length === 0)
            return "";
        var parts = [];
        for (var i = 0; i < mission.starting_force.length; i++) {
            var unit = mission.starting_force[i];
            if (unit)
                parts.push(qsTr("%1 × %2").arg(Design.Numerals.roman(unit.count)).arg(Design.Icons.humanise(String(unit.type))));
        }
        return parts.join(", ");
    }

    function starting_supplies_text(mission) {
        if (!mission || !mission.starting_resources || mission.starting_resources.length === 0)
            return "";
        var parts = [];
        for (var i = 0; i < mission.starting_resources.length; i++) {
            var stock = mission.starting_resources[i];
            if (stock)
                parts.push(qsTr("%1 %2").arg(Design.Numerals.grouped(stock.amount)).arg(Design.Icons.humanise(String(stock.type))));
        }
        return parts.join(", ");
    }

    function orders_heading(mission) {
        if (!mission)
            return "";
        if (objective_lines(mission).length < 2)
            return qsTr("Orders");
        return String(mission.victory_mode).toLowerCase() === "any" ? qsTr("Orders — any one of these ends it") : qsTr("Orders — all of them, or none");
    }

    function field_size(mission) {
        if (!mission || !mission.map_width || !mission.map_height)
            return "";
        return mission.map_width + " × " + mission.map_height;
    }

    function mission_meta(mission) {
        if (!mission)
            return "";
        var parts = [];
        if (mission.map_name)
            parts.push(String(mission.map_name));
        var size = field_size(mission);
        if (size.length > 0)
            parts.push(size);
        return parts.join("  •  ");
    }

    function preview_configs(mission) {
        if (!mission || !mission.map_path)
            return [];
        return [{
                "player_id": 1,
                "playerName": qsTr("You"),
                "colorIndex": 0,
                "team_id": 1,
                "nationId": "roman_republic",
                "isHuman": true
            }];
    }

    function start_selected() {
        if (selected && selected.file_path)
            mission_chosen(String(selected.file_path));
    }

    anchors.fill: parent
    focus: true

    onVisibleChanged: {
        if (visible)
            refresh_missions();
    }

    onSelected_indexChanged: Qt.callLater(function () {
            if (root.selected_index >= 0)
                mission_list.positionViewAtIndex(root.selected_index, ListView.Contain);
            briefing_scroll.contentY = 0;
            objectives_scroll.contentY = 0;
        })

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            if (selected_index < missions.length - 1)
                selected_index++;
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (selected_index > 0)
                selected_index--;
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            start_selected();
            event.accepted = true;
        }
    }

    Connections {
        function onMissions_changed() {
            root.missions = game.setup.missions || [];
            if (root.missions.length === 0)
                root.selected_index = -1;
            else if (root.selected_index < 0 || root.selected_index >= root.missions.length)
                root.selected_index = 0;
        }

        target: (typeof game !== "undefined") ? game.setup : null
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.94
    }

    Rectangle {
        id: container

        width: Math.max(Design.A11y.scaled(420), Math.min(parent.width - root.frame_margin * 2, Design.A11y.scaled(1420)))
        height: Math.max(Design.A11y.scaled(460), Math.min(parent.height - root.frame_margin * 2, Design.A11y.scaled(940)))
        anchors.centerIn: parent
        radius: Design.Metrics.radiusLarge
        color: Design.Theme.panelLeather
        border.color: Design.Theme.borderStrong
        border.width: Design.Metrics.borderFocus
        clip: true
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Mission orders")

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.shallow ? Design.Metrics.space16 : Design.Metrics.space24
            spacing: root.shallow ? Design.Metrics.space12 : Design.Metrics.space16

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.Metrics.space4

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("FIELD ORDERS  /  SECOND PUNIC WAR")
                        color: Design.Theme.accent
                        font.family: Design.Typography.titleFamily
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Text {
                        text: qsTr("Missions")
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.displayFamily
                        font.pixelSize: root.shallow ? Design.Typography.title : Design.Typography.hero
                        font.weight: Design.Typography.bold
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !root.shallow
                        text: qsTr("One small field, one order to carry out. No campaign to lose, no second army to worry about.")
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.bodyLarge
                        wrapMode: Text.WordWrap
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: Design.A11y.scaled(190)
                    visible: !root.compact && root.missions.length > 0
                    spacing: Design.Metrics.space4

                    Text {
                        text: qsTr("ORDERS FULFILLED") + "  " + Design.Numerals.ratio(root.completed_count, root.missions.length)
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                    }

                    Design.IronProgressBar {
                        Layout.fillWidth: true
                        value: root.missions.length > 0 ? root.completed_count / root.missions.length : 0
                        fillColor: root.completed_count === root.missions.length ? Design.Theme.success : Design.Theme.accent
                        Accessible.name: qsTr("Mission completion")
                    }
                }

                Design.IronButton {
                    text: Design.Icons.mirrored ? qsTr("Back ›") : qsTr("‹ Back")
                    accessibleName: qsTr("Back")
                    onClicked: root.cancelled()
                }
            }

            Design.IronDivider {
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Design.Metrics.space16

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.max(Design.A11y.scaled(232), Math.min(Design.A11y.scaled(380), Math.round(container.width * (root.compact ? 0.34 : 0.3))))
                    Layout.maximumWidth: Design.A11y.scaled(400)
                    radius: Design.Metrics.radiusMedium
                    color: Design.Theme.panelIron
                    border.color: Design.Theme.borderSubtle
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Design.Metrics.space8
                        spacing: Design.Metrics.space8

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("AVAILABLE DISPATCHES")
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.titleFamily
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                                elide: Text.ElideRight
                            }

                            Design.IronBadge {
                                text: Design.Numerals.roman(root.missions.length)
                                tone: Design.Theme.accent
                            }
                        }

                        Design.IronDivider {
                            Layout.fillWidth: true
                        }

                        ListView {
                            id: mission_list

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: root.missions
                            spacing: Design.Metrics.space8
                            clip: true
                            currentIndex: root.selected_index
                            boundsBehavior: Flickable.StopAtBounds
                            visible: root.missions.length > 0

                            ScrollBar.vertical: Design.IronScrollBar {
                                objectName: "missionListScrollBar"
                            }

                            delegate: Rectangle {
                                id: mission_row

                                required property int index
                                required property var modelData
                                readonly property bool is_selected: root.selected_index === index
                                readonly property bool is_done: !!(modelData && modelData.completed)
                                readonly property color tone: is_done ? Design.Theme.success : Design.Theme.accent

                                width: mission_list.width - Design.Metrics.scrollBarThickness - Design.Metrics.space4
                                implicitHeight: Math.max(Design.A11y.scaled(76), mission_row_content.implicitHeight + Design.Metrics.space16)
                                radius: Design.Metrics.radiusSmall
                                color: is_selected ? Design.Theme.panelLeather : Design.Theme.backgroundDeep
                                border.color: is_selected ? tone : Design.Theme.borderSubtle
                                border.width: is_selected ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                                Accessible.role: Accessible.ListItem
                                Accessible.name: modelData ? String(modelData.title) : ""
                                Accessible.selected: is_selected

                                RowLayout {
                                    id: mission_row_content
                                    anchors.fill: parent
                                    anchors.margins: Design.Metrics.space8
                                    spacing: Design.Metrics.space8

                                    Text {
                                        text: Design.Numerals.ordinal(mission_row.index)
                                        color: mission_row.tone
                                        font.pixelSize: Design.Typography.label
                                        font.weight: Design.Typography.bold
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Design.Metrics.space2

                                        Text {
                                            Layout.fillWidth: true
                                            text: mission_row.modelData ? String(mission_row.modelData.title) : ""
                                            color: Design.Theme.textPrimary
                                            font.family: Design.Typography.displayFamily
                                            font.pixelSize: Design.Typography.bodyLarge
                                            font.weight: Design.Typography.bold
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.mission_meta(mission_row.modelData)
                                            color: Design.Theme.textSecondary
                                            font.pixelSize: Design.Typography.caption
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Design.IronBadge {
                                        visible: mission_row.is_done
                                        text: qsTr("DONE")
                                        tone: Design.Theme.success
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onContainsMouseChanged: {
                                        if (containsMouse)
                                            Design.UiSound.hover();
                                    }
                                    onClicked: {
                                        Design.UiSound.activate();
                                        root.selected_index = mission_row.index;
                                    }
                                    onDoubleClicked: {
                                        root.selected_index = mission_row.index;
                                        root.start_selected();
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.missions.length === 0
                            text: qsTr("No missions are installed.")
                            color: Design.Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    id: detail_panel

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Design.Metrics.radiusMedium
                    color: Design.Theme.panelLeather
                    border.color: root.selected ? root.selected_tone : Design.Theme.borderSubtle
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: root.shallow ? Design.Metrics.space12 : Design.Metrics.space16
                        spacing: Design.Metrics.space8
                        visible: root.selected !== null

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("MISSION BRIEF")
                                color: root.selected_tone
                                font.family: Design.Typography.titleFamily
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                            }

                            Design.IronBadge {
                                text: root.selected && root.selected.completed ? qsTr("ORDER FULFILLED") : qsTr("OPEN ORDER")
                                tone: root.selected_tone
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.selected ? String(root.selected.title) : ""
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.displayFamily
                            font.pixelSize: root.shallow ? Design.Typography.heading : Design.Typography.title
                            font.weight: Design.Typography.bold
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.mission_meta(root.selected)
                            color: Design.Theme.textSecondary
                            font.pixelSize: Design.Typography.label
                            elide: Text.ElideRight
                        }

                        Design.IronDivider {
                            Layout.fillWidth: true
                        }

                        // Keep the win conditions outside the supplementary briefing's
                        // scroll area. They are the first thing a player needs to read
                        // when choosing a mission, even on a short display.
                        Rectangle {
                            id: selected_objectives

                            objectName: "selectedMissionObjectives"
                            readonly property var lines: root.selected_objective_lines

                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(objectives_content.implicitHeight + Design.Metrics.space16, root.shallow ? Design.A11y.scaled(136) : Design.A11y.scaled(220))
                            Layout.minimumHeight: Design.A11y.scaled(68)
                            color: Design.Theme.panelIron
                            border.color: Design.Theme.accent
                            border.width: Design.Metrics.borderThin
                            radius: Design.Metrics.radiusSmall
                            clip: true
                            Accessible.role: Accessible.Pane
                            Accessible.name: qsTr("Orders")

                            Flickable {
                                id: objectives_scroll

                                anchors.fill: parent
                                anchors.margins: Design.Metrics.space8
                                contentWidth: width
                                contentHeight: objectives_content.implicitHeight
                                clip: true
                                flickableDirection: Flickable.VerticalFlick
                                boundsBehavior: Flickable.StopAtBounds

                                ScrollBar.vertical: Design.IronScrollBar {
                                    objectName: "selectedMissionObjectivesScrollBar"
                                }

                                MissionOrderList {
                                    id: objectives_content

                                    objectName: "selectedMissionObjectivesList"
                                    width: objectives_scroll.width - Design.Metrics.scrollBarThickness - Design.Metrics.space4
                                    heading: root.orders_heading(root.selected)
                                    heading_color: Design.Theme.accent
                                    marker: Design.Icons.objective
                                    lines: root.selected_objective_lines
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: root.compact ? Design.Metrics.space8 : Design.Metrics.space16

                            Flickable {
                                id: briefing_scroll

                                readonly property int gutter: Design.Metrics.scrollBarThickness + Design.Metrics.space4

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: width
                                contentHeight: briefing_column.implicitHeight
                                flickableDirection: Flickable.VerticalFlick
                                boundsBehavior: Flickable.StopAtBounds

                                ScrollBar.vertical: Design.IronScrollBar {
                                    objectName: "missionBriefingScrollBar"
                                }

                                ColumnLayout {
                                    id: briefing_column

                                    width: Math.max(0, briefing_scroll.width - briefing_scroll.gutter)
                                    spacing: Design.Metrics.space12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Design.Metrics.space4

                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("COMMANDER'S INTENT")
                                            color: Design.Theme.accent
                                            font.pixelSize: Design.Typography.caption
                                            font.weight: Design.Typography.bold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.selected ? String(root.selected.summary) : ""
                                            color: Design.Theme.textPrimary
                                            font.family: Design.Typography.displayFamily
                                            font.pixelSize: Design.Typography.body
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    MissionOrderList {
                                        Layout.fillWidth: true
                                        heading: qsTr("Worth doing as well")
                                        heading_color: Design.Theme.warning
                                        marker: Design.Icons.objective
                                        lines: root.bonus_lines(root.selected)
                                    }

                                    MissionOrderList {
                                        Layout.fillWidth: true
                                        heading: qsTr("You bring")
                                        heading_color: Design.Theme.textSecondary
                                        marker: Design.Icons.formation
                                        lines: root.starting_force_text(root.selected).length > 0 ? [root.starting_force_text(root.selected)] : []
                                    }

                                    MissionOrderList {
                                        Layout.fillWidth: true
                                        heading: qsTr("You start with")
                                        heading_color: Design.Theme.textSecondary
                                        marker: Design.Icons.collect
                                        lines: root.starting_supplies_text(root.selected).length > 0 ? [root.starting_supplies_text(root.selected)] : [qsTr("Nothing in the stores. Everything you spend, you gather first.")]
                                    }

                                    MissionOrderList {
                                        Layout.fillWidth: true
                                        heading: qsTr("It ends badly if")
                                        heading_color: Design.Theme.danger
                                        marker: Design.Icons.warning
                                        lines: root.failure_lines(root.selected)
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: Math.min(Design.A11y.scaled(320), Math.round(detail_panel.width * 0.34))
                                Layout.alignment: Qt.AlignTop
                                spacing: Design.Metrics.space8
                                visible: !root.compact

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("FIELD RECONNAISSANCE")
                                    color: Design.Theme.textDisabled
                                    font.pixelSize: Design.Typography.caption
                                    font.weight: Design.Typography.bold
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                MapPreview {
                                    objectName: "missionFieldPreviewFrame"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(Design.Metrics.space24 * 4, Math.min(width, Math.round(detail_panel.height * 0.33)))
                                    color: Design.Theme.backgroundDeep
                                    border.color: root.selected_tone
                                    border.width: Design.Metrics.borderThin
                                    legend_text: ""
                                    map_path: root.selected ? String(root.selected.map_path) : ""
                                    player_configs: root.preview_configs(root.selected)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.selected && root.selected.map_name ? String(root.selected.map_name) : ""
                                    color: Design.Theme.textSecondary
                                    font.pixelSize: Design.Typography.caption
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Design.IronDivider {
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.Metrics.space12

                            Text {
                                visible: !root.compact
                                text: qsTr("Enter") + "  " + qsTr("Take the field")
                                color: Design.Theme.textDisabled
                                font.pixelSize: Design.Typography.caption
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: root.selected && root.selected.completed ? qsTr("Previously carried out") : ""
                                color: Design.Theme.success
                                font.pixelSize: Design.Typography.caption
                            }

                            Design.IronButton {
                                text: root.selected && root.selected.completed ? qsTr("Take it again") : qsTr("Take the field")
                                tone: "primary"
                                accessibleName: text
                                onClicked: root.start_selected()
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        width: Math.max(0, parent.width - Design.Metrics.space24 * 2)
                        visible: root.selected === null
                        text: qsTr("Choose a dispatch to review its field orders.")
                        color: Design.Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}

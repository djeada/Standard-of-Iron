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
        if (missions.length > 0 && (selected_index < 0 || selected_index >= missions.length))
            selected_index = 0;
    }

    function condition_lines(conditions) {
        if (!conditions)
            return [];
        var lines = [];
        for (var i = 0; i < conditions.length; i++) {
            var line = conditions[i];
            if (line && line.description)
                lines.push(String(line.description));
        }
        return lines;
    }

    function objective_lines(mission) {
        return root.condition_lines(mission ? mission.objectives : null);
    }

    function bonus_lines(mission) {
        return root.condition_lines(mission ? mission.optional_objectives : null);
    }

    function failure_lines(mission) {
        return root.condition_lines(mission ? mission.defeat_conditions : null);
    }

    function starting_force_text(mission) {
        if (!mission || !mission.starting_force || mission.starting_force.length === 0)
            return "";
        var parts = [];
        for (var i = 0; i < mission.starting_force.length; i++) {
            var unit = mission.starting_force[i];
            if (!unit)
                continue;
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
            if (!stock)
                continue;
            parts.push(qsTr("%1 %2").arg(Design.Numerals.grouped(stock.amount)).arg(Design.Icons.humanise(String(stock.type))));
        }
        return parts.join(", ");
    }

    function orders_heading(mission) {
        if (!mission)
            return "";
        if (root.objective_lines(mission).length < 2)
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
        var size = root.field_size(mission);
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
        if (!root.selected || !root.selected.file_path)
            return;
        root.mission_chosen(String(root.selected.file_path));
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
        })

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            if (root.selected_index < root.missions.length - 1)
                root.selected_index++;
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (root.selected_index > 0)
                root.selected_index--;
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.start_selected();
            event.accepted = true;
        }
    }

    Connections {
        function onMissions_changed() {
            root.missions = game.setup.missions || [];
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
        opacity: 0.9
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: Qt.rgba(Design.Theme.backgroundDeep.r, Design.Theme.backgroundDeep.g, Design.Theme.backgroundDeep.b, 0.96)
            }

            GradientStop {
                position: 0.48
                color: Qt.rgba(Design.Theme.panelLeather.r, Design.Theme.panelLeather.g, Design.Theme.panelLeather.b, 0.78)
            }

            GradientStop {
                position: 1
                color: Qt.rgba(Design.Theme.backgroundDeep.r, Design.Theme.backgroundDeep.g, Design.Theme.backgroundDeep.b, 0.96)
            }
        }
    }

    Rectangle {
        width: container.width
        height: container.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenterOffset: Design.Metrics.space8
        anchors.verticalCenterOffset: Design.Metrics.space8
        radius: container.radius
        color: Design.Theme.shadow
        opacity: 0.65
    }

    Rectangle {
        id: container

        width: Math.max(Design.A11y.scaled(420), Math.min(parent.width - root.frame_margin * 2, Design.A11y.scaled(1420)))
        height: Math.max(Design.A11y.scaled(460), Math.min(parent.height - root.frame_margin * 2, Design.A11y.scaled(940)))
        anchors.centerIn: parent
        radius: Design.Metrics.radiusLarge
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.lighter(Design.Theme.panelLeather, 1.08)
            }

            GradientStop {
                position: 1
                color: Design.Theme.backgroundDeep
            }
        }
        border.color: Design.Theme.borderStrong
        border.width: Design.Metrics.borderFocus
        clip: true
        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Mission orders")

        Rectangle {
            anchors.fill: parent
            anchors.margins: Design.Metrics.space4
            color: "transparent"
            border.color: Design.Theme.borderSubtle
            border.width: Design.Metrics.borderThin
            radius: Math.max(Design.Metrics.radiusSmall, container.radius - Design.Metrics.space4)
            opacity: 0.6
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Design.Metrics.borderFocus
            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop {
                    position: 0
                    color: "transparent"
                }

                GradientStop {
                    position: 0.22
                    color: Design.Theme.accent
                }

                GradientStop {
                    position: 0.78
                    color: Design.Theme.accent
                }

                GradientStop {
                    position: 1
                    color: "transparent"
                }
            }
        }

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
                        font.capitalization: Font.AllUppercase
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.hintingPreference: Design.Typography.titleHinting
                        font.letterSpacing: Design.Typography.trackingWide
                        elide: Text.ElideRight
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
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Design.Metrics.space4
                    visible: !root.compact && root.missions.length > 0

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("ORDERS FULFILLED")
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.medium
                            font.letterSpacing: Design.Typography.trackingWide
                        }

                        Text {
                            text: Design.Numerals.ratio(root.completed_count, root.missions.length)
                            color: root.completed_count === root.missions.length ? Design.Theme.success : Design.Theme.accent
                            font.family: Design.Typography.displayFamily
                            font.pixelSize: Design.Typography.label
                            font.weight: Design.Typography.bold
                        }
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Design.Metrics.borderThin
                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        position: 0
                        color: Design.Theme.accent
                    }

                    GradientStop {
                        position: 0.34
                        color: Design.Theme.borderStrong
                    }

                    GradientStop {
                        position: 1
                        color: "transparent"
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.compact ? Design.Metrics.space12 : Design.Metrics.space16

                Rectangle {
                    id: dispatch_panel

                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.max(Design.A11y.scaled(232), Math.min(Design.A11y.scaled(380), Math.round(container.width * (root.compact ? 0.34 : 0.3))))
                    Layout.maximumWidth: Design.A11y.scaled(400)
                    radius: Design.Metrics.radiusMedium
                    color: Design.Theme.panelIron
                    border.color: Design.Theme.borderSubtle
                    border.width: Design.Metrics.borderThin
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Design.Metrics.space8
                        spacing: Design.Metrics.space8

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Design.Metrics.space4
                            Layout.rightMargin: Design.Metrics.space4
                            spacing: Design.Metrics.space8

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("AVAILABLE DISPATCHES")
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.titleFamily
                                font.capitalization: Font.AllUppercase
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                                font.hintingPreference: Design.Typography.titleHinting
                                font.letterSpacing: Design.Typography.trackingWide
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

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Column {
                                anchors.centerIn: parent
                                width: Math.max(0, parent.width - Design.Metrics.space24 * 2)
                                spacing: Design.Metrics.space8
                                visible: root.missions.length === 0

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: Design.Icons.briefing
                                    color: Design.Theme.textDisabled
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.glyph
                                }

                                Text {
                                    width: parent.width
                                    text: qsTr("No missions are installed.")
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.label
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }
                            }

                            ListView {
                                id: mission_list

                                anchors.fill: parent
                                anchors.rightMargin: Design.Metrics.scrollBarThickness + Design.Metrics.space4
                                model: root.missions
                                spacing: Design.Metrics.space8
                                clip: true
                                currentIndex: root.selected_index
                                boundsBehavior: Flickable.StopAtBounds
                                visible: root.missions.length > 0
                                keyNavigationWraps: false

                                ScrollBar.vertical: Design.IronScrollBar {
                                    objectName: "missionListScrollBar"
                                }

                                delegate: Rectangle {
                                    id: row

                                    required property int index
                                    required property var modelData

                                    readonly property bool is_selected: root.selected_index === row.index
                                    readonly property bool is_done: !!(row.modelData && row.modelData.completed)
                                    readonly property color tone: row.is_done ? Design.Theme.success : Design.Theme.accent

                                    width: mission_list.width
                                    implicitHeight: Math.max(Design.A11y.scaled(76), row_layout.implicitHeight + Design.Metrics.space16)
                                    radius: Design.Metrics.radiusSmall
                                    color: row.is_selected ? Qt.rgba(row.tone.r, row.tone.g, row.tone.b, Design.Theme.highContrast ? 0.24 : 0.15) : (row_mouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundDeep)
                                    border.color: row.is_selected ? row.tone : (row_mouse.containsMouse ? Design.Theme.borderStrong : Design.Theme.borderSubtle)
                                    border.width: row.is_selected ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                                    Accessible.role: Accessible.ListItem
                                    Accessible.name: row.modelData ? String(row.modelData.title) : ""
                                    Accessible.description: row.is_done ? qsTr("Carried out") : root.mission_meta(row.modelData)
                                    Accessible.selected: row.is_selected

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: row.is_selected ? Design.Metrics.space4 : Design.Metrics.space2
                                        radius: width / 2
                                        color: row.tone
                                        opacity: row.is_selected ? 1 : 0.55
                                    }

                                    RowLayout {
                                        id: row_layout

                                        anchors.fill: parent
                                        anchors.leftMargin: Design.Metrics.space12
                                        anchors.rightMargin: Design.Metrics.space8
                                        anchors.topMargin: Design.Metrics.space8
                                        anchors.bottomMargin: Design.Metrics.space8
                                        spacing: Design.Metrics.space8

                                        Rectangle {
                                            Layout.preferredWidth: Design.Metrics.commandButtonSize - Design.Metrics.space8
                                            Layout.preferredHeight: Layout.preferredWidth
                                            Layout.alignment: Qt.AlignVCenter
                                            radius: width / 2
                                            color: Qt.rgba(row.tone.r, row.tone.g, row.tone.b, 0.12)
                                            border.color: row.tone
                                            border.width: Design.Metrics.borderThin

                                            Text {
                                                anchors.centerIn: parent
                                                text: Design.Numerals.ordinal(row.index)
                                                color: row.tone
                                                font.family: Design.Typography.displayFamily
                                                font.pixelSize: Design.Typography.label
                                                font.weight: Design.Typography.bold
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Design.Metrics.space2

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Design.Metrics.space4

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: row.modelData ? String(row.modelData.title) : ""
                                                    color: Design.Theme.textPrimary
                                                    font.family: Design.Typography.displayFamily
                                                    font.pixelSize: Design.Typography.bodyLarge
                                                    font.weight: Design.Typography.bold
                                                    elide: Text.ElideRight
                                                }

                                                Design.IronBadge {
                                                    visible: row.is_done
                                                    text: qsTr("DONE")
                                                    tone: Design.Theme.success
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: root.mission_meta(row.modelData)
                                                color: Design.Theme.textSecondary
                                                font.family: Design.Typography.family
                                                font.pixelSize: Design.Typography.caption
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignVCenter
                                            text: Design.Icons.chevronForward
                                            color: row.is_selected ? row.tone : Design.Theme.textDisabled
                                            font.family: Design.Typography.family
                                            font.pixelSize: Design.Typography.subheading
                                        }
                                    }

                                    MouseArea {
                                        id: row_mouse

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onContainsMouseChanged: {
                                            if (containsMouse)
                                                Design.UiSound.hover();
                                        }
                                        onClicked: {
                                            Design.UiSound.activate();
                                            root.selected_index = row.index;
                                        }
                                        onDoubleClicked: {
                                            root.selected_index = row.index;
                                            root.start_selected();
                                        }
                                    }

                                    Behavior on color  {
                                        ColorAnimation {
                                            duration: Design.Motion.fast
                                        }
                                    }

                                    Behavior on border.color  {
                                        ColorAnimation {
                                            duration: Design.Motion.fast
                                        }
                                    }
                                }
                            }
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
                    border.width: Design.Metrics.borderThin
                    clip: true

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: Design.Metrics.space4
                        color: root.selected_tone
                        opacity: root.selected ? 0.85 : 0

                        Behavior on color  {
                            ColorAnimation {
                                duration: Design.Motion.normal
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: root.shallow ? Design.Metrics.space12 : Design.Metrics.space16
                        anchors.topMargin: (root.shallow ? Design.Metrics.space12 : Design.Metrics.space16) + Design.Metrics.space4
                        spacing: Design.Metrics.space8
                        visible: root.selected !== null

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.Metrics.space8

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("MISSION BRIEF")
                                color: root.selected_tone
                                font.family: Design.Typography.titleFamily
                                font.capitalization: Font.AllUppercase
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                                font.hintingPreference: Design.Typography.titleHinting
                                font.letterSpacing: Design.Typography.trackingWide
                            }

                            Design.IronBadge {
                                text: (root.selected && root.selected.completed) ? qsTr("ORDER FULFILLED") : qsTr("OPEN ORDER")
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.Metrics.space8

                            Text {
                                text: Design.Icons.map
                                color: Design.Theme.accent
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.body
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.mission_meta(root.selected)
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.label
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Design.Metrics.borderThin
                            color: Design.Theme.borderSubtle
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

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: summary_column.implicitHeight + Design.Metrics.space16
                                        radius: Design.Metrics.radiusSmall
                                        color: Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, Design.Theme.highContrast ? 0.14 : 0.07)
                                        border.color: Qt.rgba(Design.Theme.accent.r, Design.Theme.accent.g, Design.Theme.accent.b, Design.Theme.highContrast ? 0.6 : 0.3)
                                        border.width: Design.Metrics.borderThin

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            width: Design.Metrics.space4
                                            color: Design.Theme.accent
                                            radius: width / 2
                                        }

                                        ColumnLayout {
                                            id: summary_column

                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.leftMargin: Design.Metrics.space16
                                            anchors.rightMargin: Design.Metrics.space12
                                            spacing: Design.Metrics.space4

                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("COMMANDER'S INTENT")
                                                color: Design.Theme.accent
                                                font.family: Design.Typography.family
                                                font.pixelSize: Design.Typography.caption
                                                font.weight: Design.Typography.bold
                                                font.letterSpacing: Design.Typography.trackingWide
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
                                    }

                                    MissionOrderList {
                                        Layout.fillWidth: true
                                        heading: root.orders_heading(root.selected)
                                        heading_color: Design.Theme.accent
                                        marker: Design.Icons.objective
                                        lines: root.objective_lines(root.selected)
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
                                Layout.alignment: Qt.AlignTop
                                Layout.preferredWidth: Math.min(Design.A11y.scaled(320), Math.round(detail_panel.width * 0.34))
                                spacing: Design.Metrics.space8
                                visible: !root.compact

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("FIELD RECONNAISSANCE")
                                    color: Design.Theme.textDisabled
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    font.weight: Design.Typography.bold
                                    font.letterSpacing: Design.Typography.trackingWide
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: width
                                    radius: Design.Metrics.radiusMedium
                                    color: Design.Theme.backgroundDeep
                                    border.color: root.selected_tone
                                    border.width: Design.Metrics.borderThin

                                    MapPreview {
                                        id: field_preview

                                        anchors.fill: parent
                                        anchors.margins: Design.Metrics.space4
                                        color: Design.Theme.backgroundDeep
                                        border.color: Design.Theme.borderSubtle
                                        border.width: Design.Metrics.borderThin
                                        legend_text: ""
                                        map_path: root.selected ? String(root.selected.map_path) : ""
                                        player_configs: root.preview_configs(root.selected)
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.leftMargin: Design.Metrics.space8
                                        anchors.topMargin: Design.Metrics.space8
                                        width: Design.Metrics.space24
                                        height: Design.Metrics.borderFocus
                                        color: root.selected_tone
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.leftMargin: Design.Metrics.space8
                                        anchors.topMargin: Design.Metrics.space8
                                        width: Design.Metrics.borderFocus
                                        height: Design.Metrics.space24
                                        color: root.selected_tone
                                    }

                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.rightMargin: Design.Metrics.space8
                                        anchors.bottomMargin: Design.Metrics.space8
                                        width: Design.Metrics.space24
                                        height: Design.Metrics.borderFocus
                                        color: root.selected_tone
                                    }

                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.rightMargin: Design.Metrics.space8
                                        anchors.bottomMargin: Design.Metrics.space8
                                        width: Design.Metrics.borderFocus
                                        height: Design.Metrics.space24
                                        color: root.selected_tone
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.selected && root.selected.map_name ? String(root.selected.map_name) : ""
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.displayFamily
                                    font.pixelSize: Design.Typography.caption
                                    font.italic: true
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Design.Metrics.borderThin
                            color: Design.Theme.borderSubtle
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.Metrics.space12

                            RowLayout {
                                spacing: Design.Metrics.space8
                                visible: !root.compact

                                Design.IronHotkeyLabel {
                                    text: qsTr("Enter")
                                }

                                Text {
                                    text: qsTr("Take the field")
                                    color: Design.Theme.textDisabled
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: (root.selected && root.selected.completed) ? qsTr("Previously carried out") : ""
                                color: Design.Theme.success
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.medium
                            }

                            Design.IronButton {
                                text: (root.selected && root.selected.completed) ? qsTr("Take it again") : qsTr("Take the field")
                                tone: "primary"
                                accessibleName: text
                                onClicked: root.start_selected()
                            }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        width: Math.max(0, parent.width - Design.Metrics.space24 * 2)
                        spacing: Design.Metrics.space8
                        visible: root.selected === null

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Design.Icons.map
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.glyph
                        }

                        Text {
                            width: parent.width
                            text: qsTr("Choose a dispatch to review its field orders.")
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.label
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}

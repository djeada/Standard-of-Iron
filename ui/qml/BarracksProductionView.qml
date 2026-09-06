import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var panel: null
    property var prod: ({})

    readonly property int queueSlots: 5
    readonly property int queueTotal: (root.prod.in_progress ? 1 : 0) + (root.prod.queue_size || 0)
    readonly property int cardHeight: Math.max(Design.A11y.scaled(48), Design.Typography.caption * 3 + Design.Metrics.space8)
    readonly property int cardGap: Design.Metrics.space4
    readonly property int listGutter: Design.Metrics.scrollBarThickness + Design.Metrics.space4
    readonly property int minCardWidth: Design.A11y.scaled(172)
    readonly property int maxColumns: 3
    readonly property var cards: root.filtered_cards()
    readonly property int costColumns: root.max_cost_entries()

    signal recruit_requested(string unit_type)
    signal details_requested(string unit_type, string nation)
    signal rally_requested

    function filtered_cards() {
        if (!root.panel)
            return [];
        var out = [];
        for (var i = 0; i < root.panel.recruit_unit_cards.length; ++i) {
            var entry = root.panel.recruit_unit_cards[i];
            if (entry.carthage_only === true && root.prod.nation_id !== "carthage")
                continue;
            out.push(entry);
        }
        return out;
    }

    function max_cost_entries() {
        if (!root.panel)
            return 1;
        var widest = 1;
        var list = root.cards;
        for (var i = 0; i < list.length; ++i) {
            var info = root.panel.get_unit_production_info(list[i].unit_type, root.prod.nation_id);
            var count = root.panel.cost_entries(root.panel.reserve_cost(info), (info && info.resource_costs) || {}, true).length;
            if (count > widest)
                widest = count;
        }
        return widest;
    }

    function queue_type(slot) {
        if (slot < 0 || slot >= root.queueTotal)
            return "";
        if (slot === 0 && root.prod.in_progress)
            return root.prod.product_type || "archer";
        var queueIndex = root.prod.in_progress ? slot - 1 : slot;
        if (root.prod.production_queue && root.prod.production_queue[queueIndex])
            return root.prod.production_queue[queueIndex];
        return "archer";
    }

    function progress_value() {
        if (!root.prod.in_progress || !root.prod.build_time || root.prod.build_time <= 0)
            return 0;
        return Math.max(0, Math.min(1, 1 - Math.max(0, root.prod.time_remaining || 0) / root.prod.build_time));
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Design.Metrics.space4

        Design.IronPanel {
            id: barracksHeader

            objectName: "barracksHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: headerRow.implicitHeight + barracksHeader.contentPadding * 2
            contentPadding: Design.Metrics.space4
            raised: true
            accessibleName: qsTr("Barracks production queue")

            RowLayout {
                id: headerRow

                anchors.fill: parent
                spacing: Design.Metrics.space8

                Column {
                    id: identityColumn

                    Layout.preferredWidth: Math.min(Math.max(Design.A11y.scaled(92), reserveReadout.implicitWidth), Math.round(barracksHeader.width * 0.4))
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0

                    Text {
                        width: parent.width
                        text: qsTr("BARRACKS")
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                        elide: Text.ElideRight
                    }

                    Text {
                        id: reserveReadout

                        width: parent.width
                        text: qsTr("Reserve %1 / %2").arg(root.prod.manpower_available || 0).arg(root.prod.max_units || 0)
                        color: (root.prod.manpower_available || 0) > 0 ? Design.Theme.textPrimary : Design.Theme.danger
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        elide: Text.ElideRight

                        ToolTip.visible: reserveMouse.containsMouse
                        ToolTip.delay: Design.Metrics.tooltipDelay
                        ToolTip.text: qsTr("Manpower this barracks still holds. Every recruit spends some; civilians raised at a Home deliver more.")

                        MouseArea {
                            id: reserveMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                }

                Column {
                    id: queueColumn

                    readonly property int slotSize: Design.A11y.scaled(22)

                    Layout.alignment: Qt.AlignVCenter
                    spacing: Design.Metrics.space2

                    Row {
                        id: queueRow

                        objectName: "barracksQueueRow"
                        spacing: Design.Metrics.space4

                        Repeater {
                            model: root.queueSlots

                            delegate: Rectangle {
                                id: queueSlot

                                required property int index
                                readonly property string unitType: root.queue_type(index)
                                readonly property bool occupied: unitType !== ""
                                readonly property bool producing: index === 0 && root.prod.in_progress === true

                                width: queueColumn.slotSize
                                height: width
                                radius: Design.Metrics.radiusSmall
                                color: queueSlot.occupied ? Design.Theme.panelIron : Design.Theme.backgroundDeep
                                border.width: queueSlot.producing ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                                border.color: queueSlot.producing ? Design.Theme.accent : (queueSlot.occupied ? Design.Theme.borderStrong : Design.Theme.borderSubtle)
                                opacity: queueSlot.occupied ? 1 : 0.6

                                Image {
                                    id: queueArt

                                    anchors.fill: parent
                                    anchors.margins: Design.Metrics.space2
                                    source: queueSlot.occupied && root.panel ? root.panel.unit_icon_source(queueSlot.unitType, root.prod.nation_id) : ""
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    visible: status === Image.Ready
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: queueSlot.occupied && !queueArt.visible
                                    text: root.panel ? root.panel.unit_icon_emoji(queueSlot.unitType) : ""
                                    color: Design.Theme.textSecondary
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                }

                                ToolTip.visible: queueMouse.containsMouse && queueSlot.occupied
                                ToolTip.text: queueSlot.producing ? qsTr("Recruiting now") : qsTr("Queued unit %1").arg(index + 1)
                                ToolTip.delay: Design.Metrics.tooltipDelay

                                MouseArea {
                                    id: queueMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }
                            }
                        }
                    }

                    Design.IronProgressBar {
                        id: productionProgress

                        objectName: "barracksProductionProgress"
                        width: queueRow.width
                        height: Design.Metrics.space4
                        visible: root.prod.in_progress === true
                        value: root.progress_value()
                        fillColor: Design.Theme.accent
                    }
                }

                Design.IronIconButton {
                    Layout.preferredWidth: Design.Metrics.minTouchTarget
                    Layout.preferredHeight: Design.Metrics.minTouchTarget
                    Layout.alignment: Qt.AlignVCenter
                    iconText: Design.Icons.rally
                    tooltip: qsTr("Set where newly recruited units will gather")
                    accessibleName: qsTr("Set barracks rally point")
                    blocked: root.prod.has_barracks !== true
                    disabledReason: qsTr("Select a barracks before setting a rally point")
                    onClicked: root.rally_requested()
                }
            }
        }

        Item {
            id: gridViewport

            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: recruitGrid

                objectName: "barracksRecruitGrid"
                readonly property int trackWidth: Math.max(root.minCardWidth, width - root.listGutter)
                readonly property int columns: Math.max(1, Math.min(root.maxColumns, Math.floor(trackWidth / root.minCardWidth)))
                readonly property int cardWidth: cellWidth - root.cardGap
                readonly property int bodyWidth: Math.max(Design.A11y.scaled(40), cardWidth - root.cardHeight - Design.Metrics.space4 * 2)
                readonly property int costPitch: Math.min(Design.A11y.scaled(46), Math.floor(bodyWidth / Math.max(1, root.costColumns)))
                readonly property int wholeRows: Math.max(1, Math.floor(gridViewport.height / cellHeight))
                readonly property bool overflowing: contentHeight > height

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.min(parent.height, wholeRows * cellHeight)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                snapMode: GridView.SnapToRow
                keyNavigationWraps: false
                model: root.cards

                cacheBuffer: contentHeight
                cellWidth: Math.floor(trackWidth / columns)
                cellHeight: root.cardHeight + root.cardGap

                ScrollBar.vertical: Design.IronScrollBar {
                    objectName: "barracksRecruitScrollBar"
                }

                delegate: Item {
                    required property var modelData

                    width: recruitGrid.cellWidth
                    height: recruitGrid.cellHeight

                    RecruitCard {
                        objectName: "recruitCard_" + modelData.unit_type
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: recruitGrid.cardWidth
                        height: root.cardHeight

                        panel: root.panel
                        prod: root.prod
                        unit_type: modelData.unit_type
                        fallback_name: modelData.fallback_name
                        fallback_build_time: modelData.build_time
                        cost_pitch: recruitGrid.costPitch
                        tooltip_text: panel ? panel.recruit_tooltip(unit_info, modelData.fallback_name, modelData.build_time, modelData.carthage_only === true) : ""
                        onRecruit_requested: function (unitType) {
                            root.recruit_requested(unitType);
                        }
                        onDetails_requested: function (unitType, nation) {
                            root.details_requested(unitType, nation);
                        }
                    }
                }
            }
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: card

    property var panel: null
    property var prod: ({})
    property string unit_type: ""
    property string fallback_name: ""
    property int fallback_build_time: 5
    property string tooltip_text: ""
    property int cost_pitch: 0

    readonly property int pad: Design.Metrics.space4
    readonly property int infoSize: Math.max(Design.A11y.scaled(18), detailsLabel.implicitHeight + Design.Metrics.space2)
    readonly property int costIconSize: Design.A11y.scaled(10)
    readonly property int maxCostPitch: Design.A11y.scaled(46)

    readonly property int queue_total: (card.prod.in_progress ? 1 : 0) + (card.prod.queue_size || 0)
    readonly property var unit_info: card.panel ? card.panel.get_unit_production_info(card.unit_type, card.prod.nation_id) : ({})
    readonly property var cost_list: card.panel ? card.panel.cost_entries(card.panel.reserve_cost(card.unit_info), card.unit_info.resource_costs || {}, true) : []
    readonly property int effective_cost_pitch: card.cost_pitch > 0 ? card.cost_pitch : Math.min(card.maxCostPitch, Math.floor(cardBody.width / Math.max(1, card.cost_list.length)))
    readonly property var card_state: card.panel ? card.panel.recruit_card_state(card.prod, card.unit_info, card.queue_total) : ({
            "enabled": false,
            "reason": ""
        })
    readonly property bool is_enabled: card.card_state.enabled
    readonly property bool is_hovered: recruitMouseArea.containsMouse
    readonly property bool is_active: card.is_hovered && card.is_enabled
    readonly property string display_name: (card.unit_info && card.unit_info.display_name) || card.fallback_name || card.unit_type

    signal recruit_requested(string unit_type)
    signal details_requested(string unit_type, string nation)

    function cost_short(key, amount) {
        if (!card.panel)
            return false;
        if (key === "reserve")
            return (card.prod.manpower_available || 0) < amount;
        return card.panel.resource_amount(card.panel.current_resources(), key) < amount;
    }

    width: Design.A11y.scaled(200)
    height: Math.max(Design.A11y.scaled(48), Design.Typography.caption * 3 + Design.Metrics.space8)
    radius: Design.Metrics.radiusMedium
    clip: true
    color: card.panel ? card.panel.recruit_card_color(card.is_enabled, card.is_hovered) : "transparent"
    border.color: card.is_active ? Design.Theme.accent : (card.panel ? card.panel.recruit_card_border(card.is_enabled, false) : "transparent")
    border.width: card.is_active ? Design.Metrics.borderFocus : Design.Metrics.borderThin
    opacity: card.is_enabled ? 1 : 0.55

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Recruit %1").arg(card.display_name)
    Accessible.description: card.is_enabled ? card.tooltip_text : card.card_state.reason

    Rectangle {
        id: portraitFrame

        objectName: "recruitPortrait"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: card.pad
        width: height
        radius: Design.Metrics.radiusSmall
        color: Design.Theme.backgroundDeep
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle

        Image {
            id: recruitIcon

            objectName: "recruitPortraitImage"
            anchors.fill: parent
            anchors.margins: Design.Metrics.space2
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            source: card.panel ? card.panel.unit_icon_source(card.unit_type, card.prod.nation_id) : ""
            visible: status === Image.Ready
            opacity: card.is_enabled ? 1 : 0.5
        }

        Text {
            anchors.centerIn: parent
            visible: !recruitIcon.visible
            text: card.panel ? card.panel.unit_icon_emoji(card.unit_type) : ""
            color: card.is_enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
            font.pixelSize: Design.Typography.glyphSmall
            opacity: card.is_enabled ? 0.9 : 0.4
        }
    }

    Item {
        id: cardBody

        z: 1
        anchors.left: portraitFrame.right
        anchors.leftMargin: card.pad
        anchors.right: parent.right
        anchors.rightMargin: card.pad
        anchors.top: parent.top
        anchors.topMargin: card.pad
        anchors.bottom: parent.bottom
        anchors.bottomMargin: card.pad

        Text {
            id: unitName

            objectName: "recruitCardName"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: card.infoSize + Design.Metrics.space2
            anchors.top: parent.top
            height: Math.max(implicitHeight, card.infoSize)
            verticalAlignment: Text.AlignVCenter
            text: card.display_name
            color: card.is_enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.bold
            elide: Text.ElideRight
        }

        Row {
            id: costRow

            objectName: "recruitCostRow"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 0

            Repeater {
                model: card.cost_list

                delegate: Item {
                    id: costCell

                    required property var modelData
                    readonly property bool short_of: card.cost_short(costCell.modelData.key, costCell.modelData.amount)

                    width: card.effective_cost_pitch
                    height: Math.max(card.costIconSize, costValue.contentHeight)

                    Image {
                        id: costIcon

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: card.costIconSize
                        height: card.costIconSize
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        source: card.panel ? card.panel.cost_icon_source(costCell.modelData.key) : ""
                        opacity: card.is_enabled ? 0.95 : 0.5
                    }

                    Text {
                        id: costValue

                        anchors.left: costIcon.right
                        anchors.leftMargin: Design.Metrics.space2
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space2
                        anchors.verticalCenter: parent.verticalCenter
                        text: costCell.modelData.amount
                        color: costCell.short_of ? Design.Theme.danger : (card.is_enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled)
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: Design.Typography.minimumSize
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideNone
                    }
                }
            }
        }

        Rectangle {
            id: detailsBadge

            anchors.right: parent.right
            anchors.verticalCenter: unitName.verticalCenter
            width: card.infoSize
            height: width
            radius: width / 2
            color: detailsMouse.containsMouse ? Design.Theme.panelLeather : "transparent"
            border.width: Design.Metrics.borderThin
            border.color: detailsMouse.containsMouse ? Design.Theme.accent : Design.Theme.borderSubtle

            Text {
                id: detailsLabel

                anchors.centerIn: parent
                text: "i"
                color: detailsMouse.containsMouse ? Design.Theme.textPrimary : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                font.bold: true
            }

            MouseArea {
                id: detailsMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    Design.UiSound.activate();
                    card.details_requested(card.unit_type, card.prod.nation_id || "");
                }
                ToolTip.visible: containsMouse
                ToolTip.text: qsTr("Show unit details")
                ToolTip.delay: Design.Metrics.tooltipDelay
            }
        }
    }

    MouseArea {
        id: recruitMouseArea

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                Design.UiSound.activate();
                card.details_requested(card.unit_type, card.prod.nation_id || "");
                return;
            }
            if (card.is_enabled) {
                Design.UiSound.activate();
                card.recruit_requested(card.unit_type);
            } else {
                Design.UiSound.warning();
            }
        }
        onContainsMouseChanged: {
            if (containsMouse && card.is_enabled)
                Design.UiSound.hover();
        }
        cursorShape: card.is_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        ToolTip.visible: containsMouse
        ToolTip.text: card.is_enabled ? card.tooltip_text : card.card_state.reason
        ToolTip.delay: Design.Metrics.tooltipDelay
    }

    Rectangle {
        anchors.fill: parent
        color: "#F4E7C8"
        opacity: recruitMouseArea.pressed ? 0.2 : 0
        radius: parent.radius
    }

    Behavior on color  {
        ColorAnimation {
            duration: 150
        }
    }

    Behavior on border.color  {
        ColorAnimation {
            duration: 150
        }
    }
}

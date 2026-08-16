import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ToolTip {
    id: control

    property string title: ""
    property string hotkey: ""

    property string summary: ""

    property var details: []

    property string status: ""
    property string warning: ""

    readonly property bool hasBody: summary !== "" || status !== "" || warning !== "" || (details && details.length > 0)

    function detail_term(entry) {
        return entry && entry.term !== undefined ? entry.term : "";
    }

    function detail_text(entry) {
        return entry && entry.text !== undefined ? entry.text : "";
    }

    y: -control.height - Design.Metrics.space4
    x: {
        if (!control.parent)
            return 0;
        var centered = Math.round((control.parent.width - control.implicitWidth) / 2);
        var host = control.Overlay.overlay;
        if (!host || host.width <= 0)
            return centered;
        var left = control.parent.mapToItem(host, centered, 0).x;
        var limit = host.width - control.implicitWidth - Design.Metrics.space8;
        var clamped = Math.max(Design.Metrics.space8, Math.min(left, limit));
        return centered + (clamped - left);
    }

    delay: Design.Metrics.tooltipDelay
    timeout: -1
    padding: Design.Metrics.space8
    implicitWidth: Design.Metrics.tooltipWidth

    implicitHeight: body.implicitHeight + control.topPadding + control.bottomPadding

    background: Rectangle {
        color: Design.Theme.panelLeather
        radius: Design.Metrics.radiusSmall
        border.color: Design.Theme.borderStrong
        border.width: Design.Metrics.borderThin
    }

    contentItem: Column {
        id: body

        spacing: Design.Metrics.space4
        width: control.implicitWidth - control.leftPadding - control.rightPadding

        Item {
            width: parent.width
            height: Math.max(titleText.implicitHeight, hotkeyChip.height)

            Text {
                id: titleText

                anchors.left: parent.left
                anchors.right: hotkeyChip.visible ? hotkeyChip.left : parent.right
                anchors.rightMargin: hotkeyChip.visible ? Design.Metrics.space8 : 0
                anchors.verticalCenter: parent.verticalCenter
                visible: control.title !== ""
                text: control.title
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.bold
                elide: Text.ElideRight
            }

            Design.IronHotkeyLabel {
                id: hotkeyChip

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: control.hotkey !== ""
                text: control.hotkey
            }
        }

        Text {
            width: parent.width
            visible: control.summary !== ""
            text: control.summary
            color: Design.Theme.textPrimary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: control.details

            delegate: Row {
                id: detailRow

                required property var modelData

                readonly property int termWidth: Math.round(width * 0.3)

                width: body.width
                spacing: Design.Metrics.space8

                Text {
                    width: detailRow.termWidth
                    text: control.detail_term(detailRow.modelData)
                    color: Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: detailRow.width - detailRow.termWidth - detailRow.spacing
                    text: control.detail_text(detailRow.modelData)
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    wrapMode: Text.WordWrap
                }
            }
        }

        Text {
            width: parent.width
            visible: control.status !== ""
            text: control.status
            color: Design.Theme.accent
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.medium
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            visible: control.warning !== ""
            text: control.warning
            color: Design.Theme.warning
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.weight: Design.Typography.medium
            wrapMode: Text.WordWrap
        }
    }
}

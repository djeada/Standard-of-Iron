import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0 as Design

Design.IronPanel {
    id: hintCard

    property string hintId: ""
    property string title: ""
    property string iconText: ""
    property string closeTooltip: qsTr("Hide this")
    property string hoverTooltip: ""
    property color accent: Design.Theme.accent
    property bool gate: true

    default property alias body: bodyColumn.data

    readonly property bool armed: Core.UiHints.showing[hintCard.hintId] === true

    signal dismissed
    signal suppressed

    function dismiss() {
        Core.UiHints.dismiss(hintCard.hintId);
        hintCard.dismissed();
    }

    function stop_showing() {
        Core.UiHints.suppress(hintCard.hintId);
        hintCard.suppressed();
    }

    visible: armed && gate
    raised: true
    accessibleName: title
    implicitHeight: layout.implicitHeight + Design.Metrics.space12 * 2

    ToolTip.delay: Design.Metrics.tooltipDelay
    ToolTip.text: hintCard.hoverTooltip
    ToolTip.visible: hoverHandler.hovered && hintCard.hoverTooltip.length > 0

    HoverHandler {
        id: hoverHandler
    }

    ColumnLayout {
        id: layout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.Metrics.space4

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.Metrics.space8

            Text {
                color: hintCard.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                text: hintCard.iconText
                visible: hintCard.iconText.length > 0
            }

            Text {
                Layout.fillWidth: true
                color: Design.Theme.textPrimary
                elide: Text.ElideRight
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.bold
                text: hintCard.title
            }

            Design.IronIconButton {
                iconText: Design.Icons.close
                tooltip: hintCard.closeTooltip
                onClicked: hintCard.dismiss()
            }
        }

        ColumnLayout {
            id: bodyColumn

            Layout.fillWidth: true
            spacing: Design.Metrics.space4
        }

        Text {
            Layout.fillWidth: true
            color: neverAgainArea.containsMouse ? Design.Theme.textPrimary : Design.Theme.textDisabled
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.caption
            font.underline: neverAgainArea.containsMouse
            horizontalAlignment: Text.AlignRight
            text: qsTr("Never show this again")

            MouseArea {
                id: neverAgainArea

                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: hintCard.stop_showing()
            }
        }
    }
}

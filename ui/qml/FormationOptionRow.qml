import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: row

    property string label: ""
    property string hint: ""
    property var model: []
    property int selectedIndex: 0

    signal activated(int index)

    implicitHeight: dropdown.implicitHeight
    height: implicitHeight

    Text {
        id: caption

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        color: Design.Theme.textSecondary
        elide: Text.ElideRight
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
        text: row.label
        width: Math.floor(row.width * 0.42)

        MouseArea {
            id: captionHover

            anchors.fill: parent
            hoverEnabled: true
        }

        ToolTip.delay: Design.Metrics.tooltipDelay
        ToolTip.text: row.hint
        ToolTip.visible: captionHover.containsMouse && row.hint.length > 0
    }

    Design.IronDropdown {
        id: dropdown

        accessibleName: row.label
        anchors.left: caption.right
        anchors.leftMargin: Design.Metrics.space8
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        currentIndex: row.selectedIndex
        model: row.model

        ToolTip.delay: Design.Metrics.tooltipDelay
        ToolTip.text: row.hint
        ToolTip.visible: hovered && row.hint.length > 0

        onActivated: function (index) {
            row.activated(index);
        }
    }
}

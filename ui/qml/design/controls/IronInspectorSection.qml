import QtQuick 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property string title: ""
    property bool expanded: true
    property bool collapsible: true
    default property alias sectionContent: contentColumn.data

    implicitHeight: sectionColumn.implicitHeight + Design.Metrics.space24
    Accessible.name: root.title

    Column {
        id: sectionColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.Metrics.space8

        Item {
            width: parent.width
            height: heading.implicitHeight

            Text {
                id: disclosure

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: root.collapsible
                text: root.expanded ? Design.Icons.disclosureOpen : Design.Icons.disclosureClosed
                color: headingMouse.containsMouse ? Design.Theme.accent : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
            }

            Text {
                id: heading

                anchors.left: root.collapsible ? disclosure.right : parent.left
                anchors.leftMargin: root.collapsible ? Design.Metrics.space8 : 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: headingMouse.containsMouse ? Design.Theme.textPrimary : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                font.weight: Design.Typography.bold
                font.letterSpacing: Design.Typography.trackingWide
                elide: Text.ElideRight
            }

            MouseArea {
                id: headingMouse

                anchors.fill: parent
                enabled: root.collapsible
                hoverEnabled: root.collapsible
                cursorShape: root.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        Design.IronDivider {
            width: parent.width
        }

        Column {
            id: contentColumn

            width: parent.width
            spacing: Design.Metrics.space8
            visible: root.expanded
        }
    }
}

import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property alias toolbar: toolbarHost.data
    property alias workspace: workspaceHost.data
    property alias statusBar: statusHost.data

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
    }

    Rectangle {
        id: toolbarStrip

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.max(Design.Metrics.controlHeight + Design.Metrics.space12, Design.Metrics.space24 * 2)
        color: Design.Theme.panelIron

        Item {
            id: toolbarHost

            anchors.fill: parent
            anchors.leftMargin: Design.Metrics.space12
            anchors.rightMargin: Design.Metrics.space12
            anchors.topMargin: Design.Metrics.space4
            anchors.bottomMargin: Design.Metrics.space4
            clip: true
        }

        Design.IronDivider {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    Item {
        id: workspaceHost

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbarStrip.bottom
        anchors.bottom: statusStrip.top
        clip: true
    }

    Rectangle {
        id: statusStrip

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.max(Design.Metrics.space24, Design.Typography.body + Design.Metrics.space8)
        color: Design.Theme.panelIron

        Design.IronDivider {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
        }

        Item {
            id: statusHost

            anchors.fill: parent
            anchors.leftMargin: Design.Metrics.space12
            anchors.rightMargin: Design.Metrics.space12
            clip: true
        }
    }
}

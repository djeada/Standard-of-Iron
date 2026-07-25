import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property alias toolbar: toolbarHost.data
    property alias workspace: workspaceHost.data
    property alias statusBar: statusHost.data

    Rectangle { anchors.fill: parent; color: Design.Theme.backgroundDeep }
    Item {
        id: toolbarHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 38
    }
    Item {
        id: workspaceHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbarHost.bottom
        anchors.bottom: statusHost.top
    }
    Item {
        id: statusHost
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 24
    }
}

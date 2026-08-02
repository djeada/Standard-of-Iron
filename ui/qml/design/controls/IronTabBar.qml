import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

TabBar {
    id: root

    property var tabs: []

    onCurrentIndexChanged: Design.UiSound.tabSwitch()

    background: Rectangle {
        color: Design.Theme.backgroundRaised
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle
    }

    Repeater {
        model: root.tabs

        delegate: TabButton {
            id: tab

            required property var modelData

            readonly property string title: modelData && modelData.text !== undefined ? modelData.text : modelData
            readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)

            text: title
            implicitHeight: Design.Metrics.controlHeight
            Accessible.name: title

            contentItem: Text {
                text: tab.title
                color: tab.checked ? Design.Theme.textPrimary : Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: tab.checked ? Design.Typography.medium : Design.Typography.regular
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: tab.checked ? Design.Theme.panelIron : "transparent"
                border.width: tab.showFocusRing ? Design.Metrics.borderFocus : 0
                border.color: Design.Theme.focus

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Design.Metrics.borderFocus
                    color: Design.Theme.accent
                    visible: tab.checked
                }
            }
        }
    }
}

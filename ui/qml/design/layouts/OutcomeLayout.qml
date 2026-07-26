import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Item {
    id: root

    property string outcome: "victory"
    property string factionId: ""
    property string headline: ""
    property string subtitle: ""

    default property alias detail: detailHost.data
    property string primaryAction: qsTr("Continue")
    property string secondaryAction: ""

    readonly property bool triumphant: outcome !== "defeat"
    readonly property color tone: outcome === "defeat" ? Design.Theme.danger : outcome === "campaign" ? Design.FactionTheme.accentFor(root.factionId) : Design.Theme.success
    readonly property string crest: outcome === "campaign" ? Design.FactionTheme.glyphFor(root.factionId) : outcome === "defeat" ? Design.Icons.defeated : Design.Icons.objective

    signal primaryActivated
    signal secondaryActivated

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.headline
    Accessible.description: root.subtitle

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.88
    }

    Design.IronPanel {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.72, Design.Metrics.space24 * 30)
        height: content.implicitHeight + Design.Metrics.space24 * 2
        raised: true
        border.color: root.tone
        border.width: Design.Metrics.borderFocus

        Column {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Design.Metrics.space16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.crest
                color: root.tone
                font.family: Design.Typography.displayFamily
                font.pixelSize: Design.Typography.hero
            }

            Text {
                width: parent.width
                text: root.headline
                color: root.tone
                font.family: Design.Typography.displayFamily
                font.pixelSize: Design.Typography.title
                font.weight: Design.Typography.bold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                width: parent.width
                visible: root.subtitle !== ""
                text: root.subtitle
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item {
                id: detailHost

                width: parent.width
                height: childrenRect.height
                visible: children.length > 0
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Design.Metrics.space8

                Design.IronButton {
                    text: root.primaryAction
                    tone: "primary"
                    onClicked: root.primaryActivated()
                }

                Design.IronButton {
                    visible: root.secondaryAction !== ""
                    text: root.secondaryAction
                    onClicked: root.secondaryActivated()
                }
            }
        }
    }

    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity  {
        NumberAnimation {
            duration: Design.Motion.deliberate
            easing.type: Design.Motion.emphasizedEasing
        }
    }
}

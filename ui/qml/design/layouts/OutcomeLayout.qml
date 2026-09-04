import QtQuick 2.15
import QtQuick.Layouts 1.15
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
    readonly property bool compact: width < Design.A11y.scaled(640)
    readonly property string eyebrow: {
        switch (root.outcome) {
        case "campaign":
            return qsTr("CAMPAIGN DECREE");
        case "training":
            return qsTr("FIELD ASSESSMENT");
        case "spectator":
            return qsTr("BATTLEFIELD VERDICT");
        case "defeat":
            return qsTr("COMMAND REPORT");
        }
        return qsTr("BATTLEFIELD VERDICT");
    }

    signal primaryActivated
    signal secondaryActivated

    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.headline
    Accessible.description: root.subtitle

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.92
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "transparent"
            }

            GradientStop {
                position: 0.5
                color: Qt.rgba(root.tone.r, root.tone.g, root.tone.b, Design.Theme.highContrast ? 0.12 : 0.07)
            }

            GradientStop {
                position: 1
                color: "transparent"
            }
        }
    }

    Rectangle {
        width: outcomePanel.width
        height: outcomePanel.height
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: Design.Metrics.space8
        anchors.verticalCenterOffset: Design.Metrics.space8
        radius: outcomePanel.radius
        color: Design.Theme.shadow
        opacity: 0.72
    }

    Design.IronPanel {
        id: outcomePanel

        anchors.centerIn: parent
        width: Math.min(parent.width - Design.Metrics.space24 * 2, Design.Metrics.space24 * 30)
        height: content.implicitHeight + Design.Metrics.space24 * 2
        raised: true
        contentPadding: 0
        border.color: root.tone
        border.width: Design.Metrics.borderFocus

        Rectangle {
            anchors.fill: parent
            radius: outcomePanel.radius
            color: Qt.rgba(root.tone.r, root.tone.g, root.tone.b, Design.Theme.highContrast ? 0.12 : 0.045)
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: Design.Metrics.space4
            radius: Math.max(Design.Metrics.radiusSmall, outcomePanel.radius - Design.Metrics.space4)
            color: "transparent"
            border.color: root.tone
            border.width: Design.Metrics.borderThin
            opacity: 0.36
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Design.Metrics.space4
            color: root.tone
        }

        Column {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Design.Metrics.space24
            anchors.rightMargin: Design.Metrics.space24
            spacing: Design.Metrics.space16

            Text {
                width: parent.width
                text: root.eyebrow
                color: root.tone
                font.family: Design.Typography.titleFamily
                font.capitalization: Font.AllUppercase
                font.pixelSize: Design.Typography.caption
                font.weight: Design.Typography.bold
                font.hintingPreference: Design.Typography.titleHinting
                font.letterSpacing: Design.Typography.trackingWide
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                width: parent.width
                spacing: Design.Metrics.space12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.Metrics.borderThin
                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 1
                            color: root.tone
                        }
                    }
                    opacity: 0.72
                }

                Item {
                    Layout.preferredWidth: Design.A11y.scaled(64)
                    Layout.preferredHeight: Layout.preferredWidth

                    Rectangle {
                        anchors.centerIn: parent
                        width: Design.A11y.scaled(44)
                        height: width
                        rotation: 45
                        radius: Design.Metrics.radiusSmall
                        color: Design.Theme.backgroundDeep
                        border.color: root.tone
                        border.width: Design.Metrics.borderFocus
                    }

                    Text {
                        anchors.centerIn: parent
                        text: root.crest
                        color: root.tone
                        font.family: Design.Typography.displayFamily
                        font.pixelSize: Design.Typography.glyphSmall
                        font.weight: Design.Typography.bold
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.Metrics.borderThin
                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: root.tone
                        }

                        GradientStop {
                            position: 1
                            color: "transparent"
                        }
                    }
                    opacity: 0.72
                }
            }

            Text {
                width: parent.width
                text: root.headline
                color: root.tone
                font.family: Design.Typography.titleFamily
                font.capitalization: Font.AllUppercase
                font.pixelSize: Design.Typography.title
                font.weight: Design.Typography.bold
                font.hintingPreference: Design.Typography.titleHinting
                font.kerning: true
                font.letterSpacing: Design.Typography.trackingTitle
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: root.subtitle !== ""
                text: root.subtitle
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.bodyLarge
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item {
                id: detailHost

                width: parent.width
                height: childrenRect.height
                visible: children.length > 0
            }

            GridLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: root.compact ? 1 : 2
                columnSpacing: Design.Metrics.space8
                rowSpacing: Design.Metrics.space8

                Design.IronButton {
                    Layout.preferredWidth: Design.A11y.scaled(176)
                    text: root.primaryAction
                    tone: "primary"
                    onClicked: root.primaryActivated()
                }

                Design.IronButton {
                    Layout.preferredWidth: Design.A11y.scaled(176)
                    visible: root.secondaryAction !== ""
                    text: root.secondaryAction
                    onClicked: root.secondaryActivated()
                }
            }
        }

        transform: Translate {
            id: panelRise

            y: Design.Metrics.space12
        }

        NumberAnimation {
            target: panelRise
            property: "y"
            running: true
            from: Design.Metrics.space12
            to: 0
            duration: Design.Motion.deliberate
            easing.type: Design.Motion.emphasizedEasing
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

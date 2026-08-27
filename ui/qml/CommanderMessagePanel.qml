import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: messageRoot

    property var source: (typeof game !== 'undefined' && game && game.commander_message) ? game.commander_message : null
    readonly property bool showing: source !== null && source.active
    readonly property var faction: Design.FactionTheme.describe(source ? source.nation : "")
    readonly property color accent: faction.accent
    readonly property string speakerType: source ? source.speaker_id : ""
    readonly property string speakerPose: source ? source.pose : ""

    property int revealed: 0

    function dismiss() {
        if (messageRoot.source)
            messageRoot.source.dismiss();
    }

    implicitWidth: panel.implicitWidth
    implicitHeight: panel.implicitHeight
    visible: opacity > 0
    opacity: showing ? 1 : 0

    Behavior on opacity  {
        NumberAnimation {
            duration: Design.Motion.deliberate
            easing.type: Design.Motion.standardEasing
        }
    }

    readonly property string bodyText: messageRoot.source ? messageRoot.source.text : ""

    onBodyTextChanged: messageRoot.revealed = Design.A11y.reducedMotion ? 9999 : 0

    Timer {
        id: typewriter

        interval: 22
        repeat: true
        running: messageRoot.showing && !Design.A11y.reducedMotion && messageRoot.revealed < fullText.length
        onTriggered: messageRoot.revealed += 1
    }

    QtObject {
        id: fullText

        readonly property string text: messageRoot.source ? messageRoot.source.text : ""
        readonly property int length: text.length
    }

    Text {
        id: lineMetrics

        visible: false
        width: Design.Metrics.space24 * 15
        text: fullText.text
        wrapMode: Text.WordWrap
        font.family: Design.Typography.displayFamily
        font.pixelSize: Design.Typography.body
        font.italic: true
        lineHeight: 1.25
    }

    Design.IronPanel {
        id: panel

        raised: true
        translucent: true
        implicitWidth: content.implicitWidth + (Design.Metrics.space12 * 2)
        implicitHeight: content.implicitHeight + (Design.Metrics.space12 * 2)
        border.color: messageRoot.accent

        transform: Translate {
            x: messageRoot.showing ? 0 : Design.Metrics.space32

            Behavior on x  {
                NumberAnimation {
                    duration: Design.Motion.deliberate
                    easing.type: Design.Motion.emphasizedEasing
                }
            }
        }

        RowLayout {
            id: content

            anchors.fill: parent
            spacing: Design.Metrics.space12

            Item {
                Layout.alignment: Qt.AlignTop
                implicitWidth: Design.Metrics.space24 * 5
                implicitHeight: Design.Metrics.space24 * 6

                Rectangle {
                    id: portraitFrame

                    anchors.fill: parent
                    color: Design.Theme.backgroundDeep
                    radius: Design.Metrics.radiusSmall
                    border.width: Design.Metrics.borderFocus
                    border.color: messageRoot.accent
                    clip: true

                    CommanderPortraitView {
                        id: portrait

                        anchors.fill: parent
                        anchors.margins: Design.Metrics.borderFocus
                        troopType: messageRoot.speakerType
                        nation: messageRoot.source ? messageRoot.source.nation : ""
                        pose: messageRoot.speakerPose
                        speaking: messageRoot.showing
                    }

                    CommanderFaceOverlay {
                        id: face

                        anchorSource: portrait
                        pose: messageRoot.speakerPose
                        accent: messageRoot.accent
                        talking: messageRoot.showing && messageRoot.revealed < fullText.length
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(0.10, 0.075, 0.055, 0.52)
                    }

                    Rectangle {
                        anchors.fill: parent

                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: Qt.rgba(0.06, 0.05, 0.04, 0.75)
                            }

                            GradientStop {
                                position: 0.42
                                color: "transparent"
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: messageRoot.accent
                        opacity: 0.0
                        visible: Design.Motion.allowAmbientLoops && messageRoot.showing

                        SequentialAnimation on opacity  {
                            running: Design.Motion.allowAmbientLoops && messageRoot.showing
                            loops: Animation.Infinite

                            NumberAnimation {
                                to: 0.12
                                duration: 1700
                                easing.type: Easing.InOutSine
                            }

                            NumberAnimation {
                                to: 0.02
                                duration: 2300
                                easing.type: Easing.InOutSine
                            }
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.45

                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: "transparent"
                            }

                            GradientStop {
                                position: 1.0
                                color: Design.Theme.shadow
                            }
                        }
                    }
                }
            }

            ColumnLayout {

                Layout.fillWidth: true
                Layout.preferredWidth: Design.Metrics.space24 * 15
                Layout.maximumWidth: Design.Metrics.space24 * 15
                spacing: Design.Metrics.space4

                Text {
                    Layout.fillWidth: true
                    text: messageRoot.source ? messageRoot.source.speaker_name : ""
                    color: messageRoot.accent
                    elide: Text.ElideRight
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.subheading
                    font.weight: Design.Typography.bold
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: messageRoot.faction.name + "  " + messageRoot.faction.glyph
                    color: Design.Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.letterSpacing: Design.Typography.trackingWide
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: Design.Metrics.space4
                    height: Design.Metrics.borderThin
                    color: messageRoot.accent
                    opacity: 0.45
                }

                Text {
                    id: line

                    Layout.fillWidth: true
                    Layout.topMargin: Design.Metrics.space4
                    Layout.preferredHeight: lineMetrics.implicitHeight
                    text: Design.A11y.reducedMotion ? fullText.text : fullText.text.substring(0, messageRoot.revealed)
                    color: Design.Theme.textPrimary
                    wrapMode: Text.WordWrap
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.body
                    font.italic: true
                    lineHeight: 1.25
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: Design.Metrics.space4
                    text: qsTr("Click to dismiss")
                    color: Design.Theme.textDisabled
                    horizontalAlignment: Text.AlignRight
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            enabled: messageRoot.showing
            onClicked: messageRoot.dismiss()
        }
    }
}

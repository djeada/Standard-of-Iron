import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

// The closing beat of a battle fought as the commander. While the camera makes
// its last turn about him the HUD is put away, the frame closes to a letterbox
// and one line says what happened; the result panel follows when it is over.
Item {
    id: root

    // "" | "fallen" | "victory" | "defeat"
    property string kind: ""
    property string commanderName: ""

    readonly property bool showing: kind !== ""
    readonly property real resolutionScale: Math.max(0.75, Math.min(2.0, height / 1080))
    readonly property color toneColor: kind === "victory" ? Design.Theme.focus : Design.Theme.danger
    readonly property real barHeight: Math.round(root.height * 0.105)

    // The line is kept while the overlay fades out, so it does not blank a
    // frame before the bars have gone.
    property string shownKind: ""
    property string shownName: ""

    function title_text() {
        if (root.shownKind === "victory")
            return qsTr("The field is yours");
        if (root.shownKind === "fallen")
            return root.shownName !== "" ? qsTr("%1 has fallen").arg(root.shownName) : qsTr("Your commander has fallen");
        return qsTr("The day is lost");
    }

    onKindChanged: {
        if (root.kind !== "") {
            root.shownKind = root.kind;
            root.shownName = root.commanderName;
        }
    }

    anchors.fill: parent
    visible: curtain > 0.0

    property real curtain: showing ? 1.0 : 0.0

    Behavior on curtain  {
        NumberAnimation {
            duration: Design.Motion.reducedMotion ? 0 : (root.showing ? 620 : 260)
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.22 * root.curtain
    }

    Rectangle {
        objectName: "rpgFinaleTopBar"
        anchors.left: parent.left
        anchors.right: parent.right
        y: -root.barHeight * (1.0 - root.curtain)
        height: root.barHeight
        color: "black"
    }

    Rectangle {
        objectName: "rpgFinaleBottomBar"
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.height - root.barHeight * root.curtain
        height: root.barHeight
        color: "black"
    }

    Column {
        id: titleBlock

        anchors.horizontalCenter: parent.horizontalCenter
        y: root.height * 0.70 - height * 0.5
        spacing: Math.round(10 * root.resolutionScale)
        opacity: titleReveal

        property real titleReveal: root.showing ? 1.0 : 0.0

        Behavior on titleReveal  {
            SequentialAnimation {
                PauseAnimation {
                    duration: root.showing && !Design.Motion.reducedMotion ? 380 : 0
                }
                NumberAnimation {
                    duration: Design.Motion.reducedMotion ? 0 : (root.showing ? 700 : 200)
                    easing.type: Easing.OutCubic
                }
            }
        }

        Text {
            id: titleLabel

            objectName: "rpgFinaleTitle"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, root.width * 0.86)
            horizontalAlignment: Text.AlignHCenter
            text: root.title_text()
            color: Design.Theme.parchment
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Math.round(22 * root.resolutionScale)
            font.family: Design.Typography.titleFamily
            font.pixelSize: Math.round(58 * root.resolutionScale)
            font.letterSpacing: Design.Typography.trackingTitle
            font.capitalization: Font.AllUppercase
            font.hintingPreference: Design.Typography.titleHinting
            font.kerning: true
            style: Text.Outline
            styleColor: Qt.rgba(0, 0, 0, 0.75)
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(titleLabel.width, root.width * 0.5) * titleBlock.titleReveal
            height: Math.max(2, Math.round(3 * root.resolutionScale))
            radius: height / 2
            color: root.toneColor
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: chip

    property string caption: ""
    property string value: ""
    property string emblem_source: ""
    property color value_color: Theme.textMain
    property color outline: Theme.thumbBr
    property bool interactive: false
    property bool dimmed: false
    property string tooltip_text: ""

    readonly property bool engaged: chip_mouse.containsMouse && interactive

    signal activated

    implicitWidth: Design.Metrics.space24 * 4
    implicitHeight: Design.Metrics.controlHeight + Design.Metrics.space8
    radius: Theme.radiusSmall
    color: engaged ? Qt.lighter(Theme.cardBaseB, 1.18) : Theme.cardBaseB
    border.color: engaged ? Theme.selectedBr : outline
    border.width: engaged ? 2 : 1
    opacity: dimmed ? 0.45 : 1
    ToolTip.visible: chip_mouse.containsMouse && tooltip_text !== ""
    ToolTip.text: tooltip_text
    ToolTip.delay: Design.Metrics.tooltipDelay

    Image {
        id: emblem

        width: chip.emblem_source !== "" ? Design.Metrics.iconMedium : 0
        height: width
        visible: chip.emblem_source !== ""
        source: chip.emblem_source
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true

        anchors {
            left: parent.left
            leftMargin: Theme.spacingSmall
            verticalCenter: parent.verticalCenter
        }
    }

    Item {
        id: text_area

        anchors {
            left: emblem.visible ? emblem.right : parent.left
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            leftMargin: Theme.spacingSmall
            rightMargin: Theme.spacingSmall
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            spacing: 1

            Text {
                width: parent.width
                text: chip.caption
                visible: chip.caption !== ""
                color: Theme.textSubLite
                font.pixelSize: Design.Typography.caption
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: chip.value
                color: chip.value_color
                font.pixelSize: Design.Typography.caption
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: chip_mouse

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: chip.interactive ? Qt.LeftButton : Qt.NoButton
        cursorShape: chip.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            Design.UiSound.toggle();
            chip.activated();
        }
    }

    Behavior on color  {
        ColorAnimation {
            duration: Theme.animFast
        }
    }

    Behavior on border.color  {
        ColorAnimation {
            duration: Theme.animFast
        }
    }

    Behavior on border.width  {
        NumberAnimation {
            duration: Theme.animFast
        }
    }
}

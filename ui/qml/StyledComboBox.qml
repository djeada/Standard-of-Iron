import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

ComboBox {
    id: root

    property int textPointSize: Theme.fontSizeMedium
    property int textPixelSize: -1
    property color textColor: Theme.textMain
    property color backgroundColor: Theme.cardBase
    property color borderColor: activeFocus ? Theme.accentBr : Theme.cardBorder
    property color popupBackground: Theme.panelBase
    property color popupBorder: Theme.cardBorder
    property color highlightBackground: Theme.hoverBg
    property color highlightBorder: Theme.selectedBr
    property color itemBackground: Theme.cardBase
    property color itemBorder: Theme.cardBorder
    property var delegateText: function(data) {
        return data;
    }

    function resolveDelegateText(data) {
        return (typeof delegateText === "function") ? delegateText(data) : data;
    }

    contentItem: Text {
        text: root.displayText
        color: root.textColor
        font.pointSize: root.textPixelSize > 0 ? -1 : root.textPointSize
        font.pixelSize: root.textPixelSize > 0 ? root.textPixelSize : -1
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        anchors.fill: parent
        leftPadding: Theme.spacingSmall
        rightPadding: Theme.spacingLarge + root.indicator.width
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.backgroundColor
        border.color: root.borderColor
        border.width: 1
    }

    indicator: Canvas {
        id: rootIndicator

        width: 12
        height: 8
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = root.textColor;
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(width, 0);
            ctx.lineTo(width / 2, height);
            ctx.closePath();
            ctx.fill();
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        Connections {
            function onTextColorChanged() {
                rootIndicator.requestPaint();
            }

            target: root
        }

    }

    popup: Popup {
        y: root.height
        width: root.width
        implicitHeight: contentItem.implicitHeight
        padding: 0

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: root.popupBackground
            border.color: root.popupBorder
            border.width: 1
        }

    }

    delegate: ItemDelegate {
        width: root.width
        highlighted: root.highlightedIndex === index

        background: Rectangle {
            color: highlighted ? root.highlightBackground : root.itemBackground
            border.color: highlighted ? root.highlightBorder : root.itemBorder
            border.width: 1
        }

        contentItem: Text {
            text: root.resolveDelegateText(modelData)
            color: root.textColor
            font.pointSize: root.textPixelSize > 0 ? -1 : root.textPointSize
            font.pixelSize: root.textPixelSize > 0 ? root.textPixelSize : -1
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

    }

}

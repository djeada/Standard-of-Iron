import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

ComboBox {
    id: root

    property int text_point_size: Theme.fontSizeMedium
    property int text_pixel_size: -1
    property color text_color: Theme.textMain
    property color background_color: Theme.cardBase
    property color border_color: activeFocus ? Theme.accentBr : Theme.cardBorder
    property color popup_background: Theme.panelBase
    property color popup_border: Theme.cardBorder
    property color highlight_background: Theme.hoverBg
    property color highlight_border: Theme.selectedBr
    property color item_background: Theme.cardBase
    property color item_border: Theme.cardBorder
    property var delegate_text: function(data) {
        return data;
    }

    function resolve_delegate_text(data) {
        return (typeof delegate_text === "function") ? delegate_text(data) : data;
    }

    contentItem: Text {
        text: root.displayText
        color: root.text_color
        font.pointSize: root.text_pixel_size > 0 ? -1 : root.text_point_size
        font.pixelSize: root.text_pixel_size > 0 ? root.text_pixel_size : -1
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        anchors.fill: parent
        leftPadding: Theme.spacingSmall
        rightPadding: Theme.spacingLarge + root.indicator.width
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.background_color
        border.color: root.border_color
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
            ctx.fillStyle = root.text_color;
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
            function onText_colorChanged() {
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
            color: root.popup_background
            border.color: root.popup_border
            border.width: 1
        }

    }

    delegate: ItemDelegate {
        width: root.width
        highlighted: root.highlightedIndex === index

        background: Rectangle {
            color: highlighted ? root.highlight_background : root.item_background
            border.color: highlighted ? root.highlight_border : root.item_border
            border.width: 1
        }

        contentItem: Text {
            text: root.resolve_delegate_text(modelData)
            color: root.text_color
            font.pointSize: root.text_pixel_size > 0 ? -1 : root.text_point_size
            font.pixelSize: root.text_pixel_size > 0 ? root.text_pixel_size : -1
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

    }

}

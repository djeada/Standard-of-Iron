import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import "ui_audio.js" as UiAudio

ComboBox {
    id: root

    property int text_point_size: Theme.fontSizeMedium
    property int text_pixel_size: -1
    property color text_color: Theme.textMain
    property color background_color: Theme.cardBase
    property color border_color: Theme.cardBorder
    property color popup_background: Theme.panelBase
    property color popup_border: Theme.cardBorder
    property color highlight_background: Theme.hoverBg
    property color highlight_border: Theme.selectedBr
    property color item_background: Theme.cardBase
    property color item_border: Theme.cardBorder
    property var delegate_text: function (data) {
        return data;
    }

    function resolve_delegate_text(data) {
        return (typeof delegate_text === "function") ? delegate_text(data) : data;
    }

    onHoveredChanged: {
        if (hovered && enabled && typeof game !== "undefined")
            UiAudio.play_hover(game.audio_system);
    }
    onActivated: {
        if (typeof game !== "undefined")
            UiAudio.play_click(game.audio_system);
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
        border.color: {
            if (root.popup.visible || root.activeFocus)
                return StyleGuide.historical.bronze;
            if (root.hovered)
                return StyleGuide.palette.thumbBr;
            return root.border_color;
        }
        border.width: (root.popup.visible || root.activeFocus) ? 2 : 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(1, parent.radius - 1)
            opacity: 0.09
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "#FFFFFF"
                }

                GradientStop {
                    position: 0.5
                    color: "#FFFFFF"
                }

                GradientStop {
                    position: 1.0
                    color: "#000000"
                }
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: Math.max(3, parent.radius)
            anchors.right: parent.right
            anchors.rightMargin: Math.max(3, parent.radius)
            height: 1
            color: StyleGuide.palette.accentBright
            opacity: root.enabled ? 0.30 : 0.0
        }

        Behavior on border.color  {
            ColorAnimation {
                duration: 140
            }
        }

        Behavior on border.width  {
            NumberAnimation {
                duration: 100
            }
        }
    }

    indicator: Text {
        text: root.popup.visible ? "\u25B2" : "\u25BC"
        color: (root.hovered || root.popup.visible) ? StyleGuide.palette.accentBright : StyleGuide.palette.textSub
        font.pixelSize: 8
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingSmall + 1
        anchors.verticalCenter: parent.verticalCenter

        Behavior on color  {
            ColorAnimation {
                duration: 140
            }
        }
    }

    popup: Popup {
        y: root.height + 2
        width: root.width
        implicitHeight: contentItem.implicitHeight
        padding: 0
        onVisibleChanged: {
            if (visible && typeof game !== "undefined")
                UiAudio.play_click(game.audio_system);
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: root.popup_background
            border.color: StyleGuide.historical.bronzeDeep
            border.width: 1

            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.left: parent.left
                anchors.leftMargin: Math.max(3, parent.radius)
                anchors.right: parent.right
                anchors.rightMargin: Math.max(3, parent.radius)
                height: 1
                color: StyleGuide.palette.accentBright
                opacity: 0.25
            }
        }
    }

    delegate: ItemDelegate {
        width: root.width
        highlighted: root.highlightedIndex === index

        background: Rectangle {
            color: highlighted ? root.highlight_background : root.item_background
            border.color: highlighted ? root.highlight_border : root.item_border
            border.width: 1

            Rectangle {
                anchors.fill: parent
                opacity: highlighted ? 0.08 : 0.0
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: "#FFFFFF"
                    }

                    GradientStop {
                        position: 1.0
                        color: "#000000"
                    }
                }

                Behavior on opacity  {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }
        }

        contentItem: Text {
            text: root.resolve_delegate_text(modelData)
            color: highlighted ? StyleGuide.palette.textBright : root.text_color
            font.pointSize: root.text_pixel_size > 0 ? -1 : root.text_point_size
            font.pixelSize: root.text_pixel_size > 0 ? root.text_pixel_size : -1
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.spacingSmall

            Behavior on color  {
                ColorAnimation {
                    duration: 100
                }
            }
        }
    }
}

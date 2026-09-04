import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.Design 1.0 as Design

ColumnLayout {
    id: root

    property string heading: ""
    property color heading_color: Design.Theme.accent
    property string marker: Design.Icons.objective
    property var lines: []

    spacing: Design.Metrics.space4
    visible: root.lines && root.lines.length > 0

    RowLayout {
        Layout.fillWidth: true
        spacing: Design.Metrics.space8

        Rectangle {
            Layout.preferredWidth: Design.Metrics.space4
            Layout.preferredHeight: Design.Typography.subheading
            radius: width / 2
            color: root.heading_color
        }

        Text {
            Layout.fillWidth: true
            text: root.heading
            color: root.heading_color
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.label
            font.weight: Design.Typography.bold
            font.letterSpacing: Design.Typography.trackingNormal
        }

        Rectangle {
            Layout.preferredWidth: Design.Metrics.space24
            Layout.preferredHeight: Design.Metrics.borderThin
            color: root.heading_color
            opacity: 0.45
        }
    }

    Repeater {
        model: root.lines

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(Design.Metrics.minTouchTarget, order_text.implicitHeight + Design.Metrics.space12)
            radius: Design.Metrics.radiusSmall
            color: Qt.rgba(root.heading_color.r, root.heading_color.g, root.heading_color.b, Design.Theme.highContrast ? 0.14 : 0.07)
            border.width: Design.Metrics.borderThin
            border.color: Qt.rgba(root.heading_color.r, root.heading_color.g, root.heading_color.b, Design.Theme.highContrast ? 0.55 : 0.24)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Design.Metrics.space8
                anchors.rightMargin: Design.Metrics.space8
                spacing: Design.Metrics.space8

                Text {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Design.Metrics.iconSmall
                    text: root.marker
                    color: root.heading_color
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    font.weight: Design.Typography.bold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    id: order_text

                    Layout.fillWidth: true
                    text: modelData
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.body
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}

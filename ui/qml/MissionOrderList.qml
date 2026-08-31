import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

ColumnLayout {
    id: root

    property string heading: ""
    property color heading_color: Theme.accentBright
    property var lines: []

    spacing: Theme.spacingTiny
    visible: root.lines && root.lines.length > 0

    Label {
        text: root.heading
        color: root.heading_color
        font.pixelSize: Design.Typography.label
        font.bold: true
    }

    Repeater {
        model: root.lines

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Label {
                Layout.alignment: Qt.AlignTop
                text: "—"
                color: root.heading_color
                font.pixelSize: Design.Typography.body
            }

            Label {
                Layout.fillWidth: true
                text: modelData
                color: Theme.textSub
                font.pixelSize: Design.Typography.body
                wrapMode: Text.WordWrap
            }
        }
    }
}

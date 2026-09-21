import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Row {
    id: root

    property string difficulty_id: DifficultyCatalog.defaultId
    property var presets: []
    property bool show_summary: false
    property int icon_size: Design.Metrics.iconMedium

    readonly property var entry: DifficultyCatalog.entry(root.difficulty_id)
    readonly property string summary_text: DifficultyCatalog.summary_for(root.presets, root.difficulty_id)

    spacing: 6

    Design.IronVectorIcon {
        anchors.verticalCenter: parent.verticalCenter
        iconId: root.entry.icon
        accent: root.entry.accent
        width: root.icon_size
        height: root.icon_size
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            text: root.entry.name
            color: root.entry.accent
            font.pixelSize: Design.Typography.caption
            font.bold: true
        }

        Text {
            visible: root.show_summary && root.summary_text !== ""
            text: root.summary_text
            color: Design.Theme.textSecondary
            font.pixelSize: Design.Typography.caption
        }
    }
}

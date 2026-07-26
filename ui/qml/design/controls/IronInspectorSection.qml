import QtQuick 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property string title: ""
    property bool expanded: true
    default property alias sectionContent: contentColumn.data
    implicitHeight: sectionColumn.implicitHeight + Design.Metrics.space24
    Column {
        id: sectionColumn
        anchors.fill: parent
        spacing: Design.Metrics.space8
        IronButton {
            width: parent.width
            text: (root.expanded ? Design.Icons.disclosureOpen : Design.Icons.disclosureClosed) + "  " + root.title
            onClicked: root.expanded = !root.expanded
        }
        Column {
            id: contentColumn
            width: parent.width
            spacing: Design.Metrics.space8
            visible: root.expanded
        }
    }
}

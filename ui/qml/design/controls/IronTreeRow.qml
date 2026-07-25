import QtQuick 2.15
import ".." as Design

IronListRow {
    id: control

    property int depth: 0
    property bool expanded: false
    property bool hasChildren: false

    leftPadding: Design.Metrics.space8 + depth * Design.Metrics.space16
    text: (hasChildren ? (expanded ? Design.Icons.disclosureOpen : Design.Icons.disclosureClosed) + "  " : "") + control.Accessible.name
}

import QtQuick 2.15
import ".." as Design

IronListRow {
    property string state: "active"
    property string objectiveText: ""
    text: (state === "complete" ? "\u2713  " : state === "failed" ? "\u2715  " : "\u25C7  ") + objectiveText
}

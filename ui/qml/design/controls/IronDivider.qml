import QtQuick 2.15
import ".." as Design

Rectangle {
    property bool vertical: false
    implicitWidth: vertical ? 1 : 80
    implicitHeight: vertical ? 24 : 1
    color: Design.Theme.borderSubtle
}

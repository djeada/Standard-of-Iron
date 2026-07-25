import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property string faction: "neutral"
    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        gradient: Gradient {
            GradientStop { position: 0.0; color: Design.Theme.backgroundRaised }
            GradientStop { position: 1.0; color: Design.Theme.backgroundDeep }
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: Design.Metrics.space8
        color: "transparent"
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle
        opacity: 0.55
    }
}

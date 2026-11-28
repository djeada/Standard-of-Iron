import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    objectName: "boot_screen"

    color: "#0B0806"

    Image {
        anchors.fill: parent
        source: StyleGuide.backdrops[0]
        asynchronous: false
        fillMode: Image.PreserveAspectCrop
    }

    Rectangle {
        anchors.fill: parent
        color: "#660B0806"
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.max(40, parent.height * 0.1)
        text: qsTranslate("LoadScreen", "Loading...")
        color: Theme.textMain
        font.family: Design.Typography.titleFamily
        font.capitalization: Font.AllUppercase
        font.pixelSize: Design.Typography.hero
        font.weight: Design.Typography.bold
        font.hintingPreference: Design.Typography.titleHinting
        font.kerning: true
        font.letterSpacing: Design.Typography.trackingHero
        style: Text.Outline
        styleColor: "#120D09"
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }
}

pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property string family: "Noto Sans"
    readonly property string displayFamily: "Noto Serif"

    readonly property int caption: A11y.scaledFont(11)
    readonly property int label: A11y.scaledFont(13)
    readonly property int body: A11y.scaledFont(14)
    readonly property int heading: A11y.scaledFont(20)
    readonly property int title: A11y.scaledFont(28)
    readonly property int hero: A11y.scaledFont(40)

    readonly property int regular: Font.Normal
    readonly property int medium: Font.DemiBold
    readonly property int bold: Font.Bold

    readonly property real trackingWide: 1.5
    readonly property real trackingNormal: 0.0
}

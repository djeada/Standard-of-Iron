pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property string family: "Noto Sans"
    readonly property string displayFamily: "Noto Serif"
    readonly property int caption: 11
    readonly property int body: 14
    readonly property int label: 13
    readonly property int heading: 20
    readonly property int title: 28
    readonly property int hero: 40
    readonly property int regular: Font.Normal
    readonly property int medium: Font.DemiBold
    readonly property int bold: Font.Bold
}

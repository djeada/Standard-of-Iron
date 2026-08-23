pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property string family: "Noto Sans"
    readonly property string displayFamily: "Noto Serif"

    readonly property FontLoader brandFont: FontLoader {
        source: "qrc:/assets/fonts/StandardIronDisplay-Bold.ttf"
    }

    readonly property string titleFamily: (root.brandFont.status === FontLoader.Ready && root.brandFont.name.length > 0) ? root.brandFont.name : root.displayFamily

    readonly property int minimumSize: 12

    function scaled(px) {
        return Math.max(root.minimumSize, A11y.scaledFont(px));
    }

    function display(px) {
        return A11y.scaledFont(px);
    }

    readonly property int caption: scaled(13)
    readonly property int label: scaled(15)
    readonly property int body: scaled(16)
    readonly property int bodyLarge: scaled(19)
    readonly property int subheading: scaled(21)
    readonly property int heading: scaled(24)
    readonly property int title: display(32)
    readonly property int hero: display(40)

    readonly property int glyphSmall: display(28)
    readonly property int glyph: display(44)
    readonly property int glyphLarge: display(56)

    readonly property int regular: Font.Normal
    readonly property int medium: Font.DemiBold
    readonly property int bold: Font.Bold
    readonly property int titleHinting: Font.PreferVerticalHinting

    readonly property real trackingWide: 1.5
    readonly property real trackingTitle: 0.6
    readonly property real trackingHero: 1.0
    readonly property real trackingNormal: 0.0
}

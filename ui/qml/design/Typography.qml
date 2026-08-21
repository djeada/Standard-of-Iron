pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property string family: "Noto Sans"
    readonly property string displayFamily: "Noto Serif"

    // The brand face, for titles, headings, outcome screens and big numbers.
    // Deliberately separate from displayFamily rather than replacing it:
    // displayFamily still has to carry the symbol glyphs the interface draws as
    // icons (faction marks, command glyphs), and a caps-and-digits display face
    // has none of them. Anything asserted by tst_glyph_coverage stays on
    // family/displayFamily; titleFamily is for words the player reads.
    // Loaded from qrc rather than named, so the shipped file is what renders on
    // every machine. If it ever fails to load, fall back to the serif rather
    // than to nothing.
    readonly property FontLoader brandFont: FontLoader {
        source: "qrc:/assets/fonts/StandardIronDisplay-Bold.ttf"
    }

    // CAPS AND FIGURES ONLY. The face has no lowercase, and Qt falls back per
    // glyph, so binding this to mixed-case text renders half the word in one
    // typeface and half in another. Use it only where the text is digits, or
    // where `font.capitalization: Font.AllUppercase` is set alongside it.
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

    readonly property real trackingWide: 1.5
    readonly property real trackingTitle: 0.7
    readonly property real trackingHero: 1.2
    readonly property real trackingNormal: 0.0
}

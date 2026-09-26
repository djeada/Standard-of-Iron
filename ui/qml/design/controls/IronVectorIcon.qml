import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core
import ".." as Design

// Painted natively from Ui::IconArt. A JavaScript Canvas used to redraw every
// flattened point here, and it could not paint until its window had rendered,
// so icons (the difficulty cards first) appeared a beat after their panel.
Core.IconArtItem {
    tint: Design.Theme.textPrimary
    accent: Design.Theme.accent
    ink: Design.Theme.backgroundDeep
    edge: Design.Theme.parchment
    timber: "#8a5a32"
    quarry: "#8e8b86"
    ore: "#7f8a96"
    bullion: "#d9a441"

    implicitWidth: Design.Metrics.iconMedium
    implicitHeight: Design.Metrics.iconMedium
    visible: available
}

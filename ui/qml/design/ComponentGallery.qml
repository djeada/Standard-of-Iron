import QtQuick 2.15
import QtQuick.Controls 2.15
import "controls" as Controls
import "surfaces" as Surfaces
import "overlays" as Overlays

ScrollView {
    id: root

    contentWidth: availableWidth
    Column {
        width: root.availableWidth
        spacing: Metrics.space12
        padding: Metrics.space16
        Text { text: qsTr("Iron and Ember Components"); color: Theme.textPrimary; font.pixelSize: Typography.title }
        Row {
            spacing: Metrics.space8
            Controls.IronButton { text: qsTr("Primary"); tone: "primary" }
            Controls.IronButton { text: qsTr("Secondary") }
            Controls.IronButton { text: qsTr("Destructive"); tone: "destructive" }
            Controls.IronButton { text: qsTr("Disabled"); enabled: false; disabledReason: qsTr("Unavailable in this state") }
        }
        Row {
            spacing: Metrics.space8
            Controls.IronBadge { text: qsTr("Selected") }
            Controls.IronHotkeyLabel { text: "Ctrl+S" }
            Controls.IronResourceCounter { iconText: "\u25C8"; amount: 1250; trend: 12 }
        }
        Controls.IronSearchField { width: 300 }
        Controls.IronDropdown { width: 220; model: [qsTr("Balanced"), qsTr("Aggressive"), qsTr("Defensive")] }
        Controls.IronSlider { width: 300; value: 0.62 }
        Controls.IronProgressBar { width: 300; value: 0.72 }
        Controls.IronUnitCard { unitName: qsTr("Roman Spearmen \u00D7 24"); subtitle: qsTr("Defense formation"); health: 0.78 }
        Controls.IronObjectiveRow { width: 420; objectiveText: qsTr("Capture the eastern barracks") }
        Overlays.IronNotification { priority: "urgent"; message: qsTr("Commander under attack") }
    }
}

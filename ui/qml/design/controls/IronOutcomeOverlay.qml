import QtQuick 2.15
import ".." as Design

// The end-of-mission overlay: which outcome a match reached, what it is called,
// and whether the player is looking at the banner or the battle report.
// The screen owns none of that decision — it is handed a victory state and two
// campaign facts and works the rest out here, so "the campaign banner only
// appears after the last mission" is a rule that can be tested without running
// a match. The battle report itself is app-level and is slotted in as `detail`.
Item {
    id: root

    // "", "victory" or "defeat", straight from the victory service.
    property string victoryState: ""
    property bool isCampaignMission: false
    property bool campaignCompleted: false

    // Anything declared inside the overlay is treated as the battle report and
    // shown in place of the banner once the player asks for it.
    default property alias detail: detailHost.data

    property string factionId: ""
    property string primaryAction: qsTr("Battle Report")

    // The player asked for the report instead of the banner.
    property bool showingSummary: false
    // The player dismissed the overlay for this outcome; a new outcome revives
    // it. This is what stops a retry from reopening the banner it just closed.
    property bool manuallyHidden: false

    readonly property bool decided: root.victoryState !== ""

    // A campaign is only "won" when the run that just ended was a campaign
    // mission, it was a victory, and nothing is left to play.
    readonly property string outcomeKind: {
        if (root.victoryState !== "victory")
            return "defeat";
        return (root.isCampaignMission && root.campaignCompleted) ? "campaign" : "victory";
    }

    readonly property string headline: {
        switch (root.outcomeKind) {
        case "campaign":
            return qsTr("The Campaign is Won");
        case "victory":
            return qsTr("Victory Secured");
        }
        return qsTr("Army Broken");
    }

    readonly property string subtitle: {
        switch (root.outcomeKind) {
        case "campaign":
            return qsTr("Every mission has fallen to your standard.");
        case "victory":
            return qsTr("Enemy command has fallen.");
        }
        return qsTr("Your command has collapsed.");
    }

    signal reportRequested

    function reset() {
        root.showingSummary = false;
        root.manuallyHidden = false;
    }

    function forceHide() {
        root.showingSummary = false;
        root.manuallyHidden = true;
    }

    // Call when the victory state changes. A cleared state resets everything so
    // the next mission starts from a blank overlay; a fresh outcome un-hides it
    // even if the player dismissed the previous one.
    function onOutcomeChanged() {
        if (root.victoryState === "")
            root.reset();
        else
            root.manuallyHidden = false;
    }

    anchors.fill: parent
    visible: !root.manuallyHidden && root.decided
    z: 100

    onVictoryStateChanged: root.onOutcomeChanged()
    onVisibleChanged: {
        // Only the report is forgotten when the overlay goes away. Clearing the
        // dismissal here as well would immediately make the overlay visible
        // again -- a binding loop that quietly defeated forceHide().
        if (!visible)
            root.showingSummary = false;
    }

    Loader {
        id: bannerLoader

        objectName: "outcomeBanner"
        anchors.fill: parent
        active: !root.showingSummary
        visible: active

        sourceComponent: Design.OutcomeLayout {
            outcome: root.outcomeKind
            factionId: root.factionId
            headline: root.headline
            subtitle: root.subtitle
            primaryAction: root.primaryAction
            onPrimaryActivated: {
                root.showingSummary = true;
                root.reportRequested();
            }
        }
    }

    Item {
        id: detailHost

        objectName: "outcomeDetail"
        anchors.fill: parent
        visible: root.showingSummary
    }
}

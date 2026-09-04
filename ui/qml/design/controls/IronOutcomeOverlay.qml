import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property string victoryState: ""
    property bool isCampaignMission: false
    property bool campaignCompleted: false
    property bool isTutorial: false

    default property alias detail: detailHost.data

    property string factionId: ""

    property string outcomeReason: ""
    property string primaryAction: qsTr("Battle Report")

    property bool showingSummary: false

    property bool manuallyHidden: false

    property bool held: false

    readonly property bool decided: root.victoryState !== ""

    readonly property string outcomeKind: {
        if (root.victoryState === "spectator")
            return "spectator";
        if (root.victoryState !== "victory")
            return "defeat";
        if (root.isTutorial)
            return "training";
        return (root.isCampaignMission && root.campaignCompleted) ? "campaign" : "victory";
    }

    readonly property string headline: {
        switch (root.outcomeKind) {
        case "spectator":
            return qsTr("Battle Decided");
        case "campaign":
            return qsTr("The Campaign is Won");
        case "training":
            return qsTr("Training Complete");
        case "victory":
            return qsTr("Victory Secured");
        }
        return qsTr("Army Broken");
    }

    readonly property string subtitle: {
        if (root.outcomeKind === "defeat" && root.outcomeReason !== "")
            return root.outcomeReason;
        switch (root.outcomeKind) {
        case "spectator":
            return qsTr("One side is left holding the field.");
        case "campaign":
            return qsTr("Every mission has fallen to your standard.");
        case "training":
            return qsTr("You can run an army now. The Barcid Road is waiting.");
        case "victory":
            return root.isCampaignMission ? qsTr("Every order carried out.") : qsTr("Enemy command has fallen.");
        }
        return qsTr("Your command has collapsed.");
    }

    property string secondaryAction: root.outcomeKind === "training" ? qsTr("March the Campaign") : ""

    signal reportRequested
    signal secondaryRequested

    function reset() {
        root.showingSummary = false;
        root.manuallyHidden = false;
    }

    function forceHide() {
        root.showingSummary = false;
        root.manuallyHidden = true;
    }

    function onOutcomeChanged() {
        if (root.victoryState === "")
            root.reset();
        else
            root.manuallyHidden = false;
    }

    anchors.fill: parent
    visible: !root.manuallyHidden && root.decided && !root.held
    z: 100

    onVictoryStateChanged: root.onOutcomeChanged()
    onVisibleChanged: {
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
            secondaryAction: root.secondaryAction
            onPrimaryActivated: {
                root.showingSummary = true;
                root.reportRequested();
            }
            onSecondaryActivated: root.secondaryRequested()
        }
    }

    Item {
        id: detailHost

        objectName: "outcomeDetail"
        anchors.fill: parent
        visible: root.showingSummary
    }
}

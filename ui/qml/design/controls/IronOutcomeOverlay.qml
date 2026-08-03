import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property string victoryState: ""
    property bool isCampaignMission: false
    property bool campaignCompleted: false

    default property alias detail: detailHost.data

    property string factionId: ""
    property string primaryAction: qsTr("Battle Report")

    property bool showingSummary: false

    property bool manuallyHidden: false

    readonly property bool decided: root.victoryState !== ""

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

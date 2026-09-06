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
    property bool reportTransitioning: false

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
        reportTransitionTimer.stop();
        root.reportTransitioning = false;
        root.showingSummary = false;
        root.manuallyHidden = false;
    }

    function forceHide() {
        reportTransitionTimer.stop();
        root.reportTransitioning = false;
        root.showingSummary = false;
        root.manuallyHidden = true;
    }

    function request_report() {
        if (root.reportTransitioning || root.showingSummary)
            return;
        root.reportTransitioning = true;
        reportTransitionTimer.restart();
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
        if (!visible) {
            reportTransitionTimer.stop();
            root.reportTransitioning = false;
            root.showingSummary = false;
        }
    }

    Timer {
        id: reportTransitionTimer

        interval: 16
        repeat: false
        onTriggered: {
            if (!root.visible || !root.decided || root.manuallyHidden) {
                root.reportTransitioning = false;
                return;
            }
            root.showingSummary = true;
            root.reportRequested();
            root.reportTransitioning = false;
        }
    }

    Loader {
        id: bannerLoader

        objectName: "outcomeBanner"
        anchors.fill: parent
        active: !root.showingSummary
        visible: active
        enabled: !root.reportTransitioning

        sourceComponent: Design.OutcomeLayout {
            outcome: root.outcomeKind
            factionId: root.factionId
            headline: root.headline
            subtitle: root.subtitle
            primaryAction: root.primaryAction
            secondaryAction: root.secondaryAction
            onPrimaryActivated: root.request_report()
            onSecondaryActivated: root.secondaryRequested()
        }
    }

    Item {
        id: detailHost

        objectName: "outcomeDetail"
        anchors.fill: parent
        visible: root.showingSummary
    }

    Rectangle {
        anchors.fill: parent
        visible: root.reportTransitioning
        z: 1
        color: Qt.rgba(Design.Theme.backgroundDeep.r, Design.Theme.backgroundDeep.g, Design.Theme.backgroundDeep.b, 0.52)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.centerIn: parent
            width: transitionLabel.implicitWidth + Design.Metrics.space24 * 2
            height: Design.Metrics.controlHeight
            radius: Design.Metrics.radiusSmall
            color: Design.Theme.panelIron
            border.width: Design.Metrics.borderThin
            border.color: Design.Theme.accent

            Text {
                id: transitionLabel

                anchors.centerIn: parent
                text: root.primaryAction + "…"
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.medium
            }
        }
    }
}

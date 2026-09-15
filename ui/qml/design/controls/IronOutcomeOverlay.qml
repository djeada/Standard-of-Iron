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
    property string retryAction: ""
    property string menuAction: ""
    property string dismissAction: qsTr("Look at the field")

    property bool collapsed: false
    property int stripTopMargin: Design.Metrics.space24 * 3

    readonly property Item strip: collapsedStrip

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
    signal retryRequested
    signal menuRequested

    function reset() {
        reportTransitionTimer.stop();
        root.reportTransitioning = false;
        root.showingSummary = false;
        root.manuallyHidden = false;
        root.collapsed = false;
    }

    function forceHide() {
        reportTransitionTimer.stop();
        root.reportTransitioning = false;
        root.showingSummary = false;
        root.manuallyHidden = true;
        root.collapsed = false;
    }

    function collapse() {
        if (!root.decided)
            return;
        root.collapsed = true;
    }

    function expand() {
        root.collapsed = false;
    }

    function request_report() {
        if (root.reportTransitioning || root.showingSummary)
            return;
        root.reportTransitioning = true;
        reportTransitionTimer.restart();
    }

    function onOutcomeChanged() {
        if (root.victoryState === "") {
            root.reset();
        } else {
            root.manuallyHidden = false;
            root.collapsed = false;
        }
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
        active: !root.showingSummary && !root.collapsed
        visible: active
        enabled: !root.reportTransitioning

        sourceComponent: Design.OutcomeLayout {
            outcome: root.outcomeKind
            factionId: root.factionId
            headline: root.headline
            subtitle: root.subtitle
            primaryAction: root.primaryAction
            secondaryAction: root.secondaryAction
            retryAction: root.retryAction
            menuAction: root.menuAction
            dismissAction: root.dismissAction
            onPrimaryActivated: root.request_report()
            onSecondaryActivated: root.secondaryRequested()
            onRetryActivated: root.retryRequested()
            onMenuActivated: root.menuRequested()
            onDismissActivated: root.collapse()
        }
    }

    Design.IronPanel {
        id: collapsedStrip

        objectName: "outcomeStrip"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.stripTopMargin
        visible: root.collapsed && !root.showingSummary
        raised: true
        translucent: true
        contentPadding: Design.Metrics.space8
        implicitWidth: Math.min(parent.width - Design.Metrics.space16 * 2, stripRow.implicitWidth + Design.Metrics.space8 * 2)
        implicitHeight: stripRow.implicitHeight + Design.Metrics.space8 * 2
        border.color: root.outcomeKind === "defeat" ? Design.Theme.danger : Design.Theme.success
        accessibleName: root.headline

        clip: true

        Row {
            id: stripRow

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Design.Metrics.space8

            Text {
                height: Design.Metrics.controlHeight
                verticalAlignment: Text.AlignVCenter
                text: root.headline
                color: root.outcomeKind === "defeat" ? Design.Theme.danger : Design.Theme.success
                font.family: Design.Typography.titleFamily
                font.hintingPreference: Design.Typography.titleHinting
                font.kerning: true
                font.capitalization: Font.AllUppercase
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.bold
                font.letterSpacing: Design.Typography.trackingWide
            }

            Design.IronButton {
                objectName: "outcomeStripReport"
                text: root.primaryAction
                tone: "primary"
                onClicked: root.request_report()
            }

            Design.IronButton {
                objectName: "outcomeStripRetry"
                visible: root.retryAction !== ""
                text: root.retryAction
                onClicked: root.retryRequested()
            }

            Design.IronButton {
                objectName: "outcomeStripMenu"
                visible: root.menuAction !== ""
                text: root.menuAction
                onClicked: root.menuRequested()
            }

            Design.IronIconButton {
                iconText: Design.Icons.disclosureOpen
                tooltip: qsTr("Show the verdict again")
                onClicked: root.expand()
            }
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

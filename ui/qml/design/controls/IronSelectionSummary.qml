import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import ".." as Design

Design.IronPanel {
    id: root

    property var model: null

    property var groups: []
    property int unitCount: 0

    property int detailCap: 12

    property var inspected: null

    readonly property bool inspecting: unitCount <= 0 && !!inspected && inspected.valid === true

    readonly property bool empty: unitCount <= 0
    readonly property bool singleUnit: unitCount === 1
    readonly property bool squad: unitCount > 1 && unitCount <= detailCap
    readonly property bool army: unitCount > detailCap
    readonly property bool groupedSquad: squad && groups.length >= 4

    signal unitActivated(var unitId)
    signal groupActivated(string unitType)
    signal profileRequested(string unitType, string nation)

    property var profileLookup: null

    readonly property string focusTypeKey: root.inspecting ? (root.inspected.typeKey || "") : root.groups.length > 0 ? (root.groups[0].typeKey || "") : ""
    readonly property string focusNation: root.inspecting ? (root.inspected.nation || "") : root.groups.length > 0 ? (root.groups[0].nation || "") : ""

    readonly property var focusProfile: {
        if (!root.profileLookup || root.focusTypeKey === "")
            return null;
        var found = root.profileLookup(root.focusTypeKey, root.focusNation);
        return found && found.valid === true ? found : null;
    }

    readonly property bool canShowProfile: !!root.focusProfile

    readonly property string focusTypeName: root.inspecting ? (root.inspected.name || "") : root.groups.length > 0 ? (root.groups[0].name || "") : ""

    readonly property bool profileCoversWholeSelection: root.inspecting || root.groups.length <= 1

    readonly property int soldierCount: {
        var total = 0;
        for (var i = 0; i < root.groups.length; ++i)
            total += root.soldiersOf(root.groups[i]);
        return total;
    }

    readonly property int soldierMax: {
        var total = 0;
        for (var i = 0; i < root.groups.length; ++i)
            total += root.maxSoldiersOf(root.groups[i]);
        return total;
    }

    accessibleName: qsTr("Selected units")
    Accessible.description: header.text

    function soldiersOf(entry) {
        if (!entry)
            return 0;
        var value = entry.soldiers;
        return value === undefined || value === null ? 0 : value;
    }

    function maxSoldiersOf(entry) {
        if (!entry)
            return 0;
        var value = entry.maxSoldiers !== undefined ? entry.maxSoldiers : entry.max_soldiers;
        return value === undefined || value === null ? 0 : value;
    }

    function hasSoldierCount(entry) {
        return root.maxSoldiersOf(entry) > 1;
    }

    function strengthText(entry) {
        return qsTr("%1 / %2").arg(root.soldiersOf(entry)).arg(root.maxSoldiersOf(entry));
    }

    function strengthTextCompact(entry) {
        return qsTr("%1/%2").arg(root.soldiersOf(entry)).arg(root.maxSoldiersOf(entry));
    }

    function healthColor(ratio) {
        if (ratio > 0.6)
            return Design.Theme.success;
        if (ratio > 0.3)
            return Design.Theme.warning;
        return Design.Theme.danger;
    }

    function iconFor(typeKey, nation, name) {
        return Design.Icons.unit(typeKey !== "" ? typeKey : Design.Icons.typeKeyFromName(name), nation);
    }

    function groupCanRun(group) {
        return group.canRun === undefined || group.canRun;
    }

    function groupActivity(group) {
        return group && group.activity ? group.activity : Design.ActivityIcons.defaultActivity;
    }

    function groupActivityState(group) {
        return group && group.activityState ? group.activityState : Design.ActivityIcons.defaultState;
    }

    function inspectHeader() {
        if (!root.inspecting)
            return "";
        if (root.inspected.isOwn)
            return root.inspected.isBuilding ? qsTr("YOUR BUILDING") : qsTr("YOUR UNIT");
        if (root.inspected.isEnemy)
            return root.inspected.isBuilding ? qsTr("ENEMY BUILDING") : qsTr("ENEMY UNIT");
        return root.inspected.isBuilding ? qsTr("BUILDING") : qsTr("UNIT");
    }

    function focusTone(info) {
        if (!info)
            return Design.Theme.accent;
        if (info.isEnemy)
            return Design.Theme.danger;
        if (info.isOwn)
            return Design.Theme.accent;
        return Design.Theme.textSecondary;
    }

    function focusHealthColor(info) {
        if (!info)
            return Design.Theme.success;
        if (info.isEnemy)
            return Design.Theme.danger;
        return root.healthColor(info.healthRatio);
    }

    function attackSummary(info) {
        if (!info)
            return "";
        if (info.isEnemy) {
            if (info.attackedByLocal > 0)
                return qsTr("Under attack by %n of your units", "", info.attackedByLocal);
            return qsTr("Not engaged by your army");
        }
        if (info.attackersIncoming > 0)
            return qsTr("Under attack by %n enemies", "", info.attackersIncoming);
        return qsTr("Not under attack");
    }

    implicitWidth: Design.Metrics.space24 * 10
    implicitHeight: body.implicitHeight + root.contentPadding * 2

    contentPadding: Design.Metrics.panelPaddingCompact

    Flickable {
        id: bodyView

        objectName: "selectionScrollView"

        anchors.fill: parent
        contentWidth: width
        contentHeight: body.implicitHeight
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Design.IronScrollBar {
            objectName: "selectionScrollBar"
        }

        Column {
            id: body

            width: bodyView.width - Design.Metrics.scrollBarThickness - Design.Metrics.space4
            spacing: Design.Metrics.space8

            Item {
                width: parent.width
                height: Math.max(headerBlock.implicitHeight, selectionCountBadge.implicitHeight)

                Column {
                    id: headerBlock

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Text {
                        id: header

                        text: root.inspecting ? root.inspectHeader() : root.empty ? qsTr("SELECTION") : root.singleUnit ? qsTr("SELECTED UNIT") : qsTr("SELECTED FORCE")
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Text {
                        visible: !root.empty || root.inspecting
                        objectName: "selectionSubtitle"
                        text: root.inspecting ? root.inspected.name : root.singleUnit && root.groups.length > 0 ? root.groups[0].name : qsTr("%1 soldiers ready").arg(root.soldierMax > 0 ? root.soldierCount : root.unitCount)
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.displayFamily
                        font.pixelSize: Design.Typography.label
                        font.weight: Design.Typography.bold
                    }
                }

                Design.IronIconButton {
                    id: profileButton

                    objectName: "selectionProfileButton"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.canShowProfile
                    iconText: Design.Icons.capture
                    tooltip: qsTr("Show what this unit is for")
                    accessibleName: qsTr("Unit details")
                    onClicked: root.profileRequested(root.focusTypeKey, root.focusNation)
                }

                Design.IronBadge {
                    id: selectionCountBadge

                    anchors.right: profileButton.visible ? profileButton.left : parent.right
                    anchors.rightMargin: profileButton.visible ? Design.Metrics.space8 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.empty || root.inspecting
                    tone: root.inspecting ? root.focusTone(root.inspected) : Design.Theme.accent
                    text: root.inspecting ? (root.inspected.isEnemy ? qsTr("Enemy") : root.inspected.isOwn ? qsTr("Yours") : qsTr("Neutral")) : root.singleUnit ? qsTr("1 unit") : root.groups.length === 1 ? qsTr("%1 units").arg(root.unitCount) : qsTr("%1 units  ·  %2 types").arg(root.unitCount).arg(root.groups.length)
                }
            }

            Loader {
                width: parent.width
                active: root.empty && !root.inspecting
                visible: active
                sourceComponent: emptyView
            }

            Loader {
                width: parent.width
                active: root.inspecting
                visible: active
                sourceComponent: inspectView
            }

            Loader {
                width: parent.width
                active: root.singleUnit && root.groups.length > 0
                visible: active
                sourceComponent: singleUnitView
            }

            Loader {
                width: parent.width
                active: root.squad && !root.groupedSquad
                visible: active
                sourceComponent: chipView
            }

            Loader {
                width: parent.width
                active: root.army || root.groupedSquad
                visible: active
                sourceComponent: rosterView
            }

            Row {
                id: statStrip

                objectName: "selectionStatStrip"
                width: parent.width
                spacing: Design.Metrics.space12
                visible: root.canShowProfile && (!root.empty || root.inspecting)

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.profileCoversWholeSelection && root.focusTypeName !== ""
                    text: root.focusTypeName
                    color: Design.Theme.accent
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, statStrip.width / 2)
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.medium
                }

                Repeater {
                    model: root.canShowProfile ? [{
                            "label": qsTr("ATK"),
                            "value": String(root.focusProfile.attack_damage)
                        }, {
                            "label": qsTr("RNG"),
                            "value": Number(root.focusProfile.attack_range).toFixed(1)
                        }, {
                            "label": qsTr("SPD"),
                            "value": Number(root.focusProfile.speed).toFixed(1)
                        }] : []

                    delegate: Row {
                        required property var modelData

                        spacing: Design.Metrics.space4

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.label
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.medium
                            font.letterSpacing: Design.Typography.trackingWide
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.value
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }
                    }
                }
            }
        }
    }

    Component {
        id: inspectView

        Rectangle {
            id: inspectCard

            objectName: "selectionInspectCard"
            width: parent.width
            height: Design.Metrics.space24 * 4 + (root.hasSoldierCount(root.inspected) ? Design.Metrics.space16 : 0)
            radius: Design.Metrics.radiusMedium
            color: Design.Theme.backgroundDeep
            border.width: Design.Metrics.borderThin
            border.color: root.focusTone(root.inspected)

            Rectangle {
                id: inspectPortrait

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: Design.Metrics.space8
                width: height
                radius: Design.Metrics.radiusSmall
                color: Design.Theme.panelLeather
                border.width: Design.Metrics.borderThin
                border.color: root.focusTone(root.inspected)

                Image {
                    anchors.fill: parent
                    anchors.margins: Design.Metrics.space4
                    fillMode: Image.PreserveAspectFit
                    source: root.iconFor(root.inspected.typeKey, root.inspected.nation, root.inspected.name)
                    smooth: true
                    mipmap: true
                }
            }

            Column {
                anchors.left: inspectPortrait.right
                anchors.leftMargin: Design.Metrics.space12
                anchors.right: parent.right
                anchors.rightMargin: Design.Metrics.space12
                anchors.verticalCenter: parent.verticalCenter
                spacing: Design.Metrics.space4

                Design.IronActivityIcon {
                    objectName: "inspectActivity"
                    width: parent.width
                    activity: root.inspected.activity || Design.ActivityIcons.defaultActivity
                    state_id: root.inspected.activityState || Design.ActivityIcons.defaultState
                    showLabel: true
                    iconScale: 0.85
                }

                Column {
                    width: parent.width
                    spacing: 2

                    RowLayout {
                        width: parent.width
                        spacing: Design.Metrics.space8

                        Text {
                            objectName: "inspectHealthLabel"
                            text: qsTr("HEALTH")
                            color: Design.Theme.textPrimary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            objectName: "inspectHealthValue"
                            text: qsTr("%1 / %2").arg(root.inspected.health).arg(root.inspected.maxHealth)
                            color: root.focusHealthColor(root.inspected)
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.weight: Design.Typography.bold
                        }
                    }

                    Design.IronProgressBar {
                        objectName: "inspectHealthBar"
                        width: parent.width
                        height: Design.Metrics.space12
                        value: root.inspected.healthRatio
                        fillColor: root.focusHealthColor(root.inspected)
                    }

                    Text {
                        objectName: "inspectSoldiersValue"
                        width: parent.width
                        visible: root.hasSoldierCount(root.inspected)
                        text: qsTr("%1 soldiers").arg(root.strengthText(root.inspected))
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        elide: Text.ElideRight
                    }
                }

                Text {
                    objectName: "inspectAttackSummary"
                    width: parent.width
                    text: root.attackSummary(root.inspected)
                    color: root.inspected.isEnemy && root.inspected.attackedByLocal > 0 ? Design.Theme.warning : Design.Theme.textSecondary
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    elide: Text.ElideRight
                }
            }
        }
    }

    Component {
        id: emptyView

        Item {
            id: emptyRow

            width: parent.width
            height: Math.max(emptyGlyph.implicitHeight, emptyHint.implicitHeight)

            Text {
                id: emptyGlyph

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: Design.Icons.commander
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.subheading
            }

            Text {
                id: emptyHint

                anchors.left: emptyGlyph.right
                anchors.leftMargin: Design.Metrics.space8
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Drag a box over your troops, or click a unit")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    Component {
        id: singleUnitView

        Rectangle {
            id: singleUnitCard

            width: parent.width
            height: Design.Metrics.space24 * (root.groupCanRun(root.groups[0]) ? 4 : 3) + Design.Metrics.space8
            radius: Design.Metrics.radiusMedium
            color: Design.Theme.backgroundDeep
            border.width: Design.Metrics.borderThin
            border.color: Design.Theme.borderSubtle

            Rectangle {
                id: singlePortrait

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: Design.Metrics.space8
                width: height
                radius: Design.Metrics.radiusSmall
                color: Design.Theme.panelLeather
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.borderStrong

                Image {
                    anchors.fill: parent
                    anchors.margins: Design.Metrics.space4
                    fillMode: Image.PreserveAspectFit
                    source: root.iconFor(root.groups[0].typeKey, root.groups[0].nation, root.groups[0].name)
                    smooth: true
                    mipmap: true
                }
            }

            Column {
                anchors.left: singlePortrait.right
                anchors.leftMargin: Design.Metrics.space12
                anchors.right: parent.right
                anchors.rightMargin: Design.Metrics.space12
                anchors.verticalCenter: parent.verticalCenter
                spacing: Design.Metrics.space4

                Design.IronActivityIcon {
                    objectName: "selectionActivity"
                    width: parent.width
                    activity: root.groupActivity(root.groups[0])
                    state_id: root.groupActivityState(root.groups[0])
                    showLabel: true
                    iconScale: 0.85
                }

                Column {
                    width: parent.width
                    spacing: Design.Metrics.space2

                    Column {
                        width: parent.width
                        spacing: 1

                        RowLayout {
                            width: parent.width
                            spacing: Design.Metrics.space8

                            Text {
                                objectName: "selectionHealthLabel"
                                text: root.hasSoldierCount(root.groups[0]) ? qsTr("SOLDIERS") : qsTr("HEALTH")
                                color: Design.Theme.textPrimary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                objectName: "selectionHealthValue"
                                text: root.hasSoldierCount(root.groups[0]) ? root.strengthText(root.groups[0]) : qsTr("%1%").arg(Math.round(root.groups[0].health * 100))
                                color: root.healthColor(root.groups[0].health)
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                            }
                        }

                        Design.IronProgressBar {
                            id: healthBar

                            objectName: "selectionHealthBar"
                            width: parent.width
                            height: Design.Metrics.space8
                            value: root.groups.length > 0 && root.groups[0].health !== undefined ? root.groups[0].health : 0
                            fillColor: root.healthColor(value)
                        }
                    }

                    Column {
                        objectName: "selectionStaminaSection"
                        width: parent.width
                        spacing: 1
                        opacity: 0.7
                        visible: root.groupCanRun(root.groups[0])

                        RowLayout {
                            width: parent.width
                            spacing: Design.Metrics.space8

                            Text {
                                objectName: "selectionStaminaLabel"
                                text: qsTr("STAMINA")
                                color: Design.Theme.textSecondary
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                objectName: "selectionStaminaValue"
                                text: qsTr("%1%").arg(Math.round((root.groups[0].stamina !== undefined ? root.groups[0].stamina : 1.0) * 100))
                                color: Design.Theme.success
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                            }
                        }

                        Design.IronProgressBar {
                            id: staminaBar

                            objectName: "selectionStaminaBar"
                            width: parent.width
                            height: Design.Metrics.space4
                            value: root.groups[0].stamina !== undefined ? root.groups[0].stamina : 1.0
                            fillColor: Design.Theme.success
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.ArrowCursor
                onClicked: function (mouse) {
                    mouse.accepted = true;
                }
            }
        }
    }

    Component {
        id: chipView

        Flow {
            spacing: Design.Metrics.space4

            Repeater {
                model: root.model

                delegate: Item {
                    id: chip

                    required property var model

                    width: Design.Metrics.space24 * 2
                    height: Design.Metrics.space24 * 2

                    readonly property var row: (chip.model !== undefined && chip.model !== null && chip.model.name !== undefined) ? chip.model : ({
                            "name": "",
                            "unit_type": "",
                            "nation": "",
                            "unit_id": "",
                            "health_ratio": 0,
                            "activity": "idle",
                            "activity_state": ""
                        })
                    readonly property bool showsStrength: root.hasSoldierCount(chip.row)
                    readonly property string strength: chip.showsStrength ? root.strengthTextCompact(chip.row) : ""

                    Accessible.role: Accessible.Button
                    Accessible.name: chip.row.name
                    Accessible.description: (chip.showsStrength ? chip.strength + qsTr(" soldiers — ") : Math.round(chip.row.health_ratio * 100) + "% — ") + Design.ActivityIcons.summary(chip.row.activity, chip.row.activity_state)

                    Rectangle {
                        anchors.fill: parent
                        radius: Design.Metrics.radiusSmall
                        color: chipMouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundDeep
                        border.width: chipMouse.containsMouse ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                        border.color: chipMouse.containsMouse ? Design.Theme.selection : Design.Theme.borderSubtle
                    }

                    Image {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: chip.showsStrength ? -Design.Metrics.space8 : -Design.Metrics.space2
                        width: Design.Metrics.iconMedium
                        height: width
                        fillMode: Image.PreserveAspectFit
                        source: root.iconFor(chip.row.unit_type, chip.row.nation, chip.row.name)
                        smooth: true
                        mipmap: true
                    }

                    Text {
                        objectName: "selectionChipStrength_" + chip.row.unit_id

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Design.Metrics.space8
                        visible: chip.showsStrength
                        text: chip.strength
                        color: root.healthColor(chip.row.health_ratio)
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                    }

                    Item {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Design.Metrics.space4
                        height: Design.Metrics.space4

                        Rectangle {
                            objectName: "selectionChipHealth_" + chip.row.unit_id

                            width: parent.width * chip.row.health_ratio
                            height: parent.height
                            radius: height / 2
                            color: root.healthColor(chip.row.health_ratio)
                        }
                    }

                    Design.IronVectorIcon {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: Design.Metrics.space2
                        width: Design.Metrics.iconSmall
                        height: width
                        visible: chip.row.activity !== undefined && chip.row.activity !== "idle"
                        iconId: Design.ActivityIcons.iconFor(chip.row.activity)
                        monochrome: true
                        tint: chip.row.activity_state === "unavailable" ? Design.Theme.danger : chip.row.activity_state === "interrupted" ? Design.Theme.warning : Design.Theme.accent
                    }

                    MouseArea {
                        id: chipMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Design.UiSound.activate();
                            root.unitActivated(chip.row.unit_id);
                        }
                        onContainsMouseChanged: {
                            if (containsMouse)
                                Design.UiSound.hover();
                        }
                    }

                    ToolTip.visible: chipMouse.containsMouse
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: chip.row.name + " — " + (chip.showsStrength ? qsTr("%1 soldiers").arg(chip.strength) : Math.round(chip.row.health_ratio * 100) + "%") + "\n" + Design.ActivityIcons.summary(chip.row.activity, chip.row.activity_state)
                }
            }
        }
    }

    Component {
        id: rosterView

        Flow {
            id: groupFlow

            width: parent.width
            spacing: Design.Metrics.space8

            readonly property int minCardWidth: Design.Metrics.space24 * 3 + Design.Metrics.space8
            readonly property int fitColumns: Math.max(1, Math.floor((width + spacing) / (minCardWidth + spacing)))
            readonly property int columnCount: Math.max(1, Math.min(fitColumns, root.groups.length))
            readonly property real cardWidth: Math.floor((width - spacing * (columnCount - 1)) / columnCount)
            readonly property bool compactCards: cardWidth < Design.Metrics.space24 * 5
            readonly property real cardHeight: compactCards ? Design.Metrics.space24 * 2 + Design.Metrics.space8 : Design.Metrics.space24 * 3

            Repeater {
                model: root.groups

                delegate: Rectangle {
                    id: groupCard

                    required property var modelData

                    readonly property var row: (groupCard.modelData !== undefined && groupCard.modelData !== null && groupCard.modelData.typeKey !== undefined) ? groupCard.modelData : ({
                            "typeKey": "",
                            "name": "",
                            "nation": "",
                            "health": 0,
                            "woundedCount": 0,
                            "count": 0,
                            "mixedActivity": false
                        })

                    width: groupFlow.cardWidth
                    height: groupFlow.cardHeight
                    radius: Design.Metrics.radiusMedium
                    color: groupMouse.containsMouse ? Design.Theme.panelLeather : Design.Theme.backgroundDeep
                    border.width: groupMouse.containsMouse ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                    border.color: groupMouse.containsMouse ? Design.Theme.selection : groupCard.row.woundedCount > 0 ? root.healthColor(groupCard.row.health) : Design.Theme.borderSubtle

                    readonly property string activityText: Design.ActivityIcons.summary(root.groupActivity(groupCard.row), root.groupActivityState(groupCard.row)) + (groupCard.row.mixedActivity ? qsTr(" (mixed)") : "")

                    readonly property bool showsStrength: root.hasSoldierCount(groupCard.row)
                    readonly property string strength: groupCard.showsStrength ? root.strengthTextCompact(groupCard.row) : ""

                    Accessible.role: Accessible.Button
                    Accessible.name: groupCard.row.name + " ×" + groupCard.row.count
                    Accessible.description: (groupCard.showsStrength ? groupCard.strength + qsTr(" soldiers — ") : Math.round(groupCard.row.health * 100) + "% — ") + groupCard.activityText

                    ToolTip.visible: groupMouse.containsMouse
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    ToolTip.text: groupCard.row.name + " ×" + groupCard.row.count + (groupCard.showsStrength ? " — " + qsTr("%1 soldiers").arg(groupCard.strength) : "") + "\n" + groupCard.activityText

                    Rectangle {
                        id: portraitFrame

                        anchors.left: parent.left
                        anchors.leftMargin: groupFlow.compactCards ? Math.round((groupFlow.cardWidth - portraitFrame.width) / 2) : Design.Metrics.space8
                        anchors.top: parent.top
                        anchors.topMargin: groupFlow.compactCards ? Design.Metrics.space4 : Math.round((groupFlow.cardHeight - portraitFrame.height) / 2)
                        width: Math.min(Design.Metrics.space24 * 2, groupFlow.cardHeight - Design.Metrics.space12)
                        height: width
                        radius: Design.Metrics.radiusSmall
                        color: Design.Theme.panelLeather
                        border.width: Design.Metrics.borderThin
                        border.color: Design.Theme.borderStrong

                        Image {
                            anchors.fill: parent
                            anchors.margins: Design.Metrics.space4
                            fillMode: Image.PreserveAspectFit
                            source: root.iconFor(groupCard.row.typeKey, groupCard.row.nation, groupCard.row.name)
                            smooth: true
                            mipmap: true
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: -Design.Metrics.space4
                            anchors.top: parent.top
                            anchors.topMargin: -Design.Metrics.space4
                            width: Math.max(Design.Metrics.space16, groupCount.implicitWidth + Design.Metrics.space8)
                            height: Design.Metrics.space16
                            radius: height / 2
                            color: Design.Theme.panelLeather
                            border.width: Design.Metrics.borderThin
                            border.color: Design.Theme.accent

                            Text {
                                id: groupCount

                                anchors.centerIn: parent
                                text: "×" + groupCard.row.count
                                color: Design.Theme.accent
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                            }
                        }

                        Design.IronVectorIcon {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: -Design.Metrics.space2
                            width: Design.Metrics.iconSmall
                            height: width
                            visible: root.groupActivity(groupCard.row) !== "idle"
                            iconId: Design.ActivityIcons.iconFor(root.groupActivity(groupCard.row))
                            monochrome: true
                            tint: root.groupActivityState(groupCard.row) === "unavailable" ? Design.Theme.danger : root.groupActivityState(groupCard.row) === "interrupted" ? Design.Theme.warning : Design.Theme.accent
                        }
                    }

                    Text {
                        id: groupName

                        anchors.left: portraitFrame.right
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space8
                        anchors.top: parent.top
                        anchors.topMargin: Design.Metrics.space8
                        visible: !groupFlow.compactCards
                        text: groupCard.row.name
                        color: Design.Theme.textPrimary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.medium
                        elide: Text.ElideRight
                    }

                    Design.IronProgressBar {
                        id: groupHealth

                        objectName: "selectionGroupHealthBar_" + groupCard.row.typeKey

                        anchors.left: groupFlow.compactCards ? parent.left : portraitFrame.right
                        anchors.leftMargin: groupFlow.compactCards ? Design.Metrics.space8 : Design.Metrics.space8
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space8
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: groupFlow.compactCards ? Design.Metrics.space4 : Design.Metrics.space8
                        value: groupCard.row.health
                        fillColor: root.healthColor(value)
                    }

                    Text {
                        objectName: "selectionGroupStrength_" + groupCard.row.typeKey

                        anchors.left: portraitFrame.right
                        anchors.leftMargin: Design.Metrics.space8
                        anchors.right: parent.right
                        anchors.rightMargin: Design.Metrics.space8
                        anchors.bottom: groupHealth.top
                        anchors.bottomMargin: Design.Metrics.space2
                        visible: !groupFlow.compactCards && groupFlow.cardHeight >= Design.Metrics.space24 * 3 && (groupCard.showsStrength || groupCard.row.woundedCount > 0)
                        text: groupCard.showsStrength ? groupCard.strength : qsTr("%1 wounded").arg(groupCard.row.woundedCount)
                        color: groupCard.showsStrength ? root.healthColor(groupCard.row.health) : Design.Theme.warning
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: groupCard.showsStrength ? Design.Typography.bold : Design.Typography.regular
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: groupMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function (mouse) {
                            Design.UiSound.activate();
                            root.groupActivated(groupCard.row.typeKey);
                            mouse.accepted = true;
                        }
                        onContainsMouseChanged: {
                            if (containsMouse)
                                Design.UiSound.hover();
                        }
                    }
                }
            }
        }
    }
}

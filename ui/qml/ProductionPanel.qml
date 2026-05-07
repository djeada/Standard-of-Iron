import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: productionPanel

    property int selectionTick: 0
    property var gameInstance: null
    readonly property var hs: StyleGuide.historical

    signal recruitUnit(string unitType)
    signal rallyModeToggled()
    signal buildTower()
    signal builderConstruction(string itemType)

    function defaultProductionState() {
        return {
            "has_barracks": false,
            "produced_count": 0,
            "max_units": 0,
            "queue_size": 0,
            "in_progress": false,
            "production_queue": [],
            "product_type": "",
            "villager_cost": 1,
            "manpower_available": 0,
            "build_time": 0,
            "time_remaining": 0,
            "nation_id": "",
            "has_home": false
        };
    }

    function unitIconSource(unitType, nationKey) {
        if (typeof StyleGuide === "undefined" || !StyleGuide.unitIconSources || !unitType)
            return "";

        var sources = StyleGuide.unitIconSources[unitType];
        if (!sources)
            sources = StyleGuide.unitIconSources["default"];

        if (typeof sources === "object" && sources !== null) {
            if (nationKey && sources[nationKey])
                return sources[nationKey];

            if (sources["default"])
                return sources["default"];

        } else if (typeof sources === "string") {
            return sources;
        }
        return "";
    }

    function unitIconEmoji(unitType) {
        if (typeof StyleGuide !== "undefined" && StyleGuide.unitIcons)
            return StyleGuide.unitIcons[unitType] || StyleGuide.unitIcons["default"] || "👤";

        return "👤";
    }

    function getUnitProductionInfo(unitType, nationId) {
        if (productionPanel.gameInstance && productionPanel.gameInstance.get_unit_production_info)
            return productionPanel.gameInstance.get_unit_production_info(unitType, nationId || "");

        return {
            "cost": 50,
            "build_time": 5,
            "individuals_per_unit": 1,
            "display_name": unitType
        };
    }

    function meterColor(ratio) {
        if (ratio > 0.6)
            return "#7F9A5F";

        if (ratio > 0.3)
            return hs.bronze;

        return hs.waxHover;
    }

    function recruitCardColor(enabled, hovered) {
        if (!enabled)
            return "#120D09";

        return hovered ? hs.bannerNeutral : hs.parchmentDark;
    }

    function recruitCardBorder(enabled, hovered) {
        if (!enabled)
            return hs.parchmentLight;

        return hovered ? hs.bronze : hs.bronzeDeep;
    }

    color: hs.parchmentDark
    border.color: hs.bronze
    border.width: 2
    radius: 6

    ScrollView {
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            width: productionPanel.width - 20
            spacing: 8

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: productionContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: productionContent

                    property var prod: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.get_selected_production_state) ? productionPanel.gameInstance.get_selected_production_state() : productionPanel.defaultProductionState())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 10
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("PRODUCTION QUEUE")
                        color: hs.bronze
                        font.pointSize: 8
                        font.bold: true
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Repeater {
                            model: 5

                            Rectangle {
                                property int queueTotal: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)
                                property bool isOccupied: index < queueTotal
                                property bool isProducing: index === 0 && productionContent.prod.in_progress
                                property string queueUnitType: {
                                    if (!isOccupied)
                                        return "";

                                    if (index === 0 && productionContent.prod.in_progress)
                                        return productionContent.prod.product_type || "archer";

                                    var queueIndex = productionContent.prod.in_progress ? index - 1 : index;
                                    if (productionContent.prod.production_queue && productionContent.prod.production_queue[queueIndex])
                                        return productionContent.prod.production_queue[queueIndex];

                                    return "archer";
                                }

                                width: 36
                                height: 36
                                radius: 6
                                color: isProducing ? "#7F9A5F" : (isOccupied ? "#2F251D" : "#120D09")
                                border.color: isProducing ? "#8FA46B" : (isOccupied ? "#6F8E8C" : "#3B2F24")
                                border.width: 2

                                Image {
                                    id: queueIconImage

                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    source: parent.isOccupied ? productionPanel.unitIconSource(parent.queueUnitType, productionContent.prod.nation_id) : ""
                                    visible: parent.isOccupied && source !== ""
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.isOccupied ? productionPanel.unitIconEmoji(parent.queueUnitType) : "·"
                                    color: parent.isProducing ? "#F4E7C8" : (parent.isOccupied ? "#D4B57C" : "#6B5231")
                                    font.pointSize: parent.isOccupied ? 16 : 20
                                    font.bold: parent.isProducing
                                    visible: !queueIconImage.visible
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    text: (index + 1).toString()
                                    color: parent.isOccupied ? "#8D7146" : "#3B2F24"
                                    font.pointSize: 7
                                    font.bold: true
                                }

                                SequentialAnimation on opacity {
                                    running: isProducing
                                    loops: Animation.Infinite

                                    NumberAnimation {
                                        from: 0.7
                                        to: 1
                                        duration: 800
                                    }

                                    NumberAnimation {
                                        from: 1
                                        to: 0.7
                                        duration: 800
                                    }

                                }

                            }

                        }

                    }

                    Text {
                        property int queueTotal: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)

                        anchors.horizontalCenter: parent.horizontalCenter
                        text: queueTotal + " / 5"
                        color: queueTotal >= 5 ? "#C0403B" : "#D4B57C"
                        font.pointSize: 9
                        font.bold: queueTotal >= 5
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: 20
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2
                        visible: productionContent.prod.in_progress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!productionContent.prod.in_progress || productionContent.prod.build_time <= 0)
                                    return 0;

                                var progress = 1 - (Math.max(0, productionContent.prod.time_remaining) / productionContent.prod.build_time);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#7F9A5F"
                            radius: 8

                            SequentialAnimation on opacity {
                                running: parent.width > 0
                                loops: Animation.Infinite

                                NumberAnimation {
                                    from: 0.8
                                    to: 1
                                    duration: 600
                                }

                                NumberAnimation {
                                    from: 1
                                    to: 0.8
                                    duration: 600
                                }

                            }

                        }

                        Text {
                            anchors.centerIn: parent
                            text: productionContent.prod.in_progress ? Math.max(0, productionContent.prod.time_remaining).toFixed(1) + "s" : "Idle"
                            color: "#F4E7C8"
                            font.pointSize: 9
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Units Produced: %1 / %2").arg(productionContent.prod.produced_count || 0).arg(productionContent.prod.max_units || 0)
                        color: (productionContent.prod.produced_count >= productionContent.prod.max_units) ? "#C0403B" : "#D4B57C"
                        font.pointSize: 8
                    }

                }

            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: unitGridContent.height + 16
                color: hs.parchmentLight
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: unitGridContent

                    property var prod: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.get_selected_production_state) ? productionPanel.gameInstance.get_selected_production_state() : productionPanel.defaultProductionState())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("RECRUIT UNITS")
                        color: hs.bronze
                        font.pointSize: 8
                        font.bold: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("archer", unitGridContent.prod.nation_id)
                            property bool isHovered: archerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: archerRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("archer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !archerRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("archer")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: archerCostBadge

                                width: archerCostText.implicitWidth + 12
                                height: archerCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: archerCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 50
                                    color: archerCostBadge.parent.isEnabled ? Theme.textMain : Theme.textDim
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: archerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("archer");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Archer").arg(parent.unitInfo.cost || 50).arg((parent.unitInfo.build_time || 5).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: archerMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("swordsman", unitGridContent.prod.nation_id)
                            property bool isHovered: swordsmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: swordsmanRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("swordsman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !swordsmanRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("swordsman")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: swordsmanCostBadge

                                width: swordsmanCostText.implicitWidth + 12
                                height: swordsmanCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: swordsmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 90
                                    color: swordsmanCostBadge.parent.isEnabled ? Theme.textMain : Theme.textDim
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: swordsmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("swordsman");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Swordsman").arg(parent.unitInfo.cost || 90).arg((parent.unitInfo.build_time || 7).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: swordsmanMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("spearman", unitGridContent.prod.nation_id)
                            property bool isHovered: spearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: spearmanRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("spearman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !spearmanRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("spearman")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: spearmanCostBadge

                                width: spearmanCostText.implicitWidth + 12
                                height: spearmanCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: spearmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 75
                                    color: spearmanCostBadge.parent.isEnabled ? Theme.textMain : Theme.textDim
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: spearmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("spearman");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Spearman").arg(parent.unitInfo.cost || 75).arg((parent.unitInfo.build_time || 6).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: spearmanMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_swordsman", unitGridContent.prod.nation_id)
                            property bool isHovered: horseKnightMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: horseKnightIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("horse_swordsman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseKnightIcon.visible
                                text: productionPanel.unitIconEmoji("horse_swordsman")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: horseKnightCostBadge

                                width: horseKnightCostText.implicitWidth + 12
                                height: horseKnightCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: horseKnightCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 150
                                    color: horseKnightCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: horseKnightMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("horse_swordsman");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Mounted Knight").arg(parent.unitInfo.cost || 150).arg((parent.unitInfo.build_time || 10).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseKnightMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_archer", unitGridContent.prod.nation_id)
                            property bool isHovered: horseArcherMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: horseArcherIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("horse_archer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseArcherIcon.visible
                                text: productionPanel.unitIconEmoji("horse_archer")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: horseArcherCostBadge

                                width: horseArcherCostText.implicitWidth + 12
                                height: horseArcherCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: horseArcherCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 120
                                    color: horseArcherCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: horseArcherMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("horse_archer");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Horse Archer").arg(parent.unitInfo.cost || 120).arg((parent.unitInfo.build_time || 9).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseArcherMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_spearman", unitGridContent.prod.nation_id)
                            property bool isHovered: horseSpearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: horseSpearmanIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("horse_spearman", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !horseSpearmanIcon.visible
                                text: productionPanel.unitIconEmoji("horse_spearman")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: horseSpearmanCostBadge

                                width: horseSpearmanCostText.implicitWidth + 12
                                height: horseSpearmanCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: horseSpearmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 130
                                    color: horseSpearmanCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: horseSpearmanMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("horse_spearman");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Horse Spearman").arg(parent.unitInfo.cost || 130).arg((parent.unitInfo.build_time || 9).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: horseSpearmanMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("healer", unitGridContent.prod.nation_id)
                            property bool isHovered: healerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: healerRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("healer", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !healerRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("healer")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: healerCostBadge

                                width: healerCostText.implicitWidth + 12
                                height: healerCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: healerCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 100
                                    color: healerCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: healerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("healer");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Healer").arg(parent.unitInfo.cost || 100).arg((parent.unitInfo.build_time || 8).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: healerMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("builder", unitGridContent.prod.nation_id)
                            property bool isHovered: builderMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: builderRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("builder", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("builder")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: builderCostBadge

                                width: builderCostText.implicitWidth + 12
                                height: builderCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: builderCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 60
                                    color: builderCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: builderMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("builder");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s").arg(parent.unitInfo.display_name || "Builder").arg(parent.unitInfo.cost || 60).arg((parent.unitInfo.build_time || 6).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property int queueTotal: (unitGridContent.prod.in_progress ? 1 : 0) + (unitGridContent.prod.queue_size || 0)
                            property bool isEnabled: unitGridContent.prod.has_barracks && unitGridContent.prod.produced_count < unitGridContent.prod.max_units && queueTotal < 5
                            property var unitInfo: productionPanel.getUnitProductionInfo("elephant", unitGridContent.prod.nation_id)
                            property bool isHovered: elephantMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1
                            visible: unitGridContent.prod.nation_id === "carthage"

                            Image {
                                id: elephantRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("elephant", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !elephantRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("elephant")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: elephantCostBadge

                                width: elephantCostText.implicitWidth + 12
                                height: elephantCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#2a1d12cc" : "#1f150d99"
                                border.color: parent.isEnabled ? hs.bronze : "#8C6A3E"
                                border.width: 1

                                Text {
                                    id: elephantCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 250
                                    color: elephantCostBadge.parent.isEnabled ? "#F4E7C8" : "#8D7146"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: elephantMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("elephant");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 villagers\nBuild time: %3s\nCarthage exclusive").arg(parent.unitInfo.display_name || "War Elephant").arg(parent.unitInfo.cost || 250).arg((parent.unitInfo.build_time || 20).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: elephantMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                    }

                }

            }

            Rectangle {
                property bool has_home: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("home")))

                width: parent.width
                height: homeProductionContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: has_home

                Column {
                    id: homeProductionContent

                    property var prod: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.get_selected_home_production_state) ? productionPanel.gameInstance.get_selected_home_production_state() : productionPanel.defaultProductionState())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("HOME RECRUITMENT")
                        color: "#3498db"
                        font.pointSize: 8
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Available families: %1").arg(homeProductionContent.prod.manpower_available || 0)
                        color: "#bdc3c7"
                        font.pointSize: 8
                    }

                    Rectangle {
                        property int queueTotal: (homeProductionContent.prod.in_progress ? 1 : 0) + (homeProductionContent.prod.queue_size || 0)
                        property bool isEnabled: homeProductionContent.prod.has_home && queueTotal < 3
                        property var unitInfo: productionPanel.getUnitProductionInfo("civilian", homeProductionContent.prod.nation_id)
                        property bool isHovered: civilianMouseArea.containsMouse

                        width: 110
                        height: 80
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 6
                        color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                        border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                        border.width: isHovered && isEnabled ? 4 : 2
                        opacity: isEnabled ? 1 : 0.5
                        scale: isHovered && isEnabled ? 1.08 : 1

                        Image {
                            id: civilianRecruitIcon

                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            source: productionPanel.unitIconSource("civilian", homeProductionContent.prod.nation_id)
                            visible: source !== ""
                            opacity: parent.isEnabled ? 1 : 0.35
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !civilianRecruitIcon.visible
                            text: productionPanel.unitIconEmoji("civilian")
                            color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                            font.pointSize: 36
                            opacity: parent.isEnabled ? 0.9 : 0.4
                        }

                        Rectangle {
                            id: civilianCostBadge

                            width: civilianCostText.implicitWidth + 12
                            height: civilianCostText.implicitHeight + 6
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            radius: 8
                            color: parent.isEnabled ? "#000000b3" : "#00000066"
                            border.color: parent.isEnabled ? "#f39c12" : "#555555"
                            border.width: 1

                            Text {
                                id: civilianCostText

                                anchors.centerIn: parent
                                text: parent.parent.unitInfo.cost || 8
                                color: civilianCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
                                font.pointSize: 16
                                font.bold: true
                            }

                        }

                        MouseArea {
                            id: civilianMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (parent.isEnabled)
                                    productionPanel.recruitUnit("civilian");

                            }
                            cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: parent.isEnabled ? qsTr("Recruit %1\nCost: %2 families\nBuild time: %3s\nRight-click on a friendly barracks to send civilians in and transfer manpower.").arg(parent.unitInfo.display_name || "Civilian").arg(parent.unitInfo.cost || 8).arg((parent.unitInfo.build_time || 5).toFixed(0)) : qsTr("Cannot recruit")
                            ToolTip.delay: 300
                        }

                    }

                }

            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#3B2F24"
                visible: has_barracks || (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("home"))
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: rallyContent.height + 12
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_barracks

                Column {
                    id: rallyContent

                    property var prod: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.get_selected_production_state) ? productionPanel.gameInstance.get_selected_production_state() : productionPanel.defaultProductionState())

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 6

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.parent.width - 20
                        height: 32
                        text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? qsTr("📍 Click Map to Set Rally") : qsTr("📍 Set Rally Point")
                        focusPolicy: Qt.NoFocus
                        enabled: rallyContent.prod.has_barracks
                        onClicked: productionPanel.rallyModeToggled()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Set where newly recruited units will gather.\nRight-click to cancel.")
                        ToolTip.delay: 500

                        background: Rectangle {
                            color: parent.enabled ? (parent.down ? "#6F8E8C" : (parent.hovered ? "#82A4A1" : "#2F251D")) : "#120D09"
                            radius: 6
                            border.color: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "#82A4A1" : "#3B2F24"
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pointSize: 9
                            font.bold: true
                            color: parent.enabled ? "#F4E7C8" : "#6B5231"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? qsTr("Right-click to cancel") : ""
                        color: "#8D7146"
                        font.pointSize: 8
                        font.italic: true
                    }

                }

            }

            Item {
                property bool has_barracksSelected: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))
                property bool has_homeSelected: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("home")))

                height: 20
                visible: !has_barracksSelected && !has_homeSelected
            }

            Rectangle {
                property bool has_builder: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("builder")))

                width: parent.width
                height: builderProductionContent.height + 16
                color: "#120D09"
                radius: 6
                border.color: hs.bronzeDeep
                border.width: 1
                visible: has_builder

                Column {
                    id: builderProductionContent

                    property var builderProd: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.get_selected_builder_production_state) ? productionPanel.gameInstance.get_selected_builder_production_state() : {
                        "in_progress": false,
                        "build_time": 10,
                        "time_remaining": 0,
                        "product_type": ""
                    })

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8
                    width: parent.width - 16

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 6

                        Image {
                            id: builderHeaderIcon

                            width: 18
                            height: 18
                            source: productionPanel.unitIconSource("builder")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: source !== ""
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: builderHeaderIcon.visible ? qsTr("BUILDER CONSTRUCTION") : qsTr("🔨 BUILDER CONSTRUCTION")
                            color: "#5F7F83"
                            font.pointSize: 9
                            font.bold: true
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Build siege weapons and structures")
                        color: "#8D7146"
                        font.pointSize: 7
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: 20
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#120D09"
                        border.color: "#2F251D"
                        border.width: 2
                        visible: builderProductionContent.builderProd.in_progress

                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 2
                            height: parent.height - 4
                            width: {
                                if (!builderProductionContent.builderProd.in_progress || builderProductionContent.builderProd.build_time <= 0)
                                    return 0;

                                var progress = 1 - (Math.max(0, builderProductionContent.builderProd.time_remaining) / builderProductionContent.builderProd.build_time);
                                return Math.max(0, (parent.width - 4) * progress);
                            }
                            color: "#7F9A5F"
                            radius: 8

                            SequentialAnimation on opacity {
                                running: parent.width > 0
                                loops: Animation.Infinite

                                NumberAnimation {
                                    from: 0.8
                                    to: 1
                                    duration: 600
                                }

                                NumberAnimation {
                                    from: 1
                                    to: 0.8
                                    duration: 600
                                }

                            }

                        }

                        Text {
                            anchors.centerIn: parent
                            text: builderProductionContent.builderProd.in_progress ? Math.max(0, builderProductionContent.builderProd.time_remaining).toFixed(1) + "s" : "Idle"
                            color: "#F4E7C8"
                            font.pointSize: 9
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#120D09"
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: builderProductionContent.builderProd.in_progress ? qsTr("Building: %1").arg(builderProductionContent.builderProd.product_type) : qsTr("Select an item to build")
                        color: builderProductionContent.builderProd.in_progress ? "#7F9A5F" : "#8D7146"
                        font.pointSize: 8
                        font.bold: builderProductionContent.builderProd.in_progress
                        visible: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Rectangle {
                            property bool isEnabled: !builderProductionContent.builderProd.in_progress
                            property bool isHovered: builderCatapultMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: builderCatapultIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("catapult")
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderCatapultIcon.visible
                                text: productionPanel.unitIconEmoji("catapult")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: qsTr("Catapult")
                                color: parent.isEnabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                id: builderCatapultMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.builderConstruction("catapult");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Build Catapult\nLong-range siege weapon\nEffective against structures\nBuild time: 15s") : qsTr("Already building...")
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderCatapultMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property bool isEnabled: !builderProductionContent.builderProd.in_progress
                            property bool isHovered: builderBallistaMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: builderBallistaIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("ballista")
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderBallistaIcon.visible
                                text: productionPanel.unitIconEmoji("ballista")
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: qsTr("Ballista")
                                color: parent.isEnabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                id: builderBallistaMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.builderConstruction("ballista");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Build Ballista\nPrecision siege weapon\nEffective against units\nBuild time: 12s") : qsTr("Already building...")
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderBallistaMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property bool isEnabled: !builderProductionContent.builderProd.in_progress
                            property bool isHovered: builderDefenseTowerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: builderDefenseTowerIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("defense_tower")
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderDefenseTowerIcon.visible
                                text: "🏰"
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: qsTr("Defense Tower")
                                color: parent.isEnabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                id: builderDefenseTowerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.builderConstruction("defense_tower");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Build Defense Tower\nStationary defense structure\nShoots arrows at enemies\nBuild time: 20s") : qsTr("Already building...")
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderDefenseTowerMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                        Rectangle {
                            property bool isEnabled: !builderProductionContent.builderProd.in_progress
                            property bool isHovered: builderHomeMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: productionPanel.recruitCardColor(isEnabled, isHovered)
                            border.color: productionPanel.recruitCardBorder(isEnabled, isHovered)
                            border.width: isHovered && isEnabled ? 2 : 1
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.025 : 1

                            Image {
                                id: builderHomeIcon

                                anchors.fill: parent
                                anchors.margins: 6
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("home")
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !builderHomeIcon.visible
                                text: "🏠"
                                color: parent.isEnabled ? "#F4E7C8" : "#6B5231"
                                font.pointSize: 36
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: qsTr("Home")
                                color: parent.isEnabled ? "#D4B57C" : "#6B5231"
                                font.pointSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                id: builderHomeMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.builderConstruction("home");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Build Home\nResidential building\nAdds +50 population to nearest barracks\nBuild time: 10s") : qsTr("Already building...")
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#F4E7C8"
                                opacity: builderHomeMouseArea.pressed ? 0.2 : 0
                                radius: parent.radius
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 150
                                }

                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: 100
                                }

                            }

                        }

                    }

                }

            }

            Item {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))
                property bool has_builder: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("builder")))
                property bool has_home: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("home")))

                visible: !has_barracks && !has_builder && !has_home
                width: parent.width
                height: 200

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🏰"
                        color: "#3B2F24"
                        font.pointSize: 32
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No Barracks Selected")
                        color: "#8D7146"
                        font.pointSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Select a barracks to recruit units")
                        color: "#6F8E8C"
                        font.pointSize: 9
                    }

                }

            }

        }

    }

}

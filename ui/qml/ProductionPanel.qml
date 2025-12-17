import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: productionPanel

    property int selectionTick: 0
    property var gameInstance: null

    signal recruitUnit(string unitType)
    signal rallyModeToggled()
    signal buildTower()

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
            "build_time": 0,
            "time_remaining": 0,
            "nation_id": ""
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

    function getUnitProductionInfo(unitType) {
        if (productionPanel.gameInstance && productionPanel.gameInstance.get_unit_production_info)
            return productionPanel.gameInstance.get_unit_production_info(unitType);

        return {
            "cost": 50,
            "build_time": 5,
            "individuals_per_unit": 1
        };
    }

    color: "#0f1419"
    border.color: "#3498db"
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
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
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
                        color: "#3498db"
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
                                color: isProducing ? "#27ae60" : (isOccupied ? "#2c3e50" : "#1a1a1a")
                                border.color: isProducing ? "#229954" : (isOccupied ? "#4a6572" : "#2a2a2a")
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
                                    color: parent.isProducing ? "#ffffff" : (parent.isOccupied ? "#bdc3c7" : "#3a3a3a")
                                    font.pointSize: parent.isOccupied ? 16 : 20
                                    font.bold: parent.isProducing
                                    visible: !queueIconImage.visible
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    text: (index + 1).toString()
                                    color: parent.isOccupied ? "#7f8c8d" : "#2a2a2a"
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
                        color: queueTotal >= 5 ? "#e74c3c" : "#bdc3c7"
                        font.pointSize: 9
                        font.bold: queueTotal >= 5
                    }

                    Rectangle {
                        width: parent.width - 20
                        height: 20
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 10
                        color: "#0a0f14"
                        border.color: "#2c3e50"
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
                            color: "#27ae60"
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
                            color: "#ecf0f1"
                            font.pointSize: 9
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#000000"
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Units Produced: %1 / %2").arg(productionContent.prod.produced_count || 0).arg(productionContent.prod.max_units || 0)
                        color: (productionContent.prod.produced_count >= productionContent.prod.max_units) ? "#e74c3c" : "#bdc3c7"
                        font.pointSize: 8
                    }

                }

            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: unitGridContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
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
                        color: "#3498db"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("archer")
                            property bool isHovered: archerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: archerCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 50
                                    color: archerCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Archer\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 50).arg((parent.unitInfo.build_time || 5).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("swordsman")
                            property bool isHovered: swordsmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: swordsmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 90
                                    color: swordsmanCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Swordsman\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 90).arg((parent.unitInfo.build_time || 7).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("spearman")
                            property bool isHovered: spearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: spearmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 75
                                    color: spearmanCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Spearman\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 75).arg((parent.unitInfo.build_time || 6).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_swordsman")
                            property bool isHovered: horseKnightMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: horseKnightCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 150
                                    color: horseKnightCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Mounted Knight\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 150).arg((parent.unitInfo.build_time || 10).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_archer")
                            property bool isHovered: horseArcherMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: horseArcherCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 120
                                    color: horseArcherCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Horse Archer\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 120).arg((parent.unitInfo.build_time || 9).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("horse_spearman")
                            property bool isHovered: horseSpearmanMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: horseSpearmanCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 130
                                    color: horseSpearmanCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Horse Spearman\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 130).arg((parent.unitInfo.build_time || 9).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("catapult")
                            property bool isHovered: catapultMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

                            Image {
                                id: catapultRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("catapult", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !catapultRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("catapult")
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: catapultCostBadge

                                width: catapultCostText.implicitWidth + 12
                                height: catapultCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: catapultCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 200
                                    color: catapultCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: catapultMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("catapult");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Catapult\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 200).arg((parent.unitInfo.build_time || 12).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
                                opacity: catapultMouseArea.pressed ? 0.2 : 0
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("ballista")
                            property bool isHovered: ballistaMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

                            Image {
                                id: ballistaRecruitIcon

                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                source: productionPanel.unitIconSource("ballista", unitGridContent.prod.nation_id)
                                visible: source !== ""
                                opacity: parent.isEnabled ? 1 : 0.35
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !ballistaRecruitIcon.visible
                                text: productionPanel.unitIconEmoji("ballista")
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                font.pointSize: 42
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Rectangle {
                                id: ballistaCostBadge

                                width: ballistaCostText.implicitWidth + 12
                                height: ballistaCostText.implicitHeight + 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                radius: 8
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: ballistaCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 180
                                    color: ballistaCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
                                    font.pointSize: 16
                                    font.bold: true
                                }

                            }

                            MouseArea {
                                id: ballistaMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.recruitUnit("ballista");

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Ballista\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 180).arg((parent.unitInfo.build_time || 10).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
                                opacity: ballistaMouseArea.pressed ? 0.2 : 0
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("healer")
                            property bool isHovered: healerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: healerCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 100
                                    color: healerCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Healer\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 100).arg((parent.unitInfo.build_time || 8).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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
                            property var unitInfo: productionPanel.getUnitProductionInfo("builder")
                            property bool isHovered: builderMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

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
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
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
                                color: parent.isEnabled ? "#000000b3" : "#00000066"
                                border.color: parent.isEnabled ? "#f39c12" : "#555555"
                                border.width: 1

                                Text {
                                    id: builderCostText

                                    anchors.centerIn: parent
                                    text: parent.parent.unitInfo.cost || 60
                                    color: builderCostBadge.parent.isEnabled ? "#fdf7e3" : "#8a8a8a"
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
                                ToolTip.text: parent.isEnabled ? qsTr("Recruit Builder\nCost: %1 villagers\nBuild time: %2s").arg(parent.unitInfo.cost || 60).arg((parent.unitInfo.build_time || 6).toFixed(0)) : (parent.queueTotal >= 5 ? qsTr("Queue is full (5/5)") : (unitGridContent.prod.produced_count >= unitGridContent.prod.max_units ? qsTr("Unit cap reached") : qsTr("Cannot recruit")))
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
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

                    }

                }

            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#34495e"
                visible: has_barracks
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: rallyContent.height + 12
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
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
                            color: parent.enabled ? (parent.down ? "#16a085" : (parent.hovered ? "#1abc9c" : "#2c3e50")) : "#1a1a1a"
                            radius: 6
                            border.color: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? "#1abc9c" : "#34495e"
                            border.width: 2
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pointSize: 9
                            font.bold: true
                            color: parent.enabled ? "#ecf0f1" : "#5a5a5a"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (typeof gameView !== 'undefined' && gameView.setRallyMode) ? qsTr("Right-click to cancel") : ""
                        color: "#7f8c8d"
                        font.pointSize: 8
                        font.italic: true
                    }

                }

            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: 1
                color: "#34495e"
                visible: has_barracks
            }

            Rectangle {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                width: parent.width
                height: buildingsContent.height + 16
                color: "#1a252f"
                radius: 6
                border.color: "#34495e"
                border.width: 1
                visible: has_barracks

                Column {
                    id: buildingsContent

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("BUILD STRUCTURES")
                        color: "#3498db"
                        font.pointSize: 8
                        font.bold: true
                    }

                    Grid {
                        anchors.horizontalCenter: parent.horizontalCenter
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 8

                        Rectangle {
                            property bool isEnabled: true
                            property bool isHovered: defenseTowerMouseArea.containsMouse

                            width: 110
                            height: 80
                            radius: 6
                            color: isEnabled ? (isHovered ? "#1f8dd9" : "#2c3e50") : "#1a1a1a"
                            border.color: isEnabled ? (isHovered ? "#00d4ff" : "#4a6572") : "#2a2a2a"
                            border.width: isHovered && isEnabled ? 4 : 2
                            opacity: isEnabled ? 1 : 0.5
                            scale: isHovered && isEnabled ? 1.1 : 1

                            Text {
                                anchors.centerIn: parent
                                text: "🏰"
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                font.pointSize: 36
                                opacity: parent.isEnabled ? 0.9 : 0.4
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                text: qsTr("Defense Tower")
                                color: parent.isEnabled ? "#ecf0f1" : "#5a5a5a"
                                font.pointSize: 8
                                font.bold: true
                            }

                            MouseArea {
                                id: defenseTowerMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (parent.isEnabled)
                                        productionPanel.buildTower();

                                }
                                cursorShape: parent.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: qsTr("Build Defense Tower\nClick map to place.\nShoots arrows at nearby enemies.")
                                ToolTip.delay: 300
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#ffffff"
                                opacity: defenseTowerMouseArea.pressed ? 0.2 : 0
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
                property bool has_barracksSelected: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                height: 20
                visible: !has_barracksSelected
            }

            Item {
                property bool has_barracks: (productionPanel.selectionTick, (productionPanel.gameInstance && productionPanel.gameInstance.has_selected_type && productionPanel.gameInstance.has_selected_type("barracks")))

                visible: !has_barracks
                width: parent.width
                height: 200

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🏰"
                        color: "#34495e"
                        font.pointSize: 32
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No Barracks Selected")
                        color: "#7f8c8d"
                        font.pointSize: 11
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Select a barracks to recruit units")
                        color: "#5a6c7d"
                        font.pointSize: 9
                    }

                }

            }

        }

    }

}

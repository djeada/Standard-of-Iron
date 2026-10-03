import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Rectangle {
    id: marketplaceSection
    required property var panel
    readonly property var hs: StyleGuide.historical

    property bool has_marketplace_selected: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("marketplace")))
    readonly property var market_state: (panel.selection_tick, (panel.production && panel.production.selected_marketplace_state) ? panel.production.selected_marketplace_state() : panel.default_marketplace_state())
    readonly property bool trading: market_state.has_marketplace === true
    readonly property int lot: Math.max(0, market_state.trade_quantity || 0)
    readonly property int gold: panel.resource_amount(panel.current_resources(), "gold")
    readonly property int row_height: Math.max(Design.A11y.scaled(26), Design.Typography.label + 12)

    objectName: "marketplaceSection"
    width: parent.width
    height: marketplaceContent.implicitHeight + Design.Metrics.space8 * 2
    color: "#120D09"
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_marketplace_selected

    Column {
        id: marketplaceContent

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Design.Metrics.space8
        spacing: Design.Metrics.space4

        RowLayout {
            width: parent.width
            spacing: Design.Metrics.space4

            Image {
                id: marketplaceHeaderIcon

                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: panel.unit_icon_source("marketplace", marketplaceSection.market_state.nation_id)
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: status === Image.Ready
            }

            Text {
                text: qsTr("MARKETPLACE")
                color: hs.bronze
                font.pixelSize: Design.Typography.caption
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Image {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                source: panel.cost_icon_source("gold")
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: marketplaceSection.trading
            }

            Text {
                objectName: "marketplaceGold"
                visible: marketplaceSection.trading
                text: marketplaceSection.gold
                color: "#F4E7C8"
                font.pixelSize: Design.Typography.caption
                font.bold: true
            }

            Text {
                visible: marketplaceSection.trading
                text: qsTr("· lots of %1").arg(marketplaceSection.lot)
                color: "#8D7146"
                font.pixelSize: Design.Typography.caption
            }
        }

        Text {
            visible: !marketplaceSection.trading
            width: parent.width
            text: qsTr("Only your own marketplace can trade. Select it to buy or sell.")
            color: "#8D7146"
            font.pixelSize: Design.Typography.caption
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Repeater {
            model: marketplaceSection.trading ? panel.marketplace_trade_specs : []

            delegate: Rectangle {
                id: tradeRow

                readonly property string resource_key: modelData.key
                readonly property string resource_label: modelData.label
                readonly property int stock: panel.resource_amount(panel.current_resources(), resource_key)
                readonly property int buy_price: panel.trade_price(marketplaceSection.market_state.buy_prices, resource_key)
                readonly property int sell_price: panel.trade_price(marketplaceSection.market_state.sell_prices, resource_key)
                readonly property bool can_buy: panel.can_buy_trade_resource(marketplaceSection.market_state, resource_key)
                readonly property bool can_sell: panel.can_sell_trade_resource(marketplaceSection.market_state, resource_key)

                objectName: "marketplaceRow_" + resource_key
                width: marketplaceContent.width
                height: marketplaceSection.row_height
                radius: 4
                color: "#1A120C"
                border.color: hs.bronzeDeep
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Design.Metrics.space4
                    anchors.rightMargin: 2
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    spacing: Design.Metrics.space4

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        source: panel.cost_icon_source(tradeRow.resource_key)
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("%1 %2").arg(tradeRow.resource_label).arg(tradeRow.stock)
                        color: "#F4E7C8"
                        font.pixelSize: Design.Typography.caption
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Repeater {
                        model: [{
                                "buying": true
                            }, {
                                "buying": false
                            }]

                        delegate: Rectangle {
                            id: tradeButton

                            readonly property bool buying: modelData.buying
                            readonly property bool allowed: buying ? tradeRow.can_buy : tradeRow.can_sell
                            readonly property int price: buying ? tradeRow.buy_price : tradeRow.sell_price

                            objectName: (buying ? "marketplaceBuy_" : "marketplaceSell_") + tradeRow.resource_key
                            Layout.fillHeight: true
                            Layout.preferredWidth: Math.max(tradeLabel.implicitWidth + Design.Metrics.space12, Math.round(tradeRow.width * 0.3))
                            radius: 3
                            color: !allowed ? "#140E0A" : tradeMouse.pressed ? "#4A3520" : tradeMouse.containsMouse ? "#3A2A1A" : "#2A1D12"
                            border.width: 1
                            border.color: !allowed ? "#3B2F24" : tradeMouse.containsMouse ? Theme.accent : hs.bronze
                            Accessible.role: Accessible.Button
                            Accessible.name: tradeLabel.text

                            Text {
                                id: tradeLabel

                                anchors.centerIn: parent
                                text: tradeButton.buying ? qsTr("Buy %1 · %2g").arg(marketplaceSection.lot).arg(tradeButton.price) : qsTr("Sell %1 · +%2g").arg(marketplaceSection.lot).arg(tradeButton.price)
                                color: tradeButton.allowed ? "#F4E7C8" : "#5E4B36"
                                font.pixelSize: Design.Typography.caption
                                font.bold: true
                            }

                            MouseArea {
                                id: tradeMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: tradeButton.allowed ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                                onClicked: {
                                    if (!panel.production)
                                        return;
                                    if (!tradeButton.allowed) {
                                        Design.UiSound.warning();
                                        return;
                                    }
                                    Design.UiSound.activate();
                                    if (tradeButton.buying)
                                        panel.production.marketplace_buy(tradeRow.resource_key);
                                    else
                                        panel.production.marketplace_sell(tradeRow.resource_key);
                                }
                            }

                            ToolTip.visible: tradeMouse.containsMouse
                            ToolTip.delay: Design.Metrics.tooltipDelay
                            ToolTip.text: {
                                var goods = tradeRow.resource_label.toLowerCase();
                                if (tradeButton.buying)
                                    return tradeButton.allowed ? qsTr("Spend %1 gold for %2 %3").arg(tradeButton.price).arg(marketplaceSection.lot).arg(goods) : qsTr("Not enough gold: %1 needed").arg(tradeButton.price);
                                return tradeButton.allowed ? qsTr("Sell %1 %2 for %3 gold").arg(marketplaceSection.lot).arg(goods).arg(tradeButton.price) : qsTr("Not enough %1: %2 needed").arg(goods).arg(marketplaceSection.lot);
                            }
                        }
                    }
                }
            }
        }

        Column {
            id: allyExchange

            readonly property var allies: (panel.selection_tick, (panel.production && panel.production.marketplace_allies) ? panel.production.marketplace_allies() : [])
            property int ally_index: 0
            property string resource_key: "wood"
            property int amount: 50
            readonly property var ally: allies.length > 0 ? allies[Math.min(ally_index, allies.length - 1)] : null
            readonly property int held: panel.resource_amount(panel.current_resources(), resource_key)

            objectName: "allyExchange"
            width: parent.width
            spacing: Design.Metrics.space4
            visible: marketplaceSection.trading

            Text {
                text: qsTr("ALLIES")
                color: hs.bronze
                font.pixelSize: Design.Typography.caption
                font.bold: true
            }

            Text {
                width: parent.width
                visible: allyExchange.allies.length === 0
                text: qsTr("No allies to trade with in this battle.")
                color: Theme.textDim
                font.pixelSize: Design.Typography.caption
                wrapMode: Text.WordWrap
            }

            Flow {
                width: parent.width
                spacing: Design.Metrics.space4
                visible: allyExchange.allies.length > 0

                Repeater {
                    model: allyExchange.allies

                    delegate: Design.IronButton {
                        required property var modelData
                        required property int index

                        objectName: "allyChip" + index
                        text: modelData.name
                        tone: allyExchange.ally_index === index ? "primary" : "secondary"
                        implicitHeight: marketplaceSection.row_height
                        onClicked: allyExchange.ally_index = index
                    }
                }
            }

            Row {
                spacing: Design.Metrics.space4
                visible: allyExchange.allies.length > 0

                Repeater {
                    model: ["gold", "food", "wood", "stone", "iron"]

                    delegate: Rectangle {
                        required property string modelData

                        objectName: "allyResource_" + modelData
                        width: marketplaceSection.row_height
                        height: marketplaceSection.row_height
                        radius: 4
                        color: allyExchange.resource_key === modelData ? "#553A2410" : "transparent"
                        border.color: allyExchange.resource_key === modelData ? hs.bronze : hs.bronzeDeep
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: parent.width - 8
                            height: width
                            source: panel.cost_icon_source(modelData)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: allyExchange.resource_key = modelData
                        }
                    }
                }
            }

            Row {
                spacing: Design.Metrics.space4
                visible: allyExchange.allies.length > 0

                Repeater {
                    model: [25, 50, 100, 200]

                    delegate: Design.IronButton {
                        required property int modelData

                        objectName: "allyAmount" + modelData
                        text: modelData
                        tone: allyExchange.amount === modelData ? "primary" : "secondary"
                        implicitHeight: marketplaceSection.row_height
                        implicitWidth: Math.max(40, contentItem.implicitWidth + Design.Metrics.space12)
                        onClicked: allyExchange.amount = modelData
                    }
                }
            }

            Row {
                spacing: Design.Metrics.space8
                visible: allyExchange.allies.length > 0

                Design.IronButton {
                    id: allySendButton

                    objectName: "allySendButton"
                    text: qsTr("Send")
                    tone: "primary"
                    enabled: allyExchange.ally !== null && allyExchange.held >= allyExchange.amount
                    implicitHeight: marketplaceSection.row_height
                    onClicked: {
                        if (panel.production && allyExchange.ally)
                            panel.production.send_to_ally(allyExchange.ally.owner_id, allyExchange.resource_key, allyExchange.amount);
                    }

                    Design.IronTooltip {
                        parent: allyExchange
                        x: Math.round((allyExchange.width - width) / 2)
                        y: -height - Design.Metrics.space4
                        visible: allySendButton.hovered && text !== ""
                        text: allyExchange.ally ? qsTr("Give %1 %2 to %3").arg(allyExchange.amount).arg(allyExchange.resource_key).arg(allyExchange.ally.name) : ""
                    }
                }

                Design.IronButton {
                    id: allyRequestButton

                    objectName: "allyRequestButton"
                    text: qsTr("Request")
                    enabled: allyExchange.ally !== null && allyExchange.ally.is_ai === true
                    implicitHeight: marketplaceSection.row_height
                    onClicked: {
                        if (panel.production && allyExchange.ally)
                            panel.production.request_from_ally(allyExchange.ally.owner_id, allyExchange.resource_key, allyExchange.amount);
                    }

                    Design.IronTooltip {
                        parent: allyExchange
                        x: Math.round((allyExchange.width - width) / 2)
                        y: -height - Design.Metrics.space4
                        visible: allyRequestButton.hovered && text !== ""
                        text: allyExchange.ally ? qsTr("Ask %1 for %2 %3. A generous commander with plenty to spare says yes; a warlike or poor one keeps it.").arg(allyExchange.ally.name).arg(allyExchange.amount).arg(allyExchange.resource_key) : ""
                    }
                }
            }
        }
    }
}

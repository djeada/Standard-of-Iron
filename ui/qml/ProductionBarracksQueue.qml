import QtQuick 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    required property var panel
    readonly property var hs: StyleGuide.historical
    property bool has_barracks: (panel.selection_tick, (panel.production && panel.production.has_selected_type && panel.production.has_selected_type("barracks")))

    width: parent.width
    height: productionContent.height + 12
    color: hs.parchmentLight
    radius: 6
    border.color: hs.bronzeDeep
    border.width: 1
    visible: has_barracks

    Column {
        id: productionContent

        property var prod: (panel.selection_tick, (panel.production && panel.production.selected_state) ? panel.production.selected_state() : panel.default_production_state())

        readonly property int queue_total: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)
        readonly property int queueSlotSize: Design.A11y.scaled(32)

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 6
        width: parent.width - 12

        Flow {
            width: parent.width
            spacing: 8

            Row {
                height: productionContent.queueSlotSize
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("QUEUE")
                    color: hs.bronze
                    font.pixelSize: Design.Typography.caption
                    font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: productionContent.queue_total + " / 5"
                    color: productionContent.queue_total >= 5 ? "#C0403B" : "#D4B57C"
                    font.pixelSize: Design.Typography.caption
                    font.bold: productionContent.queue_total >= 5
                }
            }

            Row {
                height: productionContent.queueSlotSize
                spacing: 4

                Repeater {
                    model: 5

                    Rectangle {
                        property int queue_total: (productionContent.prod.in_progress ? 1 : 0) + (productionContent.prod.queue_size || 0)
                        property bool is_occupied: index < queue_total
                        property bool is_producing: index === 0 && productionContent.prod.in_progress === true
                        property string queue_unit_type: {
                            if (!is_occupied)
                                return "";
                            if (index === 0 && productionContent.prod.in_progress)
                                return productionContent.prod.product_type || "archer";
                            var queueIndex = productionContent.prod.in_progress ? index - 1 : index;
                            if (productionContent.prod.production_queue && productionContent.prod.production_queue[queueIndex])
                                return productionContent.prod.production_queue[queueIndex];
                            return "archer";
                        }

                        width: Design.A11y.scaled(32)
                        height: Design.A11y.scaled(32)
                        radius: 5
                        color: is_producing ? "#7F9A5F" : (is_occupied ? "#2F251D" : "#120D09")
                        border.color: is_producing ? "#8FA46B" : (is_occupied ? "#6F8E8C" : "#3B2F24")
                        border.width: 2

                        Image {
                            id: queueIconImage

                            anchors.centerIn: parent
                            width: Design.A11y.scaled(25)
                            height: Design.A11y.scaled(25)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            source: parent.is_occupied ? panel.unit_icon_source(parent.queue_unit_type, productionContent.prod.nation_id) : ""
                            visible: parent.is_occupied && source !== ""
                        }

                        Text {
                            anchors.centerIn: parent
                            text: parent.is_occupied ? panel.unit_icon_emoji(parent.queue_unit_type) : "·"
                            color: parent.is_producing ? "#F4E7C8" : (parent.is_occupied ? "#D4B57C" : "#6B5231")
                            font.pixelSize: Design.Typography.subheading
                            font.bold: parent.is_producing
                            visible: !queueIconImage.visible
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 2
                            text: (index + 1).toString()
                            color: parent.is_occupied ? "#8D7146" : "#3B2F24"
                            font.pixelSize: Design.Typography.caption
                            font.bold: true
                        }

                        SequentialAnimation on opacity  {
                            running: is_producing
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
                objectName: "barracksReserveLabel"
                height: productionContent.queueSlotSize
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Barracks reserve: %1 / %2").arg(productionContent.prod.manpower_available || 0).arg(productionContent.prod.max_units || 0)
                color: (productionContent.prod.manpower_available <= 0) ? "#C0403B" : "#D4B57C"
                font.pixelSize: Design.Typography.caption
            }
        }

        Rectangle {
            width: parent.width
            height: Math.max(Design.A11y.scaled(14), Design.Typography.caption + 2)
            radius: 8
            color: "#120D09"
            border.color: "#2F251D"
            border.width: 1
            visible: productionContent.prod.in_progress === true

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

                SequentialAnimation on opacity  {
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
                text: productionContent.prod.in_progress ? qsTr("%1s").arg(Math.max(0, productionContent.prod.time_remaining).toFixed(1)) : qsTr("Idle")
                color: "#F4E7C8"
                font.pixelSize: Design.Typography.caption
                font.bold: true
                style: Text.Outline
                styleColor: "#120D09"
            }
        }
    }
}

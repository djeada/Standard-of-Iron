import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

FocusScope {
    id: root

    property string outcome: "victory"
    property string factionId: ""
    property string headline: ""
    property string subtitle: ""
    property string missionName: ""
    property string durationText: ""
    property var armies: []
    property string primaryAction: qsTr("Return to Menu")
    property string secondaryAction: qsTr("Close Report")

    readonly property color tone: root.outcome === "defeat" ? Design.Theme.danger : root.outcome === "campaign" ? Design.FactionTheme.accentFor(root.factionId) : Design.Theme.success
    readonly property string crest: root.outcome === "campaign" ? Design.FactionTheme.glyphFor(root.factionId) : root.outcome === "defeat" ? Design.Icons.defeated : Design.Icons.objective

    readonly property var statColumns: [{
            "key": "kills",
            "label": qsTr("Kills"),
            "glyph": Design.Icons.attack,
            "essential": true,
            "grim": false
        }, {
            "key": "losses",
            "label": qsTr("Losses"),
            "glyph": Design.Icons.defeated,
            "essential": true,
            "grim": true
        }, {
            "key": "trained",
            "label": qsTr("Trained"),
            "glyph": Design.Icons.population,
            "essential": false,
            "grim": false
        }, {
            "key": "villages",
            "label": qsTr("Villages"),
            "glyph": Design.Icons.capture,
            "essential": false,
            "grim": false
        }]

    readonly property real sheetWidth: Math.max(0, Math.min(root.width - Design.Metrics.space24 * 2, Design.Metrics.space24 * 44))
    readonly property bool compact: root.sheetWidth < Design.Metrics.space24 * 31
    readonly property real figureCellWidth: Math.round(Design.Metrics.space24 * 3.2)
    readonly property real scoreCellWidth: Math.round(Design.Metrics.space24 * 4.4)

    readonly property var visibleColumns: {
        var columns = [];
        for (var i = 0; i < root.statColumns.length; ++i) {
            if (!root.compact || root.statColumns[i].essential)
                columns.push(root.statColumns[i]);
        }
        return columns;
    }

    readonly property bool showTiles: root.localArmy !== null && (root.height - Design.A11y.scaled(300) - root.armies.length * Design.A11y.scaled(64)) > Design.A11y.scaled(120)

    readonly property var localArmy: {
        for (var i = 0; i < root.armies.length; ++i) {
            if (root.armies[i].isLocal)
                return root.armies[i];
        }
        return null;
    }

    signal dismissed
    signal menuRequested

    function figure(army, key) {
        var value = army && army[key] !== undefined ? army[key] : 0;
        return Design.Numerals.tally(value, Design.Numerals.needsArabic(root.column_values(key)));
    }

    function column_values(key) {
        var values = [];
        for (var i = 0; i < root.armies.length; ++i)
            values.push(Number(root.armies[i][key]) || 0);
        return values;
    }

    function column_best(key) {
        var best = 0;
        for (var i = 0; i < root.armies.length; ++i)
            best = Math.max(best, Number(root.armies[i][key]) || 0);
        return best;
    }

    function share(value, best) {
        var top = Number(best) || 0;
        if (top <= 0)
            return 0;
        return Math.max(0, Math.min(1, (Number(value) || 0) / top));
    }

    function army_accent(army) {
        if (army && army.accent)
            return army.accent;
        if (army && army.factionId)
            return Design.FactionTheme.accentFor(army.factionId);
        return Design.Theme.borderSubtle;
    }

    function verdict(army) {
        return army && army.isWinner ? qsTr("Held the Field") : qsTr("Routed");
    }

    function spoken_figures(army) {
        var parts = [];
        for (var i = 0; i < root.statColumns.length; ++i) {
            var column = root.statColumns[i];
            parts.push(column.label + " " + root.figure(army, column.key));
        }
        parts.push(qsTr("Score") + " " + root.figure(army, "score"));
        return parts.join(", ");
    }

    anchors.fill: parent
    focus: true
    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.headline
    Accessible.description: root.subtitle

    Keys.onEscapePressed: root.dismissed()

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.backgroundDeep
        opacity: 0.94

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onWheel: wheel => wheel.accepted = true
        }
    }

    Design.IronPanel {
        id: sheet

        objectName: "battleReportSheet"

        readonly property real chromeHeight: footerRule.height + reportFooter.height + Design.Metrics.space16 * 2 + Design.Metrics.space12 * 2

        anchors.centerIn: parent
        width: root.sheetWidth
        height: Math.max(0, Math.min(root.height - Design.Metrics.space24 * 2, sheetColumn.implicitHeight + sheet.chromeHeight))
        raised: true
        border.color: root.tone
        border.width: Design.Metrics.borderFocus
        accessibleName: root.headline

        Flickable {
            id: sheetFlick

            readonly property bool scrolling: contentHeight > height + 1

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: footerRule.top
            anchors.bottomMargin: Design.Metrics.space16
            contentWidth: width
            contentHeight: sheetColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                id: sheetScrollBar

                policy: sheetFlick.scrolling ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            Column {
                id: sheetColumn

                width: sheetFlick.width - (sheetFlick.scrolling ? sheetScrollBar.width + Design.Metrics.space8 : 0)
                spacing: Design.Metrics.space16

                Item {
                    width: parent.width
                    height: Math.max(crestMedallion.height, headlineColumn.implicitHeight)

                    Rectangle {
                        id: crestMedallion

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Design.A11y.scaled(64)
                        height: width
                        radius: width / 2
                        color: Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.14)
                        border.width: Design.Metrics.borderThin
                        border.color: root.tone

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - Design.Metrics.space8
                            text: root.crest
                            color: root.tone
                            font.family: Design.Typography.displayFamily
                            font.pixelSize: Design.Typography.glyphSmall
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: Design.Typography.minimumSize
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    Column {
                        id: headlineColumn

                        anchors.left: crestMedallion.right
                        anchors.leftMargin: Design.Metrics.space16
                        anchors.right: durationChip.visible ? durationChip.left : parent.right
                        anchors.rightMargin: durationChip.visible ? Design.Metrics.space12 : 0
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Design.Metrics.space4

                        Text {
                            width: parent.width
                            text: root.headline
                            color: root.tone
                            font.family: Design.Typography.titleFamily
                            font.capitalization: Font.AllUppercase
                            font.pixelSize: Design.Typography.title
                            font.weight: Design.Typography.bold
                            font.letterSpacing: Design.Typography.trackingTitle
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            visible: root.subtitle !== ""
                            text: root.subtitle
                            color: Design.Theme.textSecondary
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.body
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            visible: root.missionName !== ""
                            text: root.missionName
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            font.letterSpacing: Design.Typography.trackingWide
                            elide: Text.ElideRight
                        }
                    }

                    Design.IronBadge {
                        id: durationChip

                        anchors.right: parent.right
                        anchors.top: headlineColumn.top
                        visible: root.durationText !== ""
                        tone: Design.Theme.textSecondary
                        text: root.durationText
                    }
                }

                Design.IronDivider {
                    width: parent.width
                }

                Column {
                    width: parent.width
                    visible: root.showTiles
                    spacing: Design.Metrics.space8

                    Text {
                        text: qsTr("YOUR COMMAND")
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Row {
                        id: tileRow

                        readonly property real tileWidth: (width - spacing * (root.statColumns.length - 1)) / root.statColumns.length

                        width: parent.width
                        spacing: Design.Metrics.space8

                        Repeater {
                            model: root.statColumns

                            delegate: Rectangle {
                                id: tile

                                required property var modelData

                                objectName: "battleReportTile"

                                readonly property real best: root.column_best(tile.modelData.key)
                                readonly property real value: root.localArmy ? (Number(root.localArmy[tile.modelData.key]) || 0) : 0

                                width: tileRow.tileWidth
                                height: tileColumn.implicitHeight + Design.Metrics.space16
                                radius: Design.Metrics.radiusSmall
                                color: Design.Theme.panelIron
                                border.width: Design.Metrics.borderThin
                                border.color: Design.Theme.borderSubtle
                                Accessible.role: Accessible.StaticText
                                Accessible.name: tile.modelData.label + " " + root.figure(root.localArmy, tile.modelData.key)

                                Column {
                                    id: tileColumn

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: Design.Metrics.space12
                                    anchors.rightMargin: Design.Metrics.space12
                                    spacing: Design.Metrics.space8

                                    Row {
                                        width: parent.width
                                        spacing: Design.Metrics.space8

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: tile.modelData.glyph
                                            color: Design.Theme.accent
                                            font.family: Design.Typography.family
                                            font.pixelSize: Design.Typography.subheading
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - parent.spacing - Design.Typography.subheading
                                            text: root.figure(root.localArmy, tile.modelData.key)
                                            color: Design.Theme.textPrimary

                                            font.family: Design.Typography.titleFamily
                                            font.pixelSize: Design.Typography.heading
                                            font.weight: Design.Typography.bold
                                            fontSizeMode: Text.HorizontalFit
                                            minimumPixelSize: Design.Typography.minimumSize
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: Design.Metrics.space4
                                        radius: height / 2
                                        color: Design.Theme.backgroundDeep
                                        visible: root.armies.length > 1

                                        Rectangle {
                                            width: parent.width * root.share(tile.value, tile.best)
                                            height: parent.height
                                            radius: parent.radius
                                            color: tile.modelData.grim ? Design.Theme.danger : Design.Theme.accent
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: tile.modelData.label
                                        color: Design.Theme.textSecondary
                                        font.family: Design.Typography.family
                                        font.pixelSize: Design.Typography.caption
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    visible: root.armies.length > 0
                    spacing: Design.Metrics.space8

                    Text {
                        text: qsTr("ARMIES OF THE FIELD")
                        color: Design.Theme.textSecondary
                        font.family: Design.Typography.family
                        font.pixelSize: Design.Typography.caption
                        font.weight: Design.Typography.bold
                        font.letterSpacing: Design.Typography.trackingWide
                    }

                    Item {
                        width: parent.width
                        height: headerRow.implicitHeight

                        Row {
                            id: headerRow

                            anchors.right: parent.right
                            anchors.rightMargin: Design.Metrics.space12
                            spacing: 0

                            Repeater {
                                model: root.visibleColumns

                                delegate: Text {
                                    required property var modelData

                                    width: root.figureCellWidth
                                    text: modelData.label
                                    color: Design.Theme.textDisabled
                                    font.family: Design.Typography.family
                                    font.pixelSize: Design.Typography.caption
                                    horizontalAlignment: Text.AlignRight
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                width: root.scoreCellWidth
                                text: qsTr("Score")
                                color: Design.Theme.accent
                                font.family: Design.Typography.family
                                font.pixelSize: Design.Typography.caption
                                font.weight: Design.Typography.bold
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Design.Metrics.space4 + Design.Metrics.space12
                            anchors.right: headerRow.left
                            anchors.rightMargin: Design.Metrics.space12
                            anchors.verticalCenter: headerRow.verticalCenter
                            text: qsTr("Army")
                            color: Design.Theme.textDisabled
                            font.family: Design.Typography.family
                            font.pixelSize: Design.Typography.caption
                            elide: Text.ElideRight
                        }
                    }

                    Repeater {
                        model: root.armies

                        delegate: Rectangle {
                            id: armyRow

                            required property var modelData
                            required property int index

                            objectName: "battleReportRow"

                            readonly property color accent: root.army_accent(armyRow.modelData)

                            width: sheetColumn.width
                            height: rowBody.implicitHeight + Design.Metrics.space16
                            radius: Design.Metrics.radiusSmall
                            color: armyRow.modelData.isLocal ? Design.Theme.backgroundRaised : "transparent"
                            border.width: armyRow.modelData.isLocal ? Design.Metrics.borderThin : 0
                            border.color: Design.Theme.borderStrong
                            Accessible.role: Accessible.StaticText
                            Accessible.name: armyRow.modelData.name + " " + root.verdict(armyRow.modelData)
                            Accessible.description: root.spoken_figures(armyRow.modelData)

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * root.share(armyRow.modelData.score, root.column_best("score"))
                                radius: parent.radius
                                visible: root.armies.length > 1

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal

                                    GradientStop {
                                        position: 0
                                        color: Qt.rgba(armyRow.accent.r, armyRow.accent.g, armyRow.accent.b, 0.22)
                                    }

                                    GradientStop {
                                        position: 1
                                        color: Qt.rgba(armyRow.accent.r, armyRow.accent.g, armyRow.accent.b, 0)
                                    }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: Design.Metrics.borderThin
                                visible: armyRow.index < root.armies.length - 1
                                color: Design.Theme.borderSubtle
                                opacity: 0.35
                            }

                            Rectangle {
                                id: armyStripe

                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.topMargin: Design.Metrics.space4
                                anchors.bottomMargin: Design.Metrics.space4
                                width: Design.Metrics.space4
                                radius: width / 2
                                color: armyRow.accent
                            }

                            Item {
                                id: rowBody

                                anchors.left: armyStripe.right
                                anchors.right: parent.right
                                anchors.leftMargin: Design.Metrics.space12
                                anchors.rightMargin: Design.Metrics.space12
                                anchors.verticalCenter: parent.verticalCenter
                                implicitHeight: Math.max(nameColumn.implicitHeight, figureRow.implicitHeight)
                                height: implicitHeight

                                Column {
                                    id: nameColumn

                                    anchors.left: parent.left
                                    anchors.right: figureRow.left
                                    anchors.rightMargin: Design.Metrics.space12
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Design.Metrics.space4

                                    Row {
                                        width: parent.width
                                        spacing: Design.Metrics.space8

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: Math.min(implicitWidth, parent.width - (youBadge.visible ? youBadge.width + parent.spacing : 0))
                                            text: armyRow.modelData.name
                                            color: Design.Theme.textPrimary
                                            font.family: Design.Typography.displayFamily
                                            font.pixelSize: Design.Typography.subheading
                                            font.weight: Design.Typography.bold
                                            elide: Text.ElideRight
                                        }

                                        Design.IronBadge {
                                            id: youBadge

                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: armyRow.modelData.isLocal === true
                                            text: qsTr("YOU")
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        spacing: Design.Metrics.space8

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: root.verdict(armyRow.modelData)
                                            color: armyRow.modelData.isWinner ? Design.Theme.success : Design.Theme.danger
                                            font.family: Design.Typography.family
                                            font.pixelSize: Design.Typography.caption
                                            font.weight: Design.Typography.medium
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: Math.min(implicitWidth, parent.width / 2)
                                            visible: text !== ""
                                            text: armyRow.modelData.factionName ? armyRow.modelData.factionName : ""
                                            color: Design.Theme.textDisabled
                                            font.family: Design.Typography.family
                                            font.pixelSize: Design.Typography.caption
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                Row {
                                    id: figureRow

                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 0

                                    Repeater {
                                        model: root.visibleColumns

                                        delegate: Text {
                                            required property var modelData

                                            objectName: "battleReportFigure"
                                            width: root.figureCellWidth
                                            text: root.figure(armyRow.modelData, modelData.key)
                                            color: Design.Theme.textPrimary
                                            font.family: Design.Typography.family
                                            font.pixelSize: Design.Typography.label
                                            font.weight: Design.Typography.medium
                                            horizontalAlignment: Text.AlignRight
                                            fontSizeMode: Text.HorizontalFit
                                            minimumPixelSize: Design.Typography.minimumSize
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        objectName: "battleReportScore"
                                        width: root.scoreCellWidth
                                        text: root.figure(armyRow.modelData, "score")
                                        color: Design.Theme.accent
                                        font.family: Design.Typography.titleFamily
                                        font.pixelSize: Design.Typography.subheading
                                        font.weight: Design.Typography.bold
                                        horizontalAlignment: Text.AlignRight
                                        fontSizeMode: Text.HorizontalFit
                                        minimumPixelSize: Design.Typography.minimumSize
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Design.IronDivider {
            id: footerRule

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: reportFooter.top
            anchors.bottomMargin: Design.Metrics.space16
        }

        Item {
            id: reportFooter

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: actionRow.implicitHeight

            Text {
                anchors.left: parent.left
                anchors.right: actionRow.left
                anchors.rightMargin: Design.Metrics.space12
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.compact
                text: qsTr("Esc closes the report")
                color: Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                elide: Text.ElideRight
            }

            Row {
                id: actionRow

                anchors.right: parent.right
                spacing: Design.Metrics.space8

                Design.IronButton {
                    objectName: "battleReportClose"
                    text: root.secondaryAction
                    onClicked: root.dismissed()
                }

                Design.IronButton {
                    objectName: "battleReportMenu"
                    text: root.primaryAction
                    tone: "primary"
                    onClicked: root.menuRequested()
                }
            }
        }
    }

    opacity: 0
    Component.onCompleted: opacity = 1

    Behavior on opacity  {
        NumberAnimation {
            duration: Design.Motion.deliberate
            easing.type: Design.Motion.emphasizedEasing
        }
    }
}

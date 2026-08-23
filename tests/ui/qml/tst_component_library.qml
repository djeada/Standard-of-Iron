import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "ComponentLibrary"
    when: windowShown
    width: 640
    height: 480
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function test_a_progress_fill_does_not_sweep_when_the_bar_is_laid_out() {
        var host = Qt.createQmlObject('import QtQuick 2.15; import StandardOfIron.Design 1.0 as Design; Item { width: 0; height: 20; property alias bar: inner; Design.IronProgressBar { id: inner; anchors.fill: parent; value: 0.75 } }', testCase, "progressLayoutHost");
        verify(host !== null);
        wait(0);
        host.width = 200;
        wait(0);
        var fill = host.bar.contentItem.children[0];
        verify(fill !== null, "progress bar should expose its fill rectangle");
        fuzzyCompare(fill.width, 150.0, 1.0, "the fill must follow the layout immediately, not animate up from empty");
        host.destroy();
    }

    function test_a_progress_fill_still_animates_a_real_value_change() {
        var host = Qt.createQmlObject('import QtQuick 2.15; import StandardOfIron.Design 1.0 as Design; Item { width: 200; height: 20; property alias bar: inner; Design.IronProgressBar { id: inner; anchors.fill: parent; value: 0.0 } }', testCase, "progressValueHost");
        verify(host !== null);
        wait(0);
        host.bar.value = 1.0;
        var fill = host.bar.contentItem.children[0];
        verify(fill.width < 200.0, "a value change should still ease rather than jump");
        tryCompare(fill, "width", 200.0, 2000);
        host.destroy();
    }

    function test_every_published_control_instantiates_data() {
        return [{
                "tag": "IronPanel",
                "qml": "IronPanel {}"
            }, {
                "tag": "IronButton",
                "qml": "IronButton { text: \"Attack\" }"
            }, {
                "tag": "IronIconButton",
                "qml": "IronIconButton {}"
            }, {
                "tag": "IronTabBar",
                "qml": "IronTabBar {}"
            }, {
                "tag": "IronTooltip",
                "qml": "IronTooltip {}"
            }, {
                "tag": "IronCommandTooltip",
                "qml": "IronCommandTooltip { title: \"Guard\" }"
            }, {
                "tag": "IronDialog",
                "qml": "IronDialog {}"
            }, {
                "tag": "IronDropdown",
                "qml": "IronDropdown { model: [\"a\", \"b\"] }"
            }, {
                "tag": "IronSlider",
                "qml": "IronSlider {}"
            }, {
                "tag": "IronProgressBar",
                "qml": "IronProgressBar {}"
            }, {
                "tag": "IronListRow",
                "qml": "IronListRow {}"
            }, {
                "tag": "IronTreeRow",
                "qml": "IronTreeRow {}"
            }, {
                "tag": "IronContextMenu",
                "qml": "IronContextMenu {}"
            }, {
                "tag": "IronBadge",
                "qml": "IronBadge { text: \"3\" }"
            }, {
                "tag": "IronDivider",
                "qml": "IronDivider {}"
            }, {
                "tag": "IronSearchField",
                "qml": "IronSearchField {}"
            }, {
                "tag": "IronHotkeyLabel",
                "qml": "IronHotkeyLabel { text: \"Q\" }"
            }, {
                "tag": "IronUnitCard",
                "qml": "IronUnitCard { unitName: \"Spearmen\" }"
            }, {
                "tag": "IronCommandButton",
                "qml": "IronCommandButton {}"
            }, {
                "tag": "IronVectorIcon",
                "qml": "IronVectorIcon { iconId: \"repair\" }"
            }, {
                "tag": "IronActivityIcon",
                "qml": "IronActivityIcon { activity: \"chop_wood\" }"
            }, {
                "tag": "IronOutcomeOverlay",
                "qml": "IronOutcomeOverlay {}"
            }, {
                "tag": "IronObjectiveRow",
                "qml": "IronObjectiveRow {}"
            }, {
                "tag": "IronResourceCounter",
                "qml": "IronResourceCounter { amount: 12 }"
            }, {
                "tag": "IronInspectorSection",
                "qml": "IronInspectorSection {}"
            }, {
                "tag": "IronCheckBox",
                "qml": "IronCheckBox { text: \"Reduced motion\" }"
            }, {
                "tag": "IronSelectionSummary",
                "qml": "IronSelectionSummary {}"
            }, {
                "tag": "IronSpotlight",
                "qml": "IronSpotlight {}"
            }, {
                "tag": "IronNotification",
                "qml": "IronNotification { message: \"Ready\" }"
            }, {
                "tag": "NotificationHost",
                "qml": "NotificationHost {}"
            }, {
                "tag": "BriefingLayout",
                "qml": "BriefingLayout {}"
            }, {
                "tag": "OutcomeLayout",
                "qml": "OutcomeLayout {}"
            }, {
                "tag": "GameShell",
                "qml": "GameShell {}"
            }, {
                "tag": "ToolShell",
                "qml": "ToolShell {}"
            }, {
                "tag": "ComponentGallery",
                "qml": "ComponentGallery {}"
            }];
    }

    function test_every_published_control_instantiates(data) {
        var object = Qt.createQmlObject("import QtQuick 2.15\nimport StandardOfIron.Design 1.0\n" + data.qml, testCase, data.tag);
        verify(object !== null, data.tag + " failed to instantiate");
        object.destroy();
    }

    function test_button_exposes_its_disabled_reason_to_assistive_tech() {
        var button = buttonComponent.createObject(testCase, {
                "text": "Recruit",
                "enabled": false,
                "disabledReason": "Not enough supplies"
            });
        compare(button.Accessible.name, "Recruit");
        compare(button.Accessible.description, "Not enough supplies");
        button.destroy();
    }

    function test_focus_ring_can_be_forced_on_for_pointer_focus() {
        var button = buttonComponent.createObject(testCase, {
                "text": "Recruit"
            });
        button.forceActiveFocus(Qt.MouseFocusReason);
        verify(!button.showFocusRing, "pointer focus should stay quiet by default");
        Core.UiPreferences.alwaysShowFocus = true;
        verify(button.showFocusRing, "forced focus ring did not appear");
        button.destroy();
    }

    function test_ui_scale_grows_the_button_height() {
        var button = buttonComponent.createObject(testCase, {
                "text": "Recruit"
            });
        var baseHeight = button.implicitHeight;
        Core.UiPreferences.uiScale = 1.75;
        verify(button.implicitHeight > baseHeight, "button ignored the UI scale");
        button.destroy();
    }

    Component {
        id: buttonComponent

        IronButton {
        }
    }
}

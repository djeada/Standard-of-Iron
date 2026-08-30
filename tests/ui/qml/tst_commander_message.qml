import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "CommanderMessagePanel"
    when: windowShown
    width: 640
    height: 360
    visible: true

    function makeSource(text) {
        return sourceComponent.createObject(testCase, {
                "text": text
            });
    }

    function makePanel(source) {
        var panel = panelComponent.createObject(testCase, {
                "source": source
            });
        verify(panel !== null, "the commander message panel was not created");
        wait(1);
        return panel;
    }

    function test_the_panel_loads_without_a_dangling_signal_handler() {
        failOnWarning(/no signal of the target matches the name/);
        var source = testCase.makeSource("Hold the line.");
        var panel = testCase.makePanel(source);
        wait(1);
        panel.destroy();
        source.destroy();
    }

    function test_a_new_message_rewinds_the_typewriter() {
        var source = testCase.makeSource("Hold the line.");
        var panel = testCase.makePanel(source);
        panel.revealed = 9;
        compare(panel.revealed, 9);
        source.text = "The right flank is breaking.";
        wait(1);
        compare(panel.revealed, A11y.reducedMotion ? 9999 : 0, "the typewriter kept the previous message's progress");
        panel.destroy();
        source.destroy();
    }

    function test_the_panel_reads_the_text_it_reveals() {
        var source = testCase.makeSource("Advance.");
        var panel = testCase.makePanel(source);
        compare(panel.bodyText, "Advance.");
        panel.destroy();
        source.destroy();
    }

    function test_speech_animation_is_owned_by_the_portrait() {
        var source = testCase.makeSource("Advance.");
        var panel = testCase.makePanel(source);
        var portrait = findChild(panel, "commanderPortrait");
        verify(portrait !== null, "the commander portrait was not created");
        compare(portrait.talking, !A11y.reducedMotion, "the portrait should animate while text arrives");
        panel.revealed = source.text.length;
        compare(portrait.talking, false, "the portrait should close its mouth when the line is complete");
        panel.destroy();
        source.destroy();
    }

    Component {
        id: sourceComponent

        QtObject {
            property bool active: true
            property string text: ""
            property string nation: "carthage"
            property string speaker_id: "hannibal"
            property string pose: "neutral"
            property string speaker_name: "Hannibal"
            property string speaker_role: "Commander"
            property real duration: 4
            property bool holds_outcome: false

            function dismiss() {
            }
        }
    }

    Component {
        id: panelComponent

        CommanderMessagePanel {
        }
    }
}

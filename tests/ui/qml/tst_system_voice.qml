import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "SystemVoice"
    when: windowShown
    width: 200
    height: 200
    visible: true

    function reset() {
        Notifications.queue = [];
        voice.forget();
    }

    function spoken_lines() {
        return Notifications.queue.length;
    }

    SystemVoice {
        id: voice

        engine: null
    }

    function test_a_single_slip_is_left_to_the_cursor_chip() {
        testCase.reset();
        voice.note_refusal("no_selection");
        compare(testCase.spoken_lines(), 0, "one refusal is the cursor's business, not the system's");
    }

    function test_it_speaks_once_the_player_is_plainly_stuck() {
        testCase.reset();
        voice.note_refusal("unreachable");
        voice.note_refusal("unreachable");
        compare(testCase.spoken_lines(), 0, "two is still a slip");
        voice.note_refusal("unreachable");
        compare(testCase.spoken_lines(), 1, "the third identical refusal earns a word");
    }

    function test_it_does_not_keep_talking() {
        testCase.reset();
        for (var i = 0; i < 3; ++i)
            voice.note_refusal("manpower_cap");
        compare(testCase.spoken_lines(), 1);
        voice.note_refusal("manpower_cap");
        voice.note_refusal("manpower_cap");
        compare(testCase.spoken_lines(), 1, "it must earn the next line the same way it earned the first");
    }

    function test_a_different_mistake_starts_the_count_over() {
        testCase.reset();
        voice.note_refusal("unreachable");
        voice.note_refusal("unreachable");
        voice.note_refusal("unit_busy");
        compare(testCase.spoken_lines(), 0, "unrelated refusals must not add up to a lecture");
    }

    function test_getting_it_right_clears_the_slate() {
        testCase.reset();
        voice.note_refusal("unreachable");
        voice.note_refusal("unreachable");
        voice.forget();
        voice.note_refusal("unreachable");
        compare(testCase.spoken_lines(), 0, "an accepted order forgives what came before it");
    }

    function test_refusals_with_nothing_worth_saying_stay_quiet() {
        testCase.reset();
        for (var i = 0; i < 5; ++i)
            voice.note_refusal("command_unavailable");
        compare(testCase.spoken_lines(), 0, "the catch-all failure has no line, and must not invent one");
        testCase.reset();
        for (var j = 0; j < 5; ++j)
            voice.note_refusal("none");
        compare(testCase.spoken_lines(), 0, "a successful order is not a refusal");
    }

    function test_a_hauling_crew_is_told_at_once_and_loudly() {
        testCase.reset();
        voice.announce_busy("Hauling a load - it cannot be interrupted until the load is dropped off.");
        compare(testCase.spoken_lines(), 1, "a busy crew does not wait for the third refusal");
        compare(Notifications.current.priority, "urgent", "the refusal outranks the ambient chatter");
        compare(Notifications.current.channel, "refusal-unit_busy");
        voice.announce_busy("Hauling a load - it cannot be interrupted until the load is dropped off.");
        compare(testCase.spoken_lines(), 1, "repeats fold into the same card instead of stacking");
        compare(Notifications.current.repeats, 2);
    }

    function test_a_busy_refusal_with_no_words_stays_quiet() {
        testCase.reset();
        voice.announce_busy("");
        compare(testCase.spoken_lines(), 0);
    }

    function test_every_line_it_can_say_is_worth_saying() {
        var kinds = ["no_selection", "unreachable", "insufficient_resources", "manpower_cap", "unit_busy", "out_of_range", "wrong_owner", "invalid_target"];
        for (var i = 0; i < kinds.length; ++i) {
            var line = voice.quip_for(kinds[i]);
            verify(line.length > 0, "no line for " + kinds[i]);
            verify(line.trim() === line, "stray whitespace in the line for " + kinds[i]);
        }
    }
}

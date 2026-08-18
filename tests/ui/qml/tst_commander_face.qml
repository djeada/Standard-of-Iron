import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0 as Design
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "CommanderFaceOverlay"
    when: windowShown
    width: 640
    height: 480
    visible: true

    Item {
        id: stubAnchor

        width: 160
        height: 200

        property bool faceValid: true
        property real faceX: 0.5
        property real faceY: 0.4
        property real faceRadius: 0.25
        property real faceRoll: 0
        property real faceTurn: 0
        property real faceTilt: 0
        property real faceFacing: 1
    }

    CommanderFaceOverlay {
        id: face

        anchorSource: stubAnchor
    }

    function init() {
        stubAnchor.faceValid = true;
        stubAnchor.faceX = 0.5;
        stubAnchor.faceY = 0.4;
        stubAnchor.faceRadius = 0.25;
        stubAnchor.faceRoll = 0;
        stubAnchor.faceTurn = 0;
        stubAnchor.faceTilt = 0;
        stubAnchor.faceFacing = 1;
        face.talking = false;
    }

    function test_it_centres_on_the_published_head() {
        compare(face.headRadius, 50 * face.headScale, "head radius follows the portrait height");
        compare(face.width, face.headRadius * face.span, "the drawing is sized in head radii");
        compare(face.height, face.width, "the face is drawn square");
        compare(face.x, (0.5 * 160) - (face.width / 2));
        compare(face.y, (0.4 * 200) - (face.height / 2));
    }

    function test_it_follows_the_head_as_the_anchor_moves() {
        stubAnchor.faceX = 0.25;
        stubAnchor.faceY = 0.75;
        compare(face.x, (0.25 * 160) - (face.width / 2));
        compare(face.y, (0.75 * 200) - (face.height / 2));
    }

    function test_no_paint_without_an_anchor() {
        stubAnchor.faceValid = false;
        verify(!face.anchored);
        verify(!face.visible, "paint must not linger where the head was");
    }

    function test_it_hides_when_the_speaker_turns_away() {
        stubAnchor.faceFacing = -0.5;
        verify(!face.visible, "a painted face on the back of a head is worse than none");
        stubAnchor.faceFacing = 0.10;
        verify(face.visible);
        verify(face.opacity < 1.0, "it fades out through the profile rather than popping");
        stubAnchor.faceFacing = 1.0;
        compare(face.opacity, 1.0);
    }

    function test_projection_squashes_the_drawing() {
        stubAnchor.faceTurn = 0.6;
        fuzzyCompare(face.squashX, 0.8, 0.001);
        compare(face.squashY, 1.0);
        stubAnchor.faceTurn = 0;
        stubAnchor.faceTilt = -0.6;
        compare(face.squashX, 1.0);
        fuzzyCompare(face.squashY, 0.8, 0.001);
    }

    function test_a_tiny_head_is_not_worth_painting() {
        stubAnchor.faceRadius = 0.005;
        verify(!face.visible, "below a couple of pixels the paint is only noise");
    }

    function test_the_mouth_moves_while_the_line_arrives() {
        if (Design.A11y.reducedMotion)
            skip("reduced motion holds the face still on purpose");
        face.talking = true;
        tryVerify(function () {
                return face.mouthOpen > 0.05;
            }, 3000, "a speaking commander has to move their mouth");
    }

    function test_the_helmet_allowance_only_scales_the_drawing() {
        var before = face.x + (face.width / 2);
        face.headScale = 1.0;
        compare(face.headRadius, 50);
        compare(face.x + (face.width / 2), before, "the centre does not move");
        face.headScale = 1.3;
    }

    function test_the_mouth_closes_when_the_line_ends() {
        face.talking = true;
        face.mouthOpen = 0.8;
        face.talking = false;
        compare(face.mouthOpen, 0, "the mouth must not be left hanging open");
        compare(face.mouthWide, 0.4);
    }
}

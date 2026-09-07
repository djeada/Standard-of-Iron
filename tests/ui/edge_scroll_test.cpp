#include <cmath>
#include <gtest/gtest.h>

#include "app/core/user_settings.h"
#include "ui/edge_scroll.h"

namespace {

namespace Edge = Ui::EdgeScrollGeometry;

constexpr double k_width = 1280.0;
constexpr double k_height = 720.0;
constexpr double k_default_sensitivity = 1.0;
constexpr double k_default_scale = 1.0;

auto at(double x,
        double y,
        double sensitivity = k_default_sensitivity,
        double scale = k_default_scale) -> Edge::Vector {
  return Edge::vector_at(x, y, k_width, k_height, sensitivity, scale);
}

TEST(EdgeScrollTest, EveryScreenEdgePushesTheCameraTheRightWay) {
  const auto left = at(0.0, k_height / 2.0);
  EXPECT_LT(left.dx, 0.0);
  EXPECT_DOUBLE_EQ(left.dz, 0.0);

  const auto right = at(k_width, k_height / 2.0);
  EXPECT_GT(right.dx, 0.0);
  EXPECT_DOUBLE_EQ(right.dz, 0.0);

  const auto top = at(k_width / 2.0, 0.0);
  EXPECT_DOUBLE_EQ(top.dx, 0.0);
  EXPECT_GT(top.dz, 0.0);

  const auto bottom = at(k_width / 2.0, k_height);
  EXPECT_DOUBLE_EQ(bottom.dx, 0.0);
  EXPECT_LT(bottom.dz, 0.0);
}

TEST(EdgeScrollTest, EveryCornerPushesOnBothAxes) {
  const auto top_left = at(0.0, 0.0);
  EXPECT_LT(top_left.dx, 0.0);
  EXPECT_GT(top_left.dz, 0.0);

  const auto top_right = at(k_width, 0.0);
  EXPECT_GT(top_right.dx, 0.0);
  EXPECT_GT(top_right.dz, 0.0);

  const auto bottom_left = at(0.0, k_height);
  EXPECT_LT(bottom_left.dx, 0.0);
  EXPECT_LT(bottom_left.dz, 0.0);

  const auto bottom_right = at(k_width, k_height);
  EXPECT_GT(bottom_right.dx, 0.0);
  EXPECT_LT(bottom_right.dz, 0.0);
}

TEST(EdgeScrollTest, TheLastPixelOfEveryEdgeStillScrolls) {
  EXPECT_LT(at(1.0, k_height / 2.0).dx, 0.0);
  EXPECT_GT(at(k_width - 1.0, k_height / 2.0).dx, 0.0);
  EXPECT_GT(at(k_width / 2.0, 1.0).dz, 0.0);
  EXPECT_LT(at(k_width / 2.0, k_height - 1.0).dz, 0.0);
}

TEST(EdgeScrollTest, TheMiddleOfTheScreenNeverScrolls) {
  EXPECT_TRUE(at(k_width / 2.0, k_height / 2.0).is_zero());
  EXPECT_TRUE(at(200.0, 400.0).is_zero());
  EXPECT_TRUE(at(k_width - 200.0, 300.0).is_zero());
}

TEST(EdgeScrollTest, PushHardensAsTheCursorNearsTheEdge) {
  const double zone = Edge::horizontal_zone(k_default_sensitivity, k_default_scale);
  const double outer = at(zone * 0.75, k_height / 2.0).dx;
  const double middle = at(zone * 0.4, k_height / 2.0).dx;
  const double inner = at(0.0, k_height / 2.0).dx;

  EXPECT_LT(outer, 0.0);
  EXPECT_LT(middle, outer);
  EXPECT_LT(inner, middle);
}

TEST(EdgeScrollTest, AnUnknownCursorPositionNeverScrolls) {
  EXPECT_TRUE(at(-1.0, -1.0).is_zero());
  EXPECT_TRUE(at(-1.0, k_height / 2.0).is_zero());
  EXPECT_TRUE(at(k_width / 2.0, -1.0).is_zero());

  EXPECT_TRUE(
      Edge::vector_at(std::nan(""), 10.0, k_width, k_height, 1.0, 1.0).is_zero());
  EXPECT_TRUE(Edge::vector_at(10.0, 10.0, 0.0, 0.0, 1.0, 1.0).is_zero());
  EXPECT_TRUE(Edge::vector_at(10.0, 10.0, -5.0, k_height, 1.0, 1.0).is_zero());
}

TEST(EdgeScrollTest, ACursorPastTheSurfaceNeverScrolls) {
  EXPECT_TRUE(at(k_width + 8.0, k_height / 2.0).is_zero());
  EXPECT_TRUE(at(k_width / 2.0, k_height + 8.0).is_zero());
}

TEST(EdgeScrollTest, SensitivityWidensTheZoneAndTheStep) {
  const double slow_zone = Edge::horizontal_zone(0.25, 1.0);
  const double fast_zone = Edge::horizontal_zone(2.0, 1.0);
  EXPECT_LT(slow_zone, fast_zone);

  const double slow = at(0.0, k_height / 2.0, 0.25).dx;
  const double fast = at(0.0, k_height / 2.0, 2.0).dx;
  EXPECT_LT(fast, slow);

  const double probe = 20.0;
  EXPECT_TRUE(at(probe, k_height / 2.0, 0.25).is_zero());
  EXPECT_LT(at(probe, k_height / 2.0, 2.0).dx, 0.0);
}

TEST(EdgeScrollTest, AScaledUpInterfaceGetsAProportionallyWiderZone) {
  const double normal = Edge::horizontal_zone(1.0, 1.0);
  const double doubled = Edge::horizontal_zone(1.0, 2.0);
  EXPECT_DOUBLE_EQ(doubled, normal * 2.0);

  const double vertical = Edge::vertical_zone(1.0, 2.0);
  EXPECT_DOUBLE_EQ(vertical, Edge::vertical_zone(1.0, 1.0) * 2.0);

  const double probe = normal + 2.0;
  EXPECT_TRUE(at(probe, k_height / 2.0, 1.0, 1.0).is_zero());
  EXPECT_LT(at(probe, k_height / 2.0, 1.0, 2.0).dx, 0.0);
}

TEST(EdgeScrollTest, TheZoneNeverCollapsesToNothing) {
  EXPECT_GE(Edge::horizontal_zone(0.0, 1.0), Edge::k_min_zone);
  EXPECT_GE(Edge::vertical_zone(0.0, 0.0), Edge::k_min_zone);
  EXPECT_GE(Edge::horizontal_zone(-4.0, -4.0), Edge::k_min_zone);

  EXPECT_LT(at(0.0, k_height / 2.0, 0.0, 0.0).dx, 0.0);
}

TEST(EdgeScrollTest, AHighResolutionSurfaceStillScrollsAtItsEdges) {
  constexpr double k_wide = 3840.0;
  constexpr double k_tall = 2160.0;

  const auto left = Edge::vector_at(0.0, k_tall / 2.0, k_wide, k_tall, 1.0, 2.0);
  const auto right = Edge::vector_at(k_wide, k_tall / 2.0, k_wide, k_tall, 1.0, 2.0);
  const auto top = Edge::vector_at(k_wide / 2.0, 0.0, k_wide, k_tall, 1.0, 2.0);
  const auto bottom = Edge::vector_at(k_wide / 2.0, k_tall, k_wide, k_tall, 1.0, 2.0);

  EXPECT_LT(left.dx, 0.0);
  EXPECT_GT(right.dx, 0.0);
  EXPECT_GT(top.dz, 0.0);
  EXPECT_LT(bottom.dz, 0.0);
  EXPECT_TRUE(
      Edge::vector_at(k_wide / 2.0, k_tall / 2.0, k_wide, k_tall, 1.0, 2.0).is_zero());
}

TEST(EdgeScrollTest, TheBandIsWideEnoughToAimAt) {
  const auto why =
      "a band you have to hunt for is the single loudest complaint about edge "
      "scrolling. The default reaches 26 logical px on every side, and the "
      "weakest setting still leaves a band you can land on without looking. "
      "The minimap no longer needs this band to stay narrow: HUD.blocks_edge_"
      "scroll() refuses the minimap's own rectangle outright and "
      "mainWindow.edge_scroll_disabled covers a drag that leaves it. See "
      "docs/CAMERA_CONTROLS.md.";

  EXPECT_GE(Edge::horizontal_zone(1.0, 1.0), 24.0) << why;
  EXPECT_GE(Edge::vertical_zone(1.0, 1.0), 24.0) << why;

  const double weakest = App::Core::UserSettings::kMinEdgeScrollSensitivity;
  EXPECT_GE(Edge::horizontal_zone(weakest, 1.0), Edge::k_min_zone) << why;
  EXPECT_GE(Edge::vertical_zone(weakest, 1.0), Edge::k_min_zone) << why;
}

TEST(EdgeScrollTest, TopAndBottomAreAsReachableAsTheSides) {
  const auto why =
      "the vertical band used to be 10 px against 12 horizontally and ramped "
      "as a cube against a square, so the middle of the top band moved the "
      "camera at an eighth of the pace the middle of a side band did. Pushing "
      "up read as broken. Keep the two axes symmetric.";

  EXPECT_DOUBLE_EQ(Edge::vertical_zone(1.0, 1.0), Edge::horizontal_zone(1.0, 1.0))
      << why;

  const double zone = Edge::vertical_zone(1.0, 1.0);
  const auto side = at(zone / 2.0, k_height / 2.0);
  const auto top = at(k_width / 2.0, zone / 2.0);
  EXPECT_DOUBLE_EQ(top.dz, -side.dx) << why;
}

TEST(EdgeScrollTest, CrossingIntoTheBandMovesTheCameraAtOnce) {
  const auto why =
      "a ramp that starts at zero gives the outer half of the band no usable "
      "speed, which is indistinguishable from a band half as wide. Entering "
      "the band commits to a visible push.";

  const double zone = Edge::horizontal_zone(1.0, 1.0);
  const double just_inside = at(zone - 0.5, k_height / 2.0).dx;
  EXPECT_LT(just_inside, 0.0) << why;
  EXPECT_GE(std::abs(just_inside), Edge::k_entry_push * 0.9) << why;
  EXPECT_LT(std::abs(just_inside), 1.0) << why;
}

TEST(EdgeScrollTest, ACornerIsNoFasterThanAnEdge) {
  const auto why =
      "the two axes are summed, so an unclamped corner runs the camera sqrt(2) "
      "times faster than either edge next to it and the view lurches as the "
      "cursor rounds it.";

  const double edge = std::abs(at(0.0, k_height / 2.0).dx);
  for (const auto& corner :
       {at(0.0, 0.0), at(k_width, 0.0), at(0.0, k_height), at(k_width, k_height)}) {
    EXPECT_NEAR(std::hypot(corner.dx, corner.dz), edge, 1e-9) << why;
  }
}

TEST(EdgeScrollTest, TheStrongestSettingStillHasAnUnscrolledMiddle) {
  const double strongest = App::Core::UserSettings::kMaxEdgeScrollSensitivity;
  for (const double scale : {1.0, 1.5, 2.0}) {
    EXPECT_TRUE(Edge::vector_at(
                    k_width / 2.0, k_height / 2.0, k_width, k_height, strongest, scale)
                    .is_zero());
  }
}

TEST(EdgeScrollTest, OppositeEdgesAreMirrorImages) {
  const auto left = at(3.0, k_height / 2.0);
  const auto right = at(k_width - 3.0, k_height / 2.0);
  EXPECT_DOUBLE_EQ(left.dx, -right.dx);

  const auto top = at(k_width / 2.0, 3.0);
  const auto bottom = at(k_width / 2.0, k_height - 3.0);
  EXPECT_DOUBLE_EQ(top.dz, -bottom.dz);
}

} // namespace

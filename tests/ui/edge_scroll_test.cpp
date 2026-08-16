#include <cmath>
#include <gtest/gtest.h>
#include <string>

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

TEST(EdgeScrollTest, TheStrongestBandStaysClearOfTheMinimap) {
  constexpr double k_minimap_hit_area_ends_px_from_right = 25.0;
  constexpr double k_widest_band = 24.0;
  const double strongest = App::Core::UserSettings::kMaxEdgeScrollSensitivity;

  const auto why = [](double scale) {
    return "at interface scale " + std::to_string(scale) +
           ": the minimap is itself a camera move, so a band reaching under it "
           "leaves a minimap drag and edge scroll pushing the same camera. Its "
           "hit area ends 25 logical px from the right (hudZoneMargin plus the "
           "panel's padding), measured by hand in a running battle, and the "
           "widest band is 24 - one pixel of headroom. See the minimap "
           "clearance in docs/CAMERA_CONTROLS.md and re-measure the QML side.";
  };

  EXPECT_DOUBLE_EQ(Edge::horizontal_zone(strongest, 1.0), k_widest_band);

  for (const double scale : {1.0, 1.5, 2.0}) {
    EXPECT_LT(Edge::horizontal_zone(strongest, scale),
              k_minimap_hit_area_ends_px_from_right * scale)
        << why(scale);

    const double minimap_edge =
        k_width - (k_minimap_hit_area_ends_px_from_right * scale);
    EXPECT_TRUE(Edge::vector_at(
                    minimap_edge, k_height / 2.0, k_width, k_height, strongest, scale)
                    .is_zero())
        << why(scale);
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

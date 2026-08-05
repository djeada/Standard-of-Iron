#include <cmath>
#include <gtest/gtest.h>
#include <set>
#include <vector>

#include "render/geom/mode_indicator.h"

namespace {

using Render::Geom::GlyphLayer;
using Render::Geom::IndicatorKind;
using Render::Geom::IndicatorState;

auto layers_used(const Render::Geom::GlyphBuilder& builder) -> std::set<int> {
  std::set<int> layers;
  for (auto const& vertex : builder.vertices()) {
    layers.insert(static_cast<int>(vertex.tex_coord[0]));
  }
  return layers;
}

auto all_kinds() -> std::vector<IndicatorKind> {
  std::vector<IndicatorKind> kinds;
  kinds.reserve(Render::Geom::k_indicator_kind_count);
  for (std::size_t i = 0; i < Render::Geom::k_indicator_kind_count; ++i) {
    kinds.push_back(static_cast<IndicatorKind>(i));
  }
  return kinds;
}

TEST(ModeIndicator, CoversEveryActivityTheSimulationCanReport) {
  EXPECT_EQ(Render::Geom::k_indicator_kind_count, 16U);

  for (IndicatorKind const kind : all_kinds()) {
    auto const id = Game::Systems::activity_kind_id(kind);
    EXPECT_EQ(Game::Systems::activity_kind_from_id(id), kind)
        << "activity id round trip failed for " << id;
  }
}

TEST(ModeIndicator, EveryNoteworthyActivityHasGlyphGeometry) {
  for (IndicatorKind const kind : all_kinds()) {
    Game::Systems::UnitActivity activity{};
    activity.kind = kind;
    if (!Game::Systems::activity_is_noteworthy(activity)) {
      continue;
    }
    ASSERT_TRUE(Render::Geom::indicator_has_glyph(kind))
        << "no glyph for activity " << Game::Systems::activity_kind_id(kind);

    auto const builder = Render::Geom::build_indicator_glyph(kind);
    auto const layers = layers_used(builder);

    EXPECT_GT(builder.triangle_count(), 0U);
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Glyph)))
        << "activity " << Game::Systems::activity_kind_id(kind) << " has no icon face";
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::GlyphSide)))
        << "activity " << Game::Systems::activity_kind_id(kind)
        << " is a flat plate, not a solid";
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Outline)))
        << "activity " << Game::Systems::activity_kind_id(kind)
        << " has no silhouette to read against terrain";
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Glow)))
        << "activity " << Game::Systems::activity_kind_id(kind) << " has no glow";
  }
}

TEST(ModeIndicator, ItemsShareOneNormalisedFootprint) {
  for (IndicatorKind const kind : all_kinds()) {
    auto const builder = Render::Geom::build_indicator_glyph(kind);
    float extent = 0.0F;
    float depth = 0.0F;
    for (auto const& vertex : builder.vertices()) {
      extent = std::max(extent, std::abs(vertex.position[0]));
      extent = std::max(extent, std::abs(vertex.position[1]));
      depth = std::max(depth, std::abs(vertex.position[2]));
    }
    EXPECT_NEAR(extent, 0.5F, 1.0e-3F)
        << "activity " << Game::Systems::activity_kind_id(kind)
        << " would render at a different world size than its neighbours";
    EXPECT_GT(depth, 0.01F) << "activity " << Game::Systems::activity_kind_id(kind)
                            << " has no thickness";
  }
}

TEST(ModeIndicator, EveryActivityIsVisuallyDistinct) {
  std::vector<QVector3D> seen;
  for (IndicatorKind const kind : all_kinds()) {
    QVector3D const color = Render::Geom::indicator_base_color(kind);
    for (QVector3D const& other : seen) {
      EXPECT_GT((color - other).lengthSquared(), 0.01F)
          << "two activities share a colour";
    }
    seen.push_back(color);
  }
}

TEST(ModeIndicator, FaultStatesShiftTheColourAwayFromTheActiveOne) {
  QVector3D const active =
      Render::Geom::indicator_color(IndicatorKind::ChopWood, IndicatorState::Active);
  QVector3D const queued =
      Render::Geom::indicator_color(IndicatorKind::ChopWood, IndicatorState::Queued);
  QVector3D const unavailable = Render::Geom::indicator_color(
      IndicatorKind::ChopWood, IndicatorState::Unavailable);
  QVector3D const interrupted = Render::Geom::indicator_color(
      IndicatorKind::ChopWood, IndicatorState::Interrupted);

  EXPECT_GT((active - queued).lengthSquared(), 0.001F);
  EXPECT_GT((active - unavailable).lengthSquared(), 0.05F);
  EXPECT_GT((active - interrupted).lengthSquared(), 0.05F);
  EXPECT_GT(unavailable.x(), unavailable.y());
  EXPECT_LT(Render::Geom::indicator_state_alpha(IndicatorState::Queued),
            Render::Geom::indicator_state_alpha(IndicatorState::Active));
}

TEST(ModeIndicator, DrawsEveryOrderAtOneWorldSize) {
  EXPECT_GT(Render::Geom::indicator_world_size(), 0.0F);
  EXPECT_FLOAT_EQ(Render::Geom::indicator_world_size(),
                  Render::Geom::k_indicator_world_size);
}

TEST(ModeIndicator, StampsEveryActivityOnTheSameMedallion) {
  for (IndicatorKind const kind : all_kinds()) {
    auto const builder = Render::Geom::build_indicator_glyph(kind);
    auto const layers = layers_used(builder);
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Plaque)))
        << "activity " << Game::Systems::activity_kind_id(kind) << " has no plaque";
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Rim)))
        << "activity " << Game::Systems::activity_kind_id(kind)
        << " has no coloured rim";
    EXPECT_TRUE(layers.contains(static_cast<int>(GlyphLayer::Shadow)))
        << "activity " << Game::Systems::activity_kind_id(kind)
        << " has nothing to lift it off the terrain";
  }
}

TEST(ModeIndicator, FitsEveryIconToTheSameRadiusInsideTheMedallion) {
  float smallest = 1.0e9F;
  float largest = 0.0F;
  for (IndicatorKind const kind : all_kinds()) {
    auto const builder = Render::Geom::build_indicator_glyph(kind);
    float radius_sq = 0.0F;
    for (auto const& vertex : builder.vertices()) {
      auto const layer = static_cast<int>(vertex.tex_coord[0]);
      if (layer != static_cast<int>(GlyphLayer::Glyph) &&
          layer != static_cast<int>(GlyphLayer::Accent) &&
          layer != static_cast<int>(GlyphLayer::GlyphSide)) {
        continue;
      }
      radius_sq = std::max(radius_sq,
                           vertex.position[0] * vertex.position[0] +
                               vertex.position[1] * vertex.position[1]);
    }

    float const radius = std::sqrt(radius_sq);

    EXPECT_LT(radius, 0.318F) << "activity " << Game::Systems::activity_kind_id(kind)
                              << " overflows the medallion face";
    smallest = std::min(smallest, radius);
    largest = std::max(largest, radius);
  }

  EXPECT_GT(smallest, 0.20F);
  EXPECT_LT(largest - smallest, 0.03F)
      << "one activity reads at a noticeably different size than the others";
}

TEST(ModeIndicator, RaisesMarkersForTallerUnitsWithoutResizingThem) {
  float const infantry_height = Render::Geom::indicator_height_for_unit(1.1F, 0.6F);
  float const cavalry_height = Render::Geom::indicator_height_for_unit(2.0F, 0.8F);
  float const elephant_height = Render::Geom::indicator_height_for_unit(3.2F, 2.1F);

  EXPECT_GT(cavalry_height, infantry_height);
  EXPECT_GT(elephant_height, cavalry_height);
}

TEST(ModeIndicator, KeepsClearanceForSmallUnits) {
  EXPECT_GE(Render::Geom::indicator_height_for_unit(0.4F, 0.5F),
            Render::Geom::k_indicator_height_base + Render::Geom::k_indicator_head_gap);
}

TEST(ModeIndicator, FadesOutBeyondTheReadableDistance) {
  EXPECT_FLOAT_EQ(Render::Geom::indicator_distance_fade(0.0F), 1.0F);
  EXPECT_FLOAT_EQ(
      Render::Geom::indicator_distance_fade(Render::Geom::k_indicator_fade_end_sq),
      0.0F);

  float const midpoint =
      Render::Geom::indicator_distance_fade((Render::Geom::k_indicator_fade_start_sq +
                                             Render::Geom::k_indicator_fade_end_sq) *
                                            0.5F);
  EXPECT_GT(midpoint, 0.0F);
  EXPECT_LT(midpoint, 1.0F);
}

} // namespace

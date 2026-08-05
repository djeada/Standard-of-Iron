#include <array>
#include <gtest/gtest.h>
#include <set>
#include <tuple>

#include "game/accessibility/team_identity.h"
#include "render/geom/ground_marker_pattern.h"

namespace {

using Game::Accessibility::TeamPattern;
using Render::Geom::ground_marker_angular_coverage;
using Render::Geom::ground_marker_pattern;

constexpr std::array<TeamPattern, 6> k_all_patterns{
    TeamPattern::Solid,
    TeamPattern::Dashed,
    TeamPattern::DoubleRing,
    TeamPattern::Notched,
    TeamPattern::Dotted,
    TeamPattern::Chevron,
};

auto spec_key(TeamPattern pattern) {
  const auto& spec = ground_marker_pattern(pattern);
  return std::make_tuple(spec.dash_count,
                         spec.dash_duty,
                         spec.second_ring_start,
                         spec.second_ring_end,
                         spec.tick_count,
                         spec.tick_length);
}

} // namespace

TEST(GroundMarkerPattern, EveryTeamPatternHasASpec) {
  EXPECT_EQ(Render::Geom::k_ground_marker_patterns.size(),
            static_cast<std::size_t>(Game::Accessibility::k_team_pattern_count));

  for (const auto pattern : k_all_patterns) {
    const auto& spec = ground_marker_pattern(pattern);
    EXPECT_GT(spec.dash_count, 0.0F);
    EXPECT_GT(spec.dash_duty, 0.0F);
    EXPECT_LE(spec.dash_duty, 1.0F);
  }
}

TEST(GroundMarkerPattern, NoTwoPatternsShareAShape) {
  std::set<decltype(spec_key(TeamPattern::Solid))> keys;
  for (const auto pattern : k_all_patterns) {
    EXPECT_TRUE(keys.insert(spec_key(pattern)).second)
        << "two team patterns draw the same shape; colour would be the only signal";
  }
  EXPECT_EQ(keys.size(), k_all_patterns.size());
}

TEST(GroundMarkerPattern, BrokenPatternsCoverLessArcThanSolid) {
  const float solid =
      ground_marker_angular_coverage(ground_marker_pattern(TeamPattern::Solid));

  for (const auto pattern :
       {TeamPattern::Dashed, TeamPattern::Notched, TeamPattern::Dotted}) {
    EXPECT_LT(ground_marker_angular_coverage(ground_marker_pattern(pattern)), solid)
        << "a broken ring must read as broken";
  }
}

TEST(GroundMarkerPattern, DecoratedPatternsAddASecondFeature) {
  const auto& double_ring = ground_marker_pattern(TeamPattern::DoubleRing);
  EXPECT_GT(double_ring.second_ring_end, double_ring.second_ring_start);
  EXPECT_LT(double_ring.second_ring_end, Render::Geom::k_marker_band_inner);
  EXPECT_GE(double_ring.second_ring_start, Render::Geom::k_marker_geometry_inner);

  const auto& chevron = ground_marker_pattern(TeamPattern::Chevron);
  EXPECT_GT(chevron.tick_count, 0.0F);
  EXPECT_GT(chevron.tick_length, 0.0F);
}

TEST(GroundMarkerPattern, GeometrySpanCoversEveryPatternFeature) {
  for (const auto pattern : k_all_patterns) {
    const auto& spec = ground_marker_pattern(pattern);
    if (spec.second_ring_end > spec.second_ring_start) {
      EXPECT_GE(spec.second_ring_start, Render::Geom::k_marker_geometry_inner)
          << "the inner band would fall outside the marker mesh";
    }
    if (spec.tick_count > 0.0F) {
      EXPECT_LE(Render::Geom::k_marker_band_outer + spec.tick_length,
                Render::Geom::k_marker_geometry_outer)
          << "ticks would fall outside the marker mesh";
    }
  }
}

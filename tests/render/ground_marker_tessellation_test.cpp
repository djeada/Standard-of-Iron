#include <cstddef>
#include <gtest/gtest.h>

#include "render/geom/ground_marker_tessellation.h"

namespace {

using Render::Geom::k_marker_segment_lods;
using Render::Geom::marker_angular_step;
using Render::Geom::marker_chord_world;
using Render::Geom::marker_segment_count;
using Render::Geom::marker_segment_lod;
using Render::Geom::marker_target_arc_world;

constexpr float k_tile = 1.0F;

constexpr float k_soldier_ring = 0.35F;
constexpr float k_unit_ring = 0.95F;
constexpr float k_bow_range = 9.0F;
constexpr float k_siege_range = 22.0F;

} // namespace

TEST(GroundMarkerTessellation, ChordStaysUnderTheTargetArcAtEveryRadius) {
  const float target = marker_target_arc_world(k_tile);

  const float resolvable =
      static_cast<float>(k_marker_segment_lods.back()) * target / (2.0F * 3.14159265F);

  for (float radius = 0.05F; radius <= resolvable; radius += 0.05F) {
    const std::size_t lod = marker_segment_lod(radius, k_tile);
    EXPECT_LE(marker_chord_world(radius, lod), target * 1.001F)
        << "radius " << radius << " samples the ground too coarsely to drape on it";
  }
}

TEST(GroundMarkerTessellation, SelectionRingsAreCheaperThanTheOldFixedRing) {

  EXPECT_LT(marker_segment_count(marker_segment_lod(k_soldier_ring, k_tile)), 96);
  EXPECT_LT(marker_segment_count(marker_segment_lod(k_unit_ring, k_tile)), 96);
}

TEST(GroundMarkerTessellation, RangeRingsGetMoreSegmentsThanTheOldFixedRing) {
  EXPECT_GT(marker_segment_count(marker_segment_lod(k_bow_range, k_tile)), 96);
  EXPECT_GT(marker_segment_count(marker_segment_lod(k_siege_range, k_tile)), 96);
}

TEST(GroundMarkerTessellation, LevelNeverFallsAsTheRadiusGrows) {
  std::size_t previous = 0;
  for (float radius = 0.0F; radius < 60.0F; radius += 0.1F) {
    const std::size_t lod = marker_segment_lod(radius, k_tile);
    EXPECT_GE(lod, previous) << "radius " << radius;
    previous = lod;
  }
  EXPECT_EQ(previous, k_marker_segment_lods.size() - 1U);
}

TEST(GroundMarkerTessellation, CoarserTerrainNeedsFewerSegments) {
  const std::size_t fine = marker_segment_lod(k_bow_range, 0.5F);
  const std::size_t coarse = marker_segment_lod(k_bow_range, 4.0F);
  EXPECT_LT(coarse, fine)
      << "a ring only has to follow the ground as finely as the ground is defined";
}

TEST(GroundMarkerTessellation, DegenerateInputsStayInRange) {
  for (const float world_per_cell : {0.0F, -1.0F, 1.0F, 1e6F}) {
    for (const float radius : {-1.0F, 0.0F, 1e6F}) {
      const std::size_t lod = marker_segment_lod(radius, world_per_cell);
      EXPECT_LT(lod, k_marker_segment_lods.size());
      EXPECT_GT(marker_segment_count(lod), 0);
      EXPECT_GT(marker_angular_step(lod), 0.0F);
    }
  }
  EXPECT_EQ(marker_segment_count(k_marker_segment_lods.size() + 7U),
            k_marker_segment_lods.back());
}

TEST(GroundMarkerTessellation, AngularStepMatchesTheSegmentCount) {
  for (std::size_t lod = 0; lod < k_marker_segment_lods.size(); ++lod) {
    const float full_turn =
        marker_angular_step(lod) * static_cast<float>(marker_segment_count(lod));
    EXPECT_NEAR(full_turn, 2.0F * 3.14159265F, 1e-4F);
  }
}

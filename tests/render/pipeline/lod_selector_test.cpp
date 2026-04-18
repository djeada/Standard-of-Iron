// Stage 6 — LodSelector tests.
//
// The LOD selector is the renderer's frame-budget instrument: it decides
// whether a unit renders at Full, Simplified, Minimal detail or is Culled,
// based purely on its inputs. These tests lock the policy so that later
// pipeline refactors can't silently flip it.

#include "render/pipeline/lod_selector.h"

#include <gtest/gtest.h>

using Render::Pipeline::LodInputs;
using Render::Pipeline::LodTier;
using Render::Pipeline::compute_full_detail_max_distance_sq;
using Render::Pipeline::select_lod;

TEST(LodSelector, CulledWhenOffFrustum) {
  LodInputs in;
  in.in_frustum = false;
  EXPECT_EQ(select_lod(in), LodTier::Culled);
}

TEST(LodSelector, CulledWhenFogHidden) {
  LodInputs in;
  in.in_frustum = true;
  in.fog_visible = false;
  EXPECT_EQ(select_lod(in), LodTier::Culled);
}

TEST(LodSelector, SelectedAlwaysFullEvenIfFar) {
  LodInputs in;
  in.distance_sq = 1e6F;
  in.selected = true;
  EXPECT_EQ(select_lod(in), LodTier::Full);
}

TEST(LodSelector, HoveredAlwaysFull) {
  LodInputs in;
  in.distance_sq = 1e6F;
  in.hovered = true;
  EXPECT_EQ(select_lod(in), LodTier::Full);
}

TEST(LodSelector, NeverBatchOverridesFarDistance) {
  LodInputs in;
  in.distance_sq = 1e6F;
  in.never_batch = true;
  EXPECT_EQ(select_lod(in), LodTier::Full);
}

TEST(LodSelector, ForceBatchingGivesSimplified) {
  LodInputs in;
  in.force_batching = true;
  EXPECT_EQ(select_lod(in), LodTier::Simplified);
}

TEST(LodSelector, NearIsFull) {
  LodInputs in;
  in.distance_sq = 100.0F; // well within 30^2 = 900
  EXPECT_EQ(select_lod(in), LodTier::Full);
}

TEST(LodSelector, MidRangeIsSimplified) {
  LodInputs in;
  in.distance_sq = 1200.0F; // past 900
  in.visible_unit_count = 50;
  EXPECT_EQ(select_lod(in), LodTier::Simplified);
}

TEST(LodSelector, HighPressureFarDropsToMinimal) {
  LodInputs in;
  in.distance_sq = 1200.0F;
  in.visible_unit_count = 500; // above the 420 knee
  EXPECT_EQ(select_lod(in), LodTier::Minimal);
}

TEST(LodSelector, VeryFarDropsToMinimalEvenAtLowPressure) {
  LodInputs in;
  in.full_detail_max_distance_sq = 900.0F;
  in.distance_sq = 900.0F * 5.0F; // > 4x threshold
  in.visible_unit_count = 10;
  EXPECT_EQ(select_lod(in), LodTier::Minimal);
}

TEST(LodSelector, ForceBatchingZerosThreshold) {
  const float sq = compute_full_detail_max_distance_sq(0.5F, true);
  EXPECT_FLOAT_EQ(sq, 0.0F);
}

TEST(LodSelector, BatchingRatioShrinksFullRange) {
  const float sq0 = compute_full_detail_max_distance_sq(0.0F, false);
  const float sq1 = compute_full_detail_max_distance_sq(1.0F, false);
  EXPECT_GT(sq0, sq1);
  EXPECT_FLOAT_EQ(sq0, 30.0F * 30.0F);
  // At ratio 1.0 the multiplier is (1 - 0.7) = 0.3, so d=9, sq=81.
  EXPECT_NEAR(sq1, 81.0F, 1e-3F);
}

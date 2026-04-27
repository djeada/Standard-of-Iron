// Regression tests for the unified declarative creature LOD decision module.
// Locks the policy so future preparation-stage refactors can't silently
// flip humanoid / horse / elephant LOD behavior.

#include "render/creature/pipeline/lod_decision.h"

#include <gtest/gtest.h>

using Render::Creature::CreatureLOD;
using Render::Creature::Pipeline::CreatureLodDecisionInputs;
using Render::Creature::Pipeline::CullReason;
using Render::Creature::Pipeline::decide_creature_lod;
using Render::Creature::Pipeline::LodDistanceThresholds;
using Render::Creature::Pipeline::select_distance_lod;
using Render::Creature::Pipeline::should_render_temporal;
using Render::Creature::Pipeline::TemporalSkipParams;

namespace {

constexpr LodDistanceThresholds kDefaults{12.0F, 20.0F, 40.0F};

auto make_inputs(float distance) -> CreatureLodDecisionInputs {
  CreatureLodDecisionInputs in{};
  in.has_camera = true;
  in.distance = distance;
  in.thresholds = kDefaults;
  return in;
}

} // namespace

TEST(CreatureLodDecision, SelectDistanceLodMatchesThresholds) {
  EXPECT_EQ(select_distance_lod(0.0F, kDefaults), CreatureLOD::Full);
  EXPECT_EQ(select_distance_lod(11.99F, kDefaults), CreatureLOD::Full);
  EXPECT_EQ(select_distance_lod(12.0F, kDefaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(19.99F, kDefaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(20.0F, kDefaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(39.99F, kDefaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(40.0F, kDefaults), CreatureLOD::Billboard);
  EXPECT_EQ(select_distance_lod(1000.0F, kDefaults), CreatureLOD::Billboard);
}

TEST(CreatureLodDecision, ForcedLodBypassesEverything) {
  auto in = make_inputs(1000.0F);
  in.forced_lod = CreatureLOD::Reduced;
  in.apply_visibility_budget = true;
  in.budget_grant_full = false;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Reduced);
}

TEST(CreatureLodDecision, NoCameraDefaultsToFull) {
  CreatureLodDecisionInputs in{};
  in.has_camera = false;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Full);
}

TEST(CreatureLodDecision, BillboardDistanceCullsWithBillboardReason) {
  const auto d = decide_creature_lod(make_inputs(50.0F));
  EXPECT_TRUE(d.culled);
  EXPECT_EQ(d.reason, CullReason::Billboard);
  EXPECT_EQ(d.lod, CreatureLOD::Billboard);
}

TEST(CreatureLodDecision, BudgetDeniesFullDemotesToMinimal) {
  auto in = make_inputs(5.0F);
  in.apply_visibility_budget = true;
  in.budget_grant_full = false;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, BudgetGrantedKeepsFull) {
  auto in = make_inputs(5.0F);
  in.apply_visibility_budget = true;
  in.budget_grant_full = true;
  const auto d = decide_creature_lod(in);
  EXPECT_EQ(d.lod, CreatureLOD::Full);
}

TEST(CreatureLodDecision, BudgetIgnoredWhenNotEnabled) {
  auto in = make_inputs(5.0F);
  in.apply_visibility_budget = false;
  in.budget_grant_full = false;
  const auto d = decide_creature_lod(in);
  EXPECT_EQ(d.lod, CreatureLOD::Full);
}

TEST(CreatureLodDecision, TemporalSkipMinimalFiresWhenFarAndOffPhase) {
  auto in = make_inputs(46.0F);
  in.thresholds = {12.0F, 20.0F, 80.0F}; // 46 -> Minimal
  in.temporal = TemporalSkipParams{35.0F, 45.0F, 2U, 3U};
  in.frame_index = 1U;
  in.instance_seed = 0U; // (1+0)%3 == 1 != 0 -> temporal skip culls
  const auto d = decide_creature_lod(in);
  EXPECT_EQ(d.lod, CreatureLOD::Minimal);
  EXPECT_TRUE(d.culled);
  EXPECT_EQ(d.reason, CullReason::Temporal);
}

TEST(CreatureLodDecision, TemporalSkipMinimalRendersOnPhase) {
  auto in = make_inputs(46.0F);
  in.thresholds = {12.0F, 20.0F, 80.0F};
  in.temporal = TemporalSkipParams{35.0F, 45.0F, 2U, 3U};
  in.frame_index = 2U;
  in.instance_seed = 1U; // (2+1)%3 == 0 -> render
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, TemporalSkipDoesNotApplyBelowThreshold) {
  auto in = make_inputs(20.0F);
  in.thresholds = {12.0F, 20.0F, 80.0F}; // 20 -> Minimal
  in.temporal = TemporalSkipParams{35.0F, 45.0F, 2U, 3U};
  in.frame_index = 1U;
  in.instance_seed = 0U; // would skip if applied
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, TemporalPeriodOneAlwaysRenders) {
  EXPECT_TRUE(should_render_temporal(0U, 0U, 1U));
  EXPECT_TRUE(should_render_temporal(7U, 13U, 0U));
  EXPECT_TRUE(should_render_temporal(7U, 13U, 1U));
}

TEST(CreatureLodDecision, ForcedLodWithBillboardIsNotCulled) {
  // Forced overrides take precedence; if game wants billboard explicitly
  // it gets billboard without culling — culling is the rig path.
  auto in = make_inputs(0.0F);
  in.forced_lod = CreatureLOD::Billboard;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Billboard);
}

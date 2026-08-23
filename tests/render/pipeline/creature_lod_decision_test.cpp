

#include <gtest/gtest.h>

#include "render/creature/pipeline/lod_decision.h"

using Render::Creature::CreatureLOD;
using Render::Creature::Pipeline::CreatureLodDecisionInputs;
using Render::Creature::Pipeline::CullReason;
using Render::Creature::Pipeline::decide_creature_lod;
using Render::Creature::Pipeline::LodDistanceThresholds;
using Render::Creature::Pipeline::select_distance_lod;

namespace {

constexpr LodDistanceThresholds k_defaults{12.0F, 40.0F};

auto make_inputs(float distance) -> CreatureLodDecisionInputs {
  CreatureLodDecisionInputs in{};
  in.has_camera = true;
  in.distance = distance;
  in.thresholds = k_defaults;
  return in;
}

} // namespace

TEST(CreatureLodDecision, SelectDistanceLodMatchesThresholds) {
  EXPECT_EQ(select_distance_lod(0.0F, k_defaults), CreatureLOD::Full);
  EXPECT_EQ(select_distance_lod(11.99F, k_defaults), CreatureLOD::Full);
  EXPECT_EQ(select_distance_lod(12.0F, k_defaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(39.99F, k_defaults), CreatureLOD::Minimal);
  EXPECT_EQ(select_distance_lod(40.0F, k_defaults), CreatureLOD::Culled);
  EXPECT_EQ(select_distance_lod(1000.0F, k_defaults), CreatureLOD::Culled);
}

TEST(CreatureLodDecision, ForcedLodBypassesEverything) {
  auto in = make_inputs(1000.0F);
  in.forced_lod = CreatureLOD::Minimal;
  in.apply_visibility_budget = true;
  in.budget_grant_full = false;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Minimal);
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
  EXPECT_EQ(d.reason, CullReason::Distance);
  EXPECT_EQ(d.lod, CreatureLOD::Culled);
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

TEST(CreatureLodDecision, ForcedLodWithBillboardIsNotCulled) {

  auto in = make_inputs(0.0F);
  in.forced_lod = CreatureLOD::Culled;
  const auto d = decide_creature_lod(in);
  EXPECT_FALSE(d.culled);
  EXPECT_EQ(d.lod, CreatureLOD::Culled);
}

using Render::Creature::Pipeline::lod_reference_distance;

TEST(CreatureLodDecision, TheDefaultApparentScaleLeavesTheDistanceTestAlone) {
  EXPECT_FLOAT_EQ(lod_reference_distance(30.0F, 1.0F), 30.0F);
  EXPECT_FLOAT_EQ(lod_reference_distance(30.0F, 0.0F), 30.0F);

  auto in = make_inputs(20.0F);
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Minimal);
  in.apparent_size_scale = 1.0F;
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, BodiesThatLookBiggerHoldFullDetailFurtherOut) {
  auto in = make_inputs(20.0F);
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Minimal);

  in.apparent_size_scale = 2.0F;
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Full);
}

TEST(CreatureLodDecision, BodiesThatLookSmallerLoseDetailSooner) {
  auto in = make_inputs(10.0F);
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Full);

  in.apparent_size_scale = 0.5F;
  EXPECT_EQ(decide_creature_lod(in).lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, TheCullDistanceIsAbsolute) {
  auto in = make_inputs(60.0F);
  EXPECT_TRUE(decide_creature_lod(in).culled);
  EXPECT_EQ(decide_creature_lod(in).reason, CullReason::Distance);

  for (const float scale : {0.25F, 0.5F, 2.0F, 4.0F}) {
    in.apparent_size_scale = scale;
    EXPECT_TRUE(decide_creature_lod(in).culled) << "scale " << scale;
  }

  in.distance = 39.0F;
  for (const float scale : {0.25F, 0.5F, 2.0F, 4.0F}) {
    in.apparent_size_scale = scale;
    EXPECT_FALSE(decide_creature_lod(in).culled) << "scale " << scale;
  }
}

TEST(CreatureLodDecision, ASmallViewportStillOnlyCostsDetailNotVisibility) {
  auto in = make_inputs(30.0F);
  in.apparent_size_scale = 0.25F;
  const auto decision = decide_creature_lod(in);
  EXPECT_FALSE(decision.culled);
  EXPECT_EQ(decision.lod, CreatureLOD::Minimal);
}

TEST(CreatureLodDecision, TheThreeArgumentSelectorDefaultsToTheTwoArgumentOne) {
  for (const float distance : {0.0F, 5.0F, 11.99F, 12.0F, 39.99F, 40.0F, 500.0F}) {
    EXPECT_EQ(select_distance_lod(distance, k_defaults),
              select_distance_lod(distance, k_defaults, 1.0F))
        << "distance " << distance;
  }
}

TEST(CreatureLodDecision, AForcedLodStillIgnoresTheApparentSize) {
  auto in = make_inputs(1000.0F);
  in.apparent_size_scale = 0.25F;
  in.forced_lod = CreatureLOD::Full;
  const auto decision = decide_creature_lod(in);
  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

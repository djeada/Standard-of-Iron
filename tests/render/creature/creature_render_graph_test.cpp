

#include <QMatrix4x4>
#include <QVector3D>

#include <atomic>
#include <gtest/gtest.h>

#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/graphics_settings.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/submitter.h"

namespace {

using namespace Render::Creature::Pipeline;
using namespace Render::Creature;

std::atomic<int> g_extra_role_color_calls{0};

auto counted_extra_role_color(const void*,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  g_extra_role_color_calls.fetch_add(1, std::memory_order_relaxed);
  if (base_count < max_count) {
    out[base_count] = QVector3D(0.25F, 0.5F, 0.75F);
    return base_count + 1U;
  }
  return base_count;
}

class CountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd&) override { ++rigged_calls; }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

TEST(CreatureRenderGraph, HumanoidLodConfigHasReasonableDefaults) {
  auto config = humanoid_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
  EXPECT_GT(config.temporal.period_minimal, 0U);
}

TEST(CreatureRenderGraph, HorseLodConfigHasReasonableDefaults) {
  auto config = horse_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, ElephantLodConfigHasReasonableDefaults) {
  auto config = elephant_lod_config();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, HorseAndElephantHaveLargerLodDistances) {
  auto humanoid = humanoid_lod_config();
  auto horse = horse_lod_config();
  auto elephant = elephant_lod_config();

  EXPECT_GE(horse.thresholds.full, humanoid.thresholds.full);
  EXPECT_GE(elephant.thresholds.full, humanoid.thresholds.full);
}

TEST(CreatureRenderGraph, HumanoidConfigFromSettingsReturnsValidConfig) {
  auto config = humanoid_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
  EXPECT_GT(config.temporal.period_minimal, 0U);
}

TEST(CreatureRenderGraph, HorseConfigFromSettingsReturnsValidConfig) {
  auto config = horse_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
}

TEST(CreatureRenderGraph, ElephantConfigFromSettingsReturnsValidConfig) {
  auto& settings = Render::GraphicsSettings::instance();
  settings.set_quality(Render::GraphicsQuality::High);

  auto config = elephant_lod_config_from_settings();
  EXPECT_GT(config.thresholds.full, 0.0F);
  EXPECT_GT(config.thresholds.minimal, config.thresholds.full);
  EXPECT_FLOAT_EQ(config.thresholds.full, settings.elephant_full_detail_distance());
  EXPECT_FLOAT_EQ(config.thresholds.minimal,
                  settings.elephant_minimal_detail_distance());
  EXPECT_GT(config.thresholds.full, settings.horse_full_detail_distance());
  EXPECT_GT(config.thresholds.minimal, settings.horse_minimal_detail_distance());

  settings.set_quality(Render::GraphicsQuality::Ultra);
}

TEST(ArchetypeRegistry, BaselineAnimationManifestPreservesHumanoidMappings) {
  auto& registry = ArchetypeRegistry::instance();

  EXPECT_EQ(
      registry.bpat_clip(ArchetypeRegistry::k_humanoid_base, AnimationStateId::Cast),
      registry.bpat_clip(ArchetypeRegistry::k_humanoid_base,
                         AnimationStateId::AttackBow));
  EXPECT_EQ(registry.clip_variant_count(ArchetypeRegistry::k_humanoid_base,
                                        AnimationStateId::Idle),
            5U);
  EXPECT_FALSE(
      registry.is_snapshot(ArchetypeRegistry::k_humanoid_base, AnimationStateId::Walk));
  EXPECT_EQ(
      registry.bpat_clip(ArchetypeRegistry::k_rider_base,
                         AnimationStateId::RidingBowShot),
      registry.bpat_clip(ArchetypeRegistry::k_rider_base, AnimationStateId::Cast));
}

TEST(CreatureRenderGraph, QuadrupedLodUsesElephantDistances) {
  auto& settings = Render::GraphicsSettings::instance();
  settings.set_quality(Render::GraphicsQuality::High);

  EXPECT_EQ(quadruped_lod_from_settings(CreatureKind::Elephant,
                                        settings.horse_full_detail_distance() + 1.0F),
            CreatureLOD::Full);
  EXPECT_EQ(
      quadruped_lod_from_settings(CreatureKind::Elephant,
                                  settings.elephant_full_detail_distance() + 1.0F),
      CreatureLOD::Minimal);

  settings.set_quality(Render::GraphicsQuality::Ultra);
}

TEST(CreatureRenderGraph, UltraSettingsKeepTroopLodFullAtLongDistance) {
  auto& settings = Render::GraphicsSettings::instance();
  settings.set_quality(Render::GraphicsQuality::Ultra);
  EXPECT_FALSE(settings.creature_lod_enabled());

  CreatureGraphInputs inputs;
  inputs.camera_distance = 100000.0F;
  inputs.has_camera = true;
  inputs.budget_grant_full = false;

  auto humanoid = evaluate_creature_lod(inputs, humanoid_lod_config_from_settings());
  auto horse = evaluate_creature_lod(inputs, horse_lod_config_from_settings());
  auto elephant = evaluate_creature_lod(inputs, elephant_lod_config_from_settings());

  EXPECT_EQ(humanoid.lod, CreatureLOD::Full);
  EXPECT_FALSE(humanoid.culled);
  EXPECT_EQ(horse.lod, CreatureLOD::Full);
  EXPECT_FALSE(horse.culled);
  EXPECT_EQ(elephant.lod, CreatureLOD::Full);
  EXPECT_FALSE(elephant.culled);
  EXPECT_EQ(quadruped_lod_from_settings(CreatureKind::Horse, 100000.0F),
            CreatureLOD::Full);
  EXPECT_EQ(quadruped_lod_from_settings(CreatureKind::Elephant, 100000.0F),
            CreatureLOD::Full);

  settings.set_quality(Render::GraphicsQuality::High);
  EXPECT_TRUE(settings.creature_lod_enabled());
  settings.set_quality(Render::GraphicsQuality::Ultra);
}

TEST(CreatureRenderGraph, SettingsConfigIncludesTemporalParams) {
  auto humanoid = humanoid_lod_config_from_settings();
  auto horse = horse_lod_config_from_settings();
  auto elephant = elephant_lod_config_from_settings();
}

TEST(CreatureRenderGraph, EvaluateLodReturnsFullAtCloseDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsMinimalAtMidDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 15.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Minimal);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsMinimalAtFarDistance) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 35.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Minimal);
}

TEST(CreatureRenderGraph, EvaluateLodReturnsBillboardBeyondMinimal) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Billboard);
  EXPECT_TRUE(decision.culled);
  EXPECT_EQ(decision.reason, CullReason::Billboard);
}

TEST(CreatureRenderGraph, ForcedLodOverridesDistanceCalculation) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F;
  inputs.has_camera = true;
  inputs.forced_lod = CreatureLOD::Full;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, NoCameraMeansFullLod) {
  CreatureGraphInputs inputs;
  inputs.camera_distance = 100.0F;
  inputs.has_camera = false;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(decision.culled);
}

TEST(CreatureRenderGraph, BuildBaseOutputSetsLodFromDecision) {
  CreatureGraphInputs const inputs;
  CreatureLodDecision decision;
  decision.lod = CreatureLOD::Minimal;
  decision.culled = false;

  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.lod, CreatureLOD::Minimal);
  EXPECT_FALSE(output.culled);
}

TEST(CreatureRenderGraph, BuildBaseOutputSetsCulledFromDecision) {
  CreatureGraphInputs const inputs;
  CreatureLodDecision decision;
  decision.lod = CreatureLOD::Billboard;
  decision.culled = true;
  decision.reason = CullReason::Billboard;

  auto output = build_base_graph_output(inputs, decision);

  EXPECT_TRUE(output.culled);
  EXPECT_EQ(output.cull_reason, CullReason::Billboard);
}

TEST(CreatureRenderGraph, BuildBaseOutputSetsPassIntentFromContext) {
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision const decision;
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(CreatureRenderGraph, BuildBaseOutputCopiesWorldMatrix) {
  Render::GL::DrawContext ctx{};
  ctx.model.translate(1.0F, 2.0F, 3.0F);

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision const decision;
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.world_matrix, ctx.model);
}

TEST(CreatureRenderBatch, StartsEmpty) {
  CreatureRenderBatch const batch;
  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0U);
}

TEST(CreatureRenderBatch, AddHumanoidIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_EQ(batch.size(), 1U);
  EXPECT_FALSE(batch.empty());
}

TEST(CreatureRenderBatch, StableRoleColorsAreCachedByVariant) {
  g_extra_role_color_calls.store(0, std::memory_order_relaxed);
  const auto archetype = ArchetypeRegistry::instance().register_unit_archetype(
      "test.role_color_cache", CreatureKind::Humanoid, {}, &counted_extra_role_color);

  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.archetype_id = archetype;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);
  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 2U);
  EXPECT_EQ(batch.requests()[0].role_color_count, batch.requests()[1].role_color_count);
  EXPECT_EQ(g_extra_role_color_calls.load(std::memory_order_relaxed), 1)
      << "same asset/archetype/variant should reuse cached role colors";
}

TEST(CreatureRenderBatch, WearProfileBreaksRoleColorCacheReuse) {
  g_extra_role_color_calls.store(0, std::memory_order_relaxed);
  const auto archetype = ArchetypeRegistry::instance().register_unit_archetype(
      "test.role_color_cache_wear",
      CreatureKind::Humanoid,
      {},
      &counted_extra_role_color);

  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.archetype_id = archetype;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant first{};
  first.weathering = 0.15F;
  first.pattern_seed = 0.25F;
  Render::GL::HumanoidVariant second = first;
  second.weathering = 0.45F;
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, first, anim);
  batch.add_humanoid(output, pose, second, anim);

  ASSERT_EQ(batch.requests().size(), 2U);
  EXPECT_EQ(g_extra_role_color_calls.load(std::memory_order_relaxed), 2)
      << "wear-sensitive humanoid variants should not share cached role colors";
}

TEST(CreatureRenderBatch, VariantTableCanOverrideRequestSelection) {
  auto const override_archetype = ArchetypeRegistry::instance().register_unit_archetype(
      "test.variant_table_override", CreatureKind::Humanoid, {});
  ASSERT_NE(override_archetype, k_invalid_archetype);

  ArchetypeVariantTable table{};
  auto const idle_idx = static_cast<std::size_t>(Render::Creature::PoseIntent::Idle);
  table.archetype_for_pose[idle_idx] = override_archetype;
  table.state_for_pose[idle_idx] = AnimationStateId::AttackSpear;

  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.archetype_id = ArchetypeRegistry::k_humanoid_base;
  output.spec.animation_manifest.variant_table = &table;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].archetype, override_archetype);
  EXPECT_EQ(batch.requests()[0].state, AnimationStateId::AttackSpear);
}

TEST(CreatureRenderBatch, PrecomputedHumanoidSelectionOverridesResolverWork) {
  auto const override_archetype = ArchetypeRegistry::instance().register_unit_archetype(
      "test.precomputed_selection_override", CreatureKind::Humanoid, {});
  ASSERT_NE(override_archetype, k_invalid_archetype);

  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.creature_asset_id = k_humanoid_sword_asset;
  output.spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  HumanoidAnimationSelection selection{};
  selection.requested_archetype = ArchetypeRegistry::k_humanoid_base;
  selection.resolved_archetype = override_archetype;
  selection.state = AnimationStateId::AttackSpear;
  selection.phase = 0.73F;
  selection.clip_variant = 2U;
  output.humanoid_selection = selection;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  auto const& req = batch.requests().front();
  EXPECT_EQ(req.archetype, override_archetype);
  EXPECT_EQ(req.state, AnimationStateId::AttackSpear);
  EXPECT_FLOAT_EQ(req.phase, 0.73F);
  EXPECT_EQ(req.clip_variant, 2U);
}

TEST(CreatureRenderBatch, MovingCombatHumanoidKeepsMovementRequest) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.creature_asset_id = k_humanoid_spear_asset;
  output.spec.archetype_id = ArchetypeRegistry::k_humanoid_base;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = MovementAnimationState::Walk;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
  anim.gait.state = Render::GL::HumanoidMotionState::Walk;
  anim.gait.cycle_phase = 0.42F;

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  auto const& req = batch.requests()[0];
  EXPECT_EQ(req.creature_asset_id, k_humanoid_spear_asset);
  EXPECT_EQ(req.archetype, ArchetypeRegistry::k_humanoid_base);
  EXPECT_EQ(req.state, AnimationStateId::Walk);
  EXPECT_FLOAT_EQ(req.phase, anim.gait.cycle_phase);
  EXPECT_FALSE(ArchetypeRegistry::instance().is_snapshot(req.archetype, req.state));
}

TEST(CreatureRenderBatch, AllPoseVariantTableCannotChangeMovingStateByArchetype) {
  auto const override_archetype = ArchetypeRegistry::instance().register_unit_archetype(
      "test.walk_all_pose_variant", CreatureKind::Humanoid, {});
  ASSERT_NE(override_archetype, k_invalid_archetype);

  ArchetypeVariantTable table{};
  table.variant_trigger_pose = PoseIntent::Count;
  table.variant_stride = 1U;
  table.archetype_for_variant[0] = override_archetype;

  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  output.spec.creature_asset_id = k_humanoid_sword_asset;
  output.spec.archetype_id = ArchetypeRegistry::k_humanoid_base;
  output.spec.animation_manifest.variant_table = &table;

  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext anim{};
  anim.inputs.movement_state = MovementAnimationState::Run;
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.gait.state = Render::GL::HumanoidMotionState::Run;
  anim.gait.cycle_phase = 0.64F;

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  auto const& req = batch.requests()[0];
  EXPECT_EQ(req.creature_asset_id, k_humanoid_sword_asset);
  EXPECT_EQ(req.archetype, override_archetype);
  EXPECT_EQ(req.state, AnimationStateId::Run);
  EXPECT_FLOAT_EQ(req.phase, anim.gait.cycle_phase);
  EXPECT_FALSE(ArchetypeRegistry::instance().is_snapshot(req.archetype, req.state));
}

TEST(CreatureRenderBatch, HumanoidWearParamsPropagateToRequests) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant variant{};
  variant.weathering = 0.35F;
  variant.grime = 0.20F;
  variant.bloodiness = 0.10F;
  variant.pattern_seed = 0.75F;
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  ASSERT_EQ(batch.requests().size(), 1U);
  auto const& req = batch.requests()[0];
  EXPECT_FLOAT_EQ(req.wear_params.x(), variant.weathering);
  EXPECT_FLOAT_EQ(req.wear_params.y(), variant.grime);
  EXPECT_FLOAT_EQ(req.wear_params.z(), variant.bloodiness);
  EXPECT_FLOAT_EQ(req.wear_params.w(), variant.pattern_seed);
}

TEST(CreatureRenderBatch, CulledCreatureNotAdded) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = true;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_TRUE(batch.empty());
  EXPECT_EQ(batch.size(), 0U);
}

TEST(CreatureRenderBatch, AddHorseIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HorseVariant const variant{};

  batch.add_quadruped(output, variant, Render::Creature::AnimationStateId::Idle, 0.0F);

  EXPECT_EQ(batch.size(), 1U);
}

TEST(CreatureRenderBatch, AddElephantIncreasesSize) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::ElephantVariant const variant{};

  batch.add_quadruped(output, variant, Render::Creature::AnimationStateId::Idle, 0.0F);

  EXPECT_EQ(batch.size(), 1U);
}

TEST(CreatureRenderBatch, ClearEmptiesBatch) {
  CreatureRenderBatch batch;
  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);
  EXPECT_EQ(batch.size(), 1U);

  batch.clear();
  EXPECT_TRUE(batch.empty());
}

TEST(CreatureRenderBatch, ReserveDoesNotChangeSize) {
  CreatureRenderBatch batch;
  batch.reserve(100);
  EXPECT_TRUE(batch.empty());
}

TEST(CreatureRenderGraph, EndToEndHumanoidPrepare) {

  CreatureGraphInputs inputs;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Humanoid;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);
  EXPECT_FALSE(output.culled);

  CreatureRenderBatch batch;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  EXPECT_EQ(batch.size(), 1U);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderGraph, EndToEndHorsePrepare) {

  CreatureGraphInputs inputs;
  inputs.camera_distance = 10.0F;
  inputs.has_camera = true;

  auto config = horse_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Horse;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);

  CreatureRenderBatch batch;
  Render::GL::HorseVariant const variant{};

  batch.add_quadruped(output, variant, Render::Creature::AnimationStateId::Idle, 0.0F);

  EXPECT_EQ(batch.size(), 1U);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderGraph, EndToEndElephantPrepare) {

  CreatureGraphInputs inputs;
  inputs.camera_distance = 10.0F;
  inputs.has_camera = true;

  auto config = elephant_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Elephant;

  EXPECT_EQ(decision.lod, CreatureLOD::Full);

  CreatureRenderBatch batch;
  Render::GL::ElephantVariant const variant{};

  batch.add_quadruped(output, variant, Render::Creature::AnimationStateId::Idle, 0.0F);

  EXPECT_EQ(batch.size(), 1U);
  ASSERT_EQ(batch.requests().size(), 1U);
  EXPECT_EQ(batch.requests()[0].lod, CreatureLOD::Full);
  EXPECT_TRUE(batch.rows().empty());
}

TEST(CreatureRenderGraph, PrewarmContextSetsShadowPassIntent) {
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &ctx;

  CreatureLodDecision const decision;
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(CreatureRenderGraph, ShadowPassRowsSubmitZeroDrawCalls) {
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  req.state = AnimationStateId::Idle;
  req.lod = CreatureLOD::Full;
  req.pass = RenderPassIntent::Shadow;
  req.seed = 42U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(CreatureRenderGraph, MainPassRowsSubmitDrawCalls) {
  CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::k_humanoid_base;
  req.state = AnimationStateId::Idle;
  req.lod = CreatureLOD::Full;
  req.pass = RenderPassIntent::Main;
  req.seed = 7U;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);

  CountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1U);
}

TEST(CreaturePreparationResult, ClearEmptiesBothContainers) {
  CreaturePreparationResult result;

  CreatureGraphOutput output;
  output.culled = false;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  result.bodies.add_humanoid(output, pose, variant, anim);
  result.add_post_body_draw(RenderPassIntent::Main, PostBodyDrawRequest::Kind::None);

  EXPECT_EQ(result.bodies.size(), 1U);
  EXPECT_EQ(result.post_body_draws.size(), 1U);

  result.clear();

  EXPECT_TRUE(result.bodies.empty());
  EXPECT_TRUE(result.post_body_draws.empty());
  EXPECT_TRUE(result.shadow_batch.empty());
}

TEST(CreaturePreparationResult, ShadowBatchStartsEmpty) {
  CreaturePreparationResult const result;
  EXPECT_TRUE(result.shadow_batch.empty());
  EXPECT_EQ(result.shadow_batch.size(), 0U);
}

} // namespace

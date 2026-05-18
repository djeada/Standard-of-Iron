

#include <QMatrix4x4>
#include <QVector3D>

#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/registry.h"
#include "render/gl/camera.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"
#include "render/template_prewarm_catalog.h"

namespace {

using namespace Render::Creature::Pipeline;
using namespace Render::Creature;

class PrewarmCountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int mesh_calls{0};
  int total_calls{0};

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++mesh_calls;
    ++total_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd&) override {
    ++rigged_calls;
    ++total_calls;
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {
    ++total_calls;
  }
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {
    ++total_calls;
  }
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {
    ++total_calls;
  }
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {
    ++total_calls;
  }
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {
    ++total_calls;
  }
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {
    ++total_calls;
  }
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {
    ++total_calls;
  }
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {
    ++total_calls;
  }
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {
    ++total_calls;
  }
};

auto make_request(Render::Creature::ArchetypeId archetype,
                  CreatureLOD lod,
                  RenderPassIntent pass,
                  std::uint32_t seed = 0U) -> CreatureRenderRequest {
  CreatureRenderRequest req{};
  req.archetype = archetype;
  req.state = AnimationStateId::Idle;
  req.lod = lod;
  req.pass = pass;
  req.seed = seed;
  req.world_already_grounded = true;
  req.world.translate(0.0F, static_cast<float>(seed) * 0.1F, 0.0F);
  return req;
}

auto submit_requests_for_test(std::span<const CreatureRenderRequest> requests,
                              PrewarmCountingSubmitter& sink) -> SubmitStats {
  CreaturePreparationResult prep;
  prep.bodies.reserve(requests.size());
  for (const auto& request : requests) {
    prep.bodies.add_request(request);
  }
  return submit_preparation(prep, sink);
}

void add_test_unit(Engine::Core::Entity& entity,
                   Game::Units::SpawnType spawn_type,
                   Game::Systems::NationID nation_id,
                   int owner_id,
                   const char* renderer_id) {
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  unit->owner_id = owner_id;
  unit->health = 100;
  unit->max_health = 100;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = renderer_id;
  renderable->visible = true;
  renderable->color = {1.0F, 1.0F, 1.0F};
}

TEST(TemplatePrewarmRegression, PassIntentFromCtxDetectsPrewarm) {
  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;
  EXPECT_EQ(pass_intent_from_ctx(normal_ctx), RenderPassIntent::Main);

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;
  EXPECT_EQ(pass_intent_from_ctx(prewarm_ctx), RenderPassIntent::Shadow);
}

TEST(TemplatePrewarmRegression, RuntimePrewarmCtxSuppressesAnimationStatePersistence) {
  Render::GL::DrawContext const normal_ctx{};
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  EXPECT_TRUE(Render::GL::should_persist_animation_state(normal_ctx));
  EXPECT_FALSE(Render::GL::should_persist_animation_state(prewarm_ctx));

  auto const runtime_ctx = make_runtime_prewarm_ctx(prewarm_ctx);
  EXPECT_FALSE(runtime_ctx.template_prewarm);
  EXPECT_FALSE(runtime_ctx.allow_template_cache);
  EXPECT_TRUE(runtime_ctx.suppress_animation_state_persistence);
  EXPECT_FALSE(Render::GL::should_persist_animation_state(runtime_ctx));
}

TEST(TemplatePrewarmRegression, ShadowPassFiltersPreparedBatch) {
  std::vector<CreatureRenderRequest> requests;
  requests.reserve(5);
  for (int i = 0; i < 5; ++i) {
    requests.push_back(
        make_request(ArchetypeRegistry::k_humanoid_base,
                     CreatureLOD::Full,
                     (i < 3) ? RenderPassIntent::Main : RenderPassIntent::Shadow,
                     static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 3U);
}

TEST(TemplatePrewarmRegression, AllShadowPassRowsProduceZeroDraws) {
  std::vector<CreatureRenderRequest> requests;
  requests.reserve(10);
  for (int i = 0; i < 10; ++i) {
    requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                    CreatureLOD::Full,
                                    RenderPassIntent::Shadow,
                                    static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.mesh_calls, 0);
}

TEST(TemplatePrewarmRegression, GraphOutputReflectsPrewarmContext) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;
  prewarm_ctx.model.translate(5.0F, 0.0F, 10.0F);

  CreatureGraphInputs inputs;
  inputs.ctx = &prewarm_ctx;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);

  EXPECT_EQ(output.lod, CreatureLOD::Full);
  EXPECT_EQ(output.pass_intent, RenderPassIntent::Shadow);
}

TEST(TemplatePrewarmRegression, BatchFromPrewarmContextSubmitsNothing) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &prewarm_ctx;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto config = humanoid_lod_config();
  auto decision = evaluate_creature_lod(inputs, config);
  auto output = build_base_graph_output(inputs, decision);
  output.spec.kind = CreatureKind::Humanoid;

  CreatureRenderBatch batch;
  Render::GL::HumanoidPose const pose{};
  Render::GL::HumanoidVariant const variant{};
  Render::GL::HumanoidAnimationContext const anim{};

  batch.add_humanoid(output, pose, variant, anim);

  CreaturePreparationResult prep;
  for (const auto& request : batch.requests()) {
    prep.bodies.add_request(request);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
}

TEST(TemplatePrewarmRegression, AnimCatalogIncludesDistinctMeleeSpearAndRangedAttacks) {
  auto const catalog = Render::GL::build_template_prewarm_anim_catalog(
      ArchetypeRegistry::instance());

  auto has_state = [](const std::vector<Render::GL::AnimKey>& keys,
                      PoseIntent state) {
    for (auto const& key : keys) {
      if (key.state == state) {
        return true;
      }
    }
    return false;
  };

  EXPECT_TRUE(has_state(catalog.core_keys, PoseIntent::AttackMelee));
  EXPECT_TRUE(has_state(catalog.core_keys, PoseIntent::AttackSpear));
  EXPECT_TRUE(has_state(catalog.core_keys, PoseIntent::AttackRanged));
  EXPECT_TRUE(has_state(catalog.core_keys, PoseIntent::Dead));
  EXPECT_TRUE(has_state(catalog.extra_keys, PoseIntent::Walk));
}

TEST(TemplatePrewarmRegression, WorkItemsSkipUnsupportedAttackFamiliesPerSpawn) {
  std::vector<Render::GL::PrewarmProfile> profiles(2);
  profiles[0].renderer_id = "archer";
  profiles[0].spawn_type = Game::Units::SpawnType::Archer;
  profiles[1].renderer_id = "spearman";
  profiles[1].spawn_type = Game::Units::SpawnType::Spearman;

  std::vector<Render::GL::AnimKey> anim_keys(3);
  anim_keys[0].state = PoseIntent::AttackMelee;
  anim_keys[1].state = PoseIntent::AttackSpear;
  anim_keys[2].state = PoseIntent::AttackRanged;

  auto const items = Render::GL::build_template_prewarm_work_items(
      profiles, {1}, {0}, anim_keys);

  int archer_items = 0;
  int spearman_items = 0;
  int spear_attack_items = 0;
  int ranged_attack_items = 0;
  for (auto const& item : items) {
    if (item.profile_index == 0U) {
      ++archer_items;
      EXPECT_NE(item.anim_key.attack_family, Engine::Core::CombatAttackFamily::Spear);
    } else if (item.profile_index == 1U) {
      ++spearman_items;
    }

    if (item.anim_key.state == PoseIntent::AttackSpear) {
      ++spear_attack_items;
      EXPECT_EQ(item.anim_key.attack_family, Engine::Core::CombatAttackFamily::Spear);
    }
    if (item.anim_key.state == PoseIntent::AttackRanged) {
      ++ranged_attack_items;
      EXPECT_EQ(item.profile_index, 0U);
      EXPECT_EQ(item.anim_key.attack_family, Engine::Core::CombatAttackFamily::Bow);
    }
  }

  EXPECT_EQ(archer_items, 4);
  EXPECT_EQ(spearman_items, 2);
  EXPECT_EQ(spear_attack_items, 2);
  EXPECT_EQ(ranged_attack_items, 2);
}

TEST(TemplatePrewarmRegression, PreparedBodyStateCarriesPassIntent) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  CreatureGraphInputs inputs;
  inputs.ctx = &prewarm_ctx;
  inputs.camera_distance = 5.0F;
  inputs.has_camera = true;

  auto output = build_base_graph_output(
      inputs, evaluate_creature_lod(inputs, humanoid_lod_config()));
  output.spec.kind = CreatureKind::Humanoid;

  PreparedHumanoidBodyState state;
  state.graph = output;

  CreatureRenderBatch batch;
  batch.add_humanoid(state);
  ASSERT_EQ(batch.requests().size(), 1U);

  EXPECT_EQ(batch.requests().front().pass, RenderPassIntent::Shadow);
  EXPECT_FALSE(pass_intent_for(state.graph).emits_post_body_draws);
}

TEST(TemplatePrewarmRegression, PreparedAnimationStateHonorsOverride) {
  Render::GL::AnimationInputs override_anim{};
  override_anim.time = 3.0F;
  override_anim.is_attacking = true;

  Render::GL::DrawContext ctx{};
  ctx.animation_override = &override_anim;

  auto const state = resolve_humanoid_animation_state(ctx);

  EXPECT_TRUE(state.used_override);
  EXPECT_FLOAT_EQ(state.inputs.time, 3.0F);
  EXPECT_TRUE(state.inputs.is_attacking);
}

TEST(TemplatePrewarmRegression, ElephantPreparedAnimationPromotesMeleeLock) {
  Engine::Core::Entity entity(1);
  auto* attack = entity.add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  auto const state = resolve_elephant_animation_state(ctx);

  EXPECT_FALSE(state.used_override);
  EXPECT_TRUE(state.inputs.is_attacking);
  EXPECT_TRUE(state.inputs.is_melee);
}

TEST(TemplatePrewarmRegression, HumanoidAnimationPromotesIdleMeleeLock) {
  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::Spearman;

  auto* attack = entity.add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->in_melee_lock = true;

  auto* combat_state = entity.add_component<Engine::Core::CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  combat_state->animation_state = Engine::Core::CombatAnimationState::Idle;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  auto const state = resolve_humanoid_animation_state(ctx);

  EXPECT_FALSE(state.used_override);
  EXPECT_TRUE(state.inputs.is_attacking);
  EXPECT_TRUE(state.inputs.is_melee);
  EXPECT_EQ(state.inputs.attack_family, Engine::Core::CombatAttackFamily::Spear);
}

TEST(TemplatePrewarmRegression, PreparedHumanoidLodCarriesDistanceCull) {
  Render::GL::Camera camera;
  camera.set_position(QVector3D(0.0F, 0.0F, 0.0F));

  Render::GL::DrawContext ctx{};
  ctx.camera = &camera;

  HumanoidLodStateInputs inputs{};
  inputs.ctx = &ctx;
  inputs.soldier_world_pos = QVector3D(0.0F, 0.0F, 50.0F);
  inputs.config = humanoid_lod_config();

  auto const state = resolve_humanoid_lod_state(inputs);

  EXPECT_EQ(state.decision.lod, Render::Creature::CreatureLOD::Billboard);
  EXPECT_TRUE(state.decision.culled);
  EXPECT_EQ(state.decision.reason, CullReason::Billboard);
}

TEST(TemplatePrewarmRegression, PreparedHumanoidShadowRequiresResources) {
  Render::GL::DrawContext const ctx{};
  CreatureGraphOutput const graph{};

  HumanoidShadowStateInputs inputs{};
  inputs.ctx = &ctx;
  inputs.graph = &graph;
  inputs.lod = Render::Creature::CreatureLOD::Full;

  auto const shadow = prepare_humanoid_shadow_state(inputs);

  EXPECT_FALSE(shadow.enabled);
}

TEST(TemplatePrewarmRegression, MixedNormalAndPrewarmBatchFiltersCorrectly) {

  Render::GL::DrawContext normal_ctx{};
  normal_ctx.template_prewarm = false;

  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  std::vector<CreatureRenderRequest> requests;
  requests.reserve(10);
  for (int i = 0; i < 10; ++i) {
    requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                    CreatureLOD::Full,
                                    (i % 2 == 0) ? pass_intent_from_ctx(normal_ctx)
                                                 : pass_intent_from_ctx(prewarm_ctx),
                                    static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 5U);
}

TEST(TemplatePrewarmRegression, HorsePrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request = make_request(ArchetypeRegistry::k_horse_base,
                                    CreatureLOD::Full,
                                    pass_intent_from_ctx(prewarm_ctx),
                                    123U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1U), sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
}

TEST(TemplatePrewarmRegression, ElephantPrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request = make_request(ArchetypeRegistry::k_elephant_base,
                                    CreatureLOD::Full,
                                    pass_intent_from_ctx(prewarm_ctx),
                                    456U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1U), sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
}

TEST(TemplatePrewarmRegression, AllLodLevelsRespectPrewarmFiltering) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  std::vector<CreatureRenderRequest> requests;
  for (auto lod : {CreatureLOD::Full, CreatureLOD::Minimal}) {
    requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                    lod,
                                    pass_intent_from_ctx(prewarm_ctx),
                                    static_cast<std::uint32_t>(lod)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(stats.lod_full, 0U);
  EXPECT_EQ(stats.lod_minimal, 0U);
}

TEST(TemplatePrewarmRegression, SeedDerivationIsDeterministic) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = false;

  auto seed1 = derive_unit_seed(ctx, nullptr);
  auto seed2 = derive_unit_seed(ctx, nullptr);
  auto seed3 = derive_unit_seed(ctx, nullptr);

  EXPECT_EQ(seed1, seed2);
  EXPECT_EQ(seed2, seed3);
}

TEST(TemplatePrewarmRegression, SeedOverrideRespected) {
  Render::GL::DrawContext ctx{};
  ctx.has_seed_override = true;
  ctx.seed_override = 0xCAFEBABEU;

  auto seed = derive_unit_seed(ctx, nullptr);
  EXPECT_EQ(seed, 0xCAFEBABEU);
}

TEST(TemplatePrewarmRegression, LodStatsNotIncrementedForShadowRows) {
  std::vector<CreatureRenderRequest> requests;
  requests.push_back(make_request(
      ArchetypeRegistry::k_humanoid_base, CreatureLOD::Full, RenderPassIntent::Shadow));
  requests.push_back(make_request(
      ArchetypeRegistry::k_horse_base, CreatureLOD::Minimal, RenderPassIntent::Shadow));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(stats.lod_full, 0U);
  EXPECT_EQ(stats.lod_minimal, 0U);
}

TEST(TemplatePrewarmRegression, MainRowsIncrementLodStats) {
  std::vector<CreatureRenderRequest> requests;
  requests.push_back(make_request(
      ArchetypeRegistry::k_humanoid_base, CreatureLOD::Full, RenderPassIntent::Main));
  requests.push_back(make_request(
      ArchetypeRegistry::k_horse_base, CreatureLOD::Minimal, RenderPassIntent::Main));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 2U);
  EXPECT_EQ(stats.lod_full, 1U);
  EXPECT_EQ(stats.lod_minimal, 1U);
}

TEST(TemplatePrewarmRegression, QuadrupedHorseShadowRequiresResources) {
  Render::GL::DrawContext const ctx{};
  CreatureGraphOutput const graph{};
  QuadrupedShadowStateInputs inputs{};
  inputs.ctx = &ctx;
  inputs.graph = &graph;
  inputs.lod = Render::Creature::CreatureLOD::Full;
  inputs.kind = CreatureKind::Horse;

  auto const shadow = prepare_quadruped_shadow_state(inputs);

  EXPECT_FALSE(shadow.enabled);
}

TEST(TemplatePrewarmRegression, QuadrupedElephantShadowRequiresResources) {
  Render::GL::DrawContext const ctx{};
  CreatureGraphOutput const graph{};
  QuadrupedShadowStateInputs inputs{};
  inputs.ctx = &ctx;
  inputs.graph = &graph;
  inputs.lod = Render::Creature::CreatureLOD::Full;
  inputs.kind = CreatureKind::Elephant;

  auto const shadow = prepare_quadruped_shadow_state(inputs);

  EXPECT_FALSE(shadow.enabled);
}

TEST(TemplatePrewarmRegression, QuadrupedShadowNullCtxReturnDisabled) {
  QuadrupedShadowStateInputs inputs{};
  inputs.ctx = nullptr;
  inputs.graph = nullptr;

  auto const shadow = prepare_quadruped_shadow_state(inputs);

  EXPECT_FALSE(shadow.enabled);
}

TEST(TemplatePrewarmRegression, WorldPrewarmSupplementsMissingBuilderProfiles) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  Game::Systems::Nation roman{};
  roman.id = NationID::RomanRepublic;
  roman.display_name = "Roman Republic";
  roman.formation_type = Game::Systems::FormationType::Roman;
  nation_registry.register_nation(std::move(roman));
  Game::Systems::TroopProfileService::instance().clear();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  ASSERT_TRUE(renderer.initialize());

  Engine::Core::World world;
  Engine::Core::Entity* archer = world.create_entity();
  ASSERT_NE(archer, nullptr);
  add_test_unit(
      *archer, SpawnType::Archer, NationID::RomanRepublic, 1, "troops/roman/archer");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto builder_renderer = registry.get("troops/roman/builder");
  ASSERT_TRUE(static_cast<bool>(builder_renderer));

  Engine::Core::Entity builder(9001);
  add_test_unit(
      builder, SpawnType::Builder, NationID::RomanRepublic, 1, "troops/roman/builder");

  renderer.rigged_mesh_cache().reset_frame_stats();

  Render::GL::DrawContext ctx{renderer.resources(), &builder, nullptr, QMatrix4x4()};
  ctx.renderer_id = "troops/roman/builder";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_variant_override = true;
  ctx.variant_override = 0;

  builder_renderer(ctx, renderer);

  const auto& stats = renderer.rigged_mesh_cache().frame_stats();
  EXPECT_EQ(stats.misses, 0U);
  EXPECT_GT(stats.hits, 0U);

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

TEST(TemplatePrewarmRegression, WorldPrewarmsRomanCivilianTemplates) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  Game::Systems::Nation roman{};
  roman.id = NationID::RomanRepublic;
  roman.display_name = "Roman Republic";
  roman.formation_type = Game::Systems::FormationType::Roman;
  nation_registry.register_nation(std::move(roman));
  Game::Systems::TroopProfileService::instance().clear();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  ASSERT_TRUE(renderer.initialize());

  Engine::Core::World world;
  Engine::Core::Entity* civilian = world.create_entity();
  ASSERT_NE(civilian, nullptr);
  add_test_unit(*civilian,
                SpawnType::Civilian,
                NationID::RomanRepublic,
                1,
                "troops/roman/civilian");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto civilian_renderer = registry.get("troops/roman/civilian");
  ASSERT_TRUE(static_cast<bool>(civilian_renderer));

  Engine::Core::Entity civilian_entity(9002);
  add_test_unit(civilian_entity,
                SpawnType::Civilian,
                NationID::RomanRepublic,
                1,
                "troops/roman/civilian");

  renderer.rigged_mesh_cache().reset_frame_stats();

  Render::GL::DrawContext ctx{
      renderer.resources(), &civilian_entity, nullptr, QMatrix4x4()};
  ctx.renderer_id = "troops/roman/civilian";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_variant_override = true;
  ctx.variant_override = 0;

  civilian_renderer(ctx, renderer);

  const auto& stats = renderer.rigged_mesh_cache().frame_stats();
  EXPECT_EQ(stats.misses, 0U);
  EXPECT_GT(stats.hits, 0U);

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

TEST(TemplatePrewarmRegression, WorldPrewarmsCarthageCivilianTemplates) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  Game::Systems::Nation carthage{};
  carthage.id = NationID::Carthage;
  carthage.display_name = "Carthage";
  carthage.formation_type = Game::Systems::FormationType::Carthage;
  nation_registry.register_nation(std::move(carthage));
  Game::Systems::TroopProfileService::instance().clear();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  ASSERT_TRUE(renderer.initialize());

  Engine::Core::World world;
  Engine::Core::Entity* civilian = world.create_entity();
  ASSERT_NE(civilian, nullptr);
  add_test_unit(*civilian,
                SpawnType::Civilian,
                NationID::Carthage,
                1,
                "troops/carthage/civilian");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto civilian_renderer = registry.get("troops/carthage/civilian");
  ASSERT_TRUE(static_cast<bool>(civilian_renderer));

  Engine::Core::Entity civilian_entity(9003);
  add_test_unit(civilian_entity,
                SpawnType::Civilian,
                NationID::Carthage,
                1,
                "troops/carthage/civilian");

  renderer.rigged_mesh_cache().reset_frame_stats();

  Render::GL::DrawContext ctx{
      renderer.resources(), &civilian_entity, nullptr, QMatrix4x4()};
  ctx.renderer_id = "troops/carthage/civilian";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_variant_override = true;
  ctx.variant_override = 0;

  civilian_renderer(ctx, renderer);

  const auto& stats = renderer.rigged_mesh_cache().frame_stats();
  EXPECT_EQ(stats.misses, 0U);
  EXPECT_GT(stats.hits, 0U);

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

TEST(TemplatePrewarmRegression, WorldPrewarmsCarthageSpearmanFacialHairVariants) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  Game::Systems::Nation carthage{};
  carthage.id = NationID::Carthage;
  carthage.display_name = "Carthage";
  carthage.formation_type = Game::Systems::FormationType::Carthage;
  nation_registry.register_nation(std::move(carthage));
  Game::Systems::TroopProfileService::instance().clear();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  ASSERT_TRUE(renderer.initialize());

  Engine::Core::World world;
  Engine::Core::Entity* spearman = world.create_entity();
  ASSERT_NE(spearman, nullptr);
  add_test_unit(*spearman,
                SpawnType::Spearman,
                NationID::Carthage,
                1,
                "troops/carthage/spearman");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto spearman_renderer = registry.get("troops/carthage/spearman");
  ASSERT_TRUE(static_cast<bool>(spearman_renderer));

  Engine::Core::Entity runtime_entity(9003);
  add_test_unit(runtime_entity,
                SpawnType::Spearman,
                NationID::Carthage,
                1,
                "troops/carthage/spearman");

  Render::GL::DrawContext ctx{
      renderer.resources(), &runtime_entity, nullptr, QMatrix4x4()};
  ctx.renderer_id = "troops/carthage/spearman";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_seed_override = true;

  for (std::uint32_t const seed : {0U, 4U, 11U, 12U}) {
    renderer.rigged_mesh_cache().reset_frame_stats();
    ctx.seed_override = seed;
    spearman_renderer(ctx, renderer);

    const auto& stats = renderer.rigged_mesh_cache().frame_stats();
    EXPECT_EQ(stats.misses, 0U) << "seed=" << seed;
    EXPECT_GT(stats.hits, 0U) << "seed=" << seed;
  }

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

TEST(TemplatePrewarmRegression,
     MixedCarthageSpearmanBeardVariantsRemainVisibleInQueuedFrame) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  Game::Systems::Nation carthage{};
  carthage.id = NationID::Carthage;
  carthage.display_name = "Carthage";
  carthage.formation_type = Game::Systems::FormationType::Carthage;
  nation_registry.register_nation(std::move(carthage));
  Game::Systems::TroopProfileService::instance().clear();

  Render::GL::Renderer renderer(Render::ShaderQuality::None);
  ASSERT_TRUE(renderer.initialize());

  Render::GL::Camera camera;
  camera.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
  camera.look_at(QVector3D(0.0F, 3.0F, 8.0F),
                 QVector3D(0.0F, 1.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));
  renderer.set_camera(&camera);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto spearman_renderer = registry.get("troops/carthage/spearman");
  ASSERT_TRUE(static_cast<bool>(spearman_renderer));

  Engine::Core::Entity clean_entity(9101);
  add_test_unit(clean_entity,
                SpawnType::Spearman,
                NationID::Carthage,
                1,
                "troops/carthage/spearman");
  Engine::Core::Entity bearded_entity(9102);
  add_test_unit(bearded_entity,
                SpawnType::Spearman,
                NationID::Carthage,
                1,
                "troops/carthage/spearman");

  auto render_variant = [&](Engine::Core::Entity& entity, float x, std::uint32_t seed) {
    QMatrix4x4 model;
    model.translate(x, 0.0F, 0.0F);
    Render::GL::DrawContext ctx{renderer.resources(), &entity, nullptr, model};
    ctx.renderer_id = "troops/carthage/spearman";
    ctx.backend = renderer.backend();
    ctx.camera = &camera;
    ctx.allow_template_cache = true;
    ctx.force_humanoid_lod = true;
    ctx.forced_humanoid_lod = CreatureLOD::Full;
    ctx.force_single_soldier = true;
    ctx.has_seed_override = true;
    ctx.seed_override = seed;
    spearman_renderer(ctx, renderer);
  };

  renderer.begin_frame();
  render_variant(clean_entity, -1.6F, 0U);
  render_variant(bearded_entity, 1.6F, 11U);
  renderer.end_frame();

  const QImage image = renderer.render_software_preview(160, 120);
  ASSERT_FALSE(image.isNull());
  const QColor clear = image.pixelColor(0, 0);

  auto count_non_clear = [&](int start_x, int end_x) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = start_x; x < end_x; ++x) {
        if (image.pixelColor(x, y) != clear) {
          ++count;
        }
      }
    }
    return count;
  };

  EXPECT_GT(count_non_clear(0, image.width() / 2), 20);
  EXPECT_GT(count_non_clear(image.width() / 2, image.width()), 20);

  renderer.shutdown();
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

} // namespace

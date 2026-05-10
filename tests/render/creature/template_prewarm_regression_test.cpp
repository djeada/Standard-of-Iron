

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

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <vector>

namespace {

using namespace Render::Creature::Pipeline;
using namespace Render::Creature;

class PrewarmCountingSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int mesh_calls{0};
  int total_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
    ++total_calls;
  }
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
    ++total_calls;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {
    ++total_calls;
  }
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {
    ++total_calls;
  }
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {
    ++total_calls;
  }
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {
    ++total_calls;
  }
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {
    ++total_calls;
  }
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {
    ++total_calls;
  }
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {
    ++total_calls;
  }
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {
    ++total_calls;
  }
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {
    ++total_calls;
  }
};

auto make_request(Render::Creature::ArchetypeId archetype, CreatureLOD lod,
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
                              PrewarmCountingSubmitter &sink) -> SubmitStats {
  CreaturePreparationResult prep;
  prep.bodies.reserve(requests.size());
  for (const auto &request : requests) {
    prep.bodies.add_request(request);
  }
  return submit_preparation(prep, sink);
}

void add_test_unit(Engine::Core::Entity &entity,
                   Game::Units::SpawnType spawn_type,
                   Game::Systems::NationID nation_id, int owner_id,
                   const char *renderer_id) {
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  unit->owner_id = owner_id;
  unit->health = 100;
  unit->max_health = 100;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
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

TEST(TemplatePrewarmRegression, ShadowPassFiltersPreparedBatch) {
  std::vector<CreatureRenderRequest> requests;
  for (int i = 0; i < 5; ++i) {
    requests.push_back(make_request(
        ArchetypeRegistry::k_humanoid_base, CreatureLOD::Full,
        (i < 3) ? RenderPassIntent::Main : RenderPassIntent::Shadow,
        static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 3u);
}

TEST(TemplatePrewarmRegression, AllShadowPassRowsProduceZeroDraws) {
  std::vector<CreatureRenderRequest> requests;
  for (int i = 0; i < 10; ++i) {
    requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                    CreatureLOD::Full, RenderPassIntent::Shadow,
                                    static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
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
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext anim{};

  batch.add_humanoid(output, pose, variant, anim);

  CreaturePreparationResult prep;
  for (const auto &request : batch.requests()) {
    prep.bodies.add_request(request);
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
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
  ASSERT_EQ(batch.requests().size(), 1u);

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
  auto *attack = entity.add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  auto const state = resolve_elephant_animation_state(ctx);

  EXPECT_FALSE(state.used_override);
  EXPECT_TRUE(state.inputs.is_attacking);
  EXPECT_TRUE(state.inputs.is_melee);
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
  Render::GL::DrawContext ctx{};
  CreatureGraphOutput graph{};

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
  for (int i = 0; i < 10; ++i) {
    requests.push_back(
        make_request(ArchetypeRegistry::k_humanoid_base, CreatureLOD::Full,
                     (i % 2 == 0) ? pass_intent_from_ctx(normal_ctx)
                                  : pass_intent_from_ctx(prewarm_ctx),
                     static_cast<std::uint32_t>(i)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 5u);
}

TEST(TemplatePrewarmRegression, HorsePrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request =
      make_request(ArchetypeRegistry::k_horse_base, CreatureLOD::Full,
                   pass_intent_from_ctx(prewarm_ctx), 123U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1u), sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, ElephantPrewarmProducesZeroDraws) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  PrewarmCountingSubmitter sink;
  auto const request =
      make_request(ArchetypeRegistry::k_elephant_base, CreatureLOD::Full,
                   pass_intent_from_ctx(prewarm_ctx), 456U);
  const auto stats = submit_requests_for_test(
      std::span<const CreatureRenderRequest>(&request, 1u), sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
}

TEST(TemplatePrewarmRegression, AllLodLevelsRespectPrewarmFiltering) {
  Render::GL::DrawContext prewarm_ctx{};
  prewarm_ctx.template_prewarm = true;

  std::vector<CreatureRenderRequest> requests;
  for (auto lod : {CreatureLOD::Full, CreatureLOD::Minimal}) {
    requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base, lod,
                                    pass_intent_from_ctx(prewarm_ctx),
                                    static_cast<std::uint32_t>(lod)));
  }

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
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
  requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                  CreatureLOD::Full, RenderPassIntent::Shadow));
  requests.push_back(make_request(ArchetypeRegistry::k_horse_base,
                                  CreatureLOD::Minimal,
                                  RenderPassIntent::Shadow));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 0u);
  EXPECT_EQ(stats.lod_full, 0u);
  EXPECT_EQ(stats.lod_minimal, 0u);
}

TEST(TemplatePrewarmRegression, MainRowsIncrementLodStats) {
  std::vector<CreatureRenderRequest> requests;
  requests.push_back(make_request(ArchetypeRegistry::k_humanoid_base,
                                  CreatureLOD::Full, RenderPassIntent::Main));
  requests.push_back(make_request(ArchetypeRegistry::k_horse_base,
                                  CreatureLOD::Minimal,
                                  RenderPassIntent::Main));

  PrewarmCountingSubmitter sink;
  const auto stats = submit_requests_for_test(requests, sink);

  EXPECT_EQ(stats.entities_submitted, 2u);
  EXPECT_EQ(stats.lod_full, 1u);
  EXPECT_EQ(stats.lod_minimal, 1u);
}

TEST(TemplatePrewarmRegression, QuadrupedHorseShadowRequiresResources) {
  Render::GL::DrawContext ctx{};
  CreatureGraphOutput graph{};
  QuadrupedShadowStateInputs inputs{};
  inputs.ctx = &ctx;
  inputs.graph = &graph;
  inputs.lod = Render::Creature::CreatureLOD::Full;
  inputs.kind = CreatureKind::Horse;

  auto const shadow = prepare_quadruped_shadow_state(inputs);

  EXPECT_FALSE(shadow.enabled);
}

TEST(TemplatePrewarmRegression, QuadrupedElephantShadowRequiresResources) {
  Render::GL::DrawContext ctx{};
  CreatureGraphOutput graph{};
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

  auto &nation_registry = Game::Systems::NationRegistry::instance();
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
  Engine::Core::Entity *archer = world.create_entity();
  ASSERT_NE(archer, nullptr);
  add_test_unit(*archer, SpawnType::Archer, NationID::RomanRepublic, 1,
                "troops/roman/archer");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto builder_renderer = registry.get("troops/roman/builder");
  ASSERT_TRUE(static_cast<bool>(builder_renderer));

  Engine::Core::Entity builder(9001);
  add_test_unit(builder, SpawnType::Builder, NationID::RomanRepublic, 1,
                "troops/roman/builder");

  renderer.rigged_mesh_cache().reset_frame_stats();

  Render::GL::DrawContext ctx{renderer.resources(), &builder, nullptr,
                              QMatrix4x4()};
  ctx.renderer_id = "troops/roman/builder";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_variant_override = true;
  ctx.variant_override = 0;

  builder_renderer(ctx, renderer);

  const auto &stats = renderer.rigged_mesh_cache().frame_stats();
  EXPECT_EQ(stats.misses, 0U);
  EXPECT_GT(stats.hits, 0U);

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

TEST(TemplatePrewarmRegression, WorldPrewarmsCivilianAliases) {
  using Game::Systems::NationID;
  using Game::Units::SpawnType;
  using Render::Creature::CreatureLOD;

  auto &nation_registry = Game::Systems::NationRegistry::instance();
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
  Engine::Core::Entity *civilian = world.create_entity();
  ASSERT_NE(civilian, nullptr);
  add_test_unit(*civilian, SpawnType::Civilian, NationID::RomanRepublic, 1,
                "troops/roman/civilian");

  renderer.prewarm_unit_templates(&world);
  Render::Creature::set_runtime_bake_forbidden(true);

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  const auto civilian_renderer = registry.get("troops/roman/civilian");
  ASSERT_TRUE(static_cast<bool>(civilian_renderer));

  Engine::Core::Entity civilian_entity(9002);
  add_test_unit(civilian_entity, SpawnType::Civilian, NationID::RomanRepublic,
                1, "troops/roman/civilian");

  renderer.rigged_mesh_cache().reset_frame_stats();

  Render::GL::DrawContext ctx{renderer.resources(), &civilian_entity, nullptr,
                              QMatrix4x4()};
  ctx.renderer_id = "troops/roman/civilian";
  ctx.backend = renderer.backend();
  ctx.allow_template_cache = true;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = CreatureLOD::Full;
  ctx.has_variant_override = true;
  ctx.variant_override = 0;

  civilian_renderer(ctx, renderer);

  const auto &stats = renderer.rigged_mesh_cache().frame_stats();
  EXPECT_EQ(stats.misses, 0U);
  EXPECT_GT(stats.hits, 0U);

  renderer.shutdown();
  Render::Creature::set_runtime_bake_forbidden(false);
  Game::Systems::TroopProfileService::instance().clear();
  nation_registry.clear();
}

} // namespace

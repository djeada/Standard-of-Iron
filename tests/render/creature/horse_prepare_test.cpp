// Phase A regression — horse prepare module.
//
// Verifies make_horse_prepared_row stamps the correct kind/pass/lod and that
// Shadow-tagged horse rows are filtered by submit().

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/snapshot_mesh_asset.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/horse/prepare.h"
#include "render/submitter.h"
#include "render/template_cache.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {}
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

struct ScopedFlatTerrain {
  explicit ScopedFlatTerrain(float height) {
    auto &terrain = Game::Map::TerrainService::instance();
    std::vector<float> heights(9, height);
    std::vector<Game::Map::TerrainType> terrain_types(
        9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                    {}, Game::Map::BiomeSettings{});
  }

  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
};

auto find_assets_dir() -> std::string {
  for (auto const *candidate :
       {"assets/creatures", "../assets/creatures", "../../assets/creatures"}) {
    std::filesystem::path p{candidate};
    if (std::filesystem::exists(p / "horse.bpat")) {
      return std::filesystem::absolute(p).string();
    }
  }
  return {};
}

auto wrap_phase(float phase) -> float {
  phase = std::fmod(phase, 1.0F);
  if (phase < 0.0F) {
    phase += 1.0F;
  }
  return phase;
}

auto phase_distance(float a, float b) -> float {
  float const delta = std::abs(wrap_phase(a) - wrap_phase(b));
  return std::min(delta, 1.0F - delta);
}

TEST(HorsePrepare, MakePreparedHorseRowStampsKindAndPass) {
  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;

  Render::Horse::HorseSpecPose pose{};
  Render::GL::HorseVariant variant{};
  QMatrix4x4 world;
  const auto row = Render::Creature::Pipeline::make_prepared_horse_row(
      spec, pose, variant, world, /*seed*/ 11,
      Render::Creature::CreatureLOD::Minimal,
      /*entity_id*/ 0, Render::Creature::Pipeline::RenderPassIntent::Shadow);

  EXPECT_EQ(row.spec.kind, Render::Creature::Pipeline::CreatureKind::Horse);
  EXPECT_EQ(row.lod, Render::Creature::CreatureLOD::Minimal);
  EXPECT_EQ(row.pass, Render::Creature::Pipeline::RenderPassIntent::Shadow);
  EXPECT_EQ(row.seed, 11u);
}

TEST(HorsePrepare, ShadowHorseRowProducesNoDraw) {
  NullSubmitter sink;
  Render::Creature::Pipeline::CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kHorseBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  (void)Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(sink.rigged_calls, 0);
}

TEST(HorsePrepare, MainHorseRowProducesEntitySubmission) {
  NullSubmitter sink;
  Render::Creature::Pipeline::CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kHorseBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Main;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
}

TEST(HorsePrepare, TemplatePrewarmRenderWarmsSnapshotCache) {
  Render::GL::HorseRendererBase renderer;
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;

  Render::GL::AnimationInputs anim{};
  Render::GL::HumanoidAnimationContext rider_ctx{};
  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));
  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();

  renderer.render(ctx, anim, rider_ctx, profile, nullptr, nullptr, recorder);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0u);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(HorsePrepare, MinimalRenderUsesPrebakedSnapshotAssetWithoutRiggedBake) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(
      bpat.load_species(Render::Creature::Bpat::kSpeciesHorse, root + "/horse.bpat"));

  auto &snapshot_reg = Render::Creature::Snapshot::SnapshotMeshRegistry::instance();
  snapshot_reg.clear();

  auto const temp_dir =
      std::filesystem::temp_directory_path() / "soi_horse_snapshot_test";
  std::filesystem::create_directories(temp_dir);
  auto const asset_path = temp_dir / "horse_minimal.bpsm";

  Render::Creature::Snapshot::SnapshotMeshWriter writer(
      Render::Creature::Bpat::kSpeciesHorse,
      Render::Creature::CreatureLOD::Minimal, 3U,
      std::array<std::uint32_t, 3>{0U, 1U, 2U});
  writer.add_clip({"idle", 1U});
  std::array<Render::GL::RiggedVertex, 3> vertices{};
  vertices[0].position_bone_local = {-1.0F, 0.0F, 0.0F};
  vertices[1].position_bone_local = {1.0F, 0.0F, 0.0F};
  vertices[2].position_bone_local = {0.0F, 1.0F, 0.0F};
  for (auto &v : vertices) {
    v.normal_bone_local = {0.0F, 1.0F, 0.0F};
    v.bone_indices = {0, 0, 0, 0};
    v.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
  }
  writer.append_clip_vertices(vertices);

  std::ofstream out(asset_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.good());
  ASSERT_TRUE(writer.write(out));
  out.close();
  ASSERT_TRUE(snapshot_reg.load_species(Render::Creature::Bpat::kSpeciesHorse,
                                        Render::Creature::CreatureLOD::Minimal,
                                        asset_path.string()))
      << snapshot_reg.last_error();

  Render::GL::HorseRendererBase renderer;
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->owner_id = 1;
  unit->max_health = 100;
  unit->health = 100;
  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  Render::GL::HumanoidAnimationContext rider_ctx{};
  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));
  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  recorder.rigged_mesh_cache().clear();

  renderer.render(ctx, anim, rider_ctx, profile, nullptr, nullptr, recorder,
                  Render::GL::HorseLOD::Minimal);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0u);
  EXPECT_EQ(recorder.rigged_mesh_cache().size(), 0u);
}

TEST(HorsePrepare, MinimalPreparationSnapsHorseHoofContactToTerrainHeight) {
  ScopedFlatTerrain terrain(1.9F);

  Render::GL::HorseRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.model.translate(-0.3F, 6.0F, 0.45F);

  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));

  Render::Horse::HorsePreparation prep;
  Render::Horse::prepare_horse_minimal(owner, ctx, profile, nullptr, prep);

  auto const requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1u);

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_animated(
      profile.dims, profile.gait, Render::Horse::HorsePoseMotion{}, pose);
  float const hoof_contact_y =
      Render::Creature::Pipeline::grounded_horse_contact_y(pose, 0U, 0.0F);

  EXPECT_GT(requests[0].world.map(QVector3D(0.0F, 0.0F, 0.0F)).y(), 1.9F);
  EXPECT_NEAR(requests[0].world.map(QVector3D(0.0F, hoof_contact_y, 0.0F)).y(),
              1.9F, 0.0001F);
}

TEST(HorsePrepare,
     MoveToIdleTransitionKeepsLocomotionPoseActiveUntilBlendEnds) {
  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));

  Render::Creature::HorseAnimationStateComponent state{};
  Render::GL::HumanoidAnimationContext rider_ctx{};
  rider_ctx.gait.state = Render::GL::HumanoidMotionState::Walk;
  rider_ctx.gait.speed = 1.8F;
  rider_ctx.gait.normalized_speed = 0.45F;
  rider_ctx.gait.cycle_time = 1.0F;
  rider_ctx.gait.cycle_phase = 0.35F;

  Render::GL::AnimationInputs moving_anim{};
  moving_anim.time = 0.0F;
  moving_anim.is_moving = true;
  (void)Render::GL::evaluate_horse_motion(profile, moving_anim, rider_ctx,
                                          &state);
  moving_anim.time = 0.4F;
  auto const moving_sample = Render::GL::evaluate_horse_motion(
      profile, moving_anim, rider_ctx, &state);
  EXPECT_EQ(moving_sample.gait_type, Render::GL::GaitType::WALK);

  rider_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
  rider_ctx.gait.speed = 0.0F;
  rider_ctx.gait.normalized_speed = 0.0F;
  rider_ctx.gait.cycle_time = 0.0F;
  Render::GL::AnimationInputs idle_anim{};
  idle_anim.time = 0.45F;
  idle_anim.is_moving = false;

  auto const stop_sample =
      Render::GL::evaluate_horse_motion(profile, idle_anim, rider_ctx, &state);

  EXPECT_EQ(stop_sample.gait_type, Render::GL::GaitType::WALK);
  EXPECT_TRUE(stop_sample.is_moving);
}

TEST(HorsePrepare, MoveToIdleTransitionKeepsBpatPhaseContinuous) {
  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));

  Render::Creature::HorseAnimationStateComponent state{};
  Render::GL::HumanoidAnimationContext rider_ctx{};
  rider_ctx.gait.state = Render::GL::HumanoidMotionState::Walk;
  rider_ctx.gait.speed = 1.8F;
  rider_ctx.gait.normalized_speed = 0.45F;
  rider_ctx.gait.cycle_time = 1.0F;
  rider_ctx.gait.cycle_phase = 0.35F;

  Render::GL::AnimationInputs moving_anim{};
  moving_anim.time = 0.0F;
  moving_anim.is_moving = true;
  (void)Render::GL::evaluate_horse_motion(profile, moving_anim, rider_ctx,
                                          &state);
  moving_anim.time = 0.4F;
  auto const moving_sample = Render::GL::evaluate_horse_motion(
      profile, moving_anim, rider_ctx, &state);

  rider_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
  rider_ctx.gait.speed = 0.0F;
  rider_ctx.gait.normalized_speed = 0.0F;
  rider_ctx.gait.cycle_time = 0.0F;

  Render::GL::AnimationInputs idle_anim{};
  idle_anim.time = 0.45F;
  idle_anim.is_moving = false;
  auto const stop_sample =
      Render::GL::evaluate_horse_motion(profile, idle_anim, rider_ctx, &state);

  float const expected_continuation = wrap_phase(
      moving_sample.phase + (idle_anim.time - moving_anim.time) / 1.1F);
  float const old_global_clock_phase =
      wrap_phase(idle_anim.time / 1.1F + profile.gait.phase_offset);

  EXPECT_NEAR(stop_sample.phase, expected_continuation, 0.0001F);
  EXPECT_GT(phase_distance(stop_sample.phase, old_global_clock_phase), 0.05F);
}

} // namespace

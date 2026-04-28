// Phase A regression — elephant prepare module.

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/snapshot_mesh_asset.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_motion.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/elephant/prepare.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/submitter.h"
#include "render/template_cache.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
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
    if (std::filesystem::exists(p / "elephant.bpat")) {
      return std::filesystem::absolute(p).string();
    }
  }
  return {};
}

auto make_test_elephant_profile() -> Render::GL::ElephantProfile {
  Render::GL::ElephantProfile profile{};
  profile.dims.body_length = 3.2F;
  profile.dims.body_width = 1.4F;
  profile.dims.body_height = 2.4F;
  profile.dims.barrel_center_y = 2.1F;
  profile.dims.neck_length = 1.0F;
  profile.dims.neck_width = 0.45F;
  profile.dims.head_length = 1.1F;
  profile.dims.head_width = 0.8F;
  profile.dims.head_height = 0.9F;
  profile.dims.trunk_length = 1.5F;
  profile.dims.trunk_base_radius = 0.18F;
  profile.dims.trunk_tip_radius = 0.05F;
  profile.dims.foot_radius = 0.24F;
  profile.dims.leg_length = 1.7F;
  profile.dims.howdah_height = 0.6F;
  profile.dims.idle_bob_amplitude = 0.05F;
  profile.dims.move_bob_amplitude = 0.12F;
  profile.gait.cycle_time = 1.4F;
  profile.gait.front_leg_phase = 0.0F;
  profile.gait.rear_leg_phase = 0.5F;
  profile.gait.stride_swing = 0.3F;
  profile.gait.stride_lift = 0.12F;
  return profile;
}

TEST(ElephantPrepare, MakePreparedElephantRowStampsKindAndPass) {
  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Elephant;

  QMatrix4x4 world;
  const auto row = Render::Creature::Pipeline::make_prepared_creature_row(
      spec, Render::Creature::Pipeline::CreatureKind::Elephant, world, /*seed*/ 23,
      Render::Creature::CreatureLOD::Minimal,
      /*entity_id*/ 0, Render::Creature::Pipeline::RenderPassIntent::Shadow);

  EXPECT_EQ(row.spec.kind, Render::Creature::Pipeline::CreatureKind::Elephant);
  EXPECT_EQ(row.lod, Render::Creature::CreatureLOD::Minimal);
  EXPECT_EQ(row.pass, Render::Creature::Pipeline::RenderPassIntent::Shadow);
  EXPECT_EQ(row.seed, 23u);
}

TEST(ElephantPrepare, ShadowElephantRowProducesNoDraw) {
  NullSubmitter sink;
  Render::Creature::Pipeline::CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kElephantBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  (void)Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(sink.rigged_calls, 0);
}

TEST(ElephantPrepare, MainElephantRowProducesEntitySubmission) {
  NullSubmitter sink;
  Render::Creature::Pipeline::CreaturePreparationResult prep;
  Render::Creature::CreatureRenderRequest req{};
  req.archetype = Render::Creature::ArchetypeRegistry::kElephantBase;
  req.state = Render::Creature::AnimationStateId::Idle;
  req.lod = Render::Creature::CreatureLOD::Full;
  req.pass = Render::Creature::Pipeline::RenderPassIntent::Main;
  req.world_already_grounded = true;
  prep.bodies.add_request(req);
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
}

TEST(ElephantPrepare, MinimalRenderUsesPrebakedSnapshotAssetWithoutRiggedBake) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::kSpeciesElephant,
                                root + "/elephant.bpat"));

  auto &snapshot_reg = Render::Creature::Snapshot::SnapshotMeshRegistry::instance();
  snapshot_reg.clear();

  auto const temp_dir =
      std::filesystem::temp_directory_path() / "soi_elephant_snapshot_test";
  std::filesystem::create_directories(temp_dir);
  auto const asset_path = temp_dir / "elephant_minimal.bpsm";

  Render::Creature::Snapshot::SnapshotMeshWriter writer(
      Render::Creature::Bpat::kSpeciesElephant,
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
  ASSERT_TRUE(
      snapshot_reg.load_species(Render::Creature::Bpat::kSpeciesElephant,
                                Render::Creature::CreatureLOD::Minimal,
                                asset_path.string()))
      << snapshot_reg.last_error();

  Render::GL::ElephantRendererBase renderer;
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Elephant;
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
  Render::GL::ElephantProfile profile = make_test_elephant_profile();
  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  recorder.rigged_mesh_cache().clear();

  renderer.render(ctx, anim, profile, nullptr, nullptr, recorder,
                  Render::GL::HorseLOD::Minimal);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0u);
  EXPECT_EQ(recorder.rigged_mesh_cache().size(), 0u);
}

TEST(ElephantPrepare, MotionSampleCarriesResolvedRenderState) {
  Render::GL::ElephantProfile profile = make_test_elephant_profile();
  Render::GL::AnimationInputs anim{
      .time = 1.25F,
      .is_moving = true,
      .is_running = false,
      .is_attacking = true,
      .is_melee = false,
      .is_in_hold_mode = false,
      .is_exiting_hold = false,
      .hold_exit_progress = 0.0F,
      .combat_phase = Render::GL::CombatAnimPhase::WindUp,
      .combat_phase_progress = 0.0F,
      .attack_variant = 0,
      .is_hit_reacting = false,
      .hit_reaction_intensity = 0.0F,
      .is_healing = false,
      .healing_target_dx = 0.0F,
      .healing_target_dz = 0.0F,
      .is_constructing = false,
      .construction_progress = 0.0F,
  };

  float const baseline_cycle = profile.gait.cycle_time;
  Render::GL::HowdahAttachmentFrame const base_howdah =
      Render::GL::compute_howdah_frame(profile);

  Render::GL::ElephantMotionSample const motion =
      Render::GL::evaluate_elephant_motion(profile, anim);

  EXPECT_FLOAT_EQ(motion.gait.cycle_time, baseline_cycle);
  EXPECT_FLOAT_EQ(profile.gait.cycle_time, baseline_cycle);
  EXPECT_NEAR(motion.howdah.howdah_center.y(),
              base_howdah.howdah_center.y() + motion.bob, 0.0001F);
  EXPECT_NEAR(motion.howdah.seat_position.y(),
              base_howdah.seat_position.y() + motion.bob, 0.0001F);
  EXPECT_NEAR(motion.barrel_center.y(),
              profile.dims.barrel_center_y + motion.bob, 0.0001F);
  EXPECT_NEAR(motion.barrel_center.x(), motion.body_sway, 0.0001F);
  EXPECT_GT(motion.chest_center.z(), motion.barrel_center.z());
  EXPECT_LT(motion.rump_center.z(), motion.barrel_center.z());
  EXPECT_GT(motion.head_center.z(), motion.neck_top.z());
  EXPECT_TRUE(motion.is_fighting);
}

TEST(ElephantPrepare, PoseMotionBuildsFromPreparedSample) {
  Render::GL::ElephantProfile profile = make_test_elephant_profile();
  Render::GL::AnimationInputs anim{
      .time = 0.75F,
      .is_moving = true,
      .is_running = false,
      .is_attacking = false,
      .is_melee = false,
      .is_in_hold_mode = false,
      .is_exiting_hold = false,
      .hold_exit_progress = 0.0F,
      .combat_phase = Render::GL::CombatAnimPhase::Recover,
      .combat_phase_progress = 0.0F,
      .attack_variant = 0,
      .is_hit_reacting = false,
      .hit_reaction_intensity = 0.0F,
      .is_healing = false,
      .healing_target_dx = 0.0F,
      .healing_target_dz = 0.0F,
      .is_constructing = false,
      .construction_progress = 0.0F,
  };

  Render::GL::ElephantMotionSample const motion =
      Render::GL::evaluate_elephant_motion(profile, anim);
  auto const pose_motion = Render::GL::build_elephant_pose_motion(motion, anim);

  EXPECT_FLOAT_EQ(pose_motion.phase, motion.phase);
  EXPECT_FLOAT_EQ(pose_motion.bob, motion.bob);
  EXPECT_EQ(pose_motion.is_moving, motion.is_moving);
  EXPECT_EQ(pose_motion.is_fighting, motion.is_fighting);
  EXPECT_FLOAT_EQ(pose_motion.anim_time, anim.time);
  EXPECT_EQ(pose_motion.combat_phase, anim.combat_phase);
}

TEST(ElephantPrepare, MotionScalesSwayWithGaitIntensity) {
  Render::GL::ElephantProfile low = make_test_elephant_profile();
  Render::GL::ElephantProfile high = low;
  low.gait.stride_swing = 0.10F;
  high.gait.stride_swing = 0.80F;
  low.gait.stride_lift = 0.08F;
  high.gait.stride_lift = 0.24F;

  Render::GL::AnimationInputs anim{
      .time = high.gait.cycle_time * 0.25F,
      .is_moving = true,
      .is_running = true,
  };

  Render::GL::ElephantMotionSample const low_motion =
      Render::GL::evaluate_elephant_motion(low, anim);
  Render::GL::ElephantMotionSample const high_motion =
      Render::GL::evaluate_elephant_motion(high, anim);

  EXPECT_GT(std::abs(high_motion.body_sway), std::abs(low_motion.body_sway));
  EXPECT_NE(low_motion.bob, 0.0F);
  EXPECT_NE(high_motion.bob, 0.0F);
}

TEST(ElephantPrepare, MovingMotionAddsForeAftWeightTransfer) {
  Render::GL::ElephantProfile profile = make_test_elephant_profile();
  Render::GL::AnimationInputs anim{
      .time = profile.gait.cycle_time * 0.25F,
      .is_moving = true,
      .is_running = false,
  };

  Render::GL::ElephantMotionSample const motion =
      Render::GL::evaluate_elephant_motion(profile, anim);

  EXPECT_GT(motion.chest_center.z(), motion.barrel_center.z());
  EXPECT_LT(motion.rump_center.z(), motion.barrel_center.z());
  EXPECT_NEAR(motion.chest_center.y(), motion.rump_center.y(), 0.3F);
  EXPECT_GT(std::abs(motion.chest_center.y() - motion.rump_center.y()),
            0.0001F);
}

TEST(ElephantPrepare, MinimalPreparationSnapsElephantBodyToTerrainHeight) {
  ScopedFlatTerrain terrain(3.1F);

  Render::GL::ElephantRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.model.translate(0.2F, 11.0F, -0.35F);

  Render::GL::ElephantProfile profile = Render::GL::make_elephant_profile(
      29U, QVector3D(0.5F, 0.2F, 0.1F), QVector3D(0.7F, 0.7F, 0.2F));

  Render::GL::AnimationInputs anim{};
  Render::Elephant::ElephantPreparation prep;
  Render::Elephant::prepare_elephant_render(
      owner, ctx, anim, profile, nullptr, nullptr,
      Render::Creature::CreatureLOD::Minimal, prep);

  auto const requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_NEAR(requests[0].world.map(QVector3D(0.0F, 0.0F, 0.0F)).y(), 3.1F,
              0.0001F);
}

TEST(ElephantPrepare, FullPreparationEmitsWalkingShadowRequest) {
  ScopedFlatTerrain terrain(0.0F);

  Render::GL::ElephantRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;
  Render::GL::AnimationInputs anim{
      .time = 0.4F,
      .is_moving = true,
      .is_running = false,
  };
  Render::GL::ElephantProfile profile = make_test_elephant_profile();

  Render::Elephant::ElephantPreparation prep;
  Render::Elephant::prepare_elephant_render(
      owner, ctx, anim, profile, nullptr, nullptr,
      Render::Creature::CreatureLOD::Full, prep);

  auto const rows = prep.bodies.rows();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].spec.kind, Render::Creature::Pipeline::CreatureKind::Elephant);
  auto const requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(requests[0].state, Render::Creature::AnimationStateId::Walk);
  EXPECT_GT(requests[0].phase, 0.0F);
}

TEST(ElephantPrepare, FullStationaryPreparationEmitsIdleShadowRequest) {
  ScopedFlatTerrain terrain(0.0F);

  Render::GL::ElephantRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;
  Render::GL::AnimationInputs anim{
      .time = 0.2F,
      .is_moving = false,
      .is_running = false,
  };
  Render::GL::ElephantProfile profile = make_test_elephant_profile();

  Render::Elephant::ElephantPreparation prep;
  Render::Elephant::prepare_elephant_render(
      owner, ctx, anim, profile, nullptr, nullptr,
      Render::Creature::CreatureLOD::Full, prep);

  auto const rows = prep.bodies.rows();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].spec.kind, Render::Creature::Pipeline::CreatureKind::Elephant);
  auto const requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(requests[0].state, Render::Creature::AnimationStateId::Idle);
  EXPECT_GE(requests[0].phase, 0.0F);
}

TEST(ElephantPrepare, TemplatePrewarmRenderWarmsSnapshotCache) {
  Render::GL::ElephantRendererBase renderer;
  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Elephant;
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
  auto profile = make_test_elephant_profile();
  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();

  renderer.render(ctx, anim, profile, nullptr, nullptr, recorder);

  EXPECT_GT(recorder.snapshot_mesh_cache().size(), 0u);
  EXPECT_TRUE(recorder.commands().empty());
}

} // namespace

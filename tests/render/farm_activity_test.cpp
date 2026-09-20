#include <chrono>
#include <gtest/gtest.h>

#include "game/core/component_economy.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "render/creature/archetype_registry.h"
#include "render/entity/farm_activity.h"
#include "render/entity/registry.h"
#include "render/graphics_settings.h"

namespace {
using namespace Render::GL;

class FarmRecorder final : public ISubmitter {
public:
  // Field workers are rigged humanoids, so they arrive as rigged() commands.
  // A mesh() here would mean someone reintroduced a hand-built figure.
  std::vector<QMatrix4x4> models;
  std::vector<std::uint32_t> bone_counts;
  std::vector<const void*> meshes;
  std::vector<float> poses;
  void rigged(const RiggedCreatureCmd& cmd) override {
    models.push_back(cmd.world);
    bone_counts.push_back(cmd.bone_count);
    meshes.push_back(static_cast<const void*>(cmd.mesh));
    // The animation lives in the bone palette now, so that is what has to move.
    float signature = cmd.palette_lerp;
    const auto bones = std::min<std::uint32_t>(cmd.bone_count, 8U);
    for (std::uint32_t bone = 0; cmd.bone_palette != nullptr && bone < bones; ++bone) {
      const auto& matrix = cmd.bone_palette[bone];
      for (int column = 0; column < 4; ++column) {
        signature += static_cast<float>(bone + 1U) *
                     matrix.column(column).lengthSquared() * (column + 1);
      }
    }
    poses.push_back(signature);
  }
  void clear() {
    models.clear();
    bone_counts.clear();
    meshes.clear();
    poses.clear();
  }
  void mesh(Mesh*, const QMatrix4x4&, const QVector3D&, Texture*, float, int) override {
    ADD_FAILURE() << "field workers must be rigged humanoids, not loose meshes";
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {
    ADD_FAILURE();
  }
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
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {
    ADD_FAILURE();
  }
};

class FarmActivityTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Registering the built-ins is what publishes each nation's field-worker
    // visual, so this also proves the wiring from the troop renderers.
    static const bool registered = [] {
      static EntityRendererRegistry shared_registry;
      Render::GL::register_built_in_entity_renderers(shared_registry);
      return true;
    }();
    (void)registered;
    old_quality = Render::GraphicsSettings::instance().quality();
    Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::High);
    entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = 1;
    unit->health = 600;
    unit->max_health = 600;
    entity->add_component<Engine::Core::FarmComponent>()->growth = 0.5F;
    ctx.entity = entity;
    ctx.world = &world;
    ctx.farm_activity = &activity;
    ctx.submission_visibility = &visibility;
    ctx.submission_fog_mode = SubmissionFogMode::Revealed;
    visibility.reset(nullptr, nullptr);
    ctx.model.scale(7.14F, 5.1F, 7.14F);
  }
  void TearDown() override {
    Render::GraphicsSettings::instance().set_quality(old_quality);
  }
  void draw(float time = 1.0F, bool carthage = false) {
    recorder.clear();
    ctx.animation_time = time;
    activity.begin_frame(&world, time);
    submit_farm_activity(ctx, recorder, carthage);
  }
  Engine::Core::World world;
  Engine::Core::Entity* entity{};
  FarmActivity activity;
  SubmissionVisibilityPolicy visibility;
  DrawContext ctx;
  FarmRecorder recorder;
  Render::GraphicsQuality old_quality{};
};

TEST_F(FarmActivityTest, BothFactionsUseTheirOwnCivilianRig) {
  for (bool carthage : {false, true}) {
    const auto& visual = farm_worker_visual(carthage);
    const auto& rig = nation_civilian_rig(carthage);
    ASSERT_TRUE(visual.valid());
    ASSERT_TRUE(rig.valid());
    EXPECT_NE(rig.idle, rig.working);
    EXPECT_EQ(rig.spec.kind, Render::Creature::Pipeline::CreatureKind::Humanoid);
  }
  EXPECT_NE(nation_civilian_rig(false).idle, nation_civilian_rig(true).idle);
  // Every worker wears the straw hat, and the sleeper wears it over its face;
  // a silently dropped attachment would leave these counts equal.
  const auto& registry = Render::Creature::ArchetypeRegistry::instance();
  for (bool carthage : {false, true}) {
    const auto& visual = farm_worker_visual(carthage);
    const auto& rig = nation_civilian_rig(carthage);
    const auto* bare_tend = registry.get(rig.idle);
    const auto* bare_reap = registry.get(rig.working);
    const auto* hatted_tend = registry.get(visual.hatted_tending);
    const auto* hatted_reap = registry.get(visual.hatted_reaping);
    const auto* gathering = registry.get(visual.hatted_gathering);
    const auto* napping = registry.get(visual.napping);
    ASSERT_NE(napping, nullptr);
    EXPECT_EQ(hatted_tend->bake_attachment_count, bare_tend->bake_attachment_count + 1);
    EXPECT_EQ(hatted_reap->bake_attachment_count, bare_reap->bake_attachment_count + 1);
    // Hat plus sheaf.
    EXPECT_EQ(gathering->bake_attachment_count, bare_tend->bake_attachment_count + 2);
    EXPECT_EQ(napping->bake_attachment_count, bare_reap->bake_attachment_count + 1);
    EXPECT_NE(visual.napping, visual.hatted_reaping);
  }
  draw();
  ASSERT_FALSE(recorder.bone_counts.empty());
  for (auto bones : recorder.bone_counts) {
    EXPECT_GT(bones, 0U); // A rig, not a pile of boxes.
  }
}

TEST_F(FarmActivityTest, BothFactionsAnimateAndFollowGrowthWithoutChangingIt) {
  const auto entity_count = world.entity_count();
  for (bool carthage : {false, true}) {
    draw(1.0F, carthage);
    const auto first = recorder.poses;
    const auto first_meshes = recorder.meshes;
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(activity.remaining, 61);
    draw(1.5F, carthage);
    EXPECT_NE(first, recorder.poses);
    auto* farm = entity->get_component<Engine::Core::FarmComponent>();
    EXPECT_FLOAT_EQ(farm->growth, 0.5F);
    farm->growth = 1.0F;
    draw(1.5F, carthage);
    EXPECT_NE(first_meshes, recorder.meshes); // The sickle rig replaces the hoe.
    EXPECT_FLOAT_EQ(farm->growth, 1.0F);
    EXPECT_EQ(farm->harvests, 0);
    farm->growth = 0.5F;
  }
  EXPECT_EQ(world.entity_count(), entity_count);
}

TEST_F(FarmActivityTest, SnapshotReplacementDoesNotResetPresentationSchedule) {
  Engine::Core::World snapshot;
  activity.begin_frame(&world, 500, &world);
  activity.gag_field = entity->get_id();
  activity.gag_stage = 4;
  activity.gag_started = 500;
  activity.gag_seen = true;
  activity.begin_frame(&snapshot, 501, &world);
  EXPECT_FLOAT_EQ(activity.gag(entity->get_id(), 4, 501), 1);
}

TEST_F(FarmActivityTest, NeighboringFieldsHaveDifferentPhasesAndStableAnchors) {
  draw();
  const auto first = recorder.models;
  auto* other = world.create_entity();
  *other->add_component<Engine::Core::UnitComponent>() =
      *entity->get_component<Engine::Core::UnitComponent>();
  other->add_component<Engine::Core::FarmComponent>()->growth = 0.5F;
  ctx.entity = other;
  draw();
  EXPECT_NE(first, recorder.models);
  const auto second = recorder.models;
  draw();
  EXPECT_EQ(second, recorder.models);
}

TEST_F(FarmActivityTest, RaisedTerrainGroundsEachActorWithoutChangingWorld) {
  Game::Session::SessionContext session;
  Game::Map::MapDefinition map;
  map.grid.width = map.grid.height = 32;
  map.grid.tile_size = 1;
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = hill.center_z = 0;
  hill.radius = 20;
  hill.height = 3;
  map.terrain.push_back(hill);
  session.terrain().initialize(map);
  ctx.world_view = Render::WorldView::of(session);
  draw();
  ASSERT_FALSE(recorder.models.empty());
  const auto first = recorder.models.front().column(3).toVector3D();
  EXPECT_GT(first.y(), 1.0F);
  EXPECT_FLOAT_EQ(entity->get_component<Engine::Core::FarmComponent>()->growth, 0.5F);
}

TEST_F(FarmActivityTest, NoActorsOnEmptyDeadDismantledOrClaimedFields) {
  auto* farm = entity->get_component<Engine::Core::FarmComponent>();
  farm->growth = 0;
  draw();
  EXPECT_TRUE(recorder.models.empty());
  farm->growth = 1;
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  unit->health = 0;
  draw();
  EXPECT_TRUE(recorder.models.empty());
  unit->health = 100; // The building renderer already shows ruined crops.
  draw();
  EXPECT_TRUE(recorder.models.empty());
  unit->health = 600;
  auto* worker = world.create_entity();
  auto* task = worker->add_component<Engine::Core::BuilderProductionComponent>();
  task->product_type = "harvest_grain";
  task->structure_task_entity_id = entity->get_id();
  draw();
  EXPECT_TRUE(recorder.models.empty());
  task->structure_task_entity_id = 0;
  draw();
  EXPECT_FALSE(recorder.models.empty());
  entity->add_component<Engine::Core::DismantleSiteComponent>();
  draw();
  EXPECT_TRUE(recorder.models.empty());
}

TEST_F(FarmActivityTest, FogRequiresCurrentlyVisibleCells) {
  Game::Map::VisibilityService::Snapshot fog;
  fog.initialized = true;
  fog.width = fog.height = 32;
  fog.half_width = fog.half_height = 16;
  fog.cells.resize(32 * 32);
  visibility.reset(nullptr, &fog);
  for (std::uint8_t state : {0, 1, 2}) {
    std::fill(fog.cells.begin(), fog.cells.end(), state);
    draw();
    EXPECT_EQ(recorder.models.empty(), state != 2);
  }
  std::fill(fog.cells.begin(), fog.cells.end(), 0);
  ctx.submission_fog_mode = SubmissionFogMode::Ignore; // Arena fog disabled.
  draw();
  EXPECT_FALSE(recorder.models.empty());
}

TEST_F(FarmActivityTest, OffscreenAndNeutralFieldsConsumeNoActorBudget) {
  Camera camera;
  camera.set_rts_constraints(false);
  camera.look_at(QVector3D(0, 10, 20), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
  camera.set_perspective(45, 1, 0.1F, 200);
  visibility.reset(&camera, nullptr);
  ctx.model.translate(1000, 0, 0);
  draw();
  EXPECT_TRUE(recorder.models.empty());
  EXPECT_EQ(activity.remaining, 64);
  ctx.model.setToIdentity();
  entity->get_component<Engine::Core::UnitComponent>()->owner_id = -1;
  draw();
  EXPECT_TRUE(recorder.models.empty());
}

TEST_F(FarmActivityTest, DensityQualityAndScreenSizeHaveHardBudgets) {
  activity.begin_frame(&world, 1);
  for (int i = 0; i < 100; ++i) {
    submit_farm_activity(ctx, recorder, false);
  }
  EXPECT_EQ(activity.remaining, 0);
  EXPECT_LE(recorder.models.size(), 64U * 24U);
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Low);
  draw();
  EXPECT_TRUE(recorder.models.empty());
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Medium);
  draw();
  EXPECT_EQ(activity.remaining, 23);
  ctx.screen_metrics.focal_length_px = 100;
  ctx.distance_sq = 10000;
  draw();
  EXPECT_TRUE(recorder.models.empty());
}

TEST_F(FarmActivityTest, DenseSubmissionCpuSample) {
  std::vector<double> samples;
  samples.reserve(200);
  for (int frame = 0; frame < 200; ++frame) {
    recorder.models.clear();
    const auto start = std::chrono::steady_clock::now();
    activity.begin_frame(&world, static_cast<float>(frame) / 60);
    for (int field = 0; field < 100; ++field) {
      submit_farm_activity(ctx, recorder, false);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    samples.push_back(std::chrono::duration<double, std::micro>(elapsed).count());
  }
  std::sort(samples.begin(), samples.end());
  RecordProperty("dense_submission_p95_us", samples[190]);
  EXPECT_EQ(activity.remaining, 0);
}

TEST_F(FarmActivityTest, AllPartCentersStayInCropRectangle) {
  for (bool carthage : {false, true}) {
    for (int frame = 0; frame < 32; ++frame) {
      draw(static_cast<float>(frame) / 14.4F, carthage);
      for (const auto& model : recorder.models) {
        const auto p = model.column(3).toVector3D();
        EXPECT_GT(p.x(), -0.88F * 7.14F);
        EXPECT_LT(p.x(), 0.72F * 7.14F);
        EXPECT_GT(p.z(), -0.84F * 7.14F);
        EXPECT_LT(p.z(), 0.40F * 7.14F);
        EXPECT_GE(p.y(), 0.035F * 5.1F);
      }
    }
  }
}

TEST_F(FarmActivityTest, LazySequenceIsRareCappedAndCancelledOnVisibilityLoss) {
  int starts = 0;
  float last_start = -1000;
  for (int tick = 0; tick < 20000; ++tick) {
    const float time = static_cast<float>(tick) * 0.25F;
    activity.begin_frame(&world, time);
    const float phase = activity.gag(entity->get_id(), 4, time);
    if (phase == 0) {
      EXPECT_GE(time - last_start, 300);
      last_start = time;
      ++starts;
      EXPECT_LT(activity.gag(entity->get_id() + 1, 4, time), 0);
    }
  }
  EXPECT_GT(starts, 0);
  EXPECT_LT(starts, 10);
  activity.gag_field = entity->get_id();
  activity.gag_stage = 4;
  activity.gag_started = 5000;
  activity.gag_seen = true;
  activity.begin_frame(&world, 5001);
  // No submission while hidden. Next frame releases the shared slot.
  activity.begin_frame(&world, 5002);
  EXPECT_EQ(activity.gag_field, 0U);
  activity.gag_field = entity->get_id();
  activity.gag_stage = 4;
  EXPECT_LT(activity.gag(entity->get_id(), 2, 5002), 0);
  EXPECT_EQ(activity.gag_field, 0U);
}

// The wiring this pins is easy to get wrong: a farm carries a UnitComponent, so
// the render snapshot files it with the units and it is drawn from the unit
// path. Wiring FarmActivity into the building path alone renders nothing.
TEST_F(FarmActivityTest, FarmsArePublishedOnTheUnitRenderPath) {
  entity->add_component<Engine::Core::TransformComponent>();
  entity->add_component<Engine::Core::BuildingComponent>();
  entity->add_component<Engine::Core::RenderableComponent>()->renderer_id =
      "troops/roman/farm";
  world.ensure_render_snapshot();
  const auto snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);
  const auto units = snapshot->render_unit_ids();
  EXPECT_NE(std::find(units.begin(), units.end(), entity->get_id()), units.end());
  const auto buildings = snapshot->render_building_ids();
  EXPECT_EQ(std::find(buildings.begin(), buildings.end(), entity->get_id()),
            buildings.end());
}

TEST_F(FarmActivityTest, SleeperReturnsToHarvestingWithoutChangingFarmState) {
  auto* farm = entity->get_component<Engine::Core::FarmComponent>();
  farm->growth = 1;
  activity.gag_field = entity->get_id();
  activity.gag_stage = 4;
  activity.gag_started = 0;
  activity.gag_seen = true;
  activity.world = &world;
  draw(2);
  const auto napping = recorder.poses;
  draw(4);
  EXPECT_NE(napping, recorder.poses);
  draw(7);
  // Back on the tools: the seated pose is gone and the reap cycle is running.
  EXPECT_NE(napping, recorder.poses);
  EXPECT_EQ(recorder.meshes.size(), napping.size());
  EXPECT_FLOAT_EQ(farm->growth, 1);
  EXPECT_EQ(farm->harvests, 0);
  EXPECT_EQ(world.entity_count(), 1U);
}
} // namespace

#include <gtest/gtest.h>

#define private public
#include "render/scene_renderer.h"
#undef private

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/visibility_service.h"
#include "game/systems/camera_visibility_service.h"
#include "render/draw_queue.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/geom/mode_indicator.h"
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include "render/profiling/combat_animation_diagnostics.h"
#endif

namespace {

class SceneRendererEffects : public ::testing::Test {
protected:
  void SetUp() override {
    auto& fog = Game::Map::VisibilityService::instance();
    fog.initialize(256, 256, 1.0F);
    fog.reveal_all();
    Game::Systems::CameraVisibilityService::instance().clear_camera();
  }
};

auto activity_of(
    Game::Systems::ActivityKind kind,
    Game::Systems::ActivityState state = Game::Systems::ActivityState::Active)
    -> Game::Systems::UnitActivity {
  Game::Systems::UnitActivity activity{};
  activity.kind = kind;
  activity.state = state;
  return activity;
}

TEST(ModeIndicatorPlacement, IgnoresTransformScaleForWorldHeight) {
  Render::GL::Renderer renderer;
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1U);
#endif

  Engine::Core::TransformComponent transform{};
  transform.position = {1.0F, 2.0F, 3.0F};
  transform.scale = {0.5F, 0.5F, 0.5F};

  renderer.enqueue_activity_indicator(
      1U, &transform, activity_of(Game::Systems::ActivityKind::Guard), 1, 0.0F, 0.0F);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<Render::GL::ModeIndicatorCmd>(items.front()));

  auto const& cmd = std::get<Render::GL::ModeIndicatorCmd>(items.front());
  QVector3D const translation = cmd.model.column(3).toVector3D();

  EXPECT_FLOAT_EQ(translation.x(), transform.position.x);
  EXPECT_FLOAT_EQ(translation.y(),
                  transform.position.y + Render::Geom::k_indicator_height_base);
  EXPECT_FLOAT_EQ(translation.z(), transform.position.z);
  EXPECT_EQ(cmd.mode_type, static_cast<int>(Game::Systems::ActivityKind::Guard));
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  EXPECT_TRUE(diagnostics.mode_indicator_submitted(1U));
  diagnostics.set_enabled(false);
#endif
}

TEST(ModeIndicatorPlacement, UsesThePerUnitAnchorHeightWhenOneIsCached) {
  Render::GL::Renderer renderer;
  Engine::Core::TransformComponent transform{};
  transform.position = {0.0F, 0.0F, 0.0F};

  constexpr float anchor = 6.25F;
  renderer.enqueue_activity_indicator(1U,
                                      &transform,
                                      activity_of(Game::Systems::ActivityKind::Repair),
                                      1,
                                      anchor,
                                      0.0F);

  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  auto const& cmd = std::get<Render::GL::ModeIndicatorCmd>(items.front());
  EXPECT_FLOAT_EQ(cmd.model.column(3).y(), anchor);
}

TEST(ModeIndicatorPlacement, SubmitsAnIconForEveryNoteworthyActivity) {
  Render::GL::Renderer renderer;
  Engine::Core::TransformComponent transform{};

  std::size_t expected = 0;
  for (std::size_t i = 0; i < Render::Geom::k_indicator_kind_count; ++i) {
    auto const kind = static_cast<Game::Systems::ActivityKind>(i);
    auto const activity = activity_of(kind);
    if (Game::Systems::activity_is_noteworthy(activity)) {
      ++expected;
    }
    renderer.enqueue_activity_indicator(static_cast<Engine::Core::EntityID>(i + 1U),
                                        &transform,
                                        activity,
                                        1,
                                        3.0F,
                                        0.0F);
  }

  EXPECT_EQ(renderer.m_active_queue->items().size(), expected);
  EXPECT_EQ(expected, Render::Geom::k_indicator_kind_count - 2U);
}

TEST(ModeIndicatorPlacement, StaysSilentForUnitsTheLocalPlayerDoesNotOwn) {
  Render::GL::Renderer renderer;
  renderer.set_local_owner_id(1);
  Engine::Core::TransformComponent transform{};

  renderer.enqueue_activity_indicator(
      1U,
      &transform,
      activity_of(Game::Systems::ActivityKind::ChopWood),
      2,
      3.0F,
      0.0F);

  EXPECT_TRUE(renderer.m_active_queue->items().empty());
}

TEST(ModeIndicatorPlacement, DropsIconsPastTheFadeDistance) {
  Render::GL::Renderer renderer;
  Engine::Core::TransformComponent transform{};

  renderer.enqueue_activity_indicator(1U,
                                      &transform,
                                      activity_of(Game::Systems::ActivityKind::Hold),
                                      1,
                                      3.0F,
                                      Render::Geom::k_indicator_fade_end_sq + 1.0F);

  EXPECT_TRUE(renderer.m_active_queue->items().empty());
}

TEST(ModeIndicatorPlacement, CarriesTheActivityStateInTheCommandColour) {
  Render::GL::Renderer renderer;
  Engine::Core::TransformComponent transform{};

  renderer.enqueue_activity_indicator(
      1U,
      &transform,
      activity_of(Game::Systems::ActivityKind::Construct,
                  Game::Systems::ActivityState::Unavailable),
      1,
      3.0F,
      0.0F);

  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  auto const& cmd = std::get<Render::GL::ModeIndicatorCmd>(items.front());
  EXPECT_EQ(cmd.color,
            Render::Geom::indicator_color(Game::Systems::ActivityKind::Construct,
                                          Game::Systems::ActivityState::Unavailable));
}

TEST_F(SceneRendererEffects, BloodPoolEnqueuesEffectBatchCommand) {
  Render::GL::Renderer renderer;
  QVector3D const position(1.0F, 0.02F, 3.0F);

  renderer.blood_pool(position, 0.6F, 0.75F, 0.4F, 1.2F, 0.33F);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const& cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BloodPool);
  EXPECT_FLOAT_EQ(cmd.position.x(), position.x());
  EXPECT_FLOAT_EQ(cmd.position.y(), position.y());
  EXPECT_FLOAT_EQ(cmd.position.z(), position.z());
  EXPECT_FLOAT_EQ(cmd.radius, 0.6F);
  EXPECT_FLOAT_EQ(cmd.alpha_scale, 0.75F);
  EXPECT_FLOAT_EQ(cmd.rotation, 0.4F);
  EXPECT_FLOAT_EQ(cmd.aspect_ratio, 1.2F);
  EXPECT_FLOAT_EQ(cmd.seed, 0.33F);
}

TEST_F(SceneRendererEffects, FireballEnqueuesDedicatedEffectBatchCommand) {
  Render::GL::Renderer renderer;
  QVector3D const position(2.0F, 1.1F, -4.0F);

  renderer.fireball(position, QVector3D(1.0F, 0.44F, 0.10F), 0.32F, 1.25F, 3.0F);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const& cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::Fireball);
  EXPECT_FLOAT_EQ(cmd.position.x(), position.x());
  EXPECT_FLOAT_EQ(cmd.position.y(), position.y());
  EXPECT_FLOAT_EQ(cmd.position.z(), position.z());
  EXPECT_FLOAT_EQ(cmd.radius, 0.32F);
  EXPECT_FLOAT_EQ(cmd.intensity, 1.25F);
}

TEST_F(SceneRendererEffects, BloodStainUsesTerrainAlignedHeight) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* blood_entity = world.create_entity();
  blood_entity->add_component<Engine::Core::TransformComponent>(4.0F, 1.75F, -2.0F);
  blood_entity->add_component<Engine::Core::BloodStainComponent>(
      0.8F, 8.0F, 0.2F, 1.1F, 0.4F);

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const& cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BloodPool);
  EXPECT_FLOAT_EQ(cmd.position.x(), 4.0F);
  EXPECT_FLOAT_EQ(cmd.position.y(), 1.77F);
  EXPECT_FLOAT_EQ(cmd.position.z(), -2.0F);
}

TEST_F(SceneRendererEffects, CombatDustUsesStrongerDefaultsForMeleeLockUnits) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* unit = world.create_entity();
  unit->add_component<Engine::Core::TransformComponent>(4.0F, 1.25F, -2.0F);
  unit->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 6.0F);
  auto* attack = unit->add_component<Engine::Core::AttackComponent>();
  attack->in_melee_lock = true;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const& cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::CombatDust);
  EXPECT_FLOAT_EQ(cmd.position.x(), 4.0F);
  EXPECT_FLOAT_EQ(cmd.position.y(), 0.05F);
  EXPECT_FLOAT_EQ(cmd.position.z(), -2.0F);
  EXPECT_FLOAT_EQ(cmd.radius, 2.8F);
  EXPECT_FLOAT_EQ(cmd.intensity, 0.9F);
}

TEST_F(SceneRendererEffects, BurningUnitsEnqueueVisibleFlames) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* unit = world.create_entity();
  unit->add_component<Engine::Core::TransformComponent>(3.0F, 0.0F, -1.0F);
  unit->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 6.0F);
  auto* burning = unit->add_component<Engine::Core::BurningStatusComponent>();
  burning->duration = 2.0F;
  burning->remaining_duration = 1.5F;
  burning->ignition_elapsed = 0.5F;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 4U);
  float max_y = -9999.0F;
  int flame_count = 0;
  for (auto const& item : items) {
    ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(item));
    auto const& cmd = std::get<Render::GL::EffectBatchCmd>(item);
    EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BurningFlame);
    EXPECT_GT(cmd.radius, 0.0F);
    EXPECT_GT(cmd.intensity, 0.0F);
    max_y = std::max(max_y, cmd.position.y());
    ++flame_count;
  }
  EXPECT_EQ(flame_count, 4);
  EXPECT_GT(max_y, 0.65F);
}

TEST_F(SceneRendererEffects, RefreshedBurningUnitsStillEnqueueVisibleFlames) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* unit = world.create_entity();
  unit->add_component<Engine::Core::TransformComponent>(3.0F, 0.0F, -1.0F);
  unit->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 6.0F);
  auto* burning = unit->add_component<Engine::Core::BurningStatusComponent>();
  burning->duration = 2.0F;
  burning->remaining_duration = 2.0F;
  burning->ignition_elapsed = 0.3F;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 4U);
  int flame_count = 0;
  for (auto const& item : items) {
    ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(item));
    auto const& cmd = std::get<Render::GL::EffectBatchCmd>(item);
    EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BurningFlame);
    EXPECT_GT(cmd.intensity, 0.0F);
    ++flame_count;
  }
  EXPECT_EQ(flame_count, 4);
}

TEST_F(SceneRendererEffects, BurningUnitsReduceFlameCoverageAfterCasualties) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* unit = world.create_entity();
  unit->add_component<Engine::Core::TransformComponent>(3.0F, 0.0F, -1.0F);
  auto* unit_comp =
      unit->add_component<Engine::Core::UnitComponent>(25, 100, 1.0F, 6.0F);
  unit_comp->render_individuals_per_unit_override = 6;
  auto* burning = unit->add_component<Engine::Core::BurningStatusComponent>();
  burning->duration = 2.0F;
  burning->remaining_duration = 1.5F;
  burning->ignition_elapsed = 0.5F;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 2U);
  for (auto const& item : items) {
    ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(item));
    auto const& cmd = std::get<Render::GL::EffectBatchCmd>(item);
    EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BurningFlame);
    EXPECT_GT(cmd.intensity, 0.0F);
  }
}

TEST_F(SceneRendererEffects, BurningFlameAnchorsFollowUnitTransformAndYaw) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* unit = world.create_entity();
  auto* transform =
      unit->add_component<Engine::Core::TransformComponent>(10.0F, 0.0F, 20.0F);
  transform->rotation.y = 90.0F;
  unit->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 6.0F);
  auto* burning = unit->add_component<Engine::Core::BurningStatusComponent>();
  burning->duration = 2.0F;
  burning->remaining_duration = 1.5F;
  burning->ignition_elapsed = 0.5F;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 4U);

  auto const& front = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(front.kind, Render::GL::EffectBatchCmd::Kind::BurningFlame);
  EXPECT_GT(front.position.x(), transform->position.x + 0.2F);
  EXPECT_NEAR(front.position.z(), transform->position.z, 0.001F);

  int flame_count = 0;
  for (auto const& item : items) {
    ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(item));
    auto const& cmd = std::get<Render::GL::EffectBatchCmd>(item);
    EXPECT_GT(cmd.position.x(), transform->position.x - 1.0F);
    EXPECT_LT(cmd.position.x(), transform->position.x + 1.0F);
    EXPECT_GT(cmd.position.z(), transform->position.z - 1.0F);
    EXPECT_LT(cmd.position.z(), transform->position.z + 1.0F);
    EXPECT_GT(cmd.position.y(), transform->position.y + 0.25F);
    ++flame_count;
  }
  EXPECT_EQ(flame_count, 4);
}

TEST_F(SceneRendererEffects, FirePatchUsesGroundFlameClusterInsteadOfSingleChimney) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto* fire_patch_entity = world.create_entity();
  fire_patch_entity->add_component<Engine::Core::TransformComponent>(6.0F, 0.0F, -3.0F);
  auto* fire_patch =
      fire_patch_entity->add_component<Engine::Core::FirePatchComponent>();
  fire_patch->radius = 1.9F;
  fire_patch->duration = 4.0F;
  fire_patch->remaining_duration = 3.5F;

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const& items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 4U);

  float min_x = 9999.0F;
  float max_x = -9999.0F;
  float min_z = 9999.0F;
  float max_z = -9999.0F;
  for (auto const& item : items) {
    ASSERT_TRUE(std::holds_alternative<Render::GL::EffectBatchCmd>(item));
    auto const& cmd = std::get<Render::GL::EffectBatchCmd>(item);
    EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BurningFlame);
    EXPECT_LT(cmd.radius, 0.8F);
    EXPECT_GT(cmd.intensity, 0.0F);
    EXPECT_NEAR(cmd.position.y(), 0.10F, 0.001F);
    min_x = std::min(min_x, cmd.position.x());
    max_x = std::max(max_x, cmd.position.x());
    min_z = std::min(min_z, cmd.position.z());
    max_z = std::max(max_z, cmd.position.z());
  }

  EXPECT_LT(min_x, 6.0F - 0.2F);
  EXPECT_GT(max_x, 6.0F + 0.2F);
  EXPECT_LT(min_z, -3.0F - 0.2F);
  EXPECT_GT(max_z, -3.0F + 0.2F);
}

} // namespace

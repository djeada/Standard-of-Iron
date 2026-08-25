#include <gtest/gtest.h>
#include <mutex>

#include "app/commander/commander_control_controller.h"
#include "app/core/client_context.h"
#include "app/input/cursor_mode.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "scene/camera.h"

namespace {

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override { ++initialize_calls; }

  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_frame_mutex);
  }

  void set_cursor_mode(CursorMode mode) override { cursor_mode = mode; }

  int initialize_calls{0};
  CursorMode cursor_mode{CursorMode::Normal};

private:
  std::recursive_mutex m_frame_mutex;
};

class CommanderViewModelInputTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);

    m_context.world = &m_world;
    m_context.local_owner_id = 1;
    m_context.commander_camera = &m_commander_camera;
    m_context.active_camera = &m_commander_camera;

    m_camera_view_model =
        std::make_unique<App::ViewModels::CameraViewModel>(m_context, m_host);
    m_placement_view_model =
        std::make_unique<App::ViewModels::PlacementViewModel>(m_context, m_host);
    m_commander = std::make_unique<App::ViewModels::CommanderViewModel>(
        m_context, m_host, *m_camera_view_model, *m_placement_view_model);
  }

  void TearDown() override {
    m_commander.reset();
    m_placement_view_model.reset();
    m_camera_view_model.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto spawn_commander(float x, float z) -> Engine::Core::Entity* {
    auto* entity = m_world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::CommanderComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  auto spawn_enemy(float x, float z) -> Engine::Core::Entity* {
    auto* entity = m_world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 2;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  static auto attack_is_running(Engine::Core::Entity& commander) -> bool {
    auto const* action =
        commander.get_component<Engine::Core::RpgCommanderActionComponent>();
    return action != nullptr && action->action_running;
  }

  Engine::Core::World m_world;
  App::Core::ClientContext m_context;
  StubClientHost m_host;
  Render::GL::Camera m_commander_camera;
  std::unique_ptr<App::ViewModels::CameraViewModel> m_camera_view_model;
  std::unique_ptr<App::ViewModels::PlacementViewModel> m_placement_view_model;
  std::unique_ptr<App::ViewModels::CommanderViewModel> m_commander;
};

constexpr float k_tick = 1.0F / 60.0F;

TEST_F(CommanderViewModelInputTest, EnteringCommanderModeTakesControlOfTheCommander) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  ASSERT_TRUE(m_commander->available());
  m_commander->enter_mode();

  EXPECT_TRUE(m_commander->active());
  EXPECT_EQ(m_commander->controlled_commander_id(), commander->get_id());
  EXPECT_GT(m_host.initialize_calls, 0);
}

TEST_F(CommanderViewModelInputTest, APressAloneDoesNotReachTheWorld) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(spawn_enemy(0.0F, 1.2F), nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());
  ASSERT_FALSE(attack_is_running(*commander));

  m_commander->primary_action_down();

  EXPECT_EQ(m_commander->input_edges().primary_press_sequence, 1U);
  EXPECT_EQ(m_commander->input_edges().primary_consumed_sequence, 0U);
  EXPECT_FALSE(attack_is_running(*commander))
      << "the GUI layer records an input edge; simulation is the sole consumer";
}

TEST_F(CommanderViewModelInputTest,
       OnePhysicalAttackPressProducesOneConsumedAttackEdge) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(spawn_enemy(0.0F, 1.2F), nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->primary_action_down();
  m_commander->primary_action_up();
  m_commander->update_control_mode(k_tick);

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 1U);
  EXPECT_EQ(edges.primary_release_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence, 1U);
  EXPECT_EQ(edges.primary_dropped_sequence, 0U);

  for (int tick = 0; tick < 8; ++tick) {
    m_commander->update_control_mode(k_tick);
  }
  EXPECT_EQ(m_commander->input_edges().primary_consumed_sequence, 1U)
      << "a released press may not be consumed a second time";
}

TEST_F(CommanderViewModelInputTest,
       PressAndReleaseBetweenSimulationTicksIsConsumedOnce) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  for (int press = 0; press < 3; ++press) {
    m_commander->primary_action_down();
    m_commander->primary_action_up();
    m_commander->update_control_mode(k_tick);
    for (int settle = 0; settle < 40; ++settle) {
      m_commander->update_control_mode(k_tick);
    }
  }

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 3U);
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 3U);
  EXPECT_EQ(edges.primary_dropped_sequence, 0U)
      << "a press that a tick can still see must not be dropped";
}

TEST_F(CommanderViewModelInputTest, ReleasingGuardDoesNotWriteTheWorldOutsideATick) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->secondary_action_down();
  m_commander->update_control_mode(k_tick);
  auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  ASSERT_NE(guard, nullptr);
  ASSERT_TRUE(guard->active);

  m_commander->secondary_action_up();
  EXPECT_TRUE(guard->active)
      << "the release is an input edge; the tick lowers the guard";

  m_commander->update_control_mode(k_tick);
  EXPECT_FALSE(guard->active);
}

TEST_F(CommanderViewModelInputTest, FocusLossClearsEveryHeldCommanderAction) {
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_commander->key_down(Qt::Key_W);
  m_commander->key_down(Qt::Key_Shift);
  m_commander->primary_action_down();
  m_commander->secondary_action_down();
  m_commander->dodge();

  m_commander->sample_frame_intent();
  ASSERT_TRUE(m_commander->frame_intent().has_move());
  ASSERT_TRUE(m_commander->frame_intent().attack_held);
  ASSERT_TRUE(m_commander->frame_intent().guard);

  m_commander->release_input();

  m_commander->sample_frame_intent();
  auto const& intent = m_commander->frame_intent();
  EXPECT_FALSE(intent.has_move());
  EXPECT_FALSE(intent.run);
  EXPECT_FALSE(intent.attack_held);
  EXPECT_FALSE(intent.guard);
  EXPECT_FALSE(intent.dodge_pressed);
  EXPECT_FALSE(intent.jump_pressed);

  auto const& edges = m_commander->input_edges();
  EXPECT_EQ(edges.primary_release_sequence, 1U);
  EXPECT_EQ(edges.guard_release_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 1U);
  EXPECT_EQ(edges.dodge_consumed_sequence + edges.dodge_refused_sequence, 1U);

  float const start_z = transform->position.z;
  for (int tick = 0; tick < 30; ++tick) {
    m_commander->update_control_mode(k_tick);
  }
  EXPECT_NEAR(transform->position.z, start_z, 1.0e-3F)
      << "input released on focus loss must not leave the commander walking";
}

TEST_F(CommanderViewModelInputTest, LandingAHitNeverSlowsTheSimulationClock) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  auto* enemy = spawn_enemy(0.0F, 1.4F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);

  EXPECT_FLOAT_EQ(m_commander->time_effect_scale(k_tick, false), 1.0F);

  commander_data->just_struck_enemy = true;
  m_commander->update_control_mode(k_tick);

  for (int tick = 0; tick < 30; ++tick) {
    EXPECT_FLOAT_EQ(m_commander->time_effect_scale(k_tick, false), 1.0F)
        << "a landed hit must not scale the simulation clock at tick " << tick;
  }
  EXPECT_TRUE(commander_data->just_struck_enemy)
      << "the hit flag belongs to the combo chain; the view model must not eat it";
}

TEST_F(CommanderViewModelInputTest, ADeadCommanderLeavesTheModeAndTheCursorBehind) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;

  m_commander->update_control_mode(k_tick);

  EXPECT_FALSE(m_commander->active())
      << "a dead commander must not keep direct control alive";
  EXPECT_EQ(m_host.cursor_mode, CursorMode::Normal)
      << "death must hand the cursor back";

  auto const* commander_data =
      commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  EXPECT_FALSE(commander_data->fpv_controlled);
}

TEST_F(CommanderViewModelInputTest, AVanishedCommanderCannotLeaveTheModeActive) {
  constexpr float k_tick = 1.0F / 60.0F;
  auto* commander = spawn_commander(0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto const commander_id = commander->get_id();
  m_commander->enter_mode();
  ASSERT_TRUE(m_commander->active());

  m_world.destroy_entity(commander_id);
  m_commander->update_control_mode(k_tick);

  EXPECT_FALSE(m_commander->active())
      << "pending removal must not leave a live camera and input pointer";
  EXPECT_EQ(m_host.cursor_mode, CursorMode::Normal);
}

} // namespace

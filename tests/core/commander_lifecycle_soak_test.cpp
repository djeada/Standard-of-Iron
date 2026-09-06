#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_mode_coordinator.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/combat_state_processor.h"
#include "game/systems/nav_grid.h"
#include "scene/camera.h"

namespace {

using App::Core::CommanderModeContext;
using App::Core::CommanderModeCoordinator;
using App::Core::CommanderModeState;

class CommanderLifecycleSoakTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto make_commander(Engine::Core::World& world,
                             float x,
                             float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (unit == nullptr || commander == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  static auto
  make_enemy(Engine::Core::World& world, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 100000;
    unit->max_health = 100000;
    unit->owner_id = 2;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->render_individuals_per_unit_override = 1;
    return entity;
  }
};

TEST_F(CommanderLifecycleSoakTest, OneHundredEnterExitCyclesRestoreEveryFlag) {
  Engine::Core::World world;
  auto* commander = make_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto const commander_id = commander->get_id();
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  QVector3D const start(
      transform->position.x, transform->position.y, transform->position.z);

  CommanderControlController controller;
  Render::GL::Camera camera;
  CommanderModeCoordinator coordinator;

  CommanderModeContext context;
  context.world = &world;
  context.commander = commander;
  context.commander_camera = &camera;
  context.commander_control = &controller;
  context.controlled_commander_id = commander_id;
  context.local_owner_id = 1;

  for (int cycle = 0; cycle < 100; ++cycle) {
    ASSERT_TRUE(coordinator.begin_enter()) << "cycle " << cycle;
    auto const entered = coordinator.enter_commander_control_mode(context);
    coordinator.complete_transition();
    ASSERT_TRUE(entered.entered) << "cycle " << cycle;
    EXPECT_EQ(entered.controlled_commander_id, commander_id) << "cycle " << cycle;
    EXPECT_TRUE(coordinator.is_active()) << "cycle " << cycle;

    auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
    ASSERT_NE(commander_data, nullptr) << "cycle " << cycle;
    EXPECT_TRUE(commander_data->fpv_controlled) << "cycle " << cycle;

    controller.key_down(Qt::Key_W);
    controller.primary_action_down();
    controller.secondary_action_down();
    ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, 1.0F / 60.0F))
        << "cycle " << cycle;

    ASSERT_TRUE(coordinator.begin_exit()) << "cycle " << cycle;
    auto const exited = coordinator.exit_commander_control_mode(context);
    coordinator.complete_transition();
    EXPECT_EQ(exited.controlled_commander_id, 0U) << "cycle " << cycle;
    EXPECT_EQ(coordinator.state(), CommanderModeState::Inactive) << "cycle " << cycle;

    controller.release_all_input();
    controller.reset();

    commander_data = commander->get_component<Engine::Core::CommanderComponent>();
    ASSERT_NE(commander_data, nullptr) << "cycle " << cycle;
    EXPECT_FALSE(commander_data->fpv_controlled) << "cycle " << cycle;
    EXPECT_FALSE(commander_data->jump_active) << "cycle " << cycle;
    EXPECT_EQ(commander_data->combo_step, 0) << "cycle " << cycle;
    EXPECT_FALSE(commander_data->power_strike_active) << "cycle " << cycle;

    auto const& input = controller.input();
    EXPECT_FALSE(input.forward) << "cycle " << cycle;
    EXPECT_FALSE(input.primary_action) << "cycle " << cycle;
    EXPECT_FALSE(input.secondary_action) << "cycle " << cycle;

    if (auto const* guard =
            commander->get_component<Engine::Core::CommanderGuardComponent>()) {
      EXPECT_FALSE(guard->active) << "cycle " << cycle;
    }
  }

  EXPECT_LT(
      (QVector3D(transform->position.x, transform->position.y, transform->position.z) -
       start)
          .length(),
      2.0F)
      << "a hundred enter/exit cycles must not walk the commander across the map";
}

TEST_F(CommanderLifecycleSoakTest, ATenMinuteDuelKeepsEveryInputEdgeAccountedFor) {

  constexpr float k_dt = 1.0F / 60.0F;
  constexpr int k_ticks = 60 * 60 * 10;
  constexpr int k_press_interval = 90;

  Engine::Core::World world;
  auto* commander = make_commander(world, 0.0F, 0.0F);
  auto* enemy = make_enemy(world, 0.0F, 1.5F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  auto const commander_id = commander->get_id();

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  auto* rpg = commander->add_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  rpg->active = true;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.set_presentation_trace_enabled(true);

  int presses = 0;
  for (int tick = 0; tick < k_ticks; ++tick) {
    if (tick % k_press_interval == 0) {
      controller.primary_action_down();
      controller.primary_action_up();
      ++presses;
    }
    if (tick % 240 == 0) {
      controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));
    }
    ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, k_dt))
        << "tick " << tick;
    Game::Systems::Combat::process_combat_state(&world, k_dt);
  }

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.primary_press_sequence, static_cast<std::uint64_t>(presses));
  EXPECT_EQ(edges.primary_press_sequence,
            edges.primary_consumed_sequence + edges.primary_dropped_sequence)
      << "every attack press must be consumed or explicitly dropped";
  EXPECT_EQ(edges.dodge_request_sequence,
            edges.dodge_consumed_sequence + edges.dodge_refused_sequence)
      << "every dodge request must be consumed or explicitly refused";

  auto const* queue =
      commander->get_component<Engine::Core::CombatIntentQueueComponent>();
  ASSERT_NE(queue, nullptr);
  EXPECT_GT(queue->accepted_intents, 0U);
  EXPECT_EQ(queue->overflow_intents, 0U)
      << "the buffer must never silently discard a press";
  EXPECT_LE(queue->count, Engine::Core::CombatIntentQueueComponent::k_capacity);

  auto const* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_TRUE(std::isfinite(transform->position.x));
  EXPECT_TRUE(std::isfinite(transform->position.z));

  auto const* stamina = commander->get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr) {
    EXPECT_GE(stamina->stamina, 0.0F);
    EXPECT_LE(stamina->stamina, stamina->max_stamina);
  }
}

TEST_F(CommanderLifecycleSoakTest, TenMinutesOfOpenTraversalKeepsTheBodyFinite) {

  constexpr float k_dt = 1.0F / 60.0F;
  constexpr int k_ticks = 60 * 60 * 10;

  Engine::Core::World world;
  auto* commander = make_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto const commander_id = commander->get_id();
  commander->get_component<Engine::Core::CommanderComponent>()->fpv_controlled = true;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  float worst_step = 0.0F;
  QVector3D previous(
      transform->position.x, transform->position.y, transform->position.z);
  for (int tick = 0; tick < k_ticks; ++tick) {

    if (tick % 180 == 0) {
      controller.release_all_input();
      switch ((tick / 180) % 4) {
      case 0:
        controller.key_down(Qt::Key_W);
        break;
      case 1:
        controller.key_down(Qt::Key_W);
        controller.key_down(Qt::Key_D);
        controller.key_down(Qt::Key_Shift);
        break;
      case 2:
        controller.key_down(Qt::Key_S);
        break;
      default:
        break;
      }
      controller.set_view_yaw(static_cast<float>((tick / 180) % 8) * 45.0F);
    }
    ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, k_dt))
        << "tick " << tick;
    Game::Systems::Combat::process_combat_state(&world, k_dt);

    QVector3D const now(
        transform->position.x, transform->position.y, transform->position.z);
    worst_step = std::max(worst_step, (now - previous).length());
    previous = now;
  }

  EXPECT_TRUE(std::isfinite(transform->position.x));
  EXPECT_TRUE(std::isfinite(transform->position.z));
  EXPECT_LT(worst_step, 1.0F) << "ten minutes of traversal must contain no teleport";
}

TEST_F(CommanderLifecycleSoakTest, TenMinutesInsideAFriendlyCrowdNeverWedgesTheBody) {

  constexpr float k_dt = 1.0F / 60.0F;
  constexpr int k_ticks = 60 * 60 * 10;

  Engine::Core::World world;
  auto* commander = make_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);
  auto const commander_id = commander->get_id();
  commander->get_component<Engine::Core::CommanderComponent>()->fpv_controlled = true;

  for (int index = 0; index < 12; ++index) {
    float const angle = static_cast<float>(index) * 30.0F * 3.14159265F / 180.0F;
    auto* friendly =
        make_commander(world, std::sin(angle) * 1.8F, std::cos(angle) * 1.8F);
    ASSERT_NE(friendly, nullptr);
    friendly->remove_component<Engine::Core::CommanderComponent>();
  }

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.key_down(Qt::Key_W);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  float worst_step = 0.0F;
  QVector3D previous(
      transform->position.x, transform->position.y, transform->position.z);

  for (int tick = 0; tick < k_ticks; ++tick) {
    ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, k_dt))
        << "tick " << tick;
    QVector3D const now(
        transform->position.x, transform->position.y, transform->position.z);
    worst_step = std::max(worst_step, (now - previous).length());
    previous = now;
  }

  EXPECT_LT(worst_step, 1.0F) << "a crowd may push the commander, never teleport him";
  EXPECT_GT((QVector3D(transform->position.x, 0.0F, transform->position.z)).length(),
            1.0F)
      << "held forward input for ten minutes must not leave him wedged where he "
         "started";
}

TEST_F(CommanderLifecycleSoakTest, PausingDuringEveryActionPhaseLeavesNoStuckState) {

  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world, 0.0F, 0.0F);
  auto* enemy = make_enemy(world, 0.0F, 1.5F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  auto const commander_id = commander->get_id();

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->fpv_controlled = true;
  auto* attack = commander->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  for (int pause_after = 1; pause_after <= 40; ++pause_after) {
    controller.reset();
    controller.primary_action_down();
    controller.primary_action_up();
    for (int tick = 0; tick < pause_after; ++tick) {
      ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, k_dt))
          << "pause_after " << pause_after;
      Game::Systems::Combat::process_combat_state(&world, k_dt);
    }

    controller.release_all_input();
    for (int tick = 0; tick < 12; ++tick) {
      ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, 0.0F))
          << "pause_after " << pause_after;
      Game::Systems::Combat::process_combat_state(&world, 0.0F);
    }

    for (int tick = 0; tick < 120; ++tick) {
      ASSERT_TRUE(controller.update_simulation(world, commander_id, 1, k_dt))
          << "pause_after " << pause_after;
      Game::Systems::Combat::process_combat_state(&world, k_dt);
    }

    auto const* action =
        commander->get_component<Engine::Core::RpgCommanderActionComponent>();
    if (action != nullptr) {
      EXPECT_FALSE(action->action_running)
          << "an action was still running two seconds after the input stopped, "
             "paused at tick "
          << pause_after;
    }
    EXPECT_FALSE(controller.is_dodge_rolling()) << "pause_after " << pause_after;
  }
}

} // namespace

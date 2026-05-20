#include <gtest/gtest.h>

#include "app/controllers/command_controller.h"
#include "app/core/input_command_handler.h"
#include "app/models/cursor_manager.h"
#include "app/models/cursor_mode.h"
#include "app/models/hover_tracker.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "render/gl/camera.h"

namespace {

class InputCommandHandlerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);

    world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
    selection_system = world.get_system<Game::Systems::SelectionSystem>();
    ASSERT_NE(selection_system, nullptr);

    selection_controller = std::make_unique<Game::Systems::SelectionController>(
        &world, selection_system, &picking_service);
    command_controller = std::make_unique<App::Controllers::CommandController>(
        &world, selection_system, &picking_service);
    hover_tracker = std::make_unique<HoverTracker>(&picking_service);
    input_handler = std::make_unique<InputCommandHandler>(&world,
                                                          selection_controller.get(),
                                                          command_controller.get(),
                                                          &cursor_manager,
                                                          hover_tracker.get(),
                                                          &picking_service,
                                                          &camera);

    camera.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
    camera.look_at(QVector3D(0.0F, 10.0F, 10.0F),
                   QVector3D(0.0F, 0.0F, 0.0F),
                   QVector3D(0.0F, 1.0F, 0.0F));
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto create_unit(float x,
                   float z,
                   int owner_id,
                   Game::Units::SpawnType spawn_type) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }

    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    auto* movement = entity->add_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr) {
      return nullptr;
    }

    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    unit->speed = 3.0F;
    return entity;
  }

  auto world_to_screen(const QVector3D& world_pos) const -> QPointF {
    QPointF screen_pos;
    EXPECT_TRUE(
        camera.world_to_screen(world_pos, viewport.width, viewport.height, screen_pos));
    return screen_pos;
  }

  Engine::Core::World world;
  Game::Systems::SelectionSystem* selection_system = nullptr;
  Game::Systems::PickingService picking_service;
  std::unique_ptr<Game::Systems::SelectionController> selection_controller;
  std::unique_ptr<App::Controllers::CommandController> command_controller;
  CursorManager cursor_manager;
  std::unique_ptr<HoverTracker> hover_tracker;
  Render::GL::Camera camera;
  std::unique_ptr<InputCommandHandler> input_handler;
  ViewportState viewport{800, 600};
};

TEST_F(InputCommandHandlerTest, RightPressConsumesCursorModeCancellation) {
  auto* unit = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  cursor_manager.set_mode(CursorMode::Attack);

  EXPECT_TRUE(input_handler->on_right_press(400.0, 300.0, 1, viewport));
  EXPECT_EQ(cursor_manager.mode(), CursorMode::Normal);
  EXPECT_FALSE(input_handler->is_placing_formation());
}

TEST_F(InputCommandHandlerTest, RightPressConsumesCollectCursorModeCancellation) {
  auto* unit = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  cursor_manager.set_mode(CursorMode::Collect);

  EXPECT_TRUE(input_handler->on_right_press(400.0, 300.0, 1, viewport));
  EXPECT_EQ(cursor_manager.mode(), CursorMode::Normal);
}

TEST_F(InputCommandHandlerTest, RightPressConsumesBarracksRallyCursorModeCancellation) {
  auto* unit = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Barracks);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  cursor_manager.set_mode(CursorMode::PlaceBarracksRally);

  EXPECT_TRUE(input_handler->on_right_press(400.0, 300.0, 1, viewport));
  EXPECT_EQ(cursor_manager.mode(), CursorMode::Normal);
}

TEST_F(InputCommandHandlerTest, RightPressConsumesEnemyAttackCommand) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(unit->get_id());

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(enemy_screen.x(), enemy_screen.y(), 1, viewport));

  auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, enemy->get_id());
  EXPECT_TRUE(attack_target->should_chase);
}

TEST_F(InputCommandHandlerTest, RightPressAppliesAttackOnlyToEligibleUnits) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* builder = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(builder, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(builder->get_id());

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(enemy_screen.x(), enemy_screen.y(), 1, viewport));

  auto* archer_target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(archer_target, nullptr);
  EXPECT_EQ(archer_target->target_id, enemy->get_id());
  EXPECT_EQ(builder->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(InputCommandHandlerTest, RightPressStartsFormationPlacementForGroundMove) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  EXPECT_TRUE(input_handler->is_placing_formation());

  input_handler->on_formation_confirm();

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->has_target || movement->path_pending);
}

TEST_F(InputCommandHandlerTest, FormationConfirmClearsPatrolBeforeApplyingMove) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  auto* patrol = unit->add_component<Engine::Core::PatrolComponent>();
  ASSERT_NE(patrol, nullptr);
  patrol->patrolling = true;
  patrol->waypoints.emplace_back(-3.0F, 0.0F);
  patrol->waypoints.emplace_back(-1.0F, 0.0F);
  selection_system->select_unit(unit->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  input_handler->on_formation_confirm();

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->has_target || movement->path_pending);
  EXPECT_FALSE(patrol->patrolling);
  EXPECT_TRUE(patrol->waypoints.empty());
}

TEST_F(InputCommandHandlerTest, RightDoubleClickDoesNotBypassFormationPlacement) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  ASSERT_TRUE(input_handler->is_placing_formation());

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->has_target);
  EXPECT_FALSE(movement->path_pending);

  input_handler->on_right_double_click(
      ground_screen.x(), ground_screen.y(), 1, viewport);

  EXPECT_TRUE(input_handler->is_placing_formation());
  EXPECT_FALSE(movement->has_target);
  EXPECT_FALSE(movement->path_pending);

  input_handler->on_formation_confirm();

  EXPECT_TRUE(movement->has_target || movement->path_pending);
}

TEST_F(InputCommandHandlerTest, RightDoubleClickEnablesRunModeAndDispatchesMove) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Knight);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  input_handler->on_right_double_click(
      ground_screen.x(), ground_screen.y(), 1, viewport);

  auto* stamina = unit->get_component<Engine::Core::StaminaComponent>();
  ASSERT_NE(stamina, nullptr);
  EXPECT_TRUE(stamina->run_requested);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->has_target || movement->path_pending);
  EXPECT_FALSE(input_handler->is_placing_formation());
}

} // namespace

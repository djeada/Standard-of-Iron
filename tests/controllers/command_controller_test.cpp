#include <gtest/gtest.h>

#include "app/controllers/command_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "render/gl/camera.h"

namespace {

class CommandControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);

    world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
    selection_system = world.get_system<Game::Systems::SelectionSystem>();
    ASSERT_NE(selection_system, nullptr);

    command_controller = std::make_unique<App::Controllers::CommandController>(
        &world, selection_system, &picking_service);

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
    if ((transform == nullptr) || (unit == nullptr) || (movement == nullptr)) {
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
        camera.world_to_screen(world_pos, viewport_width, viewport_height, screen_pos));
    return screen_pos;
  }

  Engine::Core::World world;
  Game::Systems::SelectionSystem* selection_system = nullptr;
  Game::Systems::PickingService picking_service;
  std::unique_ptr<App::Controllers::CommandController> command_controller;
  Render::GL::Camera camera;
  int viewport_width = 800;
  int viewport_height = 600;
};

TEST_F(CommandControllerTest, AttackClickAppliesOnlyToEligibleUnits) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* builder = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(builder, nullptr);
  ASSERT_NE(enemy, nullptr);

  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(builder->get_id());

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.input_consumed);
  auto* archer_target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(archer_target, nullptr);
  EXPECT_EQ(archer_target->target_id, enemy->get_id());
  EXPECT_EQ(builder->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(CommandControllerTest, GuardClickAppliesOnlyToEligibleUnits) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* tower = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::DefenseTower);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(tower, nullptr);

  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(tower->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));
  auto const result = command_controller->on_guard_click(
      ground_screen.x(), ground_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.input_consumed);
  auto* archer_guard = archer->get_component<Engine::Core::GuardModeComponent>();
  ASSERT_NE(archer_guard, nullptr);
  EXPECT_TRUE(archer_guard->active);
  EXPECT_EQ(tower->get_component<Engine::Core::GuardModeComponent>(), nullptr);
}

TEST_F(CommandControllerTest, PatrolClickAppliesOnlyToEligibleUnits) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* tower = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::DefenseTower);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(tower, nullptr);

  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(tower->get_id());

  QPointF const first_screen = world_to_screen(QVector3D(1.0F, 0.0F, 1.0F));
  QPointF const second_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  auto const first_result = command_controller->on_patrol_click(
      first_screen.x(), first_screen.y(), viewport_width, viewport_height, &camera);
  auto const second_result = command_controller->on_patrol_click(
      second_screen.x(), second_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(first_result.input_consumed);
  EXPECT_TRUE(second_result.input_consumed);

  auto* archer_patrol = archer->get_component<Engine::Core::PatrolComponent>();
  ASSERT_NE(archer_patrol, nullptr);
  EXPECT_TRUE(archer_patrol->patrolling);
  EXPECT_EQ(archer_patrol->waypoints.size(), 2U);
  EXPECT_EQ(tower->get_component<Engine::Core::PatrolComponent>(), nullptr);
}

} // namespace

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "app/orders/command_controller.h"
#include "game/core/component_economy.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/selection_system.h"
#include "game/wildlife/wildlife_species.h"
#include "scene/camera.h"

namespace {

class CommandControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);

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

  auto create_animal(float x,
                     float z,
                     Game::Wildlife::Species species,
                     bool hostile) -> Engine::Core::Entity* {
    auto* entity = create_unit(x,
                               z,
                               Game::Core::NEUTRAL_OWNER_ID,
                               species == Game::Wildlife::Species::Wolf
                                   ? Game::Units::SpawnType::Wolf
                                   : Game::Units::SpawnType::Sheep);
    if (entity == nullptr) {
      return nullptr;
    }
    auto* wildlife = entity->add_component<Engine::Core::WildlifeComponent>();
    wildlife->species = species;
    wildlife->hostile_timer = hostile ? 4.0F : 0.0F;
    return entity;
  }

  auto world_to_screen(const QVector3D& world_pos) const -> QPointF {
    QPointF screen_pos;
    EXPECT_TRUE(
        camera.world_to_screen(world_pos, viewport_width, viewport_height, screen_pos));
    return screen_pos;
  }

  auto capture_feedback() -> std::vector<App::Core::OrderOutcome>& {
    feedback.clear();
    QObject::connect(command_controller.get(),
                     &App::Controllers::CommandController::order_feedback,
                     command_controller.get(),
                     [this](const App::Core::OrderOutcome& outcome) {
                       feedback.push_back(outcome);
                     });
    return feedback;
  }

  Engine::Core::World world;
  Game::Systems::SelectionSystem* selection_system = nullptr;
  Game::Systems::PickingService picking_service;
  std::unique_ptr<App::Controllers::CommandController> command_controller;
  Render::GL::Camera camera;
  int viewport_width = 800;
  int viewport_height = 600;
  std::vector<App::Core::OrderOutcome> feedback;
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

TEST_F(CommandControllerTest, AutoGatherIsGivenToBuildersAndToggledAsAGroup) {
  auto* first = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* second = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* archer = create_unit(-1.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(archer, nullptr);
  first->add_component<Engine::Core::BuilderProductionComponent>();
  second->add_component<Engine::Core::BuilderProductionComponent>();

  selection_system->select_unit(first->get_id());
  selection_system->select_unit(second->get_id());
  selection_system->select_unit(archer->get_id());

  auto const turned_on = command_controller->on_auto_gather_command();
  EXPECT_TRUE(turned_on.input_consumed);
  EXPECT_TRUE(
      first->get_component<Engine::Core::BuilderProductionComponent>()->auto_gather);
  EXPECT_TRUE(
      second->get_component<Engine::Core::BuilderProductionComponent>()->auto_gather);
  EXPECT_EQ(archer->get_component<Engine::Core::BuilderProductionComponent>(), nullptr);

  command_controller->on_auto_gather_command();
  EXPECT_FALSE(
      first->get_component<Engine::Core::BuilderProductionComponent>()->auto_gather);
  EXPECT_FALSE(
      second->get_component<Engine::Core::BuilderProductionComponent>()->auto_gather);
}

TEST_F(CommandControllerTest, AutoGatherCarriesThePreferredResourceThrough) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  ASSERT_NE(builder, nullptr);
  builder->add_component<Engine::Core::BuilderProductionComponent>();
  selection_system->select_unit(builder->get_id());

  command_controller->on_auto_gather_command(QStringLiteral("collect_iron_ore"));

  const auto* production =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_TRUE(production->auto_gather);
  EXPECT_EQ(production->auto_gather_priority, std::string("collect_iron_ore"));
}

TEST_F(CommandControllerTest, TheGatherPriorityCanBeChangedWithoutStoppingTheOrder) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  ASSERT_NE(builder, nullptr);
  builder->add_component<Engine::Core::BuilderProductionComponent>();
  selection_system->select_unit(builder->get_id());

  ASSERT_TRUE(command_controller->set_auto_gather(true).input_consumed);
  const auto* production =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_TRUE(production->auto_gather);
  EXPECT_TRUE(production->auto_gather_priority.empty());

  EXPECT_TRUE(command_controller->set_auto_gather(true, QStringLiteral("collect_stone"))
                  .input_consumed);
  EXPECT_TRUE(production->auto_gather)
      << "changing which resource to favour must not call the workers off";
  EXPECT_EQ(production->auto_gather_priority, std::string("collect_stone"));

  EXPECT_TRUE(command_controller->set_auto_gather(false).input_consumed);
  EXPECT_FALSE(production->auto_gather);
}

TEST_F(CommandControllerTest, SettingAutoGatherNeedsBuildersLikeTheToggleDoes) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());

  EXPECT_FALSE(command_controller->set_auto_gather(true).input_consumed);
}

TEST_F(CommandControllerTest, AutoGatherIgnoresASelectionWithoutBuilders) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());

  auto const result = command_controller->on_auto_gather_command();

  EXPECT_FALSE(result.input_consumed);
}

TEST_F(CommandControllerTest, LeftClickAttackModeReportsTheClickedTarget) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.input_consumed);
  EXPECT_TRUE(result.reset_cursor_to_normal);
  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(result.order.target, enemy->get_id());
  EXPECT_EQ(result.order.unit_count, 1U);
  EXPECT_TRUE(result.order.reason.isEmpty());

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().accepted());
  EXPECT_EQ(seen.front().target, enemy->get_id());
}

TEST_F(CommandControllerTest, AttackClickOnEmptyGroundIsRejectedWithAReason) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));
  auto const result = command_controller->on_attack_click(
      ground_screen.x(), ground_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_FALSE(result.input_consumed);
  EXPECT_TRUE(result.reset_cursor_to_normal);
  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(result.order.target, 0U);
  EXPECT_TRUE(result.order.has_destination);
  EXPECT_FALSE(result.order.reason.isEmpty());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().rejected());
}

TEST_F(CommandControllerTest, AttackClickOnAFriendlyUnitIsRejectedWithAReason) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* friendly = create_unit(0.0F, 0.0F, 1, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(friendly, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QPointF const friendly_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(friendly_screen.x(),
                                                          friendly_screen.y(),
                                                          viewport_width,
                                                          viewport_height,
                                                          &camera);

  EXPECT_FALSE(result.input_consumed);
  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.rejection, Game::Command::Rejection::FriendlyTarget);
  EXPECT_EQ(result.order.target, friendly->get_id());
  EXPECT_FALSE(result.order.reason.isEmpty());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_EQ(seen.front().rejection, Game::Command::Rejection::FriendlyTarget);
}

TEST_F(CommandControllerTest, AttackClickWithNonCombatSelectionExplainsWhy) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(builder, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(builder->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.target, enemy->get_id());
  EXPECT_EQ(result.order.reason,
            App::Core::no_eligible_units_reason(App::Core::OrderKind::Attack).text);
  EXPECT_EQ(builder->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, AttackClickWithEmptySelectionIsRejectedNotSilent) {
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(enemy, nullptr);
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_FALSE(result.input_consumed);
  EXPECT_TRUE(result.reset_cursor_to_normal);
  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.reason, App::Core::no_selection_reason().text);
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, RightClickOnEnemyAttacksTheClickedTarget) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_move_or_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera, 1);

  EXPECT_TRUE(result.input_consumed);
  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(result.order.target, enemy->get_id());
  auto* target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->target_id, enemy->get_id());
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_EQ(seen.front().kind, App::Core::OrderKind::Attack);
}

TEST_F(CommandControllerTest, RightClickOnAHostileWolfAttacksIt) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* wolf = create_animal(0.0F, 0.0F, Game::Wildlife::Species::Wolf, true);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(wolf, nullptr);
  selection_system->select_unit(archer->get_id());

  QPointF const wolf_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_move_or_attack_click(
      wolf_screen.x(), wolf_screen.y(), viewport_width, viewport_height, &camera, 1);

  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(result.order.target, wolf->get_id());
  auto* target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->target_id, wolf->get_id());
}

TEST_F(CommandControllerTest, RightClickOnAGrazingSheepIsAMoveOrder) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* sheep = create_animal(0.0F, 0.0F, Game::Wildlife::Species::Sheep, false);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(sheep, nullptr);
  selection_system->select_unit(archer->get_id());

  QPointF const sheep_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_move_or_attack_click(
      sheep_screen.x(), sheep_screen.y(), viewport_width, viewport_height, &camera, 1);

  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Move);
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(CommandControllerTest, AttackClickOnAGrazingSheepStillHunts) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* sheep = create_animal(0.0F, 0.0F, Game::Wildlife::Species::Sheep, false);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(sheep, nullptr);
  selection_system->select_unit(archer->get_id());

  QPointF const sheep_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_attack_click(
      sheep_screen.x(), sheep_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.order.accepted()) << result.order.reason.toStdString();
  auto* target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->target_id, sheep->get_id());
}

TEST_F(CommandControllerTest, RightClickOnOneOfYourOwnUnitsIsAMoveOrder) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* comrade = create_unit(0.0F, 0.0F, 1, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(comrade, nullptr);
  selection_system->select_unit(archer->get_id());

  QPointF const comrade_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_move_or_attack_click(comrade_screen.x(),
                                                                  comrade_screen.y(),
                                                                  viewport_width,
                                                                  viewport_height,
                                                                  &camera,
                                                                  1);

  EXPECT_TRUE(result.order.accepted()) << result.order.reason.toStdString();
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Move);
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(CommandControllerTest, ChainingMoveAndAttackLeavesExactlyOneLiveOrder) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* first = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  auto* second = create_unit(3.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  selection_system->select_unit(archer->get_id());

  const auto right_click = [&](const QVector3D& at) {
    const QPointF screen = world_to_screen(at);
    return command_controller->on_move_or_attack_click(
        screen.x(), screen.y(), viewport_width, viewport_height, &camera, 1);
  };

  auto move = right_click(QVector3D(-6.0F, 0.0F, 4.0F));
  ASSERT_TRUE(move.order.accepted()) << move.order.reason.toStdString();
  EXPECT_EQ(move.order.kind, App::Core::OrderKind::Move);
  auto* movement = archer->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);

  auto attack = right_click(QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_TRUE(attack.order.accepted()) << attack.order.reason.toStdString();
  EXPECT_EQ(attack.order.kind, App::Core::OrderKind::Attack);
  auto* target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->target_id, first->get_id());
  EXPECT_TRUE(target->is_player_command);

  auto retarget = right_click(QVector3D(3.0F, 0.0F, 0.0F));
  ASSERT_TRUE(retarget.order.accepted()) << retarget.order.reason.toStdString();
  target = archer->get_component<Engine::Core::AttackTargetComponent>();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->target_id, second->get_id());

  auto move_again = right_click(QVector3D(-6.0F, 0.0F, 4.0F));
  ASSERT_TRUE(move_again.order.accepted()) << move_again.order.reason.toStdString();
  EXPECT_EQ(move_again.order.kind, App::Core::OrderKind::Move);
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);

  auto const stopped = command_controller->on_stop_command();
  EXPECT_TRUE(stopped.order.accepted());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(CommandControllerTest, RightClickOnGroundReportsTheDestination) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QVector3D const destination(4.0F, 0.0F, 2.0F);
  QPointF const ground_screen = world_to_screen(destination);
  auto const result = command_controller->on_move_or_attack_click(ground_screen.x(),
                                                                  ground_screen.y(),
                                                                  viewport_width,
                                                                  viewport_height,
                                                                  &camera,
                                                                  1);

  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Move);
  EXPECT_EQ(result.order.target, 0U);
  ASSERT_TRUE(result.order.has_destination);
  EXPECT_NEAR(result.order.destination.x(), destination.x(), 0.25F);
  EXPECT_NEAR(result.order.destination.z(), destination.z(), 0.25F);
  auto* movement = archer->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, RightClickOnEnemyWithNonCombatSelectionDoesNotMove) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(builder, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(builder->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  auto const result = command_controller->on_move_or_attack_click(
      enemy_screen.x(), enemy_screen.y(), viewport_width, viewport_height, &camera, 1);

  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(result.order.target, enemy->get_id());
  EXPECT_FALSE(result.order.reason.isEmpty());
  auto* movement = builder->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, MinimapMoveReportsTheDestination) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QVector3D const destination(5.0F, 0.0F, 5.0F);
  auto const result = command_controller->on_minimap_move(destination, 1);

  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Move);
  ASSERT_TRUE(result.order.has_destination);
  EXPECT_EQ(result.order.destination, destination);
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, HoldWithoutEligibleUnitsIsRejectedWithAReason) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  ASSERT_NE(builder, nullptr);
  selection_system->select_unit(builder->get_id());
  auto& seen = capture_feedback();

  auto const result = command_controller->on_hold_command();

  EXPECT_FALSE(result.input_consumed);
  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Hold);
  EXPECT_FALSE(result.order.reason.isEmpty());
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, GuardClickReportsTheAnchor) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());
  auto& seen = capture_feedback();

  QVector3D const anchor(4.0F, 0.0F, 2.0F);
  QPointF const ground_screen = world_to_screen(anchor);
  auto const result = command_controller->on_guard_click(
      ground_screen.x(), ground_screen.y(), viewport_width, viewport_height, &camera);

  EXPECT_TRUE(result.input_consumed);
  EXPECT_TRUE(result.order.accepted());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Guard);
  ASSERT_TRUE(result.order.has_destination);
  EXPECT_NEAR(result.order.destination.x(), anchor.x(), 0.25F);
  EXPECT_NEAR(result.order.destination.z(), anchor.z(), 0.25F);
  ASSERT_EQ(seen.size(), 1U);
}

TEST_F(CommandControllerTest, SendingABuilderAtASheepStartsTheSlaughter) {
  auto* builder = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  ASSERT_NE(builder, nullptr);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  auto* sheep = create_unit(3.0F, 0.0F, 0, Game::Units::SpawnType::Sheep);
  ASSERT_NE(sheep, nullptr);
  sheep->add_component<Engine::Core::WildlifeComponent>();
  sheep->get_component<Engine::Core::UnitComponent>()->health = 40;
  sheep->get_component<Engine::Core::UnitComponent>()->max_health = 40;

  selection_system->select_unit(builder->get_id());

  auto const result = command_controller->start_food_harvest(
      sheep->get_id(),
      QString::fromLatin1(Game::Systems::k_builder_product_slaughter_sheep),
      1);

  EXPECT_TRUE(result.order.accepted()) << result.order.reason.toStdString();
  const auto* task = builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->structure_task_entity_id, sheep->get_id());
  EXPECT_EQ(task->product_type,
            std::string(Game::Systems::k_builder_product_slaughter_sheep));
  EXPECT_TRUE(task->has_construction_site);
}

TEST_F(CommandControllerTest, ASheepOrderWithNoBuilderInTheSelectionIsRefused) {
  auto* archer = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  auto* sheep = create_unit(3.0F, 0.0F, 0, Game::Units::SpawnType::Sheep);
  ASSERT_NE(sheep, nullptr);
  sheep->add_component<Engine::Core::WildlifeComponent>();
  sheep->get_component<Engine::Core::UnitComponent>()->health = 40;

  selection_system->select_unit(archer->get_id());

  auto const result = command_controller->start_food_harvest(
      sheep->get_id(),
      QString::fromLatin1(Game::Systems::k_builder_product_slaughter_sheep),
      1);

  EXPECT_TRUE(result.order.rejected());
  EXPECT_FALSE(result.order.reason.isEmpty());
}

TEST_F(CommandControllerTest, StopWithEmptySelectionIsRejectedNotSilent) {
  auto& seen = capture_feedback();

  auto const result = command_controller->on_stop_command();

  EXPECT_FALSE(result.input_consumed);
  EXPECT_TRUE(result.order.rejected());
  EXPECT_EQ(result.order.kind, App::Core::OrderKind::Stop);
  EXPECT_EQ(result.order.reason, App::Core::no_selection_reason().text);
  ASSERT_EQ(seen.size(), 1U);
}

} // namespace

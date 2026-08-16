#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "app/controllers/command_controller.h"
#include "app/core/input_command_handler.h"
#include "app/models/cursor_manager.h"
#include "app/models/cursor_mode.h"
#include "app/models/hover_tracker.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "game/view/selection_controller.h"
#include "scene/camera.h"

namespace {

class InputCommandHandlerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);

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

  std::vector<App::Core::OrderOutcome> feedback;
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

TEST_F(InputCommandHandlerTest, HudGroupClickKeepsOnlySelectedUnitsOfThatType) {
  auto* first_archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* second_archer = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* spearman = create_unit(-1.0F, 0.0F, 1, Game::Units::SpawnType::Spearman);
  auto* enemy_archer = create_unit(1.0F, 0.0F, 2, Game::Units::SpawnType::Archer);
  ASSERT_NE(first_archer, nullptr);
  ASSERT_NE(second_archer, nullptr);
  ASSERT_NE(spearman, nullptr);
  ASSERT_NE(enemy_archer, nullptr);

  selection_system->select_unit(first_archer->get_id());
  selection_system->select_unit(second_archer->get_id());
  selection_system->select_unit(spearman->get_id());
  selection_system->select_unit(enemy_archer->get_id());

  input_handler->select_selected_units_by_type(QStringLiteral("archer"), 1);

  const auto& selected = selection_system->get_selected_units();
  ASSERT_EQ(selected.size(), 2U);
  EXPECT_EQ(selected[0], first_archer->get_id());
  EXPECT_EQ(selected[1], second_archer->get_id());
}

TEST_F(InputCommandHandlerTest, StaleHudGroupDoesNotClearSelection) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  selection_system->select_unit(archer->get_id());

  input_handler->select_selected_units_by_type(QStringLiteral("spearman"), 1);

  ASSERT_EQ(selection_system->get_selected_units().size(), 1U);
  EXPECT_EQ(selection_system->get_selected_units().front(), archer->get_id());
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
  auto* second = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(second, nullptr);
  selection_system->select_unit(unit->get_id());
  selection_system->select_unit(second->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  EXPECT_TRUE(input_handler->is_placing_formation());

  input_handler->on_formation_confirm();

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, OneTroopIsMovedWithoutOpeningThePlanner) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_FALSE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  EXPECT_FALSE(input_handler->is_placing_formation());

  input_handler->on_right_click(ground_screen.x(), ground_screen.y(), 1, viewport);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, FormationConfirmClearsPatrolBeforeApplyingMove) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* second = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(second, nullptr);
  auto* patrol = unit->add_component<Engine::Core::PatrolComponent>();
  ASSERT_NE(patrol, nullptr);
  patrol->patrolling = true;
  patrol->waypoints.emplace_back(-3.0F, 0.0F);
  patrol->waypoints.emplace_back(-1.0F, 0.0F);
  selection_system->select_unit(unit->get_id());
  selection_system->select_unit(second->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  input_handler->on_formation_confirm();

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FALSE(patrol->patrolling);
  EXPECT_TRUE(patrol->waypoints.empty());
}

TEST_F(InputCommandHandlerTest, RightDoubleClickDoesNotBypassFormationPlacement) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* second = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(second, nullptr);
  selection_system->select_unit(unit->get_id());
  selection_system->select_unit(second->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));

  EXPECT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  ASSERT_TRUE(input_handler->is_placing_formation());

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());

  input_handler->on_right_double_click(
      ground_screen.x(), ground_screen.y(), 1, viewport);

  EXPECT_TRUE(input_handler->is_placing_formation());
  EXPECT_FALSE(movement->get_has_target());

  input_handler->on_formation_confirm();

  EXPECT_TRUE(movement->get_has_target());
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
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FALSE(input_handler->is_placing_formation());
}

TEST_F(InputCommandHandlerTest, MinimapRightClickMovesSelectedUnitsToWorldPosition) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  input_handler->on_minimap_right_click(QVector3D(4.0F, 0.0F, 2.0F), 1);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, MinimapRightClickDoesNothingWithNoSelection) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  input_handler->on_minimap_right_click(QVector3D(4.0F, 0.0F, 2.0F), 1);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, MinimapRightClickDoesNothingInSpectatorMode) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  input_handler->set_spectator_mode(true);
  input_handler->on_minimap_right_click(QVector3D(4.0F, 0.0F, 2.0F), 1);

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, MinimapRightClickMovesMultipleSelectedUnitsToTarget) {
  auto* unit1 = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* unit2 = create_unit(-1.0F, 0.0F, 1, Game::Units::SpawnType::Spearman);
  ASSERT_NE(unit1, nullptr);
  ASSERT_NE(unit2, nullptr);
  selection_system->select_unit(unit1->get_id());
  selection_system->select_unit(unit2->get_id());

  input_handler->on_minimap_right_click(QVector3D(4.0F, 0.0F, 2.0F), 1);

  auto* mv1 = unit1->get_component<Engine::Core::MovementComponent>();
  auto* mv2 = unit2->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(mv1, nullptr);
  ASSERT_NE(mv2, nullptr);
  EXPECT_TRUE(mv1->get_has_target());
  EXPECT_TRUE(mv2->get_has_target());
}

TEST_F(InputCommandHandlerTest, FormationSlotsKeepTheirMeaningAcrossSelections) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* spearman = create_unit(-1.0F, 0.0F, 1, Game::Units::SpawnType::Spearman);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(spearman, nullptr);

  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(spearman->get_id());
  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));
  ASSERT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));
  ASSERT_TRUE(command_controller->is_placing_formation());

  const QStringList mixed = command_controller->formation_intents();
  EXPECT_EQ(mixed.size(), 7);
  EXPECT_EQ(mixed.at(0), QStringLiteral("faction_default"));
  EXPECT_EQ(mixed.at(1), QStringLiteral("line"));
  EXPECT_EQ(mixed.at(2), QStringLiteral("column"));

  input_handler->on_formation_cancel();

  auto* second_archer = create_unit(-4.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(second_archer, nullptr);
  selection_system->clear_selection();
  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(second_archer->get_id());
  ASSERT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));

  EXPECT_EQ(command_controller->formation_intents(), mixed);
}

TEST_F(InputCommandHandlerTest, FormationsTheSelectionCannotFieldCarryAReason) {
  auto* archer = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* second_archer = create_unit(-2.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(second_archer, nullptr);
  selection_system->select_unit(archer->get_id());
  selection_system->select_unit(second_archer->get_id());

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));
  ASSERT_TRUE(
      input_handler->on_right_press(ground_screen.x(), ground_screen.y(), 1, viewport));

  const QString reason = command_controller->formation_intent_unavailable_reason(
      QStringLiteral("encirclement"));
  EXPECT_FALSE(reason.isEmpty());

  EXPECT_TRUE(
      command_controller
          ->formation_intent_unavailable_reason(QStringLiteral("faction_default"))
          .isEmpty());
}

TEST_F(InputCommandHandlerTest, DoctrineChoicesAreOfferedFromTheRegistryNotAFixedList) {
  const QVariantList options = command_controller->formation_doctrine_options();
  ASSERT_GE(options.size(), 2);

  EXPECT_TRUE(options.at(0).toMap()[QStringLiteral("id")].toString().isEmpty());
  EXPECT_FALSE(options.at(0).toMap()[QStringLiteral("name")].toString().isEmpty());

  QStringList ids;
  for (const QVariant& entry : options) {
    const QVariantMap option = entry.toMap();
    EXPECT_FALSE(option[QStringLiteral("name")].toString().isEmpty());
    ids.append(option[QStringLiteral("id")].toString());
  }
  EXPECT_TRUE(ids.contains(QStringLiteral("rome")));
  EXPECT_TRUE(ids.contains(QStringLiteral("carthage")));
}

TEST_F(InputCommandHandlerTest, ThePlacementArrowPointsWhereTheUnitsEndUpFacing) {
  auto* first = create_unit(-3.0F, -6.0F, 1, Game::Units::SpawnType::Archer);
  auto* second = create_unit(-1.0F, -6.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  selection_system->select_unit(first->get_id());
  selection_system->select_unit(second->get_id());

  const QVector3D anchor(0.0F, 0.0F, 0.0F);
  QPointF const anchor_screen = world_to_screen(anchor);
  ASSERT_TRUE(
      input_handler->on_right_press(anchor_screen.x(), anchor_screen.y(), 1, viewport));
  ASSERT_TRUE(command_controller->is_placing_formation());

  const QVector3D orient_target(-6.0F, 0.0F, 0.0F);
  QPointF const orient_screen = world_to_screen(orient_target);
  input_handler->on_right_drag_orient(orient_screen.x(), orient_screen.y(), viewport);

  const QVector3D dragged =
      orient_target - command_controller->get_formation_placement_position();
  float const dragged_degrees =
      std::atan2(dragged.x(), dragged.z()) * 180.0F / std::numbers::pi_v<float>;

  float const arrow_degrees = command_controller->get_formation_facing_degrees();
  EXPECT_NEAR(arrow_degrees, dragged_degrees, 0.5F);
  EXPECT_NEAR(command_controller->formation_preview().facing, arrow_degrees, 0.001F);

  input_handler->on_formation_confirm();

  for (const auto* unit : {first, second}) {
    const auto* transform = unit->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_TRUE(transform->has_desired_yaw);
    EXPECT_NEAR(transform->desired_yaw, arrow_degrees, 0.5F);
  }
}

TEST_F(InputCommandHandlerTest, UntouchedPlacementFacesAwayFromTheUnitsThatMarch) {
  auto* first = create_unit(-1.0F, -6.0F, 1, Game::Units::SpawnType::Archer);
  auto* second = create_unit(1.0F, -6.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  selection_system->select_unit(first->get_id());
  selection_system->select_unit(second->get_id());

  QPointF const anchor_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  ASSERT_TRUE(
      input_handler->on_right_press(anchor_screen.x(), anchor_screen.y(), 1, viewport));

  const QVector3D anchor = command_controller->get_formation_placement_position();
  const QVector3D march(anchor.x() - 0.0F, 0.0F, anchor.z() - (-6.0F));
  float const march_degrees =
      std::atan2(march.x(), march.z()) * 180.0F / std::numbers::pi_v<float>;

  EXPECT_NEAR(command_controller->get_formation_facing_degrees(), march_degrees, 0.5F);

  input_handler->on_formation_confirm();

  const auto* transform = first->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_NEAR(transform->desired_yaw, march_degrees, 0.5F);
}

TEST_F(InputCommandHandlerTest, RightPressAttackPublishesFeedbackForTheClickedEnemy) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  auto* bystander = create_unit(3.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(enemy, nullptr);
  ASSERT_NE(bystander, nullptr);
  selection_system->select_unit(unit->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(
      input_handler->on_right_press(enemy_screen.x(), enemy_screen.y(), 1, viewport));

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().accepted());
  EXPECT_EQ(seen.front().kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(seen.front().target, enemy->get_id());
  EXPECT_NE(seen.front().target, bystander->get_id());
}

TEST_F(InputCommandHandlerTest, RightPressOnEnemyWithBuildersOnlyIsRefusedAndConsumed) {
  auto* builder = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Builder);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(builder, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(builder->get_id());
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(
      input_handler->on_right_press(enemy_screen.x(), enemy_screen.y(), 1, viewport));
  EXPECT_FALSE(input_handler->is_placing_formation());

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().rejected());
  EXPECT_EQ(seen.front().kind, App::Core::OrderKind::Attack);
  EXPECT_EQ(seen.front().target, enemy->get_id());
  EXPECT_FALSE(seen.front().reason.isEmpty());
  EXPECT_EQ(builder->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
  auto* movement = builder->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(InputCommandHandlerTest, RightClickOnGroundPublishesAMoveWithTheDestination) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());
  auto& seen = capture_feedback();

  QVector3D const destination(4.0F, 0.0F, 2.0F);
  QPointF const ground_screen = world_to_screen(destination);
  input_handler->on_right_click(ground_screen.x(), ground_screen.y(), 1, viewport);

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().accepted());
  EXPECT_EQ(seen.front().kind, App::Core::OrderKind::Move);
  ASSERT_TRUE(seen.front().has_destination);
  EXPECT_NEAR(seen.front().destination.x(), destination.x(), 0.25F);
  EXPECT_NEAR(seen.front().destination.z(), destination.z(), 0.25F);
}

TEST_F(InputCommandHandlerTest, RightClickWithEmptySelectionPublishesNothing) {
  auto& seen = capture_feedback();

  input_handler->on_right_click(400.0, 300.0, 1, viewport);
  input_handler->on_minimap_right_click(QVector3D(2.0F, 0.0F, 2.0F), 1);
  EXPECT_FALSE(input_handler->on_right_press(400.0, 300.0, 1, viewport));

  EXPECT_TRUE(seen.empty());
}

TEST_F(InputCommandHandlerTest, AttackModeClickPublishesFeedbackAndResetsTheCursor) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  auto* enemy = create_unit(0.0F, 0.0F, 2, Game::Units::SpawnType::Knight);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(enemy, nullptr);
  selection_system->select_unit(unit->get_id());
  cursor_manager.set_mode(CursorMode::Attack);
  auto& seen = capture_feedback();

  QPointF const enemy_screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  input_handler->on_attack_click(enemy_screen.x(), enemy_screen.y(), viewport);

  EXPECT_EQ(cursor_manager.mode(), CursorMode::Normal);
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().accepted());
  EXPECT_EQ(seen.front().target, enemy->get_id());
}

TEST_F(InputCommandHandlerTest, AttackModeClickOnNothingExplainsTheRefusal) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());
  cursor_manager.set_mode(CursorMode::Attack);
  auto& seen = capture_feedback();

  QPointF const ground_screen = world_to_screen(QVector3D(4.0F, 0.0F, 2.0F));
  input_handler->on_attack_click(ground_screen.x(), ground_screen.y(), viewport);

  EXPECT_EQ(cursor_manager.mode(), CursorMode::Normal);
  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().rejected());
  EXPECT_FALSE(seen.front().reason.isEmpty());
  EXPECT_EQ(unit->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(InputCommandHandlerTest, MinimapRightClickPublishesMoveFeedback) {
  auto* unit = create_unit(-3.0F, 0.0F, 1, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());
  auto& seen = capture_feedback();

  input_handler->on_minimap_right_click(QVector3D(5.0F, 0.0F, 5.0F), 1);

  ASSERT_EQ(seen.size(), 1U);
  EXPECT_TRUE(seen.front().accepted());
  EXPECT_EQ(seen.front().kind, App::Core::OrderKind::Move);
  EXPECT_EQ(seen.front().destination, QVector3D(5.0F, 0.0F, 5.0F));
}

} // namespace

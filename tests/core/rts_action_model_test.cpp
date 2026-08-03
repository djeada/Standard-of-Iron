#include <gtest/gtest.h>

#include "app/core/rts_action_model.h"
#include "app/models/cursor_mode.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"

namespace {

auto add_selected_unit(Engine::Core::World& world,
                       Game::Systems::SelectionSystem& selection,
                       Game::Units::SpawnType spawn_type) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = spawn_type;
  selection.select_unit(entity->get_id());
  return entity;
}

TEST(RtsActionModel, EngagedUnitDoesNotReportAttackCommandModeWithNormalCursor) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* unit = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  unit->add_component<Engine::Core::AttackTargetComponent>()->target_id = 42U;

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Normal;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("normal"));
}

TEST(RtsActionModel, AttackCursorModeReportsAttackCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Attack;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("attack"));
}

TEST(RtsActionModel, GuardModeStillReportsGuardCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* unit = add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);
  unit->add_component<Engine::Core::GuardModeComponent>()->active = true;

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Normal;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("guard"));
}

TEST(RtsActionModel, SelectedCommanderAuraReflectsReadyActiveAndCooldownStates) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* entity =
      add_selected_unit(world, *selection, Game::Units::SpawnType::RomanVeteranConsul);
  auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);

  App::Core::ActionContext context;
  context.world = &world;
  auto aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_TRUE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_FALSE(aura[QStringLiteral("active")].toBool());

  commander->aura_ability_active = true;
  commander->aura_ability_remaining = 3.0F;
  aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_FALSE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_TRUE(aura[QStringLiteral("active")].toBool());

  commander->aura_ability_active = false;
  commander->aura_ability_remaining = 0.0F;
  commander->aura_ability_cooldown_remaining = 8.0F;
  aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_FALSE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_FALSE(aura[QStringLiteral("active")].toBool());
}

TEST(RtsActionModel, RepairIsOfferedToBuildersAndNobodyElse) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;

  EXPECT_FALSE(App::Core::get_action_states(context)
                   .value(QStringLiteral("repair"))
                   .toMap()
                   .value(QStringLiteral("enabled"))
                   .toBool());

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  EXPECT_TRUE(App::Core::get_action_states(context)
                  .value(QStringLiteral("repair"))
                  .toMap()
                  .value(QStringLiteral("enabled"))
                  .toBool());
}

TEST(RtsActionModel, ARepairingBuilderReportsTheOrderAsActive) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "repair_structure";

  App::Core::ActionContext context;
  context.world = &world;

  EXPECT_EQ(App::Core::get_toggle_state(&world, QStringLiteral("repair")),
            QStringLiteral("all"));
  EXPECT_TRUE(App::Core::get_mode_availability(&world)
                  .value(QStringLiteral("canRepair"))
                  .toBool());
}

TEST(RtsActionModel, ArmingRepairShowsUpAsTheCurrentCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Repair;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("repair"));
  EXPECT_EQ(App::Core::action_id_for_cursor_mode(CursorMode::Repair),
            QStringLiteral("repair"));
}

} // namespace

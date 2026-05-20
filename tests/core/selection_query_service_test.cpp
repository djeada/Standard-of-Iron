#include <gtest/gtest.h>

#include "app/core/selection_query_service.h"
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

TEST(SelectionQueryService, ReportsMixedHoldStateForPartialSelection) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* active = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::HoldModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);

  SelectionQueryService const service(&world);
  EXPECT_EQ(service.get_selected_units_toggle_state(QStringLiteral("hold")),
            QStringLiteral("mixed"));
}

TEST(SelectionQueryService, IgnoresIneligibleUnitsInHoldState) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* active = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::HoldModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);

  SelectionQueryService const service(&world);
  EXPECT_EQ(service.get_selected_units_toggle_state(QStringLiteral("hold")),
            QStringLiteral("all"));
}

TEST(SelectionQueryService, ReportsMixedFormationStateForPartialSelection) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* active = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::FormationModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);

  SelectionQueryService const service(&world);
  EXPECT_EQ(service.get_selected_units_toggle_state(QStringLiteral("formation")),
            QStringLiteral("mixed"));
}

TEST(SelectionQueryService, BuilderSelectionEnablesCollectMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);

  SelectionQueryService const service(&world);
  QVariantMap const availability = service.get_selected_units_mode_availability();
  EXPECT_TRUE(availability.value(QStringLiteral("canBuild")).toBool());
  EXPECT_TRUE(availability.value(QStringLiteral("canCollect")).toBool());
}

TEST(SelectionQueryService, MixedSelectionUsesUnionAvailability) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);

  SelectionQueryService const service(&world);
  QVariantMap const availability = service.get_selected_units_mode_availability();
  EXPECT_TRUE(availability.value(QStringLiteral("canAttack")).toBool());
  EXPECT_TRUE(availability.value(QStringLiteral("canBuild")).toBool());
  EXPECT_TRUE(availability.value(QStringLiteral("canCollect")).toBool());
}

TEST(SelectionQueryService, CommandModeIgnoresIneligibleUnitsForGuardState) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* archer = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  archer->add_component<Engine::Core::GuardModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Barracks);

  SelectionQueryService const service(&world);
  EXPECT_EQ(service.get_selected_units_command_mode(), QStringLiteral("guard"));
}

} // namespace

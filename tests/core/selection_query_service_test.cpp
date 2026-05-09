#include "app/core/selection_query_service.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include <gtest/gtest.h>

namespace {

auto add_selected_unit(
    Engine::Core::World &world, Game::Systems::SelectionSystem &selection,
    Game::Units::SpawnType spawn_type) -> Engine::Core::Entity * {
  auto *entity = world.create_entity();
  auto *unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = spawn_type;
  selection.select_unit(entity->get_id());
  return entity;
}

TEST(SelectionQueryService, ReportsMixedHoldStateForPartialSelection) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto *selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto *active =
      add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::HoldModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);

  SelectionQueryService service(&world);
  EXPECT_EQ(service.get_selected_units_toggle_state(QStringLiteral("hold")),
            QStringLiteral("mixed"));
}

TEST(SelectionQueryService, IgnoresIneligibleUnitsInHoldState) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto *selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto *active =
      add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::HoldModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);

  SelectionQueryService service(&world);
  EXPECT_EQ(service.get_selected_units_toggle_state(QStringLiteral("hold")),
            QStringLiteral("all"));
}

TEST(SelectionQueryService, ReportsMixedFormationStateForPartialSelection) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto *selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto *active =
      add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  active->add_component<Engine::Core::FormationModeComponent>()->active = true;
  add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);

  SelectionQueryService service(&world);
  EXPECT_EQ(
      service.get_selected_units_toggle_state(QStringLiteral("formation")),
      QStringLiteral("mixed"));
}

} // namespace

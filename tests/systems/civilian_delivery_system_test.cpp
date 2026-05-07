#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/production_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

#include <gtest/gtest.h>
#include <vector>

namespace {

TEST(CivilianDeliverySystemTest, HomeRecruitsCivilianUsingHomeManpowerPool) {
  Engine::Core::World world;

  auto *home = world.create_entity();
  auto *home_unit = home->add_component<Engine::Core::UnitComponent>();
  auto *home_prod = home->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(home_unit, nullptr);
  ASSERT_NE(home_prod, nullptr);

  home_unit->spawn_type = Game::Units::SpawnType::Home;
  home_unit->owner_id = 1;
  home_prod->product_type = Game::Units::TroopType::Civilian;
  home_prod->build_time = 5.0F;
  home_prod->max_units = 10000;
  home_prod->manpower_available = 8;

  const std::vector<Engine::Core::EntityID> selected = {home->get_id()};
  auto result = Game::Systems::ProductionService::
      start_production_for_first_selected_home(
          world, selected, 1, Game::Units::TroopType::Civilian);

  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
  EXPECT_TRUE(home_prod->in_progress);
  EXPECT_EQ(home_prod->manpower_available, 0);
}

TEST(CivilianDeliverySystemTest, CivilianEnteringBarracksTransfersManpower) {
  Engine::Core::World world;

  auto *barracks = world.create_entity();
  auto *barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto *barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto *barracks_prod =
      barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_prod, nullptr);

  barracks_transform->position = {20.0F, 0.0F, 20.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_prod->manpower_available = 0;

  auto *civilian = world.create_entity();
  auto *civilian_transform =
      civilian->add_component<Engine::Core::TransformComponent>();
  auto *civilian_unit = civilian->add_component<Engine::Core::UnitComponent>();
  auto *delivery =
      civilian->add_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(civilian_transform, nullptr);
  ASSERT_NE(civilian_unit, nullptr);
  ASSERT_NE(delivery, nullptr);

  civilian_transform->position = {20.5F, 0.0F, 20.2F};
  civilian_unit->spawn_type = Game::Units::SpawnType::Civilian;
  civilian_unit->owner_id = 1;
  delivery->target_barracks_id = barracks->get_id();

  Game::Systems::CivilianDeliverySystem system;
  system.update(&world, 0.016F);

  EXPECT_EQ(world.get_entity(civilian->get_id()), nullptr);
  EXPECT_EQ(barracks_prod->manpower_available, 8);
}

} // namespace

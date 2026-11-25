#include <gtest/gtest.h>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/component_economy.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "game/systems/town_growth_system.h"
#include "game/units/spawn_type.h"
#include "game/visuals/building_asset_key.h"

namespace {

using Game::Systems::TownGrowthSystem;

struct HomeHandles {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::BuildingComponent* building{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
};

auto add_home(Engine::Core::World& world, int owner_id, float x, float z) -> HomeHandles {
  HomeHandles home;
  home.entity = world.create_entity();
  auto* transform = home.entity->add_component<Engine::Core::TransformComponent>();
  home.unit = home.entity->add_component<Engine::Core::UnitComponent>();
  home.entity->add_component<Engine::Core::HomeComponent>();
  home.building = home.entity->add_component<Engine::Core::BuildingComponent>();
  home.renderable = home.entity->add_component<Engine::Core::RenderableComponent>();
  transform->position = {x, 0.0F, z};
  home.unit->spawn_type = Game::Units::SpawnType::Home;
  home.unit->owner_id = owner_id;
  home.unit->health = 100;
  home.building->original_nation_id = Game::Systems::NationID::RomanRepublic;
  home.renderable->renderer_id = Game::Visuals::styled_building_asset_key(
      Game::Systems::NationID::RomanRepublic, "home", "");
  return home;
}

auto tall_count(const std::vector<HomeHandles>& homes) -> int {
  int count = 0;
  for (const auto& home : homes) {
    if (home.building->visual_style == "tenement") {
      ++count;
    }
  }
  return count;
}

// One pass rebuilds at most one house per owner.
void run_passes(TownGrowthSystem& system, Engine::Core::World& world, int passes) {
  for (int i = 0; i < passes; ++i) {
    system.update(&world, 60.0F);
  }
}

TEST(TownGrowthSystemTest, TargetsFollowTheDesignThresholds) {
  EXPECT_EQ(TownGrowthSystem::tall_home_target(0), 0);
  EXPECT_EQ(TownGrowthSystem::tall_home_target(9), 0);
  EXPECT_EQ(TownGrowthSystem::tall_home_target(10), 3);
  EXPECT_EQ(TownGrowthSystem::tall_home_target(19), 6);
  EXPECT_EQ(TownGrowthSystem::tall_home_target(20), 13);
  EXPECT_EQ(TownGrowthSystem::tall_home_target(30), 20);
}

TEST(TownGrowthSystemTest, AVillageKeepsItsHouses) {
  Engine::Core::World world;
  std::vector<HomeHandles> homes;
  for (int i = 0; i < 9; ++i) {
    homes.push_back(add_home(world, 1, static_cast<float>(i) * 6.0F, 0.0F));
  }

  TownGrowthSystem system;
  run_passes(system, world, 12);

  EXPECT_EQ(tall_count(homes), 0);
}

TEST(TownGrowthSystemTest, ATownRebuildsItsHeartOneHouseAtATime) {
  Engine::Core::World world;
  std::vector<HomeHandles> homes;
  for (int i = 0; i < 10; ++i) {
    homes.push_back(add_home(world, 1, static_cast<float>(i) * 6.0F, 0.0F));
  }
  const std::string plain_key = homes.front().renderable->renderer_id;

  TownGrowthSystem system;
  system.update(&world, 60.0F);
  EXPECT_EQ(tall_count(homes), 1) << "a pass rebuilds one house, not the town at once";

  run_passes(system, world, 12);
  EXPECT_EQ(tall_count(homes), 3);

  // The row runs 0..54 m; its middle is 27 m, so the tall houses are the
  // three nearest it and the ends stay as they were built.
  EXPECT_EQ(homes[4].building->visual_style, "tenement");
  EXPECT_EQ(homes[5].building->visual_style, "tenement");
  EXPECT_TRUE(homes[0].building->visual_style.empty());
  EXPECT_TRUE(homes[9].building->visual_style.empty());

  const std::string tall_key = Game::Visuals::styled_building_asset_key(
      Game::Systems::NationID::RomanRepublic, "home", "tenement");
  EXPECT_NE(tall_key, plain_key);
  EXPECT_EQ(homes[4].renderable->renderer_id, tall_key);
  EXPECT_EQ(homes[0].renderable->renderer_id, plain_key);
}

TEST(TownGrowthSystemTest, ACityStandsMostlyTall) {
  Engine::Core::World world;
  std::vector<HomeHandles> homes;
  for (int i = 0; i < 20; ++i) {
    homes.push_back(add_home(
        world, 1, static_cast<float>(i % 5) * 6.0F, static_cast<float>(i / 5) * 6.0F));
  }

  TownGrowthSystem system;
  run_passes(system, world, 40);

  EXPECT_EQ(tall_count(homes), 13);
}

TEST(TownGrowthSystemTest, OwnersGrowApartAndRuinsDoNotCount) {
  Engine::Core::World world;
  std::vector<HomeHandles> town;
  std::vector<HomeHandles> village;
  for (int i = 0; i < 10; ++i) {
    town.push_back(add_home(world, 1, static_cast<float>(i) * 6.0F, 0.0F));
  }
  for (int i = 0; i < 6; ++i) {
    village.push_back(add_home(world, 2, static_cast<float>(i) * 6.0F, 80.0F));
  }
  // A burnt-out house is not part of the town any more.
  town[9].unit->health = 0;

  TownGrowthSystem system;
  run_passes(system, world, 12);

  EXPECT_EQ(tall_count(town), 0) << "nine standing homes are still a village";
  EXPECT_EQ(tall_count(village), 0);
}

TEST(TownGrowthSystemTest, AuthoredStylesAreLeftAlone) {
  Engine::Core::World world;
  std::vector<HomeHandles> homes;
  for (int i = 0; i < 10; ++i) {
    homes.push_back(add_home(world, 1, static_cast<float>(i) * 6.0F, 0.0F));
  }
  // The map author already drew the middle of this street as insulae.
  for (int i = 3; i <= 6; ++i) {
    homes[static_cast<std::size_t>(i)].building->visual_style = "tenement";
  }

  TownGrowthSystem system;
  run_passes(system, world, 12);

  EXPECT_EQ(tall_count(homes), 4) << "an already tall town is not rebuilt further";
}

} // namespace

#include <gtest/gtest.h>

#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/units/building_spawn_setup.h"

namespace {

TEST(BuildingSpawnSetup, AssignsCanonicalRendererKey) {
  Engine::Core::StandaloneEntity scratch(1);
  Engine::Core::Entity& entity = scratch.entity();

  auto* renderable = Game::Units::add_building_renderable(
      entity, Game::Systems::NationID::Carthage, "barracks");
  ASSERT_NE(renderable, nullptr);

  EXPECT_EQ(renderable->renderer_id, "troops/carthage/barracks");
  EXPECT_TRUE(renderable->visible);
}

TEST(BuildingSpawnSetup, EnsuresBuildingComponentTracksOriginalNation) {
  Engine::Core::StandaloneEntity scratch(2);
  Engine::Core::Entity& entity = scratch.entity();

  auto* renderable = Game::Units::add_building_renderable(
      entity, Game::Systems::NationID::RomanRepublic, "home");
  ASSERT_NE(renderable, nullptr);

  auto* building = entity.get_component<Engine::Core::BuildingComponent>();
  ASSERT_NE(building, nullptr);
  EXPECT_EQ(building->original_nation_id, Game::Systems::NationID::RomanRepublic);
}

} // namespace

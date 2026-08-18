#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/units/building_spawn_setup.h"

namespace {

TEST(BuildingSpawnSetup, AssignsCanonicalRendererKey) {
  Engine::Core::Entity entity(1);

  auto* renderable = Game::Units::add_building_renderable(
      entity, Game::Systems::NationID::Carthage, "barracks");
  ASSERT_NE(renderable, nullptr);

  EXPECT_EQ(renderable->renderer_id, "troops/carthage/barracks");
  EXPECT_TRUE(renderable->visible);
}

TEST(BuildingSpawnSetup, EnsuresBuildingComponentTracksOriginalNation) {
  Engine::Core::Entity entity(2);

  auto* renderable = Game::Units::add_building_renderable(
      entity, Game::Systems::NationID::RomanRepublic, "home");
  ASSERT_NE(renderable, nullptr);

  auto* building = entity.get_component<Engine::Core::BuildingComponent>();
  ASSERT_NE(building, nullptr);
  EXPECT_EQ(building->original_nation_id, Game::Systems::NationID::RomanRepublic);
}

} // namespace

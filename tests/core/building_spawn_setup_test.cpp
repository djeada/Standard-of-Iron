#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/units/building_spawn_setup.h"
#include "game/visuals/team_colors.h"

#include <gtest/gtest.h>

namespace {

TEST(BuildingSpawnSetup, AssignsCanonicalRendererKeyAndTeamColor) {
  Engine::Core::Entity entity(1);

  auto *renderable = Game::Units::add_building_renderable(
      entity, 3, Game::Systems::NationID::Carthage, "barracks");
  ASSERT_NE(renderable, nullptr);

  EXPECT_EQ(renderable->renderer_id, "troops/carthage/barracks");

  QVector3D const expected = Game::Visuals::team_colorForOwner(3);
  EXPECT_FLOAT_EQ(renderable->color[0], expected.x());
  EXPECT_FLOAT_EQ(renderable->color[1], expected.y());
  EXPECT_FLOAT_EQ(renderable->color[2], expected.z());
}

TEST(BuildingSpawnSetup, EnsuresBuildingComponentTracksOriginalNation) {
  Engine::Core::Entity entity(2);

  auto *renderable = Game::Units::add_building_renderable(
      entity, 1, Game::Systems::NationID::RomanRepublic, "home");
  ASSERT_NE(renderable, nullptr);

  auto *building = entity.get_component<Engine::Core::BuildingComponent>();
  ASSERT_NE(building, nullptr);
  EXPECT_EQ(building->original_nation_id,
            Game::Systems::NationID::RomanRepublic);
}

} // namespace

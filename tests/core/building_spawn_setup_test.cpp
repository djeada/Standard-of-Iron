#include <cstddef>
#include <gtest/gtest.h>
#include <string>

#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/systems/building_collision_registry.h"
#include "game/units/building_spawn_setup.h"
#include "game/units/spawn_type.h"

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

TEST(SpawnTypeNames, CompileTimeNamesMatchTheQStringNames) {
  for (std::size_t index = 0; index < Game::Units::k_spawn_type_count; ++index) {
    auto const type = static_cast<Game::Units::SpawnType>(index);
    std::string const name(Game::Units::spawn_type_name(type));
    EXPECT_EQ(QString::fromStdString(name), Game::Units::spawn_typeToQString(type));
    EXPECT_EQ(Game::Units::spawn_typeToString(type), name);

    Game::Units::SpawnType parsed = Game::Units::SpawnType::Archer;
    ASSERT_TRUE(Game::Units::try_parse_spawn_type(QString::fromStdString(name), parsed))
        << name;
    EXPECT_EQ(parsed, type) << name;
  }
}

TEST(SpawnTypeNames, BuildingSizeBySpawnTypeMatchesTheNamedLookup) {
  using Game::Systems::BuildingCollisionRegistry;
  for (std::size_t index = 0; index < Game::Units::k_spawn_type_count; ++index) {
    auto const type = static_cast<Game::Units::SpawnType>(index);
    auto const by_type = BuildingCollisionRegistry::get_building_size(type);
    auto const by_name = BuildingCollisionRegistry::get_building_size(
        Game::Units::spawn_typeToString(type));
    EXPECT_FLOAT_EQ(by_type.width, by_name.width);
    EXPECT_FLOAT_EQ(by_type.depth, by_name.depth);
  }
  EXPECT_FLOAT_EQ(
      BuildingCollisionRegistry::get_building_size(Game::Units::SpawnType::Farm).width,
      13.6F);
}

} // namespace

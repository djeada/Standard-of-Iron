#include <QVector3D>

#include <gtest/gtest.h>
#include <memory>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/units/spawn_type.h"
#include "tools/arena/terrain_alignment.h"

namespace {

using Engine::Core::BuildingComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;

class ArenaTerrainAlignmentTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto make_building(Engine::Core::World& world,
                     Game::Units::SpawnType spawn_type,
                     const char* collision_type,
                     float x,
                     float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 7.5F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 0.0F, 0.0F);
    unit->owner_id = 1;
    unit->spawn_type = spawn_type;
    entity->add_component<BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(), collision_type, x, z, 1);
    return entity;
  }
};

TEST_F(ArenaTerrainAlignmentTest, BuildingsKeepTheirPlotWhenTerrainIsRebuilt) {
  // Regenerating terrain re-seats everything on the new surface. A building
  // stands on its own registered footprint, which is unwalkable by definition,
  // so snapping it to the nearest walkable cell would evict it from its plot and
  // scramble a settlement on every regenerate.
  Engine::Core::World world;
  auto* barracks =
      make_building(world, Game::Units::SpawnType::Barracks, "barracks", 6.0F, -4.0F);
  auto* wall = make_building(
      world, Game::Units::SpawnType::WallSegment, "wall_segment", 8.0F, 0.0F);

  Arena::align_entity_to_ground(*barracks);
  Arena::align_entity_to_ground(*wall);

  const auto* barracks_transform = barracks->get_component<TransformComponent>();
  const auto* wall_transform = wall->get_component<TransformComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(wall_transform, nullptr);

  EXPECT_FLOAT_EQ(barracks_transform->position.x, 6.0F);
  EXPECT_FLOAT_EQ(barracks_transform->position.z, -4.0F);
  EXPECT_FLOAT_EQ(wall_transform->position.x, 8.0F);
  EXPECT_FLOAT_EQ(wall_transform->position.z, 0.0F);
}

TEST_F(ArenaTerrainAlignmentTest, TroopsAreStillPushedOutOfOccupiedGround) {
  Engine::Core::World world;
  make_building(world, Game::Units::SpawnType::Barracks, "barracks", 0.0F, 0.0F);

  auto* soldier = world.create_entity();
  soldier->add_component<TransformComponent>(0.0F, 7.5F, 0.0F);
  auto* unit = soldier->add_component<UnitComponent>(100, 100, 1.0F, 8.0F);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Knight;

  EXPECT_FALSE(Arena::entity_keeps_planar_position(*soldier));
  Arena::align_entity_to_ground(*soldier);

  const auto* transform = soldier->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  const float distance =
      QVector3D(transform->position.x, 0.0F, transform->position.z).length();
  EXPECT_GT(distance, 0.5F) << "a soldier standing inside a barracks must be moved out";
}

TEST_F(ArenaTerrainAlignmentTest, ConstructionSitesAreAnchoredLikeFinishedBuildings) {
  Engine::Core::World world;
  auto* site = world.create_entity();
  site->add_component<TransformComponent>(-4.0F, 0.0F, 2.0F);
  site->add_component<Engine::Core::WallConstructionSiteComponent>();

  EXPECT_TRUE(Arena::entity_keeps_planar_position(*site));
}

} // namespace

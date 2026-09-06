#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/core/world_spatial_index.h"

namespace {

using Engine::Core::BuildingComponent;
using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;
using Engine::Core::WorldSpatialIndex;

auto spawn(World& world, float x, float z, int owner_id, int health = 100) -> EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<UnitComponent>();
  unit->owner_id = owner_id;
  unit->health = health;
  return entity->get_id();
}

auto ids_in_radius(const WorldSpatialIndex& index,
                   float x,
                   float z,
                   float radius) -> std::vector<EntityID> {
  std::vector<EntityID> found;
  index.for_each_in_radius(
      x, z, radius, [&found](const WorldSpatialIndex::Entry& entry) {
        found.push_back(entry.id);
      });
  std::sort(found.begin(), found.end());
  return found;
}

TEST(WorldSpatialIndexTest, FindsOnlyEntitiesInsideTheRadius) {
  World world;
  const EntityID close = spawn(world, 1.0F, 1.0F, 1);
  const EntityID edge = spawn(world, 4.0F, 0.0F, 1);
  spawn(world, 40.0F, 40.0F, 1);

  auto& index = world.spatial_index();
  index.rebuild(world);

  const auto found = ids_in_radius(index, 0.0F, 0.0F, 5.0F);
  std::vector<EntityID> expected{close, edge};
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(found, expected);
}

TEST(WorldSpatialIndexTest, AgreesWithTheBruteForceScanAtEveryRadius) {
  World world;
  std::vector<EntityID> ids;
  for (int i = 0; i < 40; ++i) {
    const auto fi = static_cast<float>(i);
    ids.push_back(spawn(world,
                        std::fmod(fi * 7.0F, 60.0F) - 30.0F,
                        std::fmod(fi * 13.0F, 60.0F) - 30.0F,
                        1));
  }

  auto& index = world.spatial_index();
  index.rebuild(world);

  for (float radius : {0.5F, 3.0F, 9.0F, 25.0F}) {
    std::vector<EntityID> brute;
    for (const EntityID id : ids) {
      auto* entity = world.get_entity(id);
      const auto& position = entity->get_component<TransformComponent>()->position;
      const float dx = position.x - 2.0F;
      const float dz = position.z + 4.0F;
      if (dx * dx + dz * dz <= radius * radius) {
        brute.push_back(id);
      }
    }
    std::sort(brute.begin(), brute.end());
    EXPECT_EQ(ids_in_radius(index, 2.0F, -4.0F, radius), brute) << "radius " << radius;
  }
}

TEST(WorldSpatialIndexTest, CarriesTheFlagsProximityQueriesFilterOn) {
  World world;
  const EntityID soldier = spawn(world, 0.0F, 0.0F, 1);
  const EntityID corpse = spawn(world, 1.0F, 0.0F, 1, 0);

  auto* fort = world.get_entity(spawn(world, 2.0F, 0.0F, 2));
  fort->add_component<BuildingComponent>();

  auto& index = world.spatial_index();
  index.rebuild(world);

  const auto* soldier_entry = index.find(soldier);
  ASSERT_NE(soldier_entry, nullptr);
  EXPECT_TRUE(soldier_entry->is(WorldSpatialIndex::k_alive));
  EXPECT_FALSE(soldier_entry->is(WorldSpatialIndex::k_building));
  EXPECT_EQ(soldier_entry->owner_id, 1);

  const auto* corpse_entry = index.find(corpse);
  ASSERT_NE(corpse_entry, nullptr);
  EXPECT_FALSE(corpse_entry->is(WorldSpatialIndex::k_alive));

  const auto* fort_entry = index.find(fort->get_id());
  ASSERT_NE(fort_entry, nullptr);
  EXPECT_TRUE(fort_entry->is(WorldSpatialIndex::k_building));
  EXPECT_EQ(fort_entry->owner_id, 2);
}

TEST(WorldSpatialIndexTest, ForgetsEntitiesThatLeftTheWorld) {
  World world;
  const EntityID doomed = spawn(world, 0.0F, 0.0F, 1);
  const EntityID survivor = spawn(world, 1.0F, 0.0F, 1);

  auto& index = world.spatial_index();
  index.rebuild(world);
  ASSERT_NE(index.find(doomed), nullptr);

  world.destroy_entity(doomed);
  index.rebuild(world);

  EXPECT_EQ(index.find(doomed), nullptr);
  EXPECT_NE(index.find(survivor), nullptr);
  EXPECT_EQ(index.entry_count(), 1U);
}

TEST(WorldSpatialIndexTest, RefreshRebuildsOncePerTickAndNotMore) {
  World world;
  spawn(world, 0.0F, 0.0F, 1);

  auto& index = world.spatial_index();
  world.update(0.016F);
  index.refresh(world);
  const std::uint64_t after_first_tick = index.stats().rebuilds;

  index.refresh(world);
  index.refresh(world);
  index.refresh(world);
  EXPECT_EQ(index.stats().rebuilds, after_first_tick);

  world.update(0.016F);
  index.refresh(world);
  EXPECT_EQ(index.stats().rebuilds, after_first_tick + 1);
}

TEST(WorldSpatialIndexTest, RefreshNeverServesStaleDataToAnUntickedWorld) {
  World world;
  const EntityID mover = spawn(world, 0.0F, 0.0F, 1);

  auto& index = world.spatial_index();
  index.refresh(world);
  ASSERT_EQ(ids_in_radius(index, 0.0F, 0.0F, 1.0F).size(), 1U);

  world.get_entity(mover)->get_component<TransformComponent>()->position.x = 50.0F;
  index.refresh(world);

  EXPECT_TRUE(ids_in_radius(index, 0.0F, 0.0F, 1.0F).empty());
  EXPECT_EQ(ids_in_radius(index, 50.0F, 0.0F, 1.0F).size(), 1U);
}

TEST(WorldSpatialIndexTest, ExaminesFarFewerCandidatesThanAFullScan) {
  World world;
  for (int x = 0; x < 30; ++x) {
    for (int z = 0; z < 30; ++z) {
      spawn(world, static_cast<float>(x) * 3.0F, static_cast<float>(z) * 3.0F, 1);
    }
  }

  auto& index = world.spatial_index();
  index.rebuild(world);
  index.reset_stats();

  std::vector<const WorldSpatialIndex::Entry*> out;
  index.query_radius(45.0F, 45.0F, 5.0F, out);

  EXPECT_FALSE(out.empty());

  EXPECT_LT(index.stats().candidates_examined, 100U);
}

} // namespace

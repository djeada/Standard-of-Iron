#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;

auto spawn_unit(World& world, float x, float z) -> EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  entity->add_component<UnitComponent>();
  return entity->get_id();
}

TEST(WorldViewTest, VisitsOnlyEntitiesCarryingEveryComponent) {
  World world;
  const EntityID both = spawn_unit(world, 1.0F, 2.0F);

  auto* transform_only = world.create_entity();
  transform_only->add_component<TransformComponent>();

  auto* unit_only = world.create_entity();
  unit_only->add_component<UnitComponent>();

  std::vector<EntityID> visited;
  for (auto [id, transform, unit] : world.view<TransformComponent, UnitComponent>()) {
    visited.push_back(id);
    EXPECT_FLOAT_EQ(transform.position.x, 1.0F);
    EXPECT_FLOAT_EQ(transform.position.z, 2.0F);
    EXPECT_EQ(unit.health, unit.health);
  }

  ASSERT_EQ(visited.size(), 1U);
  EXPECT_EQ(visited.front(), both);
}

TEST(WorldViewTest, ComponentReferencesAreLiveAndWritable) {
  World world;
  const EntityID id = spawn_unit(world, 0.0F, 0.0F);

  for (auto [entity_id, transform, unit] :
       world.view<TransformComponent, UnitComponent>()) {
    (void)entity_id;
    (void)unit;
    transform.position.x = 42.0F;
  }

  auto* entity = world.get_entity(id);
  ASSERT_NE(entity, nullptr);
  EXPECT_FLOAT_EQ(entity->get_component<TransformComponent>()->position.x, 42.0F);
}

TEST(WorldViewTest, IteratesTheSmallestComponentSet) {
  World world;
  for (int i = 0; i < 32; ++i) {
    spawn_unit(world, static_cast<float>(i), 0.0F);
  }

  auto* attacker = world.create_entity();
  attacker->add_component<TransformComponent>();
  attacker->add_component<UnitComponent>();
  attacker->add_component<AttackComponent>();

  auto view = world.view<UnitComponent, AttackComponent>();

  EXPECT_EQ(view.candidate_count(), 1U);

  std::size_t visited = 0;
  for (auto entry : view) {
    (void)entry;
    ++visited;
  }
  EXPECT_EQ(visited, 1U);
}

TEST(WorldViewTest, EmptyForAComponentNobodyCarries) {
  World world;
  spawn_unit(world, 0.0F, 0.0F);

  EXPECT_TRUE(world.view<MovementComponent>().empty());
  EXPECT_FALSE(world.view<UnitComponent>().empty());
}

TEST(WorldViewTest, SkipsEntitiesDestroyedBeforeTheWalkReachesThem) {
  World world;
  const EntityID first = spawn_unit(world, 0.0F, 0.0F);
  const EntityID second = spawn_unit(world, 1.0F, 0.0F);
  world.destroy_entity(first);

  std::vector<EntityID> visited;
  for (auto [id, transform, unit] : world.view<TransformComponent, UnitComponent>()) {
    (void)transform;
    (void)unit;
    visited.push_back(id);
  }

  ASSERT_EQ(visited.size(), 1U);
  EXPECT_EQ(visited.front(), second);
}

TEST(WorldViewTest, MatchesTheMaterialisingQuery) {
  World world;
  for (int i = 0; i < 8; ++i) {
    spawn_unit(world, static_cast<float>(i), 0.0F);
  }

  std::vector<EntityID> collected;
  for (auto* entity : world.collect_entities_with<UnitComponent>()) {
    collected.push_back(entity->get_id());
  }

  std::vector<EntityID> viewed;
  for (auto [id, unit] : world.view<UnitComponent>()) {
    (void)unit;
    viewed.push_back(id);
  }

  std::sort(collected.begin(), collected.end());
  std::sort(viewed.begin(), viewed.end());
  EXPECT_EQ(collected, viewed);
}

} // namespace

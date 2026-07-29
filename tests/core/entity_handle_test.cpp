#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::NULL_ENTITY;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;
namespace Handle = Engine::Core::Handle;

TEST(EntityHandleTest, PacksIndexAndGenerationLosslessly) {
  const EntityID id = Handle::make(123456U, 7890U);
  EXPECT_EQ(Handle::index_of(id), 123456U);
  EXPECT_EQ(Handle::generation_of(id), 7890U);

  const EntityID big = Handle::make(0xFFFFFFFFU, 0xFFFFFFFFU);
  EXPECT_EQ(Handle::index_of(big), 0xFFFFFFFFU);
  EXPECT_EQ(Handle::generation_of(big), 0xFFFFFFFFU);
}

TEST(EntityHandleTest, NullEntityIsNeverHandedOut) {
  World world;
  for (int i = 0; i < 16; ++i) {
    EXPECT_NE(world.create_entity()->get_id(), NULL_ENTITY);
  }
}

TEST(EntityHandleTest, RecycledSlotInvalidatesTheOldHandle) {
  World world;

  auto* first = world.create_entity();
  const EntityID stale = first->get_id();
  ASSERT_TRUE(world.is_alive(stale));

  world.destroy_entity(stale);
  EXPECT_FALSE(world.is_alive(stale));
  EXPECT_EQ(world.get_entity(stale), nullptr);

  auto* second = world.create_entity();
  EXPECT_EQ(Handle::index_of(second->get_id()), Handle::index_of(stale));
  EXPECT_NE(second->get_id(), stale);
  EXPECT_FALSE(world.is_alive(stale));
  EXPECT_EQ(world.get_entity(stale), nullptr);
  EXPECT_EQ(world.get_entity(second->get_id()), second);
}

TEST(EntityHandleTest, HandlesSurviveAClearWithoutAliasing) {
  World world;
  const EntityID stale = world.create_entity()->get_id();

  world.clear();
  EXPECT_FALSE(world.is_alive(stale));

  auto* reborn = world.create_entity();
  EXPECT_NE(reborn->get_id(), stale);
  EXPECT_FALSE(world.is_alive(stale));
}

TEST(EntityHandleTest, FreshWorldKeepsLegacyLookingIds) {

  World world;
  EXPECT_EQ(world.create_entity()->get_id(), 1U);
  EXPECT_EQ(world.create_entity()->get_id(), 2U);
  EXPECT_EQ(world.create_entity()->get_id(), 3U);
}

TEST(EntityHandleTest, CreateWithIdRestoresTheExactHandle) {
  World world;

  const EntityID legacy = 42;
  auto* restored = world.create_entity_with_id(legacy);
  ASSERT_NE(restored, nullptr);
  EXPECT_EQ(restored->get_id(), legacy);
  EXPECT_EQ(world.get_entity(legacy), restored);

  const EntityID generational = Handle::make(9U, 5U);
  auto* other = world.create_entity_with_id(generational);
  ASSERT_NE(other, nullptr);
  EXPECT_EQ(other->get_id(), generational);
  EXPECT_EQ(world.get_entity(generational), other);
}

TEST(EntityHandleTest, CreateWithIdRejectsNullEntity) {
  World world;
  EXPECT_EQ(world.create_entity_with_id(NULL_ENTITY), nullptr);
}

TEST(ComponentIndexTest, TracksMembershipAcrossAddAndRemove) {
  World world;

  auto* a = world.create_entity();
  auto* b = world.create_entity();
  auto* c = world.create_entity();

  a->add_component<TransformComponent>();
  b->add_component<TransformComponent>();
  c->add_component<UnitComponent>();

  EXPECT_EQ(world.get_entities_with<TransformComponent>().size(), 2U);
  EXPECT_EQ(world.get_entities_with<UnitComponent>().size(), 1U);

  b->remove_component<TransformComponent>();
  auto transforms = world.get_entities_with<TransformComponent>();
  ASSERT_EQ(transforms.size(), 1U);
  EXPECT_EQ(transforms.front(), a);
}

TEST(ComponentIndexTest, DestroyingAnEntityDropsItFromEveryIndex) {
  World world;

  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>();
  entity->add_component<UnitComponent>();
  const EntityID id = entity->get_id();

  world.destroy_entity(id);

  EXPECT_TRUE(world.get_entities_with<TransformComponent>().empty());
  EXPECT_TRUE(world.get_entities_with<UnitComponent>().empty());
  EXPECT_EQ(world.entity_count(), 0U);
}

TEST(ComponentIndexTest, SwapRemoveKeepsEveryOtherMemberReachable) {

  World world;
  std::vector<Entity*> entities;
  for (int i = 0; i < 8; ++i) {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>();
    entities.push_back(entity);
  }

  entities[3]->remove_component<TransformComponent>();
  entities[0]->remove_component<TransformComponent>();

  auto remaining = world.get_entities_with<TransformComponent>();
  EXPECT_EQ(remaining.size(), 6U);
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const bool expected = i != 0 && i != 3;
    const bool found =
        std::find(remaining.begin(), remaining.end(), entities[i]) != remaining.end();
    EXPECT_EQ(found, expected) << "entity " << i;
  }
}

TEST(ComponentIndexTest, RecycledSlotsDoNotInheritTheOldEntitysComponents) {
  World world;

  auto* first = world.create_entity();
  first->add_component<UnitComponent>();
  const EntityID stale = first->get_id();
  world.destroy_entity(stale);

  auto* second = world.create_entity();
  ASSERT_EQ(Handle::index_of(second->get_id()), Handle::index_of(stale));
  second->add_component<TransformComponent>();

  EXPECT_TRUE(world.get_entities_with<UnitComponent>().empty());
  auto transforms = world.get_entities_with<TransformComponent>();
  ASSERT_EQ(transforms.size(), 1U);
  EXPECT_EQ(transforms.front(), second);
}

TEST(ComponentIndexTest, ReplacingAnIdClearsTheOldOccupantsIndexEntries) {
  World world;

  auto* original = world.create_entity_with_id(7);
  ASSERT_NE(original, nullptr);
  original->add_component<UnitComponent>();
  ASSERT_EQ(world.get_entities_with<UnitComponent>().size(), 1U);

  auto* replacement = world.create_entity_with_id(7);
  ASSERT_NE(replacement, nullptr);
  replacement->add_component<TransformComponent>();

  EXPECT_TRUE(world.get_entities_with<UnitComponent>().empty());
  EXPECT_EQ(world.get_entities_with<TransformComponent>().size(), 1U);
  EXPECT_EQ(world.entity_count(), 1U);
}

struct PoolProbeComponent : Engine::Core::Component {
  float value = 0.0F;
};

TEST(ComponentStorageTest, ComponentsOfOneTypeAreContiguousInMemory) {

  World world;
  std::vector<const PoolProbeComponent*> probes;
  for (int i = 0; i < 64; ++i) {
    probes.push_back(world.create_entity()->add_component<PoolProbeComponent>());
  }

  const auto stride = static_cast<std::ptrdiff_t>(sizeof(PoolProbeComponent));
  for (std::size_t i = 1; i < probes.size(); ++i) {
    const auto* previous = reinterpret_cast<const std::byte*>(probes[i - 1]);
    const auto* current = reinterpret_cast<const std::byte*>(probes[i]);
    EXPECT_EQ(current - previous, stride) << "component " << i;
  }
}

TEST(ComponentStorageTest, FreedSlotsAreReusedRatherThanGrowingForever) {
  World world;

  auto* first = world.create_entity();
  const auto* original = first->add_component<PoolProbeComponent>();
  world.destroy_entity(first->get_id());

  auto* second = world.create_entity();
  const auto* recycled = second->add_component<PoolProbeComponent>();

  EXPECT_EQ(original, recycled);
}

TEST(WorldIterationTest, ForEachVisitsExactlyTheLiveEntities) {
  World world;
  auto* keep = world.create_entity();
  auto* drop = world.create_entity();
  world.destroy_entity(drop->get_id());

  std::vector<EntityID> visited;
  world.for_each_entity(
      [&visited](Entity& entity) { visited.push_back(entity.get_id()); });

  ASSERT_EQ(visited.size(), 1U);
  EXPECT_EQ(visited.front(), keep->get_id());
  EXPECT_EQ(world.entity_count(), 1U);
}

} // namespace

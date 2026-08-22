#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/registry.h"
#include "game/core/world.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::Registry;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;

TEST(RegistryTest, ComponentsAreReachedFromTheIdWithoutAnEntityObject) {
  Registry registry;
  const EntityID id = registry.create_entity();

  EXPECT_EQ(registry.try_get<UnitComponent>(id), nullptr);
  EXPECT_FALSE(registry.has<UnitComponent>(id));

  auto* unit = registry.emplace<UnitComponent>(id, 90, 120, 1.0F, 8.0F);
  ASSERT_NE(unit, nullptr);
  EXPECT_TRUE(registry.has<UnitComponent>(id));
  EXPECT_EQ(registry.try_get<UnitComponent>(id), unit);
  EXPECT_EQ(unit->health, 90);

  EXPECT_TRUE(registry.remove<UnitComponent>(id));
  EXPECT_FALSE(registry.has<UnitComponent>(id));
  EXPECT_FALSE(registry.remove<UnitComponent>(id));
}

TEST(RegistryTest, DestroyingAnEntityDropsItsComponents) {
  Registry registry;
  const EntityID id = registry.create_entity();
  registry.emplace<TransformComponent>(id);
  registry.emplace<UnitComponent>(id);
  ASSERT_EQ(registry.entities_with<TransformComponent>().size(), 1U);

  EXPECT_TRUE(registry.destroy_entity(id));
  EXPECT_TRUE(registry.entities_with<TransformComponent>().empty());
  EXPECT_TRUE(registry.entities_with<UnitComponent>().empty());
  EXPECT_EQ(registry.try_get<TransformComponent>(id), nullptr);
}

TEST(RegistryTest, EmplaceOnADeadHandleIsRefused) {
  Registry registry;
  const EntityID id = registry.create_entity();
  registry.destroy_entity(id);

  EXPECT_EQ(registry.emplace<UnitComponent>(id), nullptr);
  EXPECT_FALSE(registry.has<UnitComponent>(id));
}

TEST(RegistryTest, EmplacingTwiceReplacesTheComponentInPlace) {
  Registry registry;
  const EntityID id = registry.create_entity();

  auto* first = registry.emplace<UnitComponent>(id, 10, 10, 1.0F, 1.0F);
  auto* second = registry.emplace<UnitComponent>(id, 42, 50, 2.0F, 3.0F);

  EXPECT_EQ(first, second);
  EXPECT_EQ(second->health, 42);
  EXPECT_EQ(registry.entities_with<UnitComponent>().size(), 1U);
}

TEST(ComponentStorageTest, OneEntitysComponentSurvivesAnothersRemoval) {
  World world;
  std::vector<EntityID> ids;
  for (int i = 0; i < 8; ++i) {
    auto* entity = world.create_entity();
    entity->add_component<UnitComponent>(100 + i, 200, 1.0F, 1.0F);
    ids.push_back(entity->get_id());
  }

  auto* survivor = world.try_get<UnitComponent>(ids.back());
  ASSERT_NE(survivor, nullptr);

  world.destroy_entity(ids.front());
  world.remove<UnitComponent>(ids[2]);

  EXPECT_EQ(world.try_get<UnitComponent>(ids.back()), survivor);
  EXPECT_EQ(survivor->health, 107);
}

TEST(ComponentStorageTest, ComponentPointersSurviveLaterInserts) {
  World world;
  auto* first = world.create_entity();
  auto* transform = first->add_component<TransformComponent>(1.0F, 2.0F, 3.0F);

  for (int i = 0; i < 512; ++i) {
    world.create_entity()->add_component<TransformComponent>();
  }

  EXPECT_EQ(world.try_get<TransformComponent>(first->get_id()), transform);
  EXPECT_FLOAT_EQ(transform->position.z, 3.0F);
}

TEST(WorldViewTest, AddingComponentsOfTheViewedTypeDuringIterationIsSafe) {
  World world;
  std::vector<EntityID> seeds;
  for (int i = 0; i < 4; ++i) {
    auto* entity = world.create_entity();
    entity->add_component<MovementComponent>();
    seeds.push_back(entity->get_id());
  }

  std::vector<EntityID> visited;
  for (auto [id, movement] : world.view<MovementComponent>()) {
    (void)movement;
    visited.push_back(id);
    world.create_entity()->add_component<MovementComponent>();
  }

  EXPECT_EQ(visited, seeds);
  EXPECT_EQ(world.entities_with<MovementComponent>().size(), 8U);
}

TEST(WorldViewTest, IteratesTheSmallestPoolAndSkipsIncompleteEntities) {
  World world;
  for (int i = 0; i < 16; ++i) {
    world.create_entity()->add_component<TransformComponent>();
  }
  auto* matching = world.create_entity();
  matching->add_component<TransformComponent>();
  matching->add_component<AttackComponent>();

  auto view = world.view<TransformComponent, AttackComponent>();
  EXPECT_EQ(view.candidate_count(), 1U);

  std::vector<EntityID> visited;
  for (auto [id, transform, attack] : view) {
    (void)transform;
    (void)attack;
    visited.push_back(id);
  }
  ASSERT_EQ(visited.size(), 1U);
  EXPECT_EQ(visited.front(), matching->get_id());
}

TEST(WorldViewTest, MissingStorageYieldsAnEmptyView) {
  World world;
  world.create_entity()->add_component<TransformComponent>();

  auto view = world.view<TransformComponent, AttackComponent>();
  EXPECT_TRUE(view.empty());
  EXPECT_EQ(view.candidate_count(), 0U);
}

} // namespace

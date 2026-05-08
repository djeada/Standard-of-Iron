#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "render/persistent_render_registry.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto has_id(const std::vector<std::uint32_t> &vec,
            std::uint32_t id) -> bool {
  return std::find(vec.begin(), vec.end(), id) != vec.end();
}

// ---------------------------------------------------------------------------
// Basic population
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, EmptyWorldHasNoEntries) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  EXPECT_TRUE(reg.unit_ids().empty());
  EXPECT_TRUE(reg.building_ids().empty());
  EXPECT_TRUE(reg.other_ids().empty());
}

TEST(PersistentRenderRegistry, UnitEntityClassifiedAsUnit) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);

  EXPECT_TRUE(has_id(reg.unit_ids(), entity->get_id()));
  EXPECT_FALSE(has_id(reg.building_ids(), entity->get_id()));
  EXPECT_FALSE(has_id(reg.other_ids(), entity->get_id()));
}

TEST(PersistentRenderRegistry, BuildingEntityClassifiedAsBuilding) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  entity->add_component<Engine::Core::BuildingComponent>();

  EXPECT_FALSE(has_id(reg.unit_ids(), entity->get_id()));
  EXPECT_TRUE(has_id(reg.building_ids(), entity->get_id()));
  EXPECT_FALSE(has_id(reg.other_ids(), entity->get_id()));
}

TEST(PersistentRenderRegistry, PlainRenderableClassifiedAsOther) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");

  EXPECT_FALSE(has_id(reg.unit_ids(), entity->get_id()));
  EXPECT_FALSE(has_id(reg.building_ids(), entity->get_id()));
  EXPECT_TRUE(has_id(reg.other_ids(), entity->get_id()));
}

// ---------------------------------------------------------------------------
// Non-renderable entities are ignored
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, NonRenderableEntityNotTracked) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);

  EXPECT_TRUE(reg.unit_ids().empty());
  EXPECT_TRUE(reg.building_ids().empty());
  EXPECT_TRUE(reg.other_ids().empty());
}

// ---------------------------------------------------------------------------
// Entity destruction
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, DestroyedEntityRemovedFromRegistry) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);
  const auto id = entity->get_id();

  EXPECT_TRUE(has_id(reg.unit_ids(), id));

  world.destroy_entity(id);

  EXPECT_FALSE(has_id(reg.unit_ids(), id));
}

// ---------------------------------------------------------------------------
// World clear
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, WorldClearEmptiesRegistry) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *e1 = world.create_entity();
  e1->add_component<Engine::Core::RenderableComponent>("", "");
  e1->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);

  auto *e2 = world.create_entity();
  e2->add_component<Engine::Core::RenderableComponent>("", "");
  e2->add_component<Engine::Core::BuildingComponent>();

  EXPECT_EQ(reg.unit_ids().size(), 1U);
  EXPECT_EQ(reg.building_ids().size(), 1U);

  world.clear();

  EXPECT_TRUE(reg.unit_ids().empty());
  EXPECT_TRUE(reg.building_ids().empty());
  EXPECT_TRUE(reg.other_ids().empty());
}

// ---------------------------------------------------------------------------
// Reclassification when components change
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, AddingUnitComponentMovesToUnitList) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  const auto id = entity->get_id();

  EXPECT_TRUE(has_id(reg.other_ids(), id));

  entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);

  EXPECT_TRUE(has_id(reg.unit_ids(), id));
  EXPECT_FALSE(has_id(reg.other_ids(), id));
}

TEST(PersistentRenderRegistry, AddingBuildingComponentMovesToBuildingList) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  const auto id = entity->get_id();

  EXPECT_TRUE(has_id(reg.other_ids(), id));

  entity->add_component<Engine::Core::BuildingComponent>();

  EXPECT_TRUE(has_id(reg.building_ids(), id));
  EXPECT_FALSE(has_id(reg.other_ids(), id));
}

// ---------------------------------------------------------------------------
// Attach to existing world picks up existing entities
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, AttachPicksUpExistingEntities) {
  Engine::Core::World world;

  auto *e1 = world.create_entity();
  e1->add_component<Engine::Core::RenderableComponent>("", "");
  e1->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);

  auto *e2 = world.create_entity();
  e2->add_component<Engine::Core::RenderableComponent>("", "");
  e2->add_component<Engine::Core::BuildingComponent>();

  auto *e3 = world.create_entity();
  e3->add_component<Engine::Core::RenderableComponent>("", "");

  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  EXPECT_EQ(reg.unit_ids().size(), 1U);
  EXPECT_EQ(reg.building_ids().size(), 1U);
  EXPECT_EQ(reg.unit_ids().size() + reg.building_ids().size() +
                reg.other_ids().size(),
            3U);
}

// ---------------------------------------------------------------------------
// Detach / is_attached_to
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, DetachClearsState) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");

  EXPECT_EQ(reg.other_ids().size(), 1U);

  reg.detach();

  EXPECT_TRUE(reg.other_ids().empty());
  EXPECT_FALSE(reg.is_attached_to(&world));
}

TEST(PersistentRenderRegistry, IsAttachedToReturnsTrueForAttachedWorld) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;

  EXPECT_FALSE(reg.is_attached_to(&world));
  reg.attach(&world);
  EXPECT_TRUE(reg.is_attached_to(&world));
}

TEST(PersistentRenderRegistry, IsAttachedToReturnsFalseForDifferentWorld) {
  Engine::Core::World world1;
  Engine::Core::World world2;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world1);

  EXPECT_TRUE(reg.is_attached_to(&world1));
  EXPECT_FALSE(reg.is_attached_to(&world2));
}

// ---------------------------------------------------------------------------
// No duplicate IDs on re-add
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, ReaddingRenderableDoesNotDuplicate) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  const auto id = entity->get_id();

  EXPECT_EQ(reg.other_ids().size(), 1U);

  // Simulate adding RenderableComponent again (e.g., world re-scan).
  // In practice add_component overwrites, firing the callback once.
  entity->add_component<Engine::Core::RenderableComponent>("new", "new");

  EXPECT_EQ(reg.other_ids().size(), 1U);
  EXPECT_TRUE(has_id(reg.other_ids(), id));
}

// ---------------------------------------------------------------------------
// UnitComponent entities with BuildingComponent stay in unit list
// ---------------------------------------------------------------------------

TEST(PersistentRenderRegistry, EntityWithBothUnitAndBuildingClassifiedAsUnit) {
  Engine::Core::World world;
  Render::PersistentRenderRegistry reg;
  reg.attach(&world);

  auto *entity = world.create_entity();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);
  entity->add_component<Engine::Core::BuildingComponent>();
  const auto id = entity->get_id();

  // UnitComponent takes priority (same as the original render_world logic).
  EXPECT_TRUE(has_id(reg.unit_ids(), id));
  EXPECT_FALSE(has_id(reg.building_ids(), id));
  EXPECT_FALSE(has_id(reg.other_ids(), id));
}

} // namespace

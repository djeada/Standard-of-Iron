#include <QJsonDocument>

#include <algorithm>
#include <gtest/gtest.h>
#include <ranges>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/save/serialization.h"
#include "game/session/session_context.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::BuildingComponent;
using Engine::Core::EntityID;
using Engine::Core::RenderableComponent;
using Engine::Core::Serialization;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

auto spawn_soldier(World& world, float x, float z) -> EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->owner_id = 1;
  unit->health = 100;
  unit->max_health = 100;
  entity->add_component<RenderableComponent>()->renderer_id = "archer";
  return entity->get_id();
}

auto spawn_barracks(World& world, float x, float z) -> EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->owner_id = 1;
  unit->health = 500;
  unit->max_health = 500;
  entity->add_component<BuildingComponent>();
  entity->add_component<RenderableComponent>()->renderer_id = "barracks";
  return entity->get_id();
}

auto published_unit_ids(World& world) -> std::vector<EntityID> {
  const auto snapshot = world.acquire_render_snapshot();
  if (snapshot == nullptr) {
    return {};
  }
  const auto ids = snapshot->render_unit_ids();
  std::vector<EntityID> sorted(ids.begin(), ids.end());
  std::ranges::sort(sorted);
  return sorted;
}

auto load_over(World& world, const QJsonDocument& save) -> void {
  world.clear();
  Serialization::deserialize_world(&world, save);
}

TEST(SaveLoadRenderSnapshotTest, LoadedEntitiesArePublishedToTheRenderSnapshot) {
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  const EntityID soldier = spawn_soldier(world, 4.0F, 4.0F);
  const EntityID barracks = spawn_barracks(world, 10.0F, 10.0F);
  world.update(1.0F / 60.0F);

  const auto before = published_unit_ids(world);
  ASSERT_EQ(before.size(), 2U);

  load_over(world, Serialization::serialize_world(&world));
  world.update(1.0F / 60.0F);

  EXPECT_EQ(published_unit_ids(world), before)
      << "a loaded battlefield must reach the renderer";

  const auto snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);
  for (const EntityID id : {soldier, barracks}) {
    auto* copied = snapshot->get_entity(id);
    ASSERT_NE(copied, nullptr) << "entity " << id << " missing from the snapshot";
    EXPECT_NE(copied->get_component<RenderableComponent>(), nullptr);
    EXPECT_NE(copied->get_component<TransformComponent>(), nullptr);
  }
}

TEST(SaveLoadRenderSnapshotTest, TheFirstTickAfterALoadShowsTheLoadedBattlefield) {
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  spawn_soldier(world, 4.0F, 4.0F);
  world.update(1.0F / 60.0F);
  ASSERT_EQ(published_unit_ids(world).size(), 1U);

  SessionContext saved_session;
  QJsonDocument save;
  {
    const ScopedSession saved_scope(saved_session);
    World& saved_world = saved_session.world();
    spawn_soldier(saved_world, 20.0F, 20.0F);
    spawn_soldier(saved_world, 21.0F, 21.0F);
    spawn_barracks(saved_world, 25.0F, 25.0F);
    save = Serialization::serialize_world(&saved_world);
  }

  load_over(world, save);
  world.update(1.0F / 60.0F);

  EXPECT_EQ(published_unit_ids(world).size(), 3U)
      << "one tick is all a restored match gets before it is drawn";
}

TEST(SaveLoadRenderSnapshotTest, AnEmptiedWorldStopsRenderingTheOldBattlefield) {
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  spawn_soldier(world, 4.0F, 4.0F);
  spawn_barracks(world, 10.0F, 10.0F);
  world.update(1.0F / 60.0F);
  ASSERT_EQ(published_unit_ids(world).size(), 2U);

  world.clear();
  world.update(1.0F / 60.0F);

  EXPECT_TRUE(published_unit_ids(world).empty())
      << "an abandoned match must not linger on screen";
}

TEST(SaveLoadRenderSnapshotTest, NothingIsReusedFromBeforeTheWorldWasWiped) {
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  world.set_presentation_enabled(false);

  const EntityID id = spawn_soldier(world, 7.0F, 7.0F);
  for (int tick = 0; tick < 8; ++tick) {
    world.update(1.0F / 60.0F);
  }
  ASSERT_GT(world.render_publication_stats().entities_reused, 0U)
      << "a still entity is normally served from its cached signature";

  const auto reused_before = world.render_publication_stats().entities_reused;
  world.clear();
  auto* restored = world.create_entity_with_id(id);
  ASSERT_NE(restored, nullptr) << "the save restores entities under their saved ids";
  auto* transform = restored->add_component<TransformComponent>();
  transform->position.x = 7.0F;
  transform->position.z = 7.0F;
  auto* unit = restored->add_component<UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Archer;
  unit->owner_id = 1;
  unit->health = 100;
  unit->max_health = 100;
  restored->add_component<RenderableComponent>()->renderer_id = "archer";

  world.update(1.0F / 60.0F);

  EXPECT_EQ(world.render_publication_stats().entities_reused, reused_before)
      << "the signature cached for this slot describes an entity that is gone";
  ASSERT_NE(world.acquire_render_snapshot()->get_entity(id), nullptr);
}

TEST(SaveLoadRenderSnapshotTest, AnUnchangedWorldIsNotRepublishedEveryFrame) {
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  spawn_soldier(world, 4.0F, 4.0F);
  world.update(1.0F / 60.0F);

  const auto published = world.acquire_render_snapshot();
  const auto publications = world.render_publication_stats().publications;

  for (int frame = 0; frame < 10; ++frame) {
    world.ensure_render_snapshot();
  }

  EXPECT_EQ(world.acquire_render_snapshot(), published);
  EXPECT_EQ(world.render_publication_stats().publications, publications)
      << "a still world must not pay for a snapshot it does not need";
}

} // namespace

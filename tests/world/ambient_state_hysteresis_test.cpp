#include <QString>

#include <gtest/gtest.h>

#include "app/core/entity_cache.h"
#include "app/world/ambient_state_manager.h"
#include "game/core/component_core.h"
#include "game/core/world.h"

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;

auto add_unit(Engine::Core::World& world,
              int owner_id,
              float x) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  return entity;
}

void advance(AmbientStateManager& manager,
             Engine::Core::World& world,
             const EntityCache& cache,
             int samples) {
  for (int i = 0; i < samples; ++i) {
    manager.update(2.0F, &world, k_local_owner, cache, QString{});
  }
}

} // namespace

TEST(AmbientStateHysteresisTest, ContactRaisesCombatOnTheNextSample) {
  Engine::Core::World world;
  EntityCache cache;
  cache.player_barracks_alive = true;
  cache.enemy_barracks_alive = true;
  add_unit(world, k_local_owner, 0.0F);
  add_unit(world, k_enemy_owner, 5.0F);

  AmbientStateManager manager;
  advance(manager, world, cache, 1);

  EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT)
      << "battle joined should reach the score immediately";
}

TEST(AmbientStateHysteresisTest, ASkirmisherDriftingOutOfRangeDoesNotDropCombat) {
  Engine::Core::World world;
  EntityCache cache;
  cache.player_barracks_alive = true;
  cache.enemy_barracks_alive = true;
  add_unit(world, k_local_owner, 0.0F);
  auto* enemy = add_unit(world, k_enemy_owner, 5.0F);

  AmbientStateManager manager;
  advance(manager, world, cache, 1);
  ASSERT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT);

  auto* enemy_transform = enemy->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_transform, nullptr);

  for (int cycle = 0; cycle < 2; ++cycle) {
    enemy_transform->position.x = 40.0F;
    advance(manager, world, cache, 2);
    EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT)
        << "four seconds of quiet must not restart the score";

    enemy_transform->position.x = 5.0F;
    advance(manager, world, cache, 1);
    EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT);
  }
}

TEST(AmbientStateHysteresisTest, SustainedQuietEventuallyReleasesCombat) {
  Engine::Core::World world;
  EntityCache cache;
  cache.player_barracks_alive = true;
  cache.enemy_barracks_alive = true;
  add_unit(world, k_local_owner, 0.0F);
  auto* enemy = add_unit(world, k_enemy_owner, 5.0F);

  AmbientStateManager manager;
  advance(manager, world, cache, 1);
  ASSERT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT);

  enemy->get_component<Engine::Core::TransformComponent>()->position.x = 40.0F;
  advance(manager, world, cache, 8);

  EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::TENSE)
      << "the battle is genuinely over once the quiet holds";
}

TEST(AmbientStateHysteresisTest, DefeatIsNeverHeldBackByTheSettle) {
  Engine::Core::World world;
  EntityCache cache;
  cache.player_barracks_alive = true;
  cache.enemy_barracks_alive = true;

  AmbientStateManager manager;
  manager.update(2.0F, &world, k_local_owner, cache, QStringLiteral("defeat"));

  EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::DEFEAT);
}

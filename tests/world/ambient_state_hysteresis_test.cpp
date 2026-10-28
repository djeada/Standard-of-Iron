#include <QString>

#include <gtest/gtest.h>

#include "app/core/entity_cache.h"
#include "app/world/ambient_state_manager.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
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
  unit->spawn_type = Game::Units::SpawnType::Swordsman;
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
  advance(manager, world, cache, 12);

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

namespace {

struct Battlefield {
  Engine::Core::World world;
  EntityCache cache;
  Engine::Core::TransformComponent* enemy = nullptr;
  AmbientStateManager manager;
  int changes = 0;

  Battlefield() {
    cache.player_barracks_alive = true;
    cache.enemy_barracks_alive = true;
    add_unit(world, k_local_owner, 0.0F);
    enemy = add_unit(world, k_enemy_owner, 40.0F)
                ->get_component<Engine::Core::TransformComponent>();
  }

  void sample(bool contact, int samples = 1) {
    enemy->position.x = contact ? 5.0F : 40.0F;
    for (int i = 0; i < samples; ++i) {
      const auto before = manager.current_state();
      manager.update(2.0F, &world, k_local_owner, cache, QString{});
      if (manager.current_state() != before) {
        ++changes;
      }
    }
  }

  [[nodiscard]] auto state() const -> Engine::Core::AmbientState {
    return manager.current_state();
  }
};

} // namespace

TEST(AmbientStateHysteresisTest, StrayContactsAfterABattleDoNotFlapTheScore) {
  Battlefield field;
  field.sample(true);
  ASSERT_EQ(field.state(), Engine::Core::AmbientState::COMBAT);

  for (int minute = 0; minute < 9; ++minute) {
    for (int burst = 0; burst < 3; ++burst) {
      field.sample(false, 10);
      field.sample(true);
    }
  }

  EXPECT_LE(field.changes, 2)
      << "a lone contact every twenty seconds must not swap the music each time";
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::TENSE);
}

TEST(AmbientStateHysteresisTest, ARenewedFightSoonAfterReturnsOnceItIsSustained) {
  Battlefield field;
  field.sample(true);
  field.sample(false, 11);
  ASSERT_EQ(field.state(), Engine::Core::AmbientState::TENSE);

  field.sample(true, 2);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::TENSE)
      << "a re-entry right after a battle waits for the fight to hold";
  field.sample(true);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::COMBAT);
}

TEST(AmbientStateHysteresisTest, ALongBattleRidesOutALull) {
  Battlefield field;
  field.sample(true, 60);
  ASSERT_EQ(field.state(), Engine::Core::AmbientState::COMBAT);

  field.sample(false, 15);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::COMBAT)
      << "thirty quiet seconds in a two-minute battle is a lull, not the end";
  field.sample(true);
  field.sample(false, 15);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::COMBAT);

  field.sample(false, 10);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::TENSE)
      << "the release is longer, not endless";
  EXPECT_EQ(field.changes, 2);
}

TEST(AmbientStateHysteresisTest, AFightLongAfterTheLastStartsTheScoreAtOnce) {
  Battlefield field;
  field.sample(true);
  field.sample(false, 11);
  ASSERT_EQ(field.state(), Engine::Core::AmbientState::TENSE);

  field.sample(false, 31);
  field.sample(true);
  EXPECT_EQ(field.state(), Engine::Core::AmbientState::COMBAT);
}

TEST(AmbientStateHysteresisTest, AWaveMarchingOnThePlayerKeepsTheBattleScore) {
  Engine::Core::World world;
  EntityCache cache;
  add_unit(world, k_local_owner, 0.0F);
  auto* raider = add_unit(world, k_enemy_owner, 5.0F);

  AmbientStateManager manager;
  advance(manager, world, cache, 1);
  ASSERT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT);

  auto* wave = add_unit(world, k_enemy_owner, 90.0F);
  wave->add_component<Engine::Core::AssaultWaveComponent>();
  raider->get_component<Engine::Core::UnitComponent>()->health = 0;
  advance(manager, world, cache, 30);
  EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::COMBAT)
      << "the next wave is still on its way, so the battle is not over";

  wave->get_component<Engine::Core::UnitComponent>()->health = 0;
  advance(manager, world, cache, 30);
  EXPECT_EQ(manager.current_state(), Engine::Core::AmbientState::PEACEFUL)
      << "once the wave is dead the score releases as before";
}

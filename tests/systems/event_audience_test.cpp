#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "core/component_combat.h"
#include "core/event_manager.h"
#include "core/local_audience.h"
#include "core/world.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_system/damage_application.h"
#include "units/spawn_type.h"

namespace {

auto add_unit(Engine::Core::World& world,
              int owner_id,
              Game::Units::SpawnType type,
              float x) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform =
      entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
  transform->scale = {0.55F, 0.55F, 0.55F};
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  unit->spawn_type = type;
  auto* attack = entity->add_component<Engine::Core::AttackComponent>();
  attack->can_melee = true;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  return entity;
}

} // namespace

TEST(LocalAudienceTest, OneSidedEventsReachTheirOwnerAndTheUnowned) {
  const Engine::Core::LocalAudience audience(1);

  EXPECT_TRUE(audience.includes(1));
  EXPECT_FALSE(audience.includes(2));

  EXPECT_TRUE(audience.includes(Engine::Core::k_owner_everyone))
      << "UI chrome and world ambience belong to nobody, so everyone gets them";
  EXPECT_TRUE(audience.includes(Game::Core::NEUTRAL_OWNER_ID))
      << "neutral subjects belong to the world, not to a rival player";
}

TEST(LocalAudienceTest, TwoSidedEventsNeedTheLocalPlayerOnOneEnd) {
  const Engine::Core::LocalAudience audience(1);

  EXPECT_TRUE(audience.involves(1, 2));
  EXPECT_TRUE(audience.involves(2, 1));
  EXPECT_FALSE(audience.involves(2, 3))
      << "two AI armies trading blows are not this player's business";
  EXPECT_TRUE(audience.involves(1, Game::Core::NEUTRAL_OWNER_ID));
}

TEST(LocalAudienceTest, OwnSideAlertsRejectNeutralAndUnownedSubjects) {
  const Engine::Core::LocalAudience audience(1);

  EXPECT_TRUE(audience.is_local(1));
  EXPECT_FALSE(audience.is_local(2));
  EXPECT_FALSE(audience.is_local(Engine::Core::k_owner_everyone));
  EXPECT_FALSE(audience.is_local(Game::Core::NEUTRAL_OWNER_ID));
}

TEST(LocalAudienceTest, ASpectatorIsFilteredOutOfNothing) {
  const Engine::Core::LocalAudience spectator;

  EXPECT_TRUE(spectator.is_spectating());
  EXPECT_TRUE(spectator.includes(2));
  EXPECT_TRUE(spectator.involves(2, 3));
}

TEST(CombatEventAttributionTest, CombatHitsNameBothSidesOfTheExchange) {
  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Engine::Core::World world;
  auto* attacker = add_unit(world, 2, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_unit(world, 3, Game::Units::SpawnType::Spearman, 2.0F);

  std::vector<Engine::Core::CombatHitEvent> hits;
  const Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent> hit_sub(
      [&hits](const Engine::Core::CombatHitEvent& event) { hits.push_back(event); });

  Game::Systems::Combat::apply_unit_damage(&world, target, 10, attacker->get_id());

  ASSERT_EQ(hits.size(), 1U);
  EXPECT_EQ(hits.front().attacker_owner_id, 2);
  EXPECT_EQ(hits.front().target_owner_id, 3);

  const Engine::Core::LocalAudience player_one(1);
  EXPECT_FALSE(
      player_one.involves(hits.front().attacker_owner_id, hits.front().target_owner_id))
      << "player 1 must not hear or see two rivals fighting each other";
  Game::Systems::BuildingCollisionRegistry::instance().clear();
}

TEST(CombatEventAttributionTest, DeathsNameTheOwnerAndTheKiller) {
  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Engine::Core::World world;
  auto* attacker = add_unit(world, 2, Game::Units::SpawnType::Knight, 0.0F);
  auto* target = add_unit(world, 3, Game::Units::SpawnType::Spearman, 2.0F);

  std::vector<Engine::Core::UnitDiedEvent> deaths;
  const Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent> death_sub(
      [&deaths](const Engine::Core::UnitDiedEvent& event) { deaths.push_back(event); });

  Game::Systems::Combat::apply_unit_damage(&world, target, 10000, attacker->get_id());

  ASSERT_EQ(deaths.size(), 1U);
  EXPECT_EQ(deaths.front().owner_id, 3);
  EXPECT_EQ(deaths.front().killer_owner_id, 2);
  Game::Systems::BuildingCollisionRegistry::instance().clear();
}

TEST(AudioCueAttributionTest, CuesDefaultToEveryoneAndCarryTheirOwnerWhenGiven) {
  const Engine::Core::AudioCueEvent chrome("ui.click");
  EXPECT_EQ(chrome.owner_id, Engine::Core::k_owner_everyone);

  const auto owned = Engine::Core::AudioCueEvent::for_owner(2, "build.unit_ready");
  EXPECT_EQ(owned.owner_id, 2);
  EXPECT_EQ(owned.cue_id, std::string("build.unit_ready"));

  const Engine::Core::LocalAudience player_one(1);
  EXPECT_TRUE(player_one.includes(chrome.owner_id));
  EXPECT_FALSE(player_one.includes(owned.owner_id))
      << "a rival finishing a unit must stay silent on this client";
}

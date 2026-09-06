#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <set>

#include "core/component_combat.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/melee_exchange.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "units/factory.h"
#include "units/unit.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::HitFeedbackComponent;
using Engine::Core::HitReactionKind;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Systems::Combat::k_melee_exchange_beats;
using Game::Systems::Combat::melee_exchange_damage;
using Game::Systems::Combat::MeleeExchangeBeat;
using Game::Systems::Combat::MeleeExchangeOutcome;
using Game::Systems::Combat::resolve_melee_exchange_beat;

TEST(MeleeExchange, EveryEightBeatCycleIsDamageAndCadenceNeutral) {
  for (Engine::Core::EntityID attacker = 1; attacker < 40; attacker += 3) {
    for (Engine::Core::EntityID target = 2; target < 40; target += 5) {
      if (attacker == target) {
        continue;
      }
      for (std::uint8_t start = 0; start < 16; ++start) {
        float damage = 0.0F;
        float interval = 0.0F;
        float delay = 0.0F;
        for (std::uint32_t i = 0; i < k_melee_exchange_beats; ++i) {
          auto const beat = resolve_melee_exchange_beat(
              attacker, target, static_cast<std::uint8_t>(start + i), true);
          damage += beat.damage_multiplier;
          interval += beat.interval_weight;
          delay += beat.delay_weight;
        }
        EXPECT_NEAR(damage, 8.0F, 0.01F) << "attacker " << attacker << " target "
                                         << target << " start " << int(start);
        EXPECT_NEAR(interval, 8.0F, 0.01F);
        EXPECT_NEAR(delay, 8.0F, 0.01F);
      }
    }
  }
}

TEST(MeleeExchange, OutcomesCarryTheirReactions) {
  std::set<MeleeExchangeOutcome> seen;
  for (std::uint8_t i = 0; i < k_melee_exchange_beats; ++i) {
    auto const beat = resolve_melee_exchange_beat(7, 11, i, true);
    seen.insert(beat.outcome);
    switch (beat.outcome) {
    case MeleeExchangeOutcome::Clean:
      EXPECT_GT(beat.damage_multiplier, 1.0F);
      EXPECT_EQ(beat.target_reaction, HitReactionKind::Flinch);
      EXPECT_FALSE(beat.attacker_recoils);
      break;
    case MeleeExchangeOutcome::Heavy:
      EXPECT_GT(beat.damage_multiplier, 1.5F);
      EXPECT_EQ(beat.target_reaction, HitReactionKind::Stagger);
      break;
    case MeleeExchangeOutcome::Blocked:
      EXPECT_LT(beat.damage_multiplier, 0.5F);
      EXPECT_GT(beat.damage_multiplier, 0.0F);
      EXPECT_EQ(beat.target_reaction, HitReactionKind::Block);
      EXPECT_TRUE(beat.attacker_recoils);
      break;
    case MeleeExchangeOutcome::Evaded:
      EXPECT_FLOAT_EQ(beat.damage_multiplier, 0.0F);
      EXPECT_EQ(beat.target_reaction, HitReactionKind::Evade);
      EXPECT_EQ(melee_exchange_damage(26, beat), 0);
      break;
    case MeleeExchangeOutcome::Plain:
      ADD_FAILURE() << "a defended exchange never resolves to the plain blow";
      break;
    }
  }
  EXPECT_EQ(seen.size(), 4U) << "one cycle must show every kind of exchange";
}

TEST(MeleeExchange, DifferentPairsStartOnDifferentBeats) {
  std::set<MeleeExchangeOutcome> first_outcomes;
  for (Engine::Core::EntityID attacker = 1; attacker < 64; ++attacker) {
    first_outcomes.insert(resolve_melee_exchange_beat(attacker, 100, 0, true).outcome);
  }
  EXPECT_GE(first_outcomes.size(), 3U);
}

TEST(MeleeExchange, TargetsThatCannotDefendAlwaysTakeTheCleanBlow) {
  for (std::uint8_t i = 0; i < k_melee_exchange_beats; ++i) {
    auto const beat = resolve_melee_exchange_beat(3, 4, i, false);
    EXPECT_EQ(beat.outcome, MeleeExchangeOutcome::Plain);
    EXPECT_FLOAT_EQ(beat.damage_multiplier, 1.0F);
    EXPECT_FLOAT_EQ(beat.interval_weight, 1.0F);
    EXPECT_FLOAT_EQ(beat.delay_weight, 1.0F);
    EXPECT_EQ(melee_exchange_damage(26, beat), 26);
  }
}

class MeleeExchangeDuelTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Enemy");
    owners.set_owner_team(2, 2);
    owners.set_local_player_id(1);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = 48;
    map_definition.grid.height = 48;
    map_definition.grid.tile_size = 1.0F;
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::NavGrid::initialize(48, 48);

    registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto spawn(Engine::Core::World& world,
             Game::Units::SpawnType type,
             int owner_id,
             const QVector3D& position,
             Game::Systems::NationID nation) -> Engine::Core::Entity* {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.nation_id = nation;
    params.is_initial_spawn = true;
    auto unit = registry->create(type, world, params);
    if (!unit) {
      return nullptr;
    }
    auto* entity = world.get_entity(unit->id());
    if (entity != nullptr) {
      if (auto* unit_comp = entity->get_component<UnitComponent>()) {
        unit_comp->render_individuals_per_unit_override = 1;
      }
    }
    return entity;
  }

  static void order_attack(Engine::Core::Entity& attacker,
                           const Engine::Core::Entity& target) {
    auto* attack_target = attacker.add_component<Engine::Core::AttackTargetComponent>();
    attack_target->target_id = target.get_id();
    attack_target->should_chase = true;
  }

  static auto separation(const Engine::Core::Entity& first,
                         const Engine::Core::Entity& second) -> float {
    const auto* from = first.get_component<TransformComponent>();
    const auto* to = second.get_component<TransformComponent>();
    return std::hypot(to->position.x - from->position.x,
                      to->position.z - from->position.z);
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> registry;
};

TEST_F(MeleeExchangeDuelTest, ASwordDuelKeepsItsDamageRateAndShowsEveryReaction) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* blue = spawn(world,
                     Game::Units::SpawnType::Knight,
                     1,
                     QVector3D(-1.4F, 0.0F, 0.0F),
                     Game::Systems::NationID::RomanRepublic);
  auto* red = spawn(world,
                    Game::Units::SpawnType::Knight,
                    2,
                    QVector3D(1.4F, 0.0F, 0.0F),
                    Game::Systems::NationID::Carthage);
  ASSERT_NE(blue, nullptr);
  ASSERT_NE(red, nullptr);
  auto* blue_unit = blue->get_component<UnitComponent>();
  auto* red_unit = red->get_component<UnitComponent>();
  blue_unit->health = blue_unit->max_health = 100000;
  red_unit->health = red_unit->max_health = 100000;
  auto const* blue_attack = blue->get_component<AttackComponent>();
  ASSERT_NE(blue_attack, nullptr);

  order_attack(*blue, *red);
  order_attack(*red, *blue);

  constexpr float k_dt = 1.0F / 30.0F;
  constexpr int k_warmup_ticks = 60;
  constexpr int k_measured_ticks = 30 * 40;
  for (int tick = 0; tick < k_warmup_ticks; ++tick) {
    world.update(k_dt);
  }
  int const red_start = red_unit->health;

  std::set<HitReactionKind> red_reactions;
  float min_separation = 1000.0F;
  float max_separation = 0.0F;
  bool locked = false;
  for (int tick = 0; tick < k_measured_ticks; ++tick) {
    world.update(k_dt);
    if (auto const* feedback = red->get_component<HitFeedbackComponent>();
        feedback != nullptr && feedback->is_reacting) {
      red_reactions.insert(feedback->reaction_kind);
    }
    if (blue_attack->in_melee_lock) {
      locked = true;
      float const gap = separation(*blue, *red);
      min_separation = std::min(min_separation, gap);
      max_separation = std::max(max_separation, gap);
    }
  }
  ASSERT_TRUE(locked);

  float const measured_seconds = k_measured_ticks * k_dt;
  float const expected_swings = measured_seconds / blue_attack->melee_cooldown;
  float const expected_damage = expected_swings * blue_attack->melee_damage;
  float const dealt = static_cast<float>(red_start - red_unit->health);
  EXPECT_GT(dealt, expected_damage * 0.70F)
      << "exchange outcomes must not starve the damage rate";
  EXPECT_LT(dealt, expected_damage * 1.10F)
      << "exchange outcomes must not inflate the damage rate";

  EXPECT_TRUE(red_reactions.count(HitReactionKind::Block) > 0U);
  EXPECT_TRUE(red_reactions.count(HitReactionKind::Evade) > 0U);
  EXPECT_TRUE(red_reactions.count(HitReactionKind::Flinch) > 0U ||
              red_reactions.count(HitReactionKind::Stagger) > 0U);

  EXPECT_GE(min_separation, 0.5F) << "footwork walked the duellists into each other";
  EXPECT_LE(max_separation,
            blue_attack->melee_range + AttackComponent::k_melee_contact_range_grace)
      << "footwork carried the duellists out of reach";
  EXPECT_GT(max_separation - min_separation, 0.08F)
      << "a duel should breathe: the measure opens and closes";
}

} // namespace

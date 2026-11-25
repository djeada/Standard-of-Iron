// NATION_ASYMMETRY.md: Rome turns men into an army, Carthage turns gold into one.
// These tests pin the rules each phase adds, on a real session with the shipped
// nation data.

#include <QJsonObject>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "game/core/component_economy.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/save/serialization.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/attack_stat_modifiers.h"
#include "game/systems/default_content.h"
#include "game/systems/economy/nation_economy_system.h"
#include "game/systems/economy/nation_recruitment.h"
#include "game/systems/economy/production_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::NationID;
using Game::Systems::ProductionResult;
using Game::Systems::ResourceType;
using Game::Units::SpawnType;
using Game::Units::TroopType;

constexpr int k_map_size = 96;
constexpr int k_rome = 2;
constexpr int k_carthage = 3;

class NationAsymmetryTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_state();
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_rome, Game::Systems::OwnerType::AI, "rome");
    owners.register_owner_with_id(k_carthage, Game::Systems::OwnerType::AI, "carthage");
    owners.set_owner_team(k_rome, 1);
    owners.set_owner_team(k_carthage, 2);

    Game::Systems::initialize_default_content(m_session->nations());
    m_session->nations().set_player_nation(k_rome, NationID::RomanRepublic);
    m_session->nations().set_player_nation(k_carthage, NationID::Carthage);

    Game::Map::MapDefinition map;
    map.grid.width = k_map_size;
    map.grid.height = k_map_size;
    map.grid.tile_size = 1.0F;
    m_session->terrain().initialize(map);
    Game::Systems::register_runtime_systems(m_session->world());

    for (const int owner : {k_rome, k_carthage}) {
      auto& economy = m_session->economy();
      economy.ensure_owner(owner);
      for (const auto type : Game::Systems::k_all_resource_types) {
        economy.set(owner, type, 1000);
      }
    }
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_state();
  }

  static void reset_shared_state() {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Map::TerrainService::instance().clear();
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::FormationCombat::invalidate_layout_cache();
  }

  auto world() -> Engine::Core::World& { return m_session->world(); }

  auto spawn(SpawnType type,
             int owner,
             float x,
             float z,
             std::optional<NationID> nation = std::nullopt) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = Game::Systems::NavGrid::grid_to_world(
        Game::Systems::Point(static_cast<int>(x), static_cast<int>(z)));
    params.player_id = owner;
    params.spawn_type = type;
    params.ai_controlled = false;
    params.is_initial_spawn = true;
    params.max_population = 200;
    params.enables_production = true;
    params.nation_id =
        nation.value_or(m_session->nations().get_nation_for_player(owner)->id);
    auto unit = m_factory->create(type, world(), params);
    return unit ? unit->id() : 0;
  }

  auto unit_of(EntityID id) -> UnitComponent* {
    return world().try_get<UnitComponent>(id);
  }

  void run_for(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        world().update(static_cast<float>(step));
      }
    }
  }

  auto gold(int owner) -> int {
    return m_session->economy().get(owner, ResourceType::Gold);
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

// Phase 0 spike: a Carthaginian player's Gallic band keeps its owner but draws,
// fights and saves as a Gaul.
TEST_F(NationAsymmetryTest, AGallicBandOwnedByCarthageFightsAndSavesAsGauls) {
  const EntityID band =
      spawn(SpawnType::Swordsman, k_carthage, 40, 40, NationID::Gauls);
  ASSERT_NE(band, 0U);
  const auto* unit = unit_of(band);
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->owner_id, k_carthage);
  EXPECT_EQ(unit->nation_id, NationID::Gauls);

  const auto& gaul = Game::Systems::TroopProfileService::instance().get_profile_ref(
      NationID::Gauls, TroopType::Swordsman);
  const auto& punic = Game::Systems::TroopProfileService::instance().get_profile_ref(
      NationID::Carthage, TroopType::Swordsman);
  ASSERT_NE(gaul.combat.max_health, punic.combat.max_health)
      << "the spike only proves something if the two profiles differ";
  EXPECT_EQ(unit->max_health, gaul.combat.max_health);
  const auto* renderable = world().try_get<Engine::Core::RenderableComponent>(band);
  ASSERT_NE(renderable, nullptr);
  EXPECT_EQ(renderable->renderer_id, "troops/gauls/swordsman");

  const QJsonObject saved =
      Engine::Core::Serialization::serialize_entity(world().get_entity(band));
  Engine::Core::World restored_world;
  auto* restored = restored_world.create_entity();
  Engine::Core::Serialization::deserialize_entity(restored, saved);
  const auto* restored_unit = restored->get_component<UnitComponent>();
  ASSERT_NE(restored_unit, nullptr);
  EXPECT_EQ(restored_unit->owner_id, k_carthage);
  EXPECT_EQ(restored_unit->nation_id, NationID::Gauls);
}

// Phase 1: Rome hires its archers from Crete with gold and arms its hastati with
// less iron than the old legionary.
TEST_F(NationAsymmetryTest, RomanPricesFollowTheRepublicsRoster) {
  const auto cretans = Game::Systems::quote_recruit(world(), k_rome, TroopType::Archer);
  EXPECT_TRUE(cretans.offered);
  EXPECT_EQ(cretans.resources.get(ResourceType::Gold), 40);
  EXPECT_EQ(cretans.resources.get(ResourceType::Iron), 0);

  const auto hastati =
      Game::Systems::quote_recruit(world(), k_rome, TroopType::Swordsman);
  EXPECT_TRUE(hastati.offered);
  EXPECT_EQ(hastati.resources.get(ResourceType::Iron), 20);
  EXPECT_EQ(hastati.building, SpawnType::Barracks);
}

// Phase 1, decision 5: Rome no longer recruits horse archers, though authored
// spawns still field them; its equites are capped at four squads.
TEST_F(NationAsymmetryTest, RomeRecruitsNoHorseArchersAndFourEquitesAtMost) {
  const EntityID barracks = spawn(SpawnType::Barracks, k_rome, 30, 30);
  ASSERT_NE(barracks, 0U);
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::HorseArcher),
            ProductionResult::NotInRoster);
  EXPECT_NE(spawn(SpawnType::HorseArcher, k_rome, 50, 50), 0U)
      << "a mission may still field a Roman horse archer";

  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::MountedSwordsman),
            ProductionResult::Success);
  for (int squad = 0; squad < 4; ++squad) {
    ASSERT_NE(spawn(SpawnType::MountedSwordsman, k_rome, 55 + squad * 3, 60), 0U);
  }
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::MountedSwordsman),
            ProductionResult::TypeLimitReached);
}

// Phase 1: Carthage's citizens are few; each delivered civilian is worth 12 men.
TEST_F(NationAsymmetryTest, ACarthaginianCivilianIsWorthFewerMen) {
  EXPECT_EQ(Game::Systems::civilian_reserve_grant(world(), k_rome), 18);
  EXPECT_EQ(Game::Systems::civilian_reserve_grant(world(), k_carthage), 12);
}

// Phase 2: Carthage hires a whole band for gold alone at its Mercenary Camp; the
// band arrives as Gauls, keeps a contract, and the camp holds one at a time.
TEST_F(NationAsymmetryTest, CarthageHiresAGallicWarbandForGoldAtTheCamp) {
  EXPECT_TRUE(Game::Systems::nation_may_build(world(), k_carthage, "mercenary_camp"));
  EXPECT_FALSE(Game::Systems::nation_may_build(world(), k_rome, "mercenary_camp"));

  const EntityID barracks = spawn(SpawnType::Barracks, k_carthage, 20, 20);
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::Slinger),
            ProductionResult::WrongBuilding)
      << "Carthage hires its Balearic slingers at the camp, it does not levy them";

  const EntityID camp = spawn(SpawnType::MercenaryCamp, k_carthage, 40, 40);
  ASSERT_NE(camp, 0U);
  const int gold_before = gold(k_carthage);
  const int iron_before = m_session->economy().get(k_carthage, ResourceType::Iron);
  ASSERT_EQ(Game::Systems::ProductionService::start_production(
                world(), camp, TroopType::Swordsman, NationID::Gauls),
            ProductionResult::Success);
  EXPECT_EQ(gold(k_carthage), gold_before - 110);
  EXPECT_EQ(m_session->economy().get(k_carthage, ResourceType::Iron), iron_before)
      << "a band costs gold only";
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), camp, TroopType::Slinger, NationID::Carthage),
            ProductionResult::QueueFull)
      << "one contract at a time";

  run_for(9.0);
  int bands = 0;
  for (auto [id, unit] : world().view<UnitComponent>()) {
    if (unit.owner_id != k_carthage || unit.spawn_type != SpawnType::Swordsman) {
      continue;
    }
    EXPECT_EQ(unit.nation_id, NationID::Gauls);
    const auto* contract =
        world().try_get<Engine::Core::MercenaryContractComponent>(id);
    ASSERT_NE(contract, nullptr) << "a hired band carries its contract";
    EXPECT_EQ(contract->upkeep_per_minute, 7);
    ++bands;
  }
  EXPECT_EQ(bands, 1);
  EXPECT_EQ(Game::Systems::NationEconomySystem::upkeep_per_minute(world(), k_carthage),
            7);
}

// Phase 2: elephants come only from the stables, for gold and food, no reserve.
TEST_F(NationAsymmetryTest, ElephantsComeFromTheStablesForGoldAndFood) {
  const EntityID barracks = spawn(SpawnType::Barracks, k_carthage, 20, 20);
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::Elephant),
            ProductionResult::WrongBuilding);
  const EntityID stables = spawn(SpawnType::ElephantStables, k_carthage, 45, 45);
  ASSERT_NE(stables, 0U);
  const int gold_before = gold(k_carthage);
  const int food_before = m_session->economy().get(k_carthage, ResourceType::Food);
  ASSERT_EQ(Game::Systems::ProductionService::start_production(
                world(), stables, TroopType::Elephant),
            ProductionResult::Success);
  EXPECT_EQ(gold(k_carthage), gold_before - 150);
  EXPECT_EQ(m_session->economy().get(k_carthage, ResourceType::Food), food_before - 90);
}

// Phase 2: bands are paid every minute; an unpaid band loses heart and strikes
// softer until it is paid again. Nobody deserts (decision 2).
TEST_F(NationAsymmetryTest, UnpaidBandsFightWorseUntilPaid) {
  const EntityID band =
      spawn(SpawnType::Swordsman, k_carthage, 40, 40, NationID::Gauls);
  const EntityID foe = spawn(SpawnType::Swordsman, k_rome, 42, 40);
  auto* contract = world().emplace<Engine::Core::MercenaryContractComponent>(band);
  ASSERT_NE(contract, nullptr);
  contract->upkeep_per_minute = 6;

  const auto multiplier = [&] {
    return Game::Systems::Combat::calculate_tactical_damage_multiplier(
        world().get_entity(band), world().get_entity(foe), unit_of(band), unit_of(foe));
  };
  const float paid_blow = multiplier();

  m_session->economy().set(k_carthage, ResourceType::Gold, 100);
  Game::Systems::NationEconomySystem::pay_upkeep(world(), k_carthage);
  EXPECT_EQ(gold(k_carthage), 94);
  EXPECT_FALSE(contract->unpaid);

  m_session->economy().set(k_carthage, ResourceType::Gold, 3);
  Game::Systems::NationEconomySystem::pay_upkeep(world(), k_carthage);
  EXPECT_TRUE(contract->unpaid);
  EXPECT_EQ(gold(k_carthage), 3) << "an empty treasury pays nothing";
  EXPECT_LT(multiplier(), paid_blow);
  ASSERT_NE(world().get_entity(band), nullptr) << "unpaid bands do not desert";

  m_session->economy().set(k_carthage, ResourceType::Gold, 50);
  Game::Systems::NationEconomySystem::pay_upkeep(world(), k_carthage);
  EXPECT_FALSE(contract->unpaid);
  EXPECT_FLOAT_EQ(multiplier(), paid_blow);
}

// Phase 2: Carthage's Emporion earns gold every minute, more for the farms
// around it; Rome's marketplace earns nothing by itself.
TEST_F(NationAsymmetryTest, TheEmporionEarnsGoldForTheFarmsAroundIt) {
  spawn(SpawnType::Marketplace, k_carthage, 40, 40);
  spawn(SpawnType::Farm, k_carthage, 52, 40);
  spawn(SpawnType::Farm, k_carthage, 40, 55);
  spawn(SpawnType::Farm, k_carthage, 90, 90);
  spawn(SpawnType::Marketplace, k_rome, 10, 80);
  EXPECT_EQ(
      Game::Systems::NationEconomySystem::trade_income_per_minute(world(), k_carthage),
      30 + 2 * 5);
  EXPECT_EQ(
      Game::Systems::NationEconomySystem::trade_income_per_minute(world(), k_rome), 0);
  const int before = gold(k_carthage);
  Game::Systems::NationEconomySystem::pay_trade_income(world(), k_carthage);
  EXPECT_EQ(gold(k_carthage), before + 40);
}

// Phase 3: a Fabrica arms Rome's infantry with a third less iron, and triarii
// are only armed where one stands.
TEST_F(NationAsymmetryTest, TheFabricaCutsIronAndArmsTheTriarii) {
  const EntityID barracks = spawn(SpawnType::Barracks, k_rome, 20, 20);
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::Spearman),
            ProductionResult::NeedsBuilding);
  EXPECT_EQ(Game::Systems::quote_recruit(world(), k_rome, TroopType::Swordsman)
                .resources.get(ResourceType::Iron),
            20);
  ASSERT_NE(spawn(SpawnType::Fabrica, k_rome, 40, 20), 0U);
  EXPECT_EQ(Game::Systems::quote_recruit(world(), k_rome, TroopType::Swordsman)
                .resources.get(ResourceType::Iron),
            13);
  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                world(), barracks, TroopType::Spearman),
            ProductionResult::Success);
  EXPECT_FALSE(Game::Systems::nation_may_build(world(), k_carthage, "fabrica"));
}

// Phase 3: the dilectus. Every levy each Roman barracks gains five men for every
// standing home near it; Carthage has no levy.
TEST_F(NationAsymmetryTest, TheLevyCallsUpMenForEveryHomeNearABarracks) {
  const EntityID barracks = spawn(SpawnType::Barracks, k_rome, 30, 30);
  spawn(SpawnType::Home, k_rome, 38, 30);
  spawn(SpawnType::Home, k_rome, 30, 40);
  spawn(SpawnType::Home, k_rome, 85, 85);
  auto* production = world().try_get<Engine::Core::ProductionComponent>(barracks);
  ASSERT_NE(production, nullptr);
  production->manpower_available = 0;
  EXPECT_EQ(Game::Systems::NationEconomySystem::call_levy(world(), k_rome), 10);
  EXPECT_EQ(production->manpower_available, 10);

  const EntityID punic = spawn(SpawnType::Barracks, k_carthage, 60, 60);
  spawn(SpawnType::Home, k_carthage, 66, 60);
  EXPECT_EQ(Game::Systems::NationEconomySystem::call_levy(world(), k_carthage), 0);
  (void)punic;

  auto* system = world().get_system<Game::Systems::NationEconomySystem>();
  ASSERT_NE(system, nullptr);
  EXPECT_GT(system->seconds_to_next_levy(world(), k_rome), 0.0F);
  EXPECT_LT(system->seconds_to_next_levy(world(), k_carthage), 0.0F);
  production->manpower_available = 0;
  run_for(121.0);
  EXPECT_GE(production->manpower_available, 10) << "the levy comes by itself";
}

// Phase 3: "the Senate will find more". A Roman squad destroyed in the field
// sends a quarter of its men back to the nearest barracks; one lost at home
// does not.
TEST_F(NationAsymmetryTest, ASquadLostInTheFieldIsPartlyReplaced) {
  const EntityID barracks = spawn(SpawnType::Barracks, k_rome, 10, 10);
  auto* production = world().try_get<Engine::Core::ProductionComponent>(barracks);
  ASSERT_NE(production, nullptr);
  production->manpower_available = 0;

  const EntityID afield = spawn(SpawnType::Swordsman, k_rome, 80, 80);
  ASSERT_NE(afield, 0U);
  unit_of(afield)->health = 0;
  run_for(0.6);
  EXPECT_EQ(production->manpower_available, 5) << "a quarter of eighteen hastati";

  const EntityID at_home = spawn(SpawnType::Swordsman, k_rome, 14, 10);
  unit_of(at_home)->health = 0;
  run_for(0.6);
  EXPECT_EQ(production->manpower_available, 5) << "no replacement for a loss at home";
}

// Phase 3: hastati that come through two battles become principes: tougher
// and harder-hitting.
TEST_F(NationAsymmetryTest, HastatiThatSurviveTwoBattlesBecomePrincipes) {
  const EntityID hastati = spawn(SpawnType::Swordsman, k_rome, 40, 40);
  auto* unit = unit_of(hastati);
  ASSERT_NE(unit, nullptr);
  const int base_health = unit->max_health;
  run_for(1.2); // the veterans' roll is called once a second
  for (int battle = 0; battle < 2; ++battle) {
    unit->health -= 10;
    run_for(1.2);
    run_for(Game::Systems::NationEconomySystem::k_battle_over_seconds + 1.5);
  }
  EXPECT_EQ(unit->battles_survived, 2);
  EXPECT_EQ(unit->rank, 1);
  EXPECT_EQ(unit->max_health, static_cast<int>(std::lround(base_health * 1.15F)));

  const QJsonObject saved =
      Engine::Core::Serialization::serialize_entity(world().get_entity(hastati));
  Engine::Core::World restored_world;
  auto* restored = restored_world.create_entity();
  Engine::Core::Serialization::deserialize_entity(restored, saved);
  EXPECT_EQ(restored->get_component<UnitComponent>()->rank, 1);
  EXPECT_EQ(restored->get_component<UnitComponent>()->battles_survived, 2);
}

} // namespace

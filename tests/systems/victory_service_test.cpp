#include <gtest/gtest.h>

#include "core/component.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/victory_service.h"

namespace {

auto survive_for(float duration) -> Game::Systems::VictoryRule {
  return Game::Systems::SurviveTimeVictoryRule{duration};
}

auto capture_structures(std::initializer_list<QString> structure_types,
                        int required_count) -> Game::Systems::VictoryRule {
  return Game::Systems::CaptureStructuresVictoryRule{
      Game::Systems::StructureRequirement{std::vector<QString>(structure_types),
                                          required_count}};
}

auto lose_structures(std::initializer_list<QString> structure_types)
    -> Game::Systems::DefeatRule {
  return Game::Systems::NoKeyStructuresDefeatRule{
      std::vector<QString>(structure_types)};
}

class VictoryServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owner_registry = Game::Systems::OwnerRegistry::instance();
    owner_registry.clear();
    owner_registry.register_owner_with_id(
        1, Game::Systems::OwnerType::Player, "Player");
    owner_registry.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Enemy");
    owner_registry.register_owner_with_id(3, Game::Systems::OwnerType::AI, "Ally");
    owner_registry.set_owner_team(1, 1);
    owner_registry.set_owner_team(2, 2);
    owner_registry.set_owner_team(3, 1);
    owner_registry.set_local_player_id(1);

    auto& nation_registry = Game::Systems::NationRegistry::instance();
    nation_registry.clear();
    nation_registry.initialize_defaults();
    nation_registry.set_player_nation(1, Game::Systems::NationID::RomanRepublic);
    nation_registry.set_player_nation(2, Game::Systems::NationID::Carthage);
    nation_registry.set_player_nation(3, Game::Systems::NationID::RomanRepublic);

    Game::Systems::GlobalStatsRegistry::instance().clear();
    m_service = std::make_unique<Game::Systems::VictoryService>();
  }

  void TearDown() override {
    m_service.reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto
  create_unit(Engine::Core::World& world,
              int owner_id,
              Game::Units::SpawnType spawn_type,
              Game::Systems::NationID original_nation_id =
                  Game::Systems::NationID::RomanRepublic) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }

    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->owner_id = owner_id;
    unit->nation_id = original_nation_id;
    unit->spawn_type = spawn_type;
    unit->health = 100;
    unit->max_health = 100;

    const auto troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      if (entity->add_component<Engine::Core::CommanderComponent>() == nullptr) {
        return nullptr;
      }
    }

    if (spawn_type == Game::Units::SpawnType::Barracks ||
        spawn_type == Game::Units::SpawnType::DefenseTower ||
        spawn_type == Game::Units::SpawnType::Home) {
      auto* building = entity->add_component<Engine::Core::BuildingComponent>();
      if (building == nullptr) {
        return nullptr;
      }
      building->original_nation_id = original_nation_id;
    }

    return entity;
  }

  void advance_past_startup_delay(Engine::Core::World& world) {
    m_service->update(world, 0.4F);
  }

  std::unique_ptr<Game::Systems::VictoryService> m_service;
};

TEST_F(VictoryServiceTest, CaptureVictoryDoesNotCountHomeStructuresAsCaptured) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(capture_structures({QStringLiteral("barracks")}, 1));
  rules.defeat_rules.push_back(lose_structures({QStringLiteral("barracks")}));

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);

  EXPECT_FALSE(m_service->is_game_over());
}

TEST_F(VictoryServiceTest, MultipleVictoryRulesUseAnySatisfiedRule) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(capture_structures({QStringLiteral("barracks")}, 2));
  rules.victory_rules.push_back(survive_for(0.2F));

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("victory"));
}

TEST_F(VictoryServiceTest, NoUnitsDefeatIgnoresAlliedUnits) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        3,
                        Game::Units::SpawnType::Spearman,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(survive_for(999.0F));
  rules.defeat_rules.emplace_back(Game::Systems::NoUnitsDefeatRule{});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("defeat"));
}

TEST_F(VictoryServiceTest, BarrackCaptureEventTriggersImmediateReevaluation) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  auto* captured_barracks = create_unit(
      world, 2, Game::Units::SpawnType::Barracks, Game::Systems::NationID::Carthage);
  ASSERT_NE(captured_barracks, nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(capture_structures({QStringLiteral("barracks")}, 1));

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);
  ASSERT_FALSE(m_service->is_game_over());

  auto* unit = captured_barracks->get_component<Engine::Core::UnitComponent>();
  auto* building = captured_barracks->get_component<Engine::Core::BuildingComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(building, nullptr);
  unit->owner_id = 1;
  building->original_nation_id = Game::Systems::NationID::Carthage;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(captured_barracks->get_id(), 2, 1));

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("victory"));
}

TEST_F(VictoryServiceTest, CommanderDeathTriggersDefeatEvenWithArmyRemaining) {
  Engine::Core::World world;
  auto* commander = create_unit(world,
                                1,
                                Game::Units::SpawnType::RomanFieldCommander,
                                Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Spearman,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(survive_for(999.0F));
  rules.defeat_rules.emplace_back(Game::Systems::NoCommanderDefeatRule{});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);
  ASSERT_FALSE(m_service->is_game_over());

  auto* commander_unit = commander->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(commander_unit, nullptr);
  commander_unit->health = 0;
  Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
      commander->get_id(), 1, Game::Units::SpawnType::RomanFieldCommander));

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("defeat"));
}

TEST_F(VictoryServiceTest, CommanderAloneWithoutBarracksTriggersDefeat) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::RomanFieldCommander,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Spearman,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(survive_for(999.0F));
  rules.defeat_rules.emplace_back(
      Game::Systems::OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);
  ASSERT_FALSE(m_service->is_game_over());

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr ||
        entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
      continue;
    }
    unit->health = 0;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(entity->get_id(), 1, unit->spawn_type));
  }

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("defeat"));
}

TEST_F(VictoryServiceTest,
       DefaultMapFallbackDoesNotTreatMissingBarracksAsImmediateDefeat) {
  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::RomanFieldCommander,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  auto* spearman = create_unit(world,
                               1,
                               Game::Units::SpawnType::Spearman,
                               Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(spearman, nullptr);

  Game::Map::VictoryConfig config;
  config.victory_type = QStringLiteral("survive_time");
  config.survive_time_duration = 999.0F;
  config.defeat_conditions.clear();

  m_service->configure(config, 1);
  advance_past_startup_delay(world);
  ASSERT_FALSE(m_service->is_game_over());

  auto* unit = spearman->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(spearman->get_id(), 1, unit->spawn_type));

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("defeat"));
}

TEST_F(VictoryServiceTest, OnlyCommanderRemainingArmsAfterPlayerHadArmyOrBarracks) {
  Engine::Core::World world;
  auto* commander = create_unit(world,
                                1,
                                Game::Units::SpawnType::RomanFieldCommander,
                                Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(commander, nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.push_back(survive_for(999.0F));
  rules.defeat_rules.emplace_back(
      Game::Systems::OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);
  ASSERT_FALSE(m_service->is_game_over());

  auto* spearman = create_unit(world,
                               1,
                               Game::Units::SpawnType::Spearman,
                               Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(spearman, nullptr);
  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      spearman->get_id(), 1, Game::Units::SpawnType::Spearman, false));
  ASSERT_FALSE(m_service->is_game_over());

  auto* unit = spearman->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(spearman->get_id(), 1, unit->spawn_type));

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("defeat"));
}

TEST_F(VictoryServiceTest, AmbientSepulcherStructuresAreIgnoredByDefault) {
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  owner_registry.register_owner_with_id(99, Game::Systems::OwnerType::AI, "Sepulcher");
  owner_registry.set_owner_team(99, 99);
  Game::Systems::NationRegistry::instance().set_player_nation(
      99, Game::Systems::NationID::IronSepulcher);

  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  ASSERT_NE(create_unit(world,
                        99,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::IronSepulcher),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.victory_rules.emplace_back(
      Game::Systems::EliminationVictoryRule{{QStringLiteral("barracks")}});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);

  EXPECT_TRUE(m_service->is_game_over());
  EXPECT_EQ(m_service->get_victory_state(), QStringLiteral("victory"));
}

TEST_F(VictoryServiceTest, AmbientSepulcherStructuresCanBeCountedWhenMissionOptsIn) {
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  owner_registry.register_owner_with_id(99, Game::Systems::OwnerType::AI, "Sepulcher");
  owner_registry.set_owner_team(99, 99);
  Game::Systems::NationRegistry::instance().set_player_nation(
      99, Game::Systems::NationID::IronSepulcher);

  Engine::Core::World world;
  ASSERT_NE(create_unit(world,
                        1,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::RomanRepublic),
            nullptr);
  ASSERT_NE(create_unit(world,
                        99,
                        Game::Units::SpawnType::Barracks,
                        Game::Systems::NationID::IronSepulcher),
            nullptr);

  Game::Systems::VictoryRuleSet rules;
  rules.include_ambient_undead = true;
  rules.victory_rules.emplace_back(
      Game::Systems::EliminationVictoryRule{{QStringLiteral("barracks")}});

  m_service->configure(rules, 1);
  advance_past_startup_delay(world);

  EXPECT_FALSE(m_service->is_game_over());
}

} // namespace

#include <QDir>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "map/campaign_definition.h"
#include "map/campaign_loader.h"
#include "map/mission_loader.h"
#include "map/mission_victory_rules.h"
#include "systems/global_stats_registry.h"
#include "systems/nation_registry.h"
#include "systems/owner_registry.h"
#include "systems/save_storage.h"
#include "systems/victory_service.h"
#include "units/spawn_type.h"

namespace {

using namespace Game::Systems;

auto repo_root() -> QDir {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/campaigns/second_punic_war.json"))) {
      return dir;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::current();
}

auto load_campaign() -> Game::Campaign::CampaignDefinition {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  EXPECT_TRUE(Game::Campaign::CampaignLoader::load_from_json_file(
      repo_root().filePath(QStringLiteral("assets/campaigns/second_punic_war.json")),
      campaign,
      &error))
      << error.toStdString();
  return campaign;
}

auto load_mission(const QString& mission_id) -> Game::Mission::MissionDefinition {
  Game::Mission::MissionDefinition mission;
  QString error;
  EXPECT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
      repo_root().filePath(QStringLiteral("assets/missions/%1.json").arg(mission_id)),
      mission,
      &error))
      << error.toStdString();
  return mission;
}

class CampaignEndToEndTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, OwnerType::Player, "Player");
    owners.register_owner_with_id(2, OwnerType::AI, "Enemy");
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
    owners.set_local_player_id(1);

    auto& nations = NationRegistry::instance();
    nations.clear();
    nations.initialize_defaults();
    nations.set_player_nation(1, NationID::Carthage);
    nations.set_player_nation(2, NationID::RomanRepublic);

    GlobalStatsRegistry::instance().clear();

    campaign = load_campaign();
    ASSERT_FALSE(campaign.missions.empty());

    storage = std::make_unique<SaveStorage>(QStringLiteral(":memory:"));
    QString error;
    ASSERT_TRUE(storage->initialize(&error)) << error.toStdString();
    ASSERT_TRUE(storage->ensure_campaign_missions_in_db(campaign, &error))
        << error.toStdString();
  }

  void TearDown() override {
    storage.reset();
    GlobalStatsRegistry::instance().clear();
    NationRegistry::instance().clear();
    OwnerRegistry::instance().clear();
  }

  static void add_captured_barracks(Engine::Core::World& world) {
    auto* entity = world.create_entity();
    ASSERT_NE(entity, nullptr);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->owner_id = 1;
    unit->nation_id = NationID::RomanRepublic;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->health = 100;
    unit->max_health = 100;
    auto* building = entity->add_component<Engine::Core::BuildingComponent>();
    ASSERT_NE(building, nullptr);
    building->original_nation_id = NationID::RomanRepublic;
  }

  static void add_soldier(Engine::Core::World& world) {
    auto* entity = world.create_entity();
    ASSERT_NE(entity, nullptr);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->owner_id = 1;
    unit->nation_id = NationID::Carthage;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    unit->health = 100;
    unit->max_health = 100;
  }

  static void add_commander(Engine::Core::World& world) {
    auto* entity = world.create_entity();
    ASSERT_NE(entity, nullptr);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->owner_id = 1;
    unit->nation_id = NationID::Carthage;
    unit->spawn_type = Game::Units::SpawnType::CarthageSpearCommander;
    unit->health = 100;
    unit->max_health = 100;
    ASSERT_NE(entity->add_component<Engine::Core::CommanderComponent>(), nullptr);
  }

  static auto add_enemy_barracks(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->owner_id = 2;
    unit->nation_id = NationID::RomanRepublic;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->health = 100;
    unit->max_health = 100;
    auto* building = entity->add_component<Engine::Core::BuildingComponent>();
    if (building == nullptr) {
      return nullptr;
    }
    building->original_nation_id = NationID::RomanRepublic;
    return entity;
  }

  static void add_own_barracks(Engine::Core::World& world) {
    auto* entity = world.create_entity();
    ASSERT_NE(entity, nullptr);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    unit->owner_id = 1;
    unit->nation_id = NationID::Carthage;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->health = 100;
    unit->max_health = 100;
    auto* building = entity->add_component<Engine::Core::BuildingComponent>();
    ASSERT_NE(building, nullptr);
    building->original_nation_id = NationID::Carthage;
  }

  Game::Campaign::CampaignDefinition campaign;
  std::unique_ptr<SaveStorage> storage;
};

TEST_F(CampaignEndToEndTest, TheOpeningMissionCanBeWonAndAdvancesTheCampaign) {
  const QString mission_id = campaign.missions.front().mission_id;
  const auto mission = load_mission(mission_id);
  const auto rules = Game::Mission::build_victory_rules(mission);
  ASSERT_FALSE(rules.victory_rules.empty())
      << mission_id.toStdString() << " ships no victory rule";

  Engine::Core::World world;
  VictoryService service;
  service.configure(rules, 1);

  add_commander(world);
  add_soldier(world);
  add_own_barracks(world);
  add_captured_barracks(world);
  auto* enemy_camp = add_enemy_barracks(world);
  ASSERT_NE(enemy_camp, nullptr);
  service.update(world, 0.4F);
  ASSERT_FALSE(service.is_game_over())
      << "the mission ended before its objective was met";

  QString reported_state;
  service.set_victory_callback(
      [&reported_state](const QString& state) { reported_state = state; });

  enemy_camp->get_component<Engine::Core::UnitComponent>()->owner_id = 1;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(enemy_camp->get_id(), 2, 1));

  ASSERT_TRUE(service.is_game_over())
      << "meeting the objective did not end the mission";
  EXPECT_EQ(reported_state, QStringLiteral("victory"));

  QString error;
  const auto advance =
      storage->complete_campaign_mission(campaign.id, mission_id, &error);
  ASSERT_TRUE(advance.has_value()) << error.toStdString();
  EXPECT_EQ(advance->unlocked_mission_id, campaign.missions[1].mission_id);
  EXPECT_FALSE(advance->campaign_completed);

  const QVariantList rows = storage->get_campaign_mission_progress(campaign.id);
  bool second_unlocked = false;
  for (const QVariant& entry : rows) {
    const QVariantMap row = entry.toMap();
    if (row.value(QStringLiteral("mission_id")).toString() ==
        campaign.missions[1].mission_id) {
      second_unlocked = row.value(QStringLiteral("unlocked")).toBool();
    }
  }
  EXPECT_TRUE(second_unlocked) << "the next mission is still locked after the win";
}

TEST_F(CampaignEndToEndTest, ADefeatLeavesTheCampaignExactlyWhereItWas) {
  const QString mission_id = campaign.missions.front().mission_id;
  const auto mission = load_mission(mission_id);
  const auto rules = Game::Mission::build_victory_rules(mission);
  ASSERT_FALSE(rules.defeat_rules.empty())
      << mission_id.toStdString() << " ships no defeat rule";

  Engine::Core::World world;
  VictoryService service;
  service.configure(rules, 1);

  QString reported_state;
  service.set_victory_callback(
      [&reported_state](const QString& state) { reported_state = state; });

  service.update(world, 0.4F);
  ASSERT_TRUE(service.is_game_over());
  EXPECT_EQ(reported_state, QStringLiteral("defeat"));

  const QVariantList rows = storage->get_campaign_mission_progress(campaign.id);
  for (const QVariant& entry : rows) {
    const QVariantMap row = entry.toMap();
    EXPECT_FALSE(row.value(QStringLiteral("completed")).toBool())
        << row.value(QStringLiteral("mission_id")).toString().toStdString()
        << " was completed by a defeat";
  }
  EXPECT_TRUE(storage->get_campaign_progress(campaign.id).isEmpty());
}

TEST_F(CampaignEndToEndTest, EveryCampaignMissionShipsRulesItCanBeJudgedBy) {
  for (const auto& entry : campaign.missions) {
    const auto mission = load_mission(entry.mission_id);
    const auto rules = Game::Mission::build_victory_rules(mission);
    EXPECT_FALSE(rules.victory_rules.empty())
        << entry.mission_id.toStdString() << " cannot be won";
    EXPECT_FALSE(rules.defeat_rules.empty())
        << entry.mission_id.toStdString() << " cannot be lost";
  }
}

} // namespace

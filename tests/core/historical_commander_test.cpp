// Historical cameo commanders (issue #1522): spawnable by id, never playable.

#include <QFile>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "game/core/component_commander.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_context.h"
#include "game/map/map_definition.h"
#include "game/map/mission_loader.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "utils/resource_utils.h"

namespace {

using Game::Systems::NationID;
using Game::Units::CommanderBarkKind;
using Game::Units::CommanderDefinition;
using Game::Units::TroopType;

const std::vector<std::string> k_expected_cameo_ids = {
    "roman_sempronius_longus",
    "roman_gaius_flaminius",
    "roman_terentius_varro",
    "roman_aemilius_paullus",
    "roman_scipio_consul_218",
    "carthage_mago_barca",
    "carthage_maharbal",
    "carthage_hanno_bomilcar",
    "carthage_hasdrubal_cavalry",
    "numidian_masinissa",
};

const std::vector<std::string> k_playable_ids = {
    "roman_legion_organizer",
    "roman_veteran_consul",
    "roman_field_commander",
    "carthage_spear_commander",
    "carthage_bow_commander",
    "carthage_sword_commander",
};

auto ids_of(const std::vector<const CommanderDefinition*>& definitions)
    -> std::set<std::string> {
  std::set<std::string> ids;
  for (const auto* definition : definitions) {
    ids.insert(definition->id);
  }
  return ids;
}

TEST(HistoricalCommanderTest, PlayableRosterIsExactlyTheSix) {
  const auto& playable = Game::Units::all_commander_definitions();
  ASSERT_EQ(playable.size(), k_playable_ids.size());
  std::set<std::string> ids;
  for (const auto& definition : playable) {
    EXPECT_TRUE(definition.playable) << definition.id;
    ids.insert(definition.id);
  }
  EXPECT_EQ(ids, std::set<std::string>(k_playable_ids.begin(), k_playable_ids.end()));
}

TEST(HistoricalCommanderTest, PickersAndSkirmishSeatsListOnlyThePlayableSix) {
  // commander_definitions_for_nation feeds the skirmish commander picker
  // (MatchSetupViewModel::commanders_for_nation) and the CPU seat roster.
  std::set<std::string> listed;
  for (const auto nation : {NationID::RomanRepublic,
                            NationID::Carthage,
                            NationID::IronSepulcher,
                            NationID::Gauls,
                            NationID::Iberians}) {
    for (const auto* definition :
         Game::Units::commander_definitions_for_nation(nation)) {
      EXPECT_TRUE(definition->playable) << definition->id;
      EXPECT_FALSE(Game::Units::is_historical_commander_id(definition->id))
          << definition->id;
      listed.insert(definition->id);
    }
  }
  EXPECT_EQ(listed,
            std::set<std::string>(k_playable_ids.begin(), k_playable_ids.end()));
  EXPECT_EQ(
      ids_of(Game::Units::commander_definitions_for_nation(NationID::RomanRepublic)),
      (std::set<std::string>{
          "roman_legion_organizer", "roman_veteran_consul", "roman_field_commander"}));
  EXPECT_EQ(ids_of(Game::Units::commander_definitions_for_nation(NationID::Carthage)),
            (std::set<std::string>{"carthage_spear_commander",
                                   "carthage_bow_commander",
                                   "carthage_sword_commander"}));

  for (const QString nation :
       {QStringLiteral("roman_republic"), QStringLiteral("carthage")}) {
    for (int seat = 0; seat < 24; ++seat) {
      const QString troop = Game::Mission::commander_troop_for_seat(nation, seat);
      EXPECT_NE(
          std::find(k_playable_ids.begin(), k_playable_ids.end(), troop.toStdString()),
          k_playable_ids.end())
          << troop.toStdString();
    }
    for (const auto& id : k_expected_cameo_ids) {
      const QString resolved =
          Game::Mission::resolve_commander_troop(nation, QString::fromStdString(id));
      EXPECT_FALSE(Game::Units::is_historical_commander_id(resolved.toStdString()))
          << "a skirmish setup may not select cameo " << id;
    }
  }
}

TEST(HistoricalCommanderTest, RosterHoldsTheTenNamedCommanders) {
  const auto& roster = Game::Units::historical_commander_definitions();
  std::vector<std::string> ids;
  std::set<std::string> renderers;
  std::set<std::string> names;
  for (const auto& definition : roster) {
    ids.push_back(definition.id);
    EXPECT_FALSE(definition.playable) << definition.id;
    EXPECT_TRUE(Game::Units::is_commander_troop(definition.troop_type))
        << definition.id;
    EXPECT_FALSE(definition.display_name.empty()) << definition.id;
    EXPECT_FALSE(definition.strategic_identity.empty()) << definition.id;
    EXPECT_FALSE(definition.renderer_id.empty()) << definition.id;
    EXPECT_NE(definition.signature.move, Game::Units::CommanderSignatureMove::None)
        << definition.id;
    renderers.insert(definition.renderer_id);
    names.insert(definition.display_name);
    EXPECT_EQ(Game::Units::find_commander_definition(definition.id), &definition);
    EXPECT_EQ(Game::Units::commander_definition(definition.troop_type)->playable, true)
        << "troop-type lookups must keep resolving to the playable commander";
  }
  EXPECT_EQ(ids, k_expected_cameo_ids);
  EXPECT_EQ(renderers.size(), roster.size());
  EXPECT_EQ(names.size(), roster.size());
  for (const auto& playable : Game::Units::all_commander_definitions()) {
    EXPECT_EQ(names.count(playable.display_name), 0U) << playable.display_name;
    EXPECT_FALSE(Game::Units::is_historical_commander_id(playable.id));
  }

  // The playable Scipio is Africanus; the cameo is his father, named apart.
  const auto* elder =
      Game::Units::historical_commander_definition("roman_scipio_consul_218");
  ASSERT_NE(elder, nullptr);
  EXPECT_NE(
      elder->display_name,
      Game::Units::commander_definition(TroopType::RomanVeteranConsul)->display_name);
  EXPECT_NE(elder->display_name.find("218"), std::string::npos);
  // Likewise the Cannae cavalry officer is not Hasdrubal Barca.
  const auto* hasdrubal =
      Game::Units::historical_commander_definition("carthage_hasdrubal_cavalry");
  ASSERT_NE(hasdrubal, nullptr);
  EXPECT_NE(
      hasdrubal->display_name,
      Game::Units::commander_definition(TroopType::CarthageBowCommander)->display_name);
  EXPECT_EQ(Game::Units::historical_commander_definition("roman_veteran_consul"),
            nullptr);
}

TEST(HistoricalCommanderTest, AuraAndRallyStayInsideThePlayableRange) {
  struct Range {
    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    void add(float value) {
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
    [[nodiscard]] auto holds(float value) const -> bool {
      return value >= lo - 1.0e-4F && value <= hi + 1.0e-4F;
    }
  };
  Range radius;
  Range morale;
  Range rally_range;
  Range rally_cooldown;
  Range rally_restore;
  Range shock_radius;
  Range shock;
  Range ability_duration;
  Range ability_cooldown;
  std::map<std::string, Range> bonus_by_type;
  for (const auto& d : Game::Units::all_commander_definitions()) {
    radius.add(d.aura_radius);
    morale.add(d.aura_morale_bonus);
    rally_range.add(d.rally_range);
    rally_cooldown.add(d.rally_cooldown);
    rally_restore.add(d.rally_morale_restore);
    shock_radius.add(d.death_shock_radius);
    shock.add(d.death_morale_shock);
    ability_duration.add(d.aura_ability_duration);
    ability_cooldown.add(d.aura_ability_cooldown);
    bonus_by_type[d.bonus_type].add(d.aura_bonus_value);
  }
  for (const auto& d : Game::Units::historical_commander_definitions()) {
    SCOPED_TRACE(d.id);
    EXPECT_TRUE(radius.holds(d.aura_radius));
    EXPECT_TRUE(morale.holds(d.aura_morale_bonus));
    EXPECT_TRUE(rally_range.holds(d.rally_range));
    EXPECT_TRUE(rally_cooldown.holds(d.rally_cooldown));
    EXPECT_TRUE(rally_restore.holds(d.rally_morale_restore));
    EXPECT_TRUE(shock_radius.holds(d.death_shock_radius));
    EXPECT_TRUE(shock.holds(d.death_morale_shock));
    EXPECT_TRUE(ability_duration.holds(d.aura_ability_duration));
    EXPECT_TRUE(ability_cooldown.holds(d.aura_ability_cooldown));
    const auto bonus = bonus_by_type.find(d.bonus_type);
    ASSERT_NE(bonus, bonus_by_type.end()) << "unknown aura type " << d.bonus_type;
    EXPECT_TRUE(bonus->second.holds(d.aura_bonus_value)) << d.aura_bonus_value;
  }
}

TEST(HistoricalCommanderTest, EveryCameoHasShortBarksButNoSpeechBank) {
  for (const auto& d : Game::Units::historical_commander_definitions()) {
    SCOPED_TRACE(d.id);
    for (const auto kind : {CommanderBarkKind::Rally,
                            CommanderBarkKind::Charge,
                            CommanderBarkKind::FallBack}) {
      const auto& lines = Game::Units::commander_bark_lines(d.id, kind);
      ASSERT_FALSE(lines.empty());
      ASSERT_LE(lines.size(), 3U);
      for (const auto& line : lines) {
        EXPECT_FALSE(line.empty());
        EXPECT_LE(QString::fromStdString(line).length(), 48);
      }
    }
    QString const bank = QStringLiteral(":/assets/data/commanders/voices/%1.json")
                             .arg(QString::fromStdString(d.id));
    EXPECT_FALSE(QFile::exists(Utils::Resources::resolve_resource_path(bank)))
        << "cameos never get the playable commanders' chatter banks";
  }
  EXPECT_TRUE(
      Game::Units::commander_bark_lines("no_such_commander", CommanderBarkKind::Rally)
          .empty());
}

TEST(HistoricalCommanderTest, CameoBodyFollowsTheOwnersNation) {
  const auto* masinissa =
      Game::Units::historical_commander_definition("numidian_masinissa");
  const auto* mago =
      Game::Units::historical_commander_definition("carthage_mago_barca");
  const auto* hanno =
      Game::Units::historical_commander_definition("carthage_hanno_bomilcar");
  ASSERT_NE(masinissa, nullptr);
  ASSERT_NE(mago, nullptr);
  ASSERT_NE(hanno, nullptr);
  using Game::Units::historical_commander_troop_for_nation;
  EXPECT_EQ(historical_commander_troop_for_nation(*masinissa, NationID::RomanRepublic),
            TroopType::RomanLegionOrganizer);
  EXPECT_EQ(historical_commander_troop_for_nation(*masinissa, NationID::Carthage),
            TroopType::CarthageSpearCommander);
  EXPECT_EQ(historical_commander_troop_for_nation(*mago, NationID::Carthage),
            TroopType::CarthageSwordCommander);
  EXPECT_EQ(historical_commander_troop_for_nation(*mago, NationID::RomanRepublic),
            TroopType::RomanVeteranConsul);
  EXPECT_EQ(historical_commander_troop_for_nation(*hanno, NationID::Carthage),
            TroopType::CarthageBowCommander);
}

class HistoricalCommanderSpawnTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Rome");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Carthage");
    owners.set_local_player_id(1);
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(1, NationID::RomanRepublic);
    nations.set_player_nation(2, NationID::Carthage);
  }

  void TearDown() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto spawn(Game::Units::SpawnType type,
             int owner,
             NationID nation,
             const std::string& commander_id = {}) -> Engine::Core::Entity* {
    Game::Units::SpawnParams params;
    params.player_id = owner;
    params.nation_id = nation;
    params.spawn_type = type;
    params.position =
        QVector3D(10.0F + static_cast<float>(m_spawned++) * 3.0F, 0.0F, 10.0F);
    params.commander_id = commander_id;
    auto unit = m_factory->create(type, m_world, params);
    return unit != nullptr ? m_world.get_entity(unit->id()) : nullptr;
  }

  Engine::Core::World m_world;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  int m_spawned = 0;
};

TEST_F(HistoricalCommanderSpawnTest, SpawnsWithTheCameoIdentityAndLook) {
  auto* varro = spawn(Game::Units::SpawnType::RomanVeteranConsul,
                      1,
                      NationID::RomanRepublic,
                      "roman_terentius_varro");
  ASSERT_NE(varro, nullptr);
  const auto* definition =
      Game::Units::historical_commander_definition("roman_terentius_varro");
  const auto* commander = varro->get_component<Engine::Core::CommanderComponent>();
  const auto* renderable = varro->get_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(renderable, nullptr);
  EXPECT_EQ(commander->commander_id, "roman_terentius_varro");
  EXPECT_EQ(commander->display_name, definition->display_name);
  EXPECT_FLOAT_EQ(commander->aura_radius, definition->aura_radius);
  EXPECT_FLOAT_EQ(commander->aura_bonus_value, definition->aura_bonus_value);
  EXPECT_EQ(commander->bonus_type, definition->bonus_type);
  EXPECT_EQ(renderable->renderer_id, "troops/roman/commanders/terentius_varro");
}

TEST_F(HistoricalCommanderSpawnTest, CameosServeBesideTheOwnersCommander) {
  ASSERT_NE(spawn(Game::Units::SpawnType::RomanVeteranConsul,
                  1,
                  NationID::RomanRepublic,
                  "roman_terentius_varro"),
            nullptr);
  ASSERT_NE(spawn(Game::Units::SpawnType::RomanLegionOrganizer,
                  1,
                  NationID::RomanRepublic,
                  "roman_aemilius_paullus"),
            nullptr);
  // A cameo does not take the owner's single playable-commander slot...
  ASSERT_NE(
      spawn(Game::Units::SpawnType::RomanFieldCommander, 1, NationID::RomanRepublic),
      nullptr);
  // ...but that slot still holds just one.
  EXPECT_EQ(
      spawn(Game::Units::SpawnType::RomanVeteranConsul, 1, NationID::RomanRepublic),
      nullptr);
  EXPECT_EQ(m_world.collect_entities_with<Engine::Core::CommanderComponent>().size(),
            3U);
}

TEST_F(HistoricalCommanderSpawnTest, CameoTakesTheBodyOfItsOwnersNation) {
  // Authored on a Roman sword body but fielded by Carthage: Mago gets
  // Carthage's sword-commander body and keeps his own look.
  auto* mago = spawn(Game::Units::SpawnType::RomanVeteranConsul,
                     2,
                     NationID::Carthage,
                     "carthage_mago_barca");
  ASSERT_NE(mago, nullptr);
  const auto* unit = mago->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::CarthageSwordCommander);
  EXPECT_EQ(mago->get_component<Engine::Core::RenderableComponent>()->renderer_id,
            "troops/carthage/commanders/mago_barca");

  auto* masinissa = spawn(Game::Units::SpawnType::RomanLegionOrganizer,
                          1,
                          NationID::RomanRepublic,
                          "numidian_masinissa");
  ASSERT_NE(masinissa, nullptr);
  EXPECT_EQ(masinissa->get_component<Engine::Core::UnitComponent>()->spawn_type,
            Game::Units::SpawnType::RomanLegionOrganizer);
}

TEST_F(HistoricalCommanderSpawnTest, UnknownCameoIdSpawnsThePlainCommander) {
  auto* consul = spawn(Game::Units::SpawnType::RomanVeteranConsul,
                       1,
                       NationID::RomanRepublic,
                       "roman_nobody_in_particular");
  ASSERT_NE(consul, nullptr);
  EXPECT_EQ(consul->get_component<Engine::Core::CommanderComponent>()->commander_id,
            "roman_veteran_consul");
}

TEST_F(HistoricalCommanderSpawnTest, CameoIdNeverPromotesARankAndFileSoldier) {
  auto* soldier = spawn(Game::Units::SpawnType::Swordsman,
                        1,
                        NationID::RomanRepublic,
                        "roman_terentius_varro");
  ASSERT_NE(soldier, nullptr);
  EXPECT_EQ(soldier->get_component<Engine::Core::UnitComponent>()->spawn_type,
            Game::Units::SpawnType::Swordsman);
  EXPECT_EQ(soldier->get_component<Engine::Core::CommanderComponent>(), nullptr);
}

TEST(HistoricalCommanderMapTest, BriefingNamesTheCameoLeadingAForce) {
  Game::Map::MapDefinition map;
  Game::Map::UnitSpawn hannibal;
  hannibal.type = Game::Units::SpawnType::CarthageSwordCommander;
  hannibal.player_id = 1;
  Game::Map::UnitSpawn mago = hannibal;
  mago.commander_id = QStringLiteral("carthage_mago_barca");
  Game::Map::UnitSpawn varro;
  varro.type = Game::Units::SpawnType::RomanVeteranConsul;
  varro.player_id = 2;
  varro.commander_id = QStringLiteral("roman_terentius_varro");
  map.spawns = {mago, hannibal, varro};

  const auto troops = Game::Mission::commander_troops_by_owner(map);
  EXPECT_EQ(troops.at(1), QStringLiteral("carthage_sword_commander"));
  EXPECT_EQ(troops.at(2), QStringLiteral("roman_veteran_consul"));

  const auto identities = Game::Mission::commander_identities_by_owner(map);
  EXPECT_EQ(identities.at(1), QStringLiteral("carthage_sword_commander"))
      << "a playable commander outranks a cameo serving beside him";
  EXPECT_EQ(identities.at(2), QStringLiteral("roman_terentius_varro"));
}

TEST(HistoricalCommanderMissionTest, CampaignBriefingsNameTheHistoricalLeaders) {
  struct Expected {
    const char* mission;
    const char* force;
    const char* commander;
  };
  const Expected expected[] = {
      {"battle_of_trebia", "roman_main_army", "roman_sempronius_longus"},
      {"battle_of_trasimene", "roman_column", "roman_gaius_flaminius"},
      {"battle_of_cannae", "roman_allied_wing", "roman_terentius_varro"},
      {"battle_of_cannae", "roman_reserve_wing", "roman_aemilius_paullus"},
      {"battle_of_ticino", "roman_screen", "roman_scipio_consul_218"},
      {"battle_of_zama", "roman_numidian_allies", "numidian_masinissa"},
      {"battle_of_cannae", "player", "carthage_hasdrubal_cavalry"},
      {"battle_of_trebia", "player", "carthage_mago_barca"},
      {"crossing_the_rhone", "player", "carthage_hanno_bomilcar"},
  };
  for (const auto& entry : expected) {
    SCOPED_TRACE(std::string(entry.mission) + "/" + entry.force);
    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(
            QStringLiteral(":/assets/missions/%1.json").arg(entry.mission)),
        mission,
        &error))
        << error.toStdString();
    const QVariantMap view = build_mission_definition_map(mission);
    QVariantMap setup;
    if (QString::fromLatin1(entry.force) == QStringLiteral("player")) {
      setup = view.value("player_setup").toMap();
    } else {
      for (const auto& ai : view.value("ai_setups").toList()) {
        if (ai.toMap().value("id").toString() == QString::fromLatin1(entry.force)) {
          setup = ai.toMap();
        }
      }
    }
    ASSERT_FALSE(setup.isEmpty());
    QStringList names;
    QStringList ids;
    for (const auto& commander : setup.value("historical_commanders").toList()) {
      ids << commander.toMap().value("id").toString();
      names << commander.toMap().value("display_name").toString();
    }
    EXPECT_TRUE(ids.contains(QString::fromLatin1(entry.commander)))
        << ids.join(",").toStdString();
    EXPECT_FALSE(names.contains(QString()));
  }
}

} // namespace

namespace {

auto barcid_road_mission_ids() -> std::vector<QString> {
  return {QStringLiteral("crossing_the_rhone"),
          QStringLiteral("crossing_the_alps"),
          QStringLiteral("battle_of_ticino"),
          QStringLiteral("battle_of_trebia"),
          QStringLiteral("battle_of_trasimene"),
          QStringLiteral("battle_of_cannae"),
          QStringLiteral("campania_campaign"),
          QStringLiteral("battle_of_zama")};
}

} // namespace

// A cameo beside Hannibal would keep the player's lose_commander rule from
// firing and announce "Hannibal has fallen" when the officer dies, so the
// player's officers stay in the briefing. An enemy cameo replaces its owner's
// single commander in the same body, so speakers and seats still line up.
TEST(HistoricalCommanderMissionTest, CampaignCameosLeadEnemyForcesTheirBriefingsName) {
  for (const auto& mission_id : barcid_road_mission_ids()) {
    SCOPED_TRACE(mission_id.toStdString());
    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        Utils::Resources::resolve_resource_path(
            QStringLiteral(":/assets/missions/%1.json").arg(mission_id)),
        mission,
        &error))
        << error.toStdString();
    const auto context = Game::Map::MapContextStore::acquire(mission.map_path, &error);
    ASSERT_TRUE(context.valid()) << error.toStdString();

    std::map<int, int> commander_spawns_by_owner;
    for (const auto& spawn : context.definition()->spawns) {
      const auto troop = Game::Units::spawn_typeToTroopType(spawn.type);
      if (troop.has_value() && Game::Units::is_commander_troop(*troop)) {
        commander_spawns_by_owner[spawn.player_id] += 1;
      }
      if (spawn.commander_id.isEmpty()) {
        continue;
      }
      EXPECT_NE(spawn.player_id, 1)
          << spawn.commander_id.toStdString() << " must not stand beside Hannibal";
      const auto ai_index = static_cast<std::size_t>(spawn.player_id - 2);
      ASSERT_LT(ai_index, mission.ai_setups.size());
      EXPECT_TRUE(mission.ai_setups[ai_index].historical_commanders.contains(
          spawn.commander_id))
          << mission.ai_setups[ai_index].id.toStdString() << " briefing omits "
          << spawn.commander_id.toStdString();
      const auto* cameo = Game::Units::historical_commander_definition(
          spawn.commander_id.toStdString());
      ASSERT_NE(cameo, nullptr) << spawn.commander_id.toStdString();
      ASSERT_TRUE(troop.has_value());
      EXPECT_EQ(*troop, cameo->troop_type)
          << spawn.commander_id.toStdString()
          << " must take over a commander spawn of its own body";
    }
    for (const auto& [owner, count] : commander_spawns_by_owner) {
      EXPECT_EQ(count, 1) << "owner " << owner << " fields one commander";
    }

    const auto identities =
        Game::Mission::commander_identities_by_owner(*context.definition());
    for (std::size_t i = 0; i < mission.ai_setups.size(); ++i) {
      const int owner = static_cast<int>(i) + 2;
      for (const auto& briefed : mission.ai_setups[i].historical_commanders) {
        ASSERT_TRUE(identities.contains(owner));
        EXPECT_EQ(identities.at(owner), briefed)
            << mission.ai_setups[i].id.toStdString()
            << " names a commander it never fields";
      }
    }
  }
}

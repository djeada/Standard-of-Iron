#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/mission/commander_message_director.h"
#include "game/mission/commander_speaker_roster.h"
#include "game/mission/commander_voice_bank.h"
#include "game/mission/commander_voice_observer.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::SessionContext;

constexpr int k_map_size = 128;

constexpr int k_player = 2;
constexpr int k_enemy = 3;
constexpr int k_player_grid = 26;
constexpr int k_enemy_grid = k_map_size - 26;
constexpr int k_minutes_to_wait = 30;

auto asset_dir_path(const QString& relative_path) -> QString {
  return QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QStringLiteral("../../assets/%1").arg(relative_path));
}

class CommanderVoiceSkirmishTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_director.clear();
    m_observer.clear();
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
    Engine::Core::EventManager::instance().clear_all_subscriptions();
  }

  auto make_match() -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);

    auto& owners = session.owners();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::AI, "player");
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "enemy");
    owners.set_local_player_id(k_player);
    owners.set_owner_team(k_player, 1);
    owners.set_owner_team(k_enemy, 2);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_player,
                                        Game::Systems::NationID::RomanRepublic);
    session.nations().set_player_nation(k_enemy, Game::Systems::NationID::Carthage);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    scatter_resources(map_definition, k_player_grid, k_player_grid);
    scatter_resources(map_definition, k_enemy_grid, k_enemy_grid);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    for (const int owner : {k_player, k_enemy}) {
      auto& economy = session.economy();
      economy.ensure_owner(owner);
      economy.set(owner, Game::Systems::ResourceType::Gold, 500);
      economy.set(owner, Game::Systems::ResourceType::Food, 200);
      economy.set(owner, Game::Systems::ResourceType::Wood, 250);
      economy.set(owner, Game::Systems::ResourceType::Stone, 120);
      economy.set(owner, Game::Systems::ResourceType::Iron, 80);
    }

    seat_town(
        session, k_player, k_player_grid, Game::Units::SpawnType::RomanVeteranConsul);
    seat_town(
        session, k_enemy, k_enemy_grid, Game::Units::SpawnType::CarthageSwordCommander);

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      for (const int owner : {k_player, k_enemy}) {
        auto profile =
            Game::Systems::AI::doctrine_profile_for_owner(session.world(), owner);
        if (profile.has_value()) {
          ai->set_ai_profile(owner, *profile);
        }
      }
    }
    return session;
  }

  static void scatter_resources(Game::Map::MapDefinition& map, int grid_x, int grid_z) {
    const auto add = [&map](Game::Map::WorldProp::Type type, int x, int z) {
      if (x < 2 || z < 2 || x >= k_map_size - 2 || z >= k_map_size - 2) {
        return;
      }
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = static_cast<float>(x);
      prop.z = static_cast<float>(z);
      map.world_props.push_back(prop);
    };
    for (int ring = 0; ring < 8; ++ring) {
      const int offset = 10 + ring * 2;
      for (int lane = -2; lane <= 2; ++lane) {
        const int shift = lane * 3;
        add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::OliveTree, grid_x - offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::PineTree, grid_x + shift, grid_z + offset);
        add(Game::Map::WorldProp::Type::PineTree, grid_x + shift, grid_z - offset);
      }
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset, grid_z + 6);
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset, grid_z + 9);
      add(Game::Map::WorldProp::Type::Boulder, grid_x - offset, grid_z + 9);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z - 6);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z - 9);
    }
  }

  void seat_town(SessionContext& session,
                 int owner,
                 int grid,
                 Game::Units::SpawnType commander) {
    spawn(session, Game::Units::SpawnType::Barracks, owner, world_of(grid, grid));
    spawn(session, commander, owner, world_of(grid + 2, grid + 2));
    for (int index = 0; index < 4; ++index) {
      spawn(session,
            Game::Units::SpawnType::Builder,
            owner,
            world_of(grid + 4, grid - 3 + index * 2));
    }
  }

  auto spawn(SessionContext& session,
             Game::Units::SpawnType type,
             int owner_id,
             QVector3D position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.ai_controlled = true;
    params.is_initial_spawn = true;
    params.max_population = 280;
    params.enables_production = true;
    const auto* nation = session.nations().get_nation_for_player(owner_id);
    params.nation_id =
        nation != nullptr ? nation->id : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, session.world(), params);
    return unit ? unit->id() : 0;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return Game::Systems::NavGrid::grid_to_world(Game::Systems::Point(grid_x, grid_z));
  }

  auto run_until_line(SessionContext& session,
                      double seconds,
                      Game::Mission::CommanderMessageTrigger trigger) -> bool {
    const double step = session.clock().tick_seconds();
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const Game::Mission::AiSystemAttackPlanSource plans(ai);
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
        m_observer.update(session.world(), &plans, static_cast<float>(step));
        m_director.update(static_cast<float>(step));
        if (m_director.has_active()) {
          const auto& cue = m_director.active();
          if (m_shown_ids.empty() || m_shown_ids.back() != cue.id) {
            m_shown_ids.push_back(cue.id);
            m_shown_triggers.push_back(m_last_trigger_of(cue.id));
          }
          if (m_shown_triggers.back() == trigger) {
            return true;
          }
          m_director.dismiss_active();
        }
      }
    }
    return false;
  }

  static auto
  m_last_trigger_of(const QString& rule_id) -> Game::Mission::CommanderMessageTrigger {
    if (rule_id.contains(QStringLiteral(".attack_launched."))) {
      return Game::Mission::CommanderMessageTrigger::AttackLaunched;
    }
    if (rule_id.contains(QStringLiteral(".match_start."))) {
      return Game::Mission::CommanderMessageTrigger::MissionStart;
    }
    if (rule_id.contains(QStringLiteral(".first_contact."))) {
      return Game::Mission::CommanderMessageTrigger::FirstContact;
    }
    if (rule_id.contains(QStringLiteral(".under_attack."))) {
      return Game::Mission::CommanderMessageTrigger::UnderAttack;
    }
    return Game::Mission::CommanderMessageTrigger::HeavyLosses;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  Game::Mission::CommanderVoiceLibrary m_library;
  Game::Mission::CommanderMessageDirector m_director;
  Game::Mission::CommanderVoiceObserver m_observer;
  std::vector<QString> m_shown_ids;
  std::vector<Game::Mission::CommanderMessageTrigger> m_shown_triggers;
};

} // namespace

TEST_F(CommanderVoiceSkirmishTest, HannibalAnnouncesHisOwnAttackOnThePlayer) {
  auto& session = make_match();

  QString error;
  m_library = Game::Mission::CommanderVoiceLibrary::load_from_directory(
      asset_dir_path(QStringLiteral("data/commanders/voices")), &error);
  ASSERT_TRUE(error.isEmpty()) << error.toStdString();

  Game::Mission::CommanderMessageScript script;
  script.speakers = Game::Mission::build_commander_speaker_roster(
      session.world(), session.owners(), k_player);
  ASSERT_EQ(script.speakers.size(), 1U);
  EXPECT_EQ(script.speakers.front().owner_id, k_enemy);
  EXPECT_EQ(script.speakers.front().troop_type,
            QStringLiteral("carthage_sword_commander"));
  EXPECT_EQ(script.speakers.front().relationship,
            Game::Mission::CommanderRelationship::Enemy);
  script.voices = &m_library;
  m_director.configure(script, k_player, nullptr);
  m_director.set_relationship_lookup(
      [&session](int a, int b) { return session.owners().are_allies(a, b); });
  m_observer.configure({k_player, k_enemy}, k_player);

  m_director.notify_mission_start();
  m_director.update(3.0F);
  ASSERT_TRUE(m_director.has_active()) << "Hannibal has no opening line";
  EXPECT_EQ(m_director.active().speaker_name, QStringLiteral("Hannibal Barca"));
  EXPECT_EQ(m_director.active().relationship, QStringLiteral("enemy"));
  m_director.dismiss_active();

  int attacks_seen = 0;
  const auto attacks =
      Engine::Core::ScopedEventSubscription<Engine::Core::AiAttackLaunchedEvent>(
          [&attacks_seen](const auto& e) {
            if (e.attacker_owner_id == k_enemy && e.target_owner_id == k_player) {
              ++attacks_seen;
            }
          });

  const bool spoke =
      run_until_line(session,
                     k_minutes_to_wait * 60.0,
                     Game::Mission::CommanderMessageTrigger::AttackLaunched);
  ASSERT_TRUE(spoke) << "in " << k_minutes_to_wait
                     << " minutes Hannibal never announced an attack; the observer saw "
                     << attacks_seen << " attack commitments";
  EXPECT_GE(attacks_seen, 1);
  ASSERT_TRUE(m_director.has_active());
  const auto& cue = m_director.active();
  EXPECT_EQ(cue.speaker_id, QStringLiteral("carthage_sword_commander"));
  EXPECT_EQ(cue.speaker_owner_id, k_enemy);
  EXPECT_TRUE(cue.id.startsWith(QStringLiteral("3:hannibal.enemy.attack_launched.")))
      << cue.id.toStdString();
  EXPECT_FALSE(cue.text.isEmpty());
}

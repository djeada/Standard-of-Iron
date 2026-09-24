#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "core/component_combat.h"
#include "core/component_commander.h"
#include "core/component_core.h"
#include "core/component_structures.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/mission_victory_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/difficulty_forces.h"
#include "game/mission/difficulty_profile.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/mission/spawn_placement.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/systems/capture_system.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "tests/support/ai_quiesce.h"

namespace {

using Engine::Core::UnitComponent;

constexpr int k_local_owner = 1;
constexpr float k_tick = 1.0F / 60.0F;
constexpr char k_mission_path[] = "assets/missions/iron_sepulcher_watch.json";

void reset_globals() {
  Game::Map::TerrainService::instance().clear();
  Game::Map::VisibilityService::instance().reset();
  Game::Systems::GlobalStatsRegistry::instance().clear();
  auto& nations = Game::Systems::NationRegistry::instance();
  nations.clear();
  Game::Systems::initialize_default_content(nations);
  Game::Systems::OwnerRegistry::instance().clear();
}

class WatchRun {
public:
  explicit WatchRun(const QString& difficulty_id,
                    QString mission_path = QString::fromLatin1(k_mission_path))
      : m_difficulty(difficulty_id)
      , m_mission_path(std::move(mission_path)) {}

  auto run(QString* out_error) -> bool {
    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    if (!m_campaign.start_mission_file(m_mission_path,
                                       selected_player_id,
                                       out_error,
                                       m_difficulty.baseline_id())) {
      return false;
    }
    const auto& mission = *m_campaign.current_mission_definition();
    const QString map_path = mission.map_path.startsWith(QStringLiteral(":/"))
                                 ? mission.map_path.mid(2)
                                 : mission.map_path;

    App::Core::SkirmishLoader loader(m_world, m_renderer, m_camera);
    const auto load_result = loader.start(map_path,
                                          build_campaign_player_configs(mission),
                                          k_local_owner,
                                          false,
                                          selected_player_id);
    if (!load_result.ok) {
      if (out_error != nullptr) {
        *out_error = load_result.error_message;
      }
      return false;
    }

    m_level.map_path = map_path;
    m_level.grid_width = load_result.grid_width;
    m_level.grid_height = load_result.grid_height;
    m_level.tile_size = load_result.tile_size;

    if (!Game::Map::MapLoader::load_from_json_file(map_path, m_map, out_error)) {
      return false;
    }
    Game::Session::configure_map_systems(m_world, m_map, nullptr);

    (void)Game::Mission::apply_starting_force_difficulty(
        m_world, m_difficulty, k_local_owner);
    (void)Game::Mission::apply_undead_wave_difficulty(m_world, m_difficulty);
    (void)m_coordinator.apply_mission_setup({.world = m_world,
                                             .campaign = m_campaign,
                                             .level = m_level,
                                             .selected_player_id = selected_player_id,
                                             .local_owner_id = k_local_owner,
                                             .pending_waves = m_waves,
                                             .difficulty = &m_difficulty});

    App::Core::SkirmishRuntimeCoordinator runtime;
    runtime.initialize_player_resources(
        {*m_session, m_level, k_local_owner, &mission, &m_difficulty});
    return true;
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_world; }
  [[nodiscard]] auto map() const -> const Game::Map::MapDefinition& { return m_map; }
  [[nodiscard]] auto mission() const -> const Game::Mission::MissionDefinition& {
    return *m_campaign.current_mission_definition();
  }
  [[nodiscard]] auto undead() -> Game::Systems::UndeadAwakeningSystem* {
    return m_world.get_system<Game::Systems::UndeadAwakeningSystem>();
  }
  [[nodiscard]] auto stockpile() const -> Game::Systems::ResourceAmounts {
    Game::Systems::ResourceAmounts amounts;
    for (const auto type : Game::Systems::k_all_resource_types) {
      amounts.set(type, m_session->economy().get(k_local_owner, type));
    }
    return amounts;
  }

  ~WatchRun() { TestSupport::quiesce_ai(m_world); }
  WatchRun(const WatchRun&) = delete;
  WatchRun(WatchRun&&) = delete;
  auto operator=(const WatchRun&) -> WatchRun& = delete;
  auto operator=(WatchRun&&) -> WatchRun& = delete;

private:
  Game::Mission::MatchDifficulty m_difficulty;
  QString m_mission_path;
  std::unique_ptr<Game::Session::SessionContext> m_session{
      std::make_unique<Game::Session::SessionContext>()};
  Game::Session::ScopedSession m_scope{*m_session};
  Engine::Core::World& m_world{m_session->world()};
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Systems::LevelSnapshot m_level;
  Game::Map::MapDefinition m_map;
  std::vector<Game::Mission::PendingMissionWave> m_waves;
};

auto living_troops(Engine::Core::World& world, int owner_id) -> int {
  int count = 0;
  for (auto* entity : world.collect_entities_with<UnitComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit->owner_id == owner_id && unit->health > 0 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      ++count;
    }
  }
  return count;
}

struct RisingReadout {
  int squads = 0;
  int waves = 0;
};

auto measure_risings(const char* difficulty_id) -> RisingReadout {
  static std::map<std::string, RisingReadout> cache;
  const auto cached = cache.find(difficulty_id);
  if (cached != cache.end()) {
    return cached->second;
  }
  reset_globals();
  RisingReadout readout;
  {
    WatchRun run{QString::fromLatin1(difficulty_id)};
    QString error;
    EXPECT_TRUE(run.run(&error)) << difficulty_id << ": " << error.toStdString();
    auto* undead = run.undead();
    if (undead != nullptr) {
      for (const auto& zone : run.map().undead_zones) {
        for (int wave = 0; wave < static_cast<int>(zone.waves.size()); ++wave) {
          readout.squads += undead->wave_squad_count(zone.id, wave);
          ++readout.waves;
        }
      }
    }
  }
  reset_globals();
  cache.emplace(difficulty_id, readout);
  return readout;
}

class IronSepulcherWatchMissionTest : public ::testing::Test {
protected:
  void SetUp() override { reset_globals(); }
  void TearDown() override { reset_globals(); }
};

TEST_F(IronSepulcherWatchMissionTest, TheWatchIsQuietUntilThePlayerWakesTheDead) {
  WatchRun run{QStringLiteral("normal")};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();
  auto& world = run.world();
  auto* undead = run.undead();
  ASSERT_NE(undead, nullptr);
  const int troops_at_start = living_troops(world, k_local_owner);
  ASSERT_GT(troops_at_start, 0);

  for (double elapsed = 0.0; elapsed < 30.0; elapsed += k_tick) {
    world.update(k_tick);
  }

  for (const auto& marker : undead->shrine_markers()) {
    EXPECT_FALSE(marker.awakened)
        << marker.zone_id.toStdString()
        << " woke with no order given: the watch must start out of reach of the dead";
  }
  EXPECT_EQ(living_troops(world, k_local_owner), troops_at_start)
      << "the player lost troops before giving a single order";
}

TEST_F(IronSepulcherWatchMissionTest, TheOpeningTreasuryCanBuyAMarketplaceOrAnArmy) {
  WatchRun run{QStringLiteral("normal")};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();

  const auto stockpile = run.stockpile();
  const auto marketplace = Game::Systems::construction_cost_info("marketplace");
  EXPECT_TRUE(stockpile.can_cover(marketplace.resource_costs))
      << "the briefing offers a marketplace the opening stockpile cannot pay for";

  Game::Systems::ResourceAmounts marketplace_and_home = marketplace.resource_costs;
  const auto home = Game::Systems::construction_cost_info("home").resource_costs;
  for (const auto type : Game::Systems::k_all_resource_types) {
    marketplace_and_home.add(type, home.get(type));
  }
  EXPECT_TRUE(stockpile.can_cover(marketplace_and_home))
      << "investing in the camp should leave room for a second household";
}

TEST_F(IronSepulcherWatchMissionTest, TheRisingsGrowWithTheChosenDifficulty) {
  const auto easy = measure_risings("easy");
  const auto normal = measure_risings("normal");
  const auto hard = measure_risings("hard");
  const auto brutal = measure_risings("very_hard");

  EXPECT_EQ(easy.waves, normal.waves) << "difficulty must not author extra risings";
  EXPECT_EQ(hard.waves, normal.waves);
  EXPECT_EQ(brutal.waves, normal.waves);
  EXPECT_LT(easy.squads, normal.squads);
  EXPECT_GT(hard.squads, normal.squads);
  EXPECT_GT(brutal.squads, hard.squads);
}

TEST_F(IronSepulcherWatchMissionTest, FabiusAnswersEveryZoneAsItWakesAndFalls) {
  WatchRun run{QStringLiteral("normal")};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();

  using Game::Mission::CommanderMessageTrigger;
  auto has_line = [&run](CommanderMessageTrigger trigger, const QString& zone_id) {
    const auto& lines = run.mission().commander_messages;
    return std::any_of(lines.begin(), lines.end(), [&](const auto& line) {
      return line.trigger == trigger && line.condition.subject_type == zone_id;
    });
  };
  for (const auto& zone : run.map().undead_zones) {
    EXPECT_TRUE(has_line(CommanderMessageTrigger::UndeadAwakened, zone.id))
        << "nobody reacts when " << zone.id.toStdString() << " wakes";
    EXPECT_TRUE(has_line(CommanderMessageTrigger::UndeadCleared, zone.id))
        << "nobody tells the player what to do once " << zone.id.toStdString()
        << " falls";
  }
}

TEST_F(IronSepulcherWatchMissionTest, ACrowdedMusterNeverSpillsIntoTheDead) {

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString map_path = dir.filePath(QStringLiteral("crowded_watch_map.json"));
  const QString mission_path = dir.filePath(QStringLiteral("crowded_watch.json"));
  const QByteArray map = R"({
    "name": "Crowded Watch", "coord_system": "grid", "max_troops_per_player": 630,
    "grid": {"width": 48, "height": 48, "tile_size": 1.0},
    "spawns": [{"type": "roman_legion_organizer", "x": 14.0, "z": 30.0,
                "player_id": 1, "team_id": 1, "nation": "roman_republic"}],
    "structures": [{"type": "barracks", "x": 11, "z": 36, "player_id": 1, "team_id": 1,
                    "nation": "roman_republic", "max_population": 60}],
    "world_props": [{"type": "ruins", "x": 24, "z": 24, "scale": 1.1}],
    "wildlife": {"enabled": false},
    "undead_zones": [{"id": "ruins_guard", "anchor_type": "ruins", "x": 24, "z": 24,
                      "radius": 8.0, "leash_radius": 14.0, "owner_id": 99, "team_id": 99,
                      "awaken_on": ["unit_enters_radius"],
                      "waves": [{"trigger": "initial", "units": {"skeleton_archer": 1}}]}]
  })";
  const QByteArray mission = QByteArray(R"({
    "id": "crowded_watch", "title": "Crowded Watch", "map_path": ")") +
                             map_path.toUtf8() + QByteArray(R"(",
    "player_setup": {"nation": "roman_republic", "faction": "roman", "color": "red",
      "starting_units": [
        {"type": "swordsman", "count": 2, "position": {"x": 10.0, "z": 31.0}},
        {"type": "archer", "count": 1, "position": {"x": 13.0, "z": 29.5}},
        {"type": "builder", "count": 1, "position": {"x": 6.0, "z": 30.0}}]},
    "ai_setups": [],
    "victory_conditions": [{"type": "clear_undead_zone", "zone_id": "ruins_guard"}],
    "defeat_conditions": [{"type": "lose_commander"}]
  })");
  QFile map_file(map_path);
  ASSERT_TRUE(map_file.open(QIODevice::WriteOnly));
  map_file.write(map);
  map_file.close();
  QFile mission_file(mission_path);
  ASSERT_TRUE(mission_file.open(QIODevice::WriteOnly));
  mission_file.write(mission);
  mission_file.close();

  WatchRun run{QStringLiteral("normal"), mission_path};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();
  run.world().update(k_tick);
  for (const auto& marker : run.undead()->shrine_markers()) {
    EXPECT_FALSE(marker.awakened)
        << "mustering the starting force woke " << marker.zone_id.toStdString();
  }
}

TEST_F(IronSepulcherWatchMissionTest, HoldingTheSilentShrineWinsTheWatch) {
  WatchRun run{QStringLiteral("normal")};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();
  auto& world = run.world();
  auto* undead = run.undead();
  ASSERT_NE(undead, nullptr);

  auto& session = Game::Session::SessionContext::active();
  Game::Systems::VictoryService victory({.stats = session.stats(),
                                         .owners = session.owners(),
                                         .nations = session.nations(),
                                         .economy = session.economy()});
  victory.configure(Game::Mission::build_victory_rules(run.mission()), k_local_owner);
  victory.set_undead_zone_query(undead);

  auto commander = [&world]() -> Engine::Core::Entity* {
    for (auto* entity : world.collect_entities_with<UnitComponent>()) {
      const auto* unit = entity->get_component<UnitComponent>();
      if (unit->owner_id == k_local_owner && unit->health > 0 &&
          entity->has_component<Engine::Core::CommanderComponent>()) {
        return entity;
      }
    }
    return nullptr;
  };
  auto stand_at = [&](const QVector3D& spot) {
    auto* entity = commander();
    ASSERT_NE(entity, nullptr);
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    transform->position.x = spot.x();
    transform->position.z = spot.z();
  };
  auto put_down_every_rising = [&](const QString& zone_id, int owner) {
    for (int wave = 0; wave < 4 && !undead->is_zone_cleared(zone_id); ++wave) {
      for (auto* entity : world.collect_entities_with<UnitComponent>()) {
        auto* unit = entity->get_component<UnitComponent>();
        if (unit->owner_id == owner &&
            !entity->has_component<Engine::Core::BuildingComponent>()) {
          unit->health = 0;
        }
      }
      for (int tick = 0; tick < 30 * 60 && !undead->is_zone_cleared(zone_id); ++tick) {
        undead->update(&world, k_tick);
        if (!undead->is_zone_cleared(zone_id) && tick % 60 == 0 &&
            undead->completed_wave_count(zone_id) > wave) {
          break;
        }
      }
    }
  };

  for (const auto& zone : run.map().undead_zones) {
    stand_at(undead->shrine_world_position(zone.id));
    undead->update(&world, k_tick);
    put_down_every_rising(zone.id, zone.owner_id);
    ASSERT_TRUE(undead->is_zone_cleared(zone.id))
        << zone.id.toStdString() << " never ran out of risings";
  }
  victory.update(world, k_tick);
  EXPECT_FALSE(victory.is_game_over())
      << "a silent shrine still has to be taken before the watch is over";

  stand_at(undead->shrine_world_position(QStringLiteral("shrine_sentinels")));
  Game::Systems::CaptureSystem capture;
  for (double t = 0.0; t < 20.0 && !victory.is_game_over(); t += 0.25) {
    capture.update(&world, 0.25F);
    undead->update(&world, 0.25F);
    victory.update(world, 0.25F);
  }
  EXPECT_TRUE(undead->is_shrine_purified(QStringLiteral("shrine_sentinels")));
  EXPECT_EQ(victory.get_victory_state(), QStringLiteral("victory"));
}

} // namespace

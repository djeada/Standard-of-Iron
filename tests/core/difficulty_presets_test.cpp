#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "core/component_commander.h"
#include "core/component_structures.h"
#include "core/world.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/wave_archetype_catalog.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/difficulty_forces.h"
#include "game/mission/difficulty_profile.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "tests/support/ai_quiesce.h"

namespace {

using Engine::Core::UnitComponent;

constexpr char k_mission_path[] = "assets/missions/battle_of_ticino.json";
constexpr int k_local_owner = 1;

struct MatchReadout {

  int player_troops = 0;

  std::map<int, int> enemy_troops_by_owner;

  int enemy_commanders = 0;

  std::map<int, int> enemy_gold_by_owner;

  int player_gold = 0;

  int wave_units = 0;

  int wave_count = 0;

  std::map<int, Game::Systems::AISystem::AIPlayerState> ai_by_owner;

  int global_max_troops = 0;
  int global_starting_gold = 0;

  std::vector<std::pair<QVector3D, float>> enemy_troop_footprints;

  std::map<QString, int> wave_roles;

  Game::Mission::DifficultyForceResult forces;
};

class MissionRun {
public:
  explicit MissionRun(const QString& difficulty_id,
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

    m_forces = Game::Mission::apply_starting_force_difficulty(
        m_world, m_difficulty, k_local_owner);

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

  [[nodiscard]] auto readout() -> MatchReadout {
    MatchReadout readout;
    for (auto* entity : m_world.collect_entities_with<UnitComponent>()) {
      const auto* unit = entity->get_component<UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        continue;
      }
      const bool commander = entity->has_component<Engine::Core::CommanderComponent>();
      if (unit->owner_id == k_local_owner) {
        if (Game::Units::is_troop_spawn(unit->spawn_type) && !commander) {
          ++readout.player_troops;
        }
        continue;
      }
      if (commander) {
        ++readout.enemy_commanders;
        continue;
      }
      if (Game::Units::is_troop_spawn(unit->spawn_type) &&
          !entity->has_component<Engine::Core::BuildingComponent>()) {
        ++readout.enemy_troops_by_owner[unit->owner_id];
      }
    }

    auto& economy = m_session->economy();
    readout.player_gold = economy.get(k_local_owner, Game::Systems::ResourceType::Gold);
    for (const auto& owner_id : m_session->owners().get_ai_owner_ids()) {
      readout.enemy_gold_by_owner[owner_id] =
          economy.get(owner_id, Game::Systems::ResourceType::Gold);
    }

    readout.forces = m_forces;
    readout.wave_count = static_cast<int>(m_waves.size());
    for (const auto& wave : m_waves) {
      readout.wave_units += Game::Mission::wave_unit_total(wave);
      for (const auto& comp : wave.composition) {
        readout.wave_roles[comp.type] += std::max(1, comp.count);
      }
    }

    if (auto* ai_system = m_world.get_system<Game::Systems::AISystem>()) {
      for (const auto& owner_id : m_session->owners().get_ai_owner_ids()) {
        const auto state = ai_system->ai_player_state(owner_id);
        if (state.valid) {
          readout.ai_by_owner[owner_id] = state;
        }
      }
    }

    readout.global_max_troops =
        Game::GameConfig::instance().get_max_troops_per_player();
    readout.global_starting_gold = Game::GameConfig::instance().get_starting_gold();

    for (auto* entity : m_world.collect_entities_with<UnitComponent>()) {
      const auto* unit = entity->get_component<UnitComponent>();
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
          unit->owner_id == k_local_owner ||
          !Game::Units::is_troop_spawn(unit->spawn_type) ||
          entity->has_component<Engine::Core::BuildingComponent>()) {
        continue;
      }
      readout.enemy_troop_footprints.emplace_back(
          QVector3D(transform->position.x, 0.0F, transform->position.z),
          Game::Systems::CommandService::get_unit_radius(m_world, entity->get_id()));
    }
    return readout;
  }

  [[nodiscard]] auto forces() const -> Game::Mission::DifficultyForceResult {
    return m_forces;
  }

  void endow_again() {
    App::Core::SkirmishRuntimeCoordinator runtime;
    runtime.initialize_player_resources(
        {*m_session,
         m_level,
         k_local_owner,
         m_campaign.current_mission_definition().has_value()
             ? &*m_campaign.current_mission_definition()
             : nullptr,
         &m_difficulty});
  }

  ~MissionRun() { TestSupport::quiesce_ai(m_world); }

  MissionRun(const MissionRun&) = delete;
  MissionRun(MissionRun&&) = delete;
  auto operator=(const MissionRun&) -> MissionRun& = delete;
  auto operator=(MissionRun&&) -> MissionRun& = delete;

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
  std::vector<Game::Mission::PendingMissionWave> m_waves;
  Game::Mission::DifficultyForceResult m_forces;
};

auto total(const std::map<int, int>& by_owner) -> int {
  return std::accumulate(by_owner.begin(),
                         by_owner.end(),
                         0,
                         [](int sum, const auto& entry) { return sum + entry.second; });
}

class DifficultyPresetsTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    for (const char* id : {"normal", "easy", "hard", "very_hard"}) {
      (void)measure(id);
    }
  }

  void SetUp() override { reset_globals(); }
  void TearDown() override { reset_globals(); }

  static void reset_globals() {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto measure(const char* difficulty_id) -> const MatchReadout& {

    static std::map<std::string, MatchReadout> cache;
    const std::string key{difficulty_id};
    const auto cached = cache.find(key);
    if (cached != cache.end()) {
      return cached->second;
    }

    reset_globals();
    MissionRun run{QString::fromLatin1(difficulty_id)};
    QString error;
    EXPECT_TRUE(run.run(&error)) << difficulty_id << ": " << error.toStdString();
    const auto inserted = cache.emplace(key, run.readout());
    reset_globals();
    return inserted.first->second;
  }
};

TEST_F(DifficultyPresetsTest, HardAndBrutalScaleEveryEnemyCategoryAtOnce) {
  const MatchReadout& normal = measure("normal");
  ASSERT_GT(total(normal.enemy_troops_by_owner), 0)
      << "the fixture mission fields no enemy troops to scale";
  ASSERT_GT(normal.wave_units, 0);
  ASSERT_GT(total(normal.enemy_gold_by_owner), 0);

  const MatchReadout& hard = measure("hard");
  const MatchReadout& brutal = measure("very_hard");

  for (const auto& [owner_id, troops] : normal.enemy_troops_by_owner) {
    EXPECT_EQ(hard.enemy_troops_by_owner.at(owner_id),
              Game::Mission::scaled_force_count(troops, 1.5F))
        << "owner " << owner_id;
    EXPECT_EQ(brutal.enemy_troops_by_owner.at(owner_id),
              Game::Mission::scaled_force_count(troops, 2.0F))
        << "owner " << owner_id;
  }

  for (const auto& [owner_id, gold] : normal.enemy_gold_by_owner) {
    EXPECT_EQ(hard.enemy_gold_by_owner.at(owner_id),
              Game::Mission::scaled_resource_amount(gold, 1.5F));
    EXPECT_EQ(brutal.enemy_gold_by_owner.at(owner_id),
              Game::Mission::scaled_resource_amount(gold, 2.0F));
  }

  EXPECT_GT(hard.wave_units, normal.wave_units);
  EXPECT_GT(brutal.wave_units, hard.wave_units);
  EXPECT_EQ(hard.wave_count, normal.wave_count)
      << "difficulty must not author extra waves";
  EXPECT_EQ(brutal.wave_count, normal.wave_count);
}

TEST_F(DifficultyPresetsTest, EasyThinsTheEnemyWithoutEmptyingIt) {
  const MatchReadout& normal = measure("normal");
  const MatchReadout& easy = measure("easy");

  for (const auto& [owner_id, troops] : normal.enemy_troops_by_owner) {
    EXPECT_EQ(easy.enemy_troops_by_owner.at(owner_id),
              Game::Mission::scaled_force_count(troops, 0.8F))
        << "owner " << owner_id;
    EXPECT_GT(easy.enemy_troops_by_owner.at(owner_id), 0) << "owner " << owner_id;
  }
  EXPECT_LT(total(easy.enemy_troops_by_owner), total(normal.enemy_troops_by_owner));
  EXPECT_LT(easy.wave_units, normal.wave_units);
  for (const auto& [owner_id, gold] : normal.enemy_gold_by_owner) {
    EXPECT_EQ(easy.enemy_gold_by_owner.at(owner_id),
              Game::Mission::scaled_resource_amount(gold, 0.8F));
  }
}

TEST_F(DifficultyPresetsTest, ThePlayersOwnForceAndTreasuryNeverMove) {
  const MatchReadout& normal = measure("normal");
  for (const char* id : {"easy", "hard", "very_hard"}) {
    const MatchReadout& other = measure(id);
    EXPECT_EQ(other.player_troops, normal.player_troops) << id;
    EXPECT_EQ(other.player_gold, normal.player_gold) << id;
  }
}

TEST_F(DifficultyPresetsTest, ScriptedCommandersAreNeverDuplicated) {
  const MatchReadout& normal = measure("normal");
  for (const char* id : {"easy", "hard", "very_hard"}) {
    const MatchReadout& other = measure(id);
    EXPECT_EQ(other.enemy_commanders, normal.enemy_commanders) << id;
  }
}

TEST_F(DifficultyPresetsTest, NormalLeavesTheAuthoredMatchExactlyAsItWas) {
  const MatchReadout& normal = measure("normal");
  EXPECT_EQ(normal.forces.owners_scaled, 0);
  EXPECT_EQ(normal.forces.units_added, 0);
  EXPECT_EQ(normal.forces.units_withdrawn, 0);

  const MatchReadout& hard = measure("hard");
  EXPECT_GT(hard.forces.units_added, 0);
  EXPECT_EQ(hard.forces.units_withdrawn, 0);

  const MatchReadout& easy = measure("easy");
  EXPECT_EQ(easy.forces.units_added, 0);
  EXPECT_GT(easy.forces.units_withdrawn, 0);
}

TEST_F(DifficultyPresetsTest, NoPresetChangesHowTheOpponentThinks) {
  const MatchReadout& normal = measure("normal");
  ASSERT_FALSE(normal.ai_by_owner.empty())
      << "the fixture mission starts no AI to compare";

  for (const char* id : {"easy", "hard", "very_hard"}) {
    const MatchReadout& other = measure(id);
    ASSERT_EQ(other.ai_by_owner.size(), normal.ai_by_owner.size()) << id;
    for (const auto& [owner_id, baseline] : normal.ai_by_owner) {
      const auto& actual = other.ai_by_owner.at(owner_id);
      EXPECT_EQ(actual.difficulty_level, baseline.difficulty_level)
          << id << " rewrote owner " << owner_id << "'s authored difficulty";
      EXPECT_FLOAT_EQ(actual.update_interval_multiplier,
                      baseline.update_interval_multiplier)
          << id << " changed how often owner " << owner_id << " thinks";
      EXPECT_FLOAT_EQ(actual.production_rate_multiplier,
                      baseline.production_rate_multiplier)
          << id << " changed owner " << owner_id << "'s production rate";
      EXPECT_FLOAT_EQ(actual.scouting_distance_multiplier,
                      baseline.scouting_distance_multiplier)
          << id << " changed owner " << owner_id << "'s scouting range";
      EXPECT_EQ(actual.strategy, baseline.strategy) << id;
      EXPECT_EQ(actual.posture, baseline.posture) << id;
      EXPECT_FLOAT_EQ(actual.aggression_modifier, baseline.aggression_modifier) << id;
      EXPECT_FLOAT_EQ(actual.defense_modifier, baseline.defense_modifier) << id;
      EXPECT_EQ(actual.proactive_attack_size, baseline.proactive_attack_size) << id;
      EXPECT_EQ(actual.reactive_attack_size, baseline.reactive_attack_size) << id;
    }
  }
}

TEST_F(DifficultyPresetsTest, NoPresetWritesToTheGlobalGameConfig) {
  const MatchReadout& normal = measure("normal");
  for (const char* id : {"easy", "hard", "very_hard"}) {
    const MatchReadout& other = measure(id);
    EXPECT_EQ(other.global_max_troops, normal.global_max_troops)
        << id << " moved the global population cap";
    EXPECT_EQ(other.global_starting_gold, normal.global_starting_gold)
        << id << " moved the global starting purse";
  }
}

TEST_F(DifficultyPresetsTest, AnAuthoredVeryHardOpponentIsUntouchedAtNormal) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());

  const auto write_mission = [&directory](const char* name, const char* authored) {
    const QString path = directory.filePath(QLatin1String(name));
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral(R"({
      "id": "authored_dial",
      "title": "Authored Dial",
      "map_path": ":/assets/maps/map_battle_ticino.json",
      "player_setup": {"nation": "carthage", "starting_resources": {"gold": 300}},
      "ai_setups": [
        {
          "id": "roman_column",
          "nation": "roman_republic",
          "team_id": 2,
          "difficulty": "%1",
          "starting_units": [
            {"type": "spearman", "count": 4, "position": {"x": 40, "z": 30}}
          ],
          "waves": [
            {
              "timing": 60,
              "strength": 1.0,
              "composition": [
                {"type": "spearman", "count": 5},
                {"type": "archer", "count": 3}
              ]
            }
          ]
        }
      ]
    })")
                   .arg(QLatin1String(authored))
                   .toUtf8());
    return path;
  };

  const QString authored_normal = write_mission("normal_dial.json", "normal");
  const QString authored_very_hard = write_mission("very_hard_dial.json", "very_hard");

  const auto run = [](const char* preset, const QString& path) {
    reset_globals();
    MissionRun mission{QString::fromLatin1(preset), path};
    QString error;
    EXPECT_TRUE(mission.run(&error)) << preset << ": " << error.toStdString();
    MatchReadout readout = mission.readout();
    reset_globals();
    return readout;
  };

  const MatchReadout authored_hard_at_normal = run("normal", authored_very_hard);
  const MatchReadout authored_hard_at_normal_again = run("normal", authored_very_hard);
  EXPECT_EQ(authored_hard_at_normal.wave_roles,
            authored_hard_at_normal_again.wave_roles)
      << "the authored baseline is not even stable against itself";

  const MatchReadout authored_plain_at_normal = run("normal", authored_normal);
  EXPECT_GT(authored_hard_at_normal.wave_units, authored_plain_at_normal.wave_units)
      << "the authored ai_setups[].difficulty stopped doing anything";

  const MatchReadout authored_hard_at_brutal = run("very_hard", authored_very_hard);
  for (const auto& [role, baseline] : authored_hard_at_normal.wave_roles) {
    EXPECT_EQ(
        authored_hard_at_brutal.wave_roles.at(role),
        Game::Mission::scale_wave_composition(
            {{.type = role, .count = baseline, .elite = false, .title = {}}}, 2.0F)
            .front()
            .count)
        << "role " << role.toStdString() << " was not scaled exactly once";
  }
}

TEST_F(DifficultyPresetsTest, AnAlliedAiIsLeftExactlyAsTheMissionAuthoredIt) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("allied_field.json"));
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(R"({
      "id": "allied_field",
      "title": "Allied Field",
      "map_path": ":/assets/maps/map_battle_ticino.json",
      "player_setup": {
        "nation": "carthage",
        "team_id": 1,
        "starting_resources": {"gold": 400}
      },
      "ai_setups": [
        {
          "id": "allied_column",
          "nation": "carthage",
          "team_id": 1,
          "starting_units": [
            {"type": "spearman", "count": 4, "position": {"x": 24, "z": 20}}
          ]
        },
        {
          "id": "roman_column",
          "nation": "roman_republic",
          "team_id": 2,
          "starting_units": [
            {"type": "spearman", "count": 4, "position": {"x": 40, "z": 30}}
          ]
        }
      ]
    })");
  }

  const auto measure_run = [&path](const char* difficulty_id) {
    reset_globals();
    MissionRun run{QString::fromLatin1(difficulty_id), path};
    QString error;
    EXPECT_TRUE(run.run(&error)) << difficulty_id << ": " << error.toStdString();
    MatchReadout readout = run.readout();
    reset_globals();
    return readout;
  };

  constexpr int k_ally = 2;
  constexpr int k_foe = 3;
  const MatchReadout normal = measure_run("normal");
  const MatchReadout brutal = measure_run("very_hard");

  ASSERT_GT(normal.enemy_troops_by_owner.count(k_ally), 0U);
  ASSERT_GT(normal.enemy_troops_by_owner.count(k_foe), 0U);

  EXPECT_EQ(brutal.enemy_troops_by_owner.at(k_ally),
            normal.enemy_troops_by_owner.at(k_ally))
      << "Brutal reinforced an AI fighting on the player's side";
  EXPECT_EQ(brutal.enemy_gold_by_owner.at(k_ally),
            normal.enemy_gold_by_owner.at(k_ally))
      << "Brutal filled an allied treasury";

  EXPECT_GT(brutal.enemy_troops_by_owner.at(k_foe),
            normal.enemy_troops_by_owner.at(k_foe));
  EXPECT_EQ(brutal.enemy_gold_by_owner.at(k_foe),
            Game::Mission::scaled_resource_amount(normal.enemy_gold_by_owner.at(k_foe),
                                                  2.0F));

  EXPECT_EQ(brutal.player_troops, normal.player_troops);
  EXPECT_EQ(brutal.player_gold, normal.player_gold);
}

TEST_F(DifficultyPresetsTest, ReturningToNormalReturnsTheOriginalMatch) {
  reset_globals();
  const auto snapshot = [](const char* id) {
    reset_globals();
    MissionRun run{QString::fromLatin1(id)};
    QString error;
    EXPECT_TRUE(run.run(&error)) << id << ": " << error.toStdString();
    MatchReadout readout = run.readout();
    reset_globals();
    return readout;
  };

  const MatchReadout first = snapshot("normal");
  (void)snapshot("hard");
  (void)snapshot("very_hard");
  const MatchReadout last = snapshot("normal");

  EXPECT_EQ(last.enemy_troops_by_owner, first.enemy_troops_by_owner);
  EXPECT_EQ(last.enemy_gold_by_owner, first.enemy_gold_by_owner);
  EXPECT_EQ(last.player_troops, first.player_troops);
  EXPECT_EQ(last.player_gold, first.player_gold);
  EXPECT_EQ(last.wave_units, first.wave_units);
  EXPECT_EQ(last.wave_roles, first.wave_roles);
  EXPECT_EQ(last.enemy_commanders, first.enemy_commanders);
  EXPECT_EQ(last.global_max_troops, first.global_max_troops);
  EXPECT_EQ(last.global_starting_gold, first.global_starting_gold);
  EXPECT_EQ(last.forces.units_added, 0);
  EXPECT_EQ(last.forces.units_withdrawn, 0);
}

TEST_F(DifficultyPresetsTest, ReinforcementsAddNoOverlappingTroops) {
  const auto overlapping_pairs = [](const MatchReadout& readout) {
    int overlaps = 0;
    const auto& troops = readout.enemy_troop_footprints;
    for (std::size_t i = 0; i < troops.size(); ++i) {
      for (std::size_t j = i + 1; j < troops.size(); ++j) {
        const float gap = (troops[i].first - troops[j].first).length();
        if (gap < (troops[i].second + troops[j].second) * 0.5F) {
          ++overlaps;
        }
      }
    }
    return overlaps;
  };

  const MatchReadout& normal = measure("normal");
  ASSERT_FALSE(normal.enemy_troop_footprints.empty());
  const int authored_overlaps = overlapping_pairs(normal);

  for (const char* id : {"hard", "very_hard"}) {
    const MatchReadout& readout = measure(id);
    EXPECT_GT(readout.enemy_troop_footprints.size(),
              normal.enemy_troop_footprints.size())
        << id << " added no troops at all, so this proves nothing";
    EXPECT_EQ(overlapping_pairs(readout), authored_overlaps)
        << id << " stacked a reinforcement on ground that was already taken";
  }
}

TEST_F(DifficultyPresetsTest, WhatCouldNotBePlacedIsCountedNotHidden) {
  const MatchReadout& brutal = measure("very_hard");
  const MatchReadout& normal = measure("normal");

  const int authored = total(normal.enemy_troops_by_owner);
  const int arrived = total(brutal.enemy_troops_by_owner);
  EXPECT_EQ(arrived + brutal.forces.units_capped,
            authored + brutal.forces.units_added + brutal.forces.units_capped)
      << "the books do not balance: asked for "
      << (brutal.forces.units_added + brutal.forces.units_capped) << ", placed "
      << brutal.forces.units_added << ", reported short " << brutal.forces.units_capped;
  EXPECT_EQ(normal.forces.units_capped, 0);
}

TEST_F(DifficultyPresetsTest, TwoSourcesOfStartingTroopsAreEachScaledOnce) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("two_sources.json"));
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));

    file.write(R"({
      "id": "two_sources",
      "title": "Two Sources",
      "map_path": ":/assets/maps/map_battle_ticino.json",
      "player_setup": {"nation": "carthage", "starting_resources": {"gold": 300}},
      "ai_setups": [
        {
          "id": "roman_column",
          "nation": "roman_republic",
          "team_id": 2,
          "starting_units": [
            {"type": "spearman", "count": 6, "position": {"x": 40, "z": 30}}
          ]
        }
      ]
    })");
  }

  const auto run = [&path](const char* preset) {
    reset_globals();
    MissionRun mission{QString::fromLatin1(preset), path};
    QString error;
    EXPECT_TRUE(mission.run(&error)) << preset << ": " << error.toStdString();
    MatchReadout readout = mission.readout();
    reset_globals();
    return readout;
  };

  const MatchReadout normal = run("normal");
  const MatchReadout brutal = run("very_hard");

  const int authored = total(normal.enemy_troops_by_owner);
  const int doubled = total(brutal.enemy_troops_by_owner);
  ASSERT_GT(authored, 6) << "the map contributed nothing, so this proves nothing";

  EXPECT_EQ(doubled, authored * 2)
      << "each starting-force source must double exactly once: " << authored
      << " became " << doubled;
  EXPECT_LT(doubled, authored * 3) << "the preset was applied more than once";
}

TEST_F(DifficultyPresetsTest, TheStartingGrantSetsAndDoesNotAccumulate) {
  reset_globals();
  MissionRun run{QStringLiteral("very_hard")};
  QString error;
  ASSERT_TRUE(run.run(&error)) << error.toStdString();

  const MatchReadout once = run.readout();
  run.endow_again();
  const MatchReadout twice = run.readout();

  EXPECT_EQ(twice.enemy_gold_by_owner, once.enemy_gold_by_owner)
      << "a second endowment compounded the opponent's treasury";
  EXPECT_EQ(twice.player_gold, once.player_gold);
  reset_globals();
}

TEST_F(DifficultyPresetsTest, MixedOpponentsEachGetTheirOwnBonus) {
  reset_globals();
  Game::Mission::MatchDifficulty difficulty;
  difficulty.set_owner(2, QStringLiteral("hard"));
  difficulty.set_owner(3, QStringLiteral("normal"));

  EXPECT_FLOAT_EQ(difficulty.profile_for(2).starting_unit_multiplier, 1.5F);
  EXPECT_FLOAT_EQ(difficulty.profile_for(3).starting_unit_multiplier, 1.0F);
  EXPECT_FLOAT_EQ(difficulty.profile_for(1).starting_unit_multiplier, 1.0F);
}

} // namespace

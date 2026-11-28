#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <chrono>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/world.h"
#include "game/core/component.h"
#include "game/core/component_commander.h"
#include "game/core/component_gameplay.h"
#include "game/map/map_context.h"
#include "game/map/mission_victory_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_runtime.h"
#include "game/save/serialization.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/walkability.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/victory_service.h"
#include "render/scene/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr char k_mission_file[] = "assets/missions/hold_the_sallow_ford.json";
constexpr char k_convoy_mission[] = "assets/missions/the_salt_road.json";
constexpr int k_local_owner = 1;
constexpr float k_tick = 1.0F / 60.0F;

class MissionStartupTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Map::MapContextStore::clear();
    Game::Map::MapContextStore::reset_statistics();
  }

  auto start_convoy(Game::Systems::VictoryService& victory)
      -> Game::Mission::MissionWaveBinding {
    const Game::Mission::MissionWaveBinding binding{.world = &m_world,
                                                    .level = &m_level,
                                                    .campaign = &m_campaign,
                                                    .victory_service = &victory,
                                                    .local_owner_id = k_local_owner};
    m_waves.bind_after_setup(binding, std::move(m_pending_waves), {});
    const auto& mission = *m_campaign.current_mission_definition();
    victory.configure(Game::Mission::build_victory_rules(mission), k_local_owner);
    return binding;
  }

  auto victory_service() -> Game::Systems::VictoryService {
    auto& session = Game::Session::session_for(m_world);
    return Game::Systems::VictoryService({.stats = session.stats(),
                                          .owners = session.owners(),
                                          .nations = session.nations(),
                                          .economy = session.economy()});
  }

  auto wagon_ids() -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    for (auto [id, unit] : m_world.view<const Engine::Core::UnitComponent>()) {
      if (unit.spawn_type == Game::Units::SpawnType::SupplyWagon) {
        ids.push_back(id);
      }
    }
    return ids;
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Map::MapContextStore::clear();
    Game::Map::MapContextStore::reset_statistics();
  }

  [[nodiscard]] auto
  prepare_mission(const char* mission_file = k_mission_file) -> bool {
    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    if (!m_campaign.start_mission_file(
            QString::fromLatin1(mission_file), selected_player_id, &m_error)) {
      return false;
    }
    if (!m_campaign.current_mission_definition().has_value()) {
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
      return false;
    }

    m_level.map_path = map_path;
    m_level.grid_width = load_result.grid_width;
    m_level.grid_height = load_result.grid_height;
    m_level.tile_size = load_result.tile_size;

    (void)m_coordinator.apply_mission_setup({.world = m_world,
                                             .campaign = m_campaign,
                                             .level = m_level,
                                             .selected_player_id = selected_player_id,
                                             .local_owner_id = k_local_owner,
                                             .pending_waves = m_pending_waves});
    return true;
  }

  QString m_error;
  Game::Session::SessionContext m_session;
  Game::Session::ScopedSession m_session_scope{m_session};
  Engine::Core::World& m_world = m_session.world();
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Systems::LevelSnapshot m_level;
  std::vector<Game::Mission::PendingMissionWave> m_pending_waves;
  Game::Mission::MissionWaveRuntime m_waves;
};

TEST_F(MissionStartupTest, ParsesTheMissionMapOnce) {
  ASSERT_TRUE(prepare_mission()) << m_error.toStdString();

  const auto after_setup = Game::Map::MapContextStore::statistics();
  EXPECT_EQ(after_setup.parses, 1U)
      << "mission startup reparsed the mission map " << after_setup.parses
      << " times; every consumer must reuse Game::Map::MapContextStore";
  EXPECT_GT(after_setup.reuses, 0U)
      << "no consumer reused the loaded map context - the store is not on the "
         "startup path";

  const auto position_to_world = Game::Mission::make_mission_position_to_world(m_level);
  (void)position_to_world({.x = 0.0F, .z = 0.0F});
  (void)Game::Mission::commander_troops_for_map(m_level.map_path);

  const auto after_consumers = Game::Map::MapContextStore::statistics();
  EXPECT_EQ(after_consumers.parses, 1U)
      << "a mission subsystem reparsed the map for coordinate conversion or "
         "metadata";
}

TEST_F(MissionStartupTest, InitialAiSnapshotsAreBuiltBeforeTheFirstPlayableFrame) {
  ASSERT_TRUE(prepare_mission()) << m_error.toStdString();

  auto* ai_system = m_world.get_system<Game::Systems::AISystem>();
  ASSERT_NE(ai_system, nullptr);
  ASSERT_GT(ai_system->ai_player_count(), 0U);
  EXPECT_FALSE(ai_system->initial_decisions_ready())
      << "initial AI state must not report ready before it is prepared";

  ai_system->prepare_initial_decisions(m_world);
  EXPECT_TRUE(ai_system->await_initial_decisions(std::chrono::milliseconds(5000)));
  EXPECT_TRUE(ai_system->initial_decisions_ready());

  const auto prepared_snapshots = ai_system->snapshot_build_count();
  EXPECT_EQ(prepared_snapshots, ai_system->ai_player_count());

  for (int step = 0; step < 12; ++step) {
    m_world.update(k_tick);
  }

  EXPECT_EQ(ai_system->snapshot_build_count(), prepared_snapshots)
      << "the first playable frames rebuilt AI world snapshots that the loading "
         "phase had already produced";
  EXPECT_GT(ai_system->completed_decision_count(), 0U)
      << "the decisions prepared during loading were never applied";
}

TEST_F(MissionStartupTest, StartingUnitsStandApartOnOpenGround) {

  ASSERT_TRUE(prepare_mission("assets/missions/the_timber_levy.json"))
      << m_error.toStdString();

  struct Placed {
    Engine::Core::EntityID id;
    QVector3D root;
    float radius;
  };
  std::vector<Placed> units;
  for (auto* entity : m_world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->owner_id != k_local_owner ||
        entity->has_component<Engine::Core::BuildingComponent>() ||
        !entity->has_component<Engine::Core::MovementComponent>()) {
      continue;
    }
    units.push_back(
        {entity->get_id(),
         QVector3D(transform->position.x, 0.0F, transform->position.z),
         Game::Systems::CommandService::get_unit_radius(m_world, entity->get_id())});
  }
  ASSERT_GE(units.size(), 9U);

  const Game::Systems::BodyProfile point_body;
  for (std::size_t first = 0; first < units.size(); ++first) {
    EXPECT_TRUE(Game::Systems::Walkability::can_stand(units[first].root, point_body))
        << "unit " << units[first].id << " starts on ground it cannot stand on at ("
        << units[first].root.x() << ", " << units[first].root.z() << ")";
    for (std::size_t second = first + 1; second < units.size(); ++second) {
      const float gap = (units[first].root - units[second].root).length() -
                        units[first].radius - units[second].radius;
      EXPECT_GE(gap, -0.05F) << "units " << units[first].id << " and "
                             << units[second].id << " start overlapping by " << -gap
                             << " m";
    }
  }
}

TEST_F(MissionStartupTest, StartingUnitsCanFieldCarthagesAllies) {
  QFile source(QString::fromLatin1(k_mission_file));
  ASSERT_TRUE(source.open(QIODevice::ReadOnly));
  QJsonObject root = QJsonDocument::fromJson(source.readAll()).object();
  QJsonObject player = root.value(QStringLiteral("player_setup")).toObject();
  player[QStringLiteral("nation")] = QStringLiteral("carthage");
  QJsonArray units = player.value(QStringLiteral("starting_units")).toArray();
  units.append(QJsonObject{{"type", "swordsman"},
                           {"count", 1},
                           {"nation", "gauls"},
                           {"position", QJsonObject{{"x", 36.0}, {"z", 66.0}}}});
  units.append(QJsonObject{{"type", "horse_swordsman"},
                           {"count", 1},
                           {"nation", "iberians"},
                           {"position", QJsonObject{{"x", 42.0}, {"z", 66.0}}}});
  player[QStringLiteral("starting_units")] = units;
  root[QStringLiteral("player_setup")] = player;

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("sallow_ford_with_allies.json"));
  QFile out(path);
  ASSERT_TRUE(out.open(QIODevice::WriteOnly));
  out.write(QJsonDocument(root).toJson());
  out.close();

  ASSERT_TRUE(prepare_mission(path.toUtf8().constData())) << m_error.toStdString();

  int gallic_swordsmen = 0;
  int iberian_riders = 0;
  int carthaginians = 0;
  for (auto* entity : m_world.collect_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (unit == nullptr || unit->owner_id != k_local_owner ||
        entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    switch (unit->nation_id) {
    case Game::Systems::NationID::Gauls:
      EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Swordsman);
      ASSERT_NE(renderable, nullptr);
      EXPECT_EQ(renderable->renderer_id, "troops/gauls/swordsman");
      ++gallic_swordsmen;
      break;
    case Game::Systems::NationID::Iberians:
      EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::MountedSwordsman);
      ASSERT_NE(renderable, nullptr);
      EXPECT_EQ(renderable->renderer_id, "troops/iberians/horse_swordsman");
      ++iberian_riders;
      break;
    case Game::Systems::NationID::Carthage:
      ++carthaginians;
      break;
    default:
      break;
    }
  }
  EXPECT_EQ(gallic_swordsmen, 1);
  EXPECT_EQ(iberian_riders, 1);
  EXPECT_GT(carthaginians, 0) << "the rest of the army keeps the owner's nation";
}

TEST_F(MissionStartupTest, SaltRoadCheckpointsAndReliefSurviveASaveRoundTrip) {
  using namespace Engine::Core;
  ASSERT_TRUE(prepare_mission(k_convoy_mission)) << m_error.toStdString();
  auto victory = victory_service();
  const auto binding = start_convoy(victory);
  const auto& definition = *m_campaign.current_mission_definition();
  ASSERT_TRUE(definition.start_in_commander_mode);
  ASSERT_TRUE(definition.convoy.has_value());
  EXPECT_EQ(wagon_ids().size(), 3U);
  EXPECT_EQ(m_waves.director().total_wave_count(), 3);

  const auto world_position = Game::Mission::make_mission_position_to_world(m_level);
  for (std::size_t i = 1; i < definition.convoy->route.size(); ++i) {
    const auto start = world_position(definition.convoy->route[i - 1].position);
    const auto end = world_position(definition.convoy->route[i].position);
    for (int j = 0; j <= 10; ++j) {
      EXPECT_TRUE(Game::Systems::Walkability::can_stand(
          start + (end - start) * (static_cast<float>(j) / 10.0F), {}))
          << "the escort needs walkable ground on route segment " << i;
    }
  }

  const auto advance = [&](float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 10.0F); ++i) {
      (void)m_waves.advance(binding, 0.1F, false);
    }
  };
  advance(24.0F);
  const auto halted = m_waves.convoy_status();
  EXPECT_TRUE(halted.value("holding").toBool());
  EXPECT_EQ(halted.value("destination").toString(), "The Broken Arch");
  EXPECT_EQ(m_waves.status().value("live_enemies").toInt(), 2);
  ASSERT_GT(halted.value("relief_eta").toInt(), 0);

  const auto original_wagons = wagon_ids();
  m_world.try_get<UnitComponent>(original_wagons.front())->health -= 160;
  const QJsonObject mission_state = m_waves.serialize();
  const QJsonDocument world_state = Serialization::serialize_world(&m_world);
  m_world.clear();
  Serialization::deserialize_world(&m_world, world_state);
  m_waves.restore(binding, mission_state);
  EXPECT_EQ(wagon_ids().size(), 3U);
  EXPECT_EQ(m_world.try_get<UnitComponent>(original_wagons.front())->health,
            definition.convoy->wagon_health - 160);
  EXPECT_EQ(m_waves.convoy_status().value("fraction"), halted.value("fraction"));
  EXPECT_EQ(m_waves.convoy_status().value("relief_eta"), halted.value("relief_eta"));

  advance(14.0F);
  EXPECT_EQ(m_waves.convoy_status().value("fraction"), halted.value("fraction"));
  EXPECT_FALSE(m_waves.convoy_status().contains("relief_eta"));
  EXPECT_EQ(m_waves.status().value("live_enemies").toInt(), 2);
  EXPECT_EQ(m_waves.director().cleared_wave_count(), 0);
  int escorts = 0;
  for (auto [id, unit, guard] :
       m_world.view<const UnitComponent, const GuardModeComponent>()) {
    if (unit.owner_id == k_local_owner && guard.active) {
      const auto* wagon = m_world.try_get<UnitComponent>(guard.guarded_entity_id);
      ASSERT_NE(wagon, nullptr);
      EXPECT_EQ(wagon->spawn_type, Game::Units::SpawnType::SupplyWagon);
      ASSERT_NE(m_world.try_get<TransformComponent>(id), nullptr);
      ++escorts;
    }
  }
  EXPECT_EQ(escorts, 5);

  // A clear leaves the column free to advance. Loss of its lead wagon still
  // leaves enough cargo for victory and the escort must attach to survivors.
  m_world.destroy_entity(original_wagons.front());
  for (int i = 0; i < 1800 && !victory.is_game_over(); ++i) {
    for (auto [id, unit, assault] :
         m_world.view<UnitComponent, const AssaultWaveComponent>()) {
      (void)id;
      (void)assault;
      unit.health = 0;
    }
    (void)m_waves.advance(binding, 0.1F, false);
    victory.update(m_world, 0.1F);
  }
  EXPECT_EQ(m_waves.director().cleared_wave_count(), 3);
  EXPECT_EQ(m_waves.convoy_status().value("delivered").toInt(), 2);
  EXPECT_EQ(victory.get_victory_state(), "victory");
  ASSERT_EQ(victory.optional_objectives().size(), 1U);
  EXPECT_FALSE(victory.optional_objectives().front().complete);
}

TEST_F(MissionStartupTest, SaltRoadIsLostWhenTheRaidersDestroyTwoWagons) {
  ASSERT_TRUE(prepare_mission(k_convoy_mission)) << m_error.toStdString();
  auto victory = victory_service();
  const auto binding = start_convoy(victory);
  const auto wagons = wagon_ids();
  ASSERT_EQ(wagons.size(), 3U);
  m_world.destroy_entity(wagons.front());
  (void)m_waves.advance(binding, 1.0F, false);
  victory.update(m_world, 1.0F);
  EXPECT_FALSE(victory.is_game_over());
  m_world.destroy_entity(wagons.back());
  (void)m_waves.advance(binding, 1.0F, false);
  victory.update(m_world, 1.0F);
  EXPECT_EQ(victory.get_victory_state(), "defeat");
  EXPECT_TRUE(victory.get_defeat_description().contains("Two supply wagons"));
}

TEST_F(MissionStartupTest, SaltRoadRaidersCanDamageAnUnprotectedWagon) {
  using namespace Engine::Core;
  ASSERT_TRUE(prepare_mission(k_convoy_mission)) << m_error.toStdString();
  auto victory = victory_service();
  const auto binding = start_convoy(victory);
  std::vector<EntityID> escort;
  for (auto [id, unit] : m_world.view<const UnitComponent>()) {
    if (unit.owner_id == k_local_owner &&
        Game::Units::is_combatant_spawn(unit.spawn_type)) {
      if (m_world.has<CommanderComponent>(id)) {
        m_world.try_get<TransformComponent>(id)->position.x -= 14.0F;
      } else {
        escort.push_back(id);
      }
    }
  }
  for (const auto id : escort) {
    m_world.destroy_entity(id);
  }

  const auto wagons = wagon_ids();
  ASSERT_EQ(wagons.size(), 3U);
  bool cargo_hit = false;
  for (int i = 0; i < 60 * 62 && !cargo_hit; ++i) {
    (void)m_session.advance(1.0 / 60.0,
                            1,
                            Game::Session::SessionContext::OverloadPolicy::KeepBacklog,
                            [&](float dt) {
                              (void)m_waves.advance(binding, dt, false);
                              m_session.step();
                            });
    for (const auto id : wagons) {
      const auto* unit = m_world.try_get<UnitComponent>(id);
      cargo_hit |= unit != nullptr && unit->health < unit->max_health;
    }
  }
  EXPECT_TRUE(cargo_hit);
}

} // namespace

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/world.h"
#include "game/map/map_context.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/systems/ai_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

constexpr char k_mission_file[] = "assets/missions/hold_the_sallow_ford.json";
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

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Map::MapContextStore::clear();
    Game::Map::MapContextStore::reset_statistics();
  }

  [[nodiscard]] auto prepare_mission() -> bool {
    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    if (!m_campaign.start_mission_file(
            QString::fromLatin1(k_mission_file), selected_player_id, &m_error)) {
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
  Engine::Core::World m_world;
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Systems::LevelSnapshot m_level;
  std::vector<Game::Mission::PendingMissionWave> m_pending_waves;
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

} // namespace

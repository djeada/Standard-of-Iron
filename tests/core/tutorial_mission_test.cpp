#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/component.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/mission/tutorial_director.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "tests/support/ai_quiesce.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;

constexpr char k_mission_path[] = "assets/missions/tutorial.json";
constexpr int k_local_owner = 1;
constexpr float k_tick = 1.0F / 60.0F;

class TutorialMissionTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();

    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    QString error;
    ASSERT_TRUE(m_campaign.start_mission_file(
        QString::fromLatin1(k_mission_path), selected_player_id, &error))
        << error.toStdString();
    ASSERT_TRUE(m_campaign.current_mission_definition().has_value());
    const auto& mission = *m_campaign.current_mission_definition();
    ASSERT_TRUE(mission.tutorial);

    const QString map_path = mission.map_path.startsWith(QStringLiteral(":/"))
                                 ? mission.map_path.mid(2)
                                 : mission.map_path;
    App::Core::SkirmishLoader loader(m_world, m_renderer, m_camera);
    const auto load_result = loader.start(map_path,
                                          build_campaign_player_configs(mission),
                                          k_local_owner,
                                          false,
                                          selected_player_id);
    ASSERT_TRUE(load_result.ok) << load_result.error_message.toStdString();

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
  }

  void TearDown() override {
    TestSupport::quiesce_ai(m_world);
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto living_units_of(int owner) -> std::vector<Engine::Core::Entity*> {
    std::vector<Engine::Core::Entity*> units;
    for (auto* entity : m_world.get_entities_with<UnitComponent>()) {
      const auto* unit = entity->get_component<UnitComponent>();
      if (unit != nullptr && unit->health > 0 && unit->owner_id == owner) {
        units.push_back(entity);
      }
    }
    return units;
  }

  static auto position(Engine::Core::Entity* entity) -> QVector3D {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  Engine::Core::World m_world;
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Mission::MissionWaves m_waves;
  Game::Systems::LevelSnapshot m_level;
  std::vector<Game::Mission::PendingMissionWave> m_pending_waves;
};

} // namespace

TEST_F(TutorialMissionTest, EveryForceFieldsExactlyOneCommander) {
  std::map<int, int> commanders;
  for (auto* entity : m_world.get_entities_with<Engine::Core::CommanderComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit != nullptr && unit->health > 0) {
      ++commanders[unit->owner_id];
    }
  }
  EXPECT_EQ(commanders[1], 1) << "the player needs a commander to lose";
  EXPECT_EQ(commanders[2], 1) << "the assault step needs a Roman commander to kill";
}

TEST_F(TutorialMissionTest, ThePlayerStartsWithWhatTheFirstStepsNeed) {
  int builders = 0;
  int soldiers = 0;
  int barracks = 0;
  int homes = 0;
  for (auto* entity : living_units_of(k_local_owner)) {
    const auto type = entity->get_component<UnitComponent>()->spawn_type;
    if (type == Game::Units::SpawnType::Builder) {
      ++builders;
    } else if (type == Game::Units::SpawnType::Barracks) {
      ++barracks;
    } else if (type == Game::Units::SpawnType::Home) {
      ++homes;
    } else if (Game::Units::is_troop_spawn(type) &&
               entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
      ++soldiers;
    }
  }
  EXPECT_GE(builders, 2) << "the gather steps put one builder on each material";
  EXPECT_GE(soldiers, 3) << "the select, move and attack steps need soldiers";
  EXPECT_LT(soldiers, Game::Mission::k_tutorial_army_size)
      << "the army step must ask for recruits, not be met on arrival";
  EXPECT_EQ(barracks, 1) << "gathered loads are dropped at the barracks yard";
  EXPECT_EQ(homes, 0) << "the build step raises the first Home";
}

TEST_F(TutorialMissionTest, TheScoutingPartyWaitsToBeAttacked) {
  std::vector<Engine::Core::Entity*> scouts;
  for (auto* entity : living_units_of(2)) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (Game::Units::is_building_spawn(unit->spawn_type) ||
        entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
      continue;
    }

    if (position(entity).z() > 0.0F) {
      scouts.push_back(entity);
    }
  }
  ASSERT_GE(static_cast<int>(scouts.size()), Game::Mission::k_tutorial_scout_count);

  std::vector<QVector3D> start;
  for (auto* scout : scouts) {
    EXPECT_EQ(scout->get_component<Engine::Core::AIControlledComponent>(), nullptr)
        << "a held scout must not be handed to the AI, or it marches away";
    EXPECT_NE(scout->get_component<Engine::Core::HoldModeComponent>(), nullptr);
    start.push_back(position(scout));
  }

  for (int tick = 0; tick < 60 * 20; ++tick) {
    m_world.update(k_tick);
  }

  for (std::size_t i = 0; i < scouts.size(); ++i) {
    EXPECT_LT((position(scouts[i]) - start[i]).length(), 3.0F)
        << "scout " << i << " wandered off before the player came for it";
  }
}

TEST_F(TutorialMissionTest, TheRaidClosesOnThePlayerCamp) {
  ASSERT_EQ(m_pending_waves.size(), 1U) << "the defend step expects one raid";
  const auto& wave = m_pending_waves.front();
  const QVector3D camp = wave.defense_reference_world_position;
  const float spawn_distance = (wave.entry_world_position - camp).length();
  ASSERT_GT(spawn_distance, 20.0F);

  const auto effects = m_waves.spawn({.world = m_world,
                                      .level = m_level,
                                      .campaign_mission_elapsed = wave.trigger_time},
                                     wave);
  ASSERT_GE(effects.spawned_entity_ids.size(), 2U);

  auto closest = [&]() {
    float best = std::numeric_limits<float>::max();
    for (const auto id : effects.spawned_entity_ids) {
      auto* entity = m_world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      best = std::min(best, (position(entity) - camp).length());
    }
    return best;
  };

  const float at_spawn = closest();
  for (int tick = 0; tick < 60 * 40; ++tick) {
    m_world.update(k_tick);
  }
  EXPECT_LT(closest(), at_spawn - 10.0F)
      << "the raid never marched on the camp the defend step guards";
}

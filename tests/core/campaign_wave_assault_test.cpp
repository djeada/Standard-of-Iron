#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <vector>

#include "app/session/skirmish_loader.h"
#include "core/component.h"
#include "core/world.h"
#include "game/audio/cue_ids.h"
#include "game/map/campaign_definition.h"
#include "game/map/campaign_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_director.h"
#include "game/mission/mission_waves.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/spawn_type.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;

constexpr char k_campaign[] = "second_punic_war";
constexpr char k_campaign_path[] = "assets/campaigns/second_punic_war.json";
constexpr int k_local_owner = 1;
constexpr float k_tick = 1.0F / 60.0F;

class CampaignWaveAssaultTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

auto position_of(Engine::Core::World& world, EntityID id) -> QVector3D {
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return {};
  }
  const auto* transform = entity->get_component<TransformComponent>();
  return transform == nullptr
             ? QVector3D()
             : QVector3D(transform->position.x, 0.0F, transform->position.z);
}

auto closest_living(Engine::Core::World& world,
                    const std::vector<EntityID>& units,
                    const QVector3D& point) -> float {
  float best = std::numeric_limits<float>::max();
  for (const auto id : units) {
    if (world.get_entity(id) == nullptr) {
      continue;
    }
    best = std::min(best, (position_of(world, id) - point).length());
  }
  return best;
}

struct WaveMarch {
  float spawn_distance = 0.0F;
  float closest_approach = 0.0F;
  float furthest_from_camp = 0.0F;
  bool met_a_defender = false;
  bool engaged_the_camp = false;
  bool engaged_a_barrier = false;
  int first_barrier_engagement_second = 0;
  bool wiped_out = false;
};

class MissionUnderTest {
public:
  auto load(const QString& mission_id) -> bool {
    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    m_campaign.start_campaign_mission(
        QStringLiteral("%1/%2").arg(QLatin1String(k_campaign), mission_id),
        selected_player_id);
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
      m_error = load_result.error_message;
      return false;
    }

    m_level.map_path = map_path;
    m_level.grid_width = load_result.grid_width;
    m_level.grid_height = load_result.grid_height;
    m_level.tile_size = load_result.tile_size;

    const auto effects =
        m_coordinator.apply_mission_setup({.world = m_world,
                                           .campaign = m_campaign,
                                           .level = m_level,
                                           .selected_player_id = selected_player_id,
                                           .local_owner_id = k_local_owner,
                                           .pending_waves = m_pending_waves});
    (void)effects;
    return true;
  }

  [[nodiscard]] auto error() const -> QString { return m_error; }
  [[nodiscard]] auto
  waves() const -> const std::vector<Game::Mission::PendingMissionWave>& {
    return m_pending_waves;
  }

  auto march(const Game::Mission::PendingMissionWave& wave, int seconds) -> WaveMarch {
    WaveMarch result;
    const QVector3D camp = wave.defense_reference_world_position;
    result.spawn_distance = (wave.entry_world_position - camp).length();

    const auto effects = m_waves.spawn({.world = m_world,
                                        .level = m_level,
                                        .campaign_mission_elapsed = wave.trigger_time},
                                       wave);
    m_spawned = effects.spawned_entity_ids;
    if (m_spawned.empty()) {
      return result;
    }

    result.closest_approach = closest_living(m_world, m_spawned, camp);
    result.furthest_from_camp = result.closest_approach;

    for (int second = 0; second < seconds; ++second) {
      for (int step = 0; step < 60; ++step) {
        m_world.update(k_tick);
      }

      const float distance = closest_living(m_world, m_spawned, camp);
      if (distance == std::numeric_limits<float>::max()) {
        result.wiped_out = true;
        break;
      }
      result.closest_approach = std::min(result.closest_approach, distance);
      result.furthest_from_camp = std::max(result.furthest_from_camp, distance);
      const auto engagement = current_engagement();
      result.met_a_defender = result.met_a_defender || engagement.defender;
      result.engaged_the_camp = result.engaged_the_camp || engagement.anything;
      if (engagement.barrier && !result.engaged_a_barrier) {
        result.engaged_a_barrier = true;
        result.first_barrier_engagement_second = second + 1;
      }
    }

    return result;
  }

  [[nodiscard]] auto spawned() const -> const std::vector<EntityID>& {
    return m_spawned;
  }

  [[nodiscard]] auto
  living_commanders_by_owner() -> std::map<int, std::vector<QString>> {
    std::map<int, std::vector<QString>> by_owner;
    for (auto* entity :
         m_world.collect_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        continue;
      }
      by_owner[unit->owner_id].push_back(
          Game::Units::spawn_typeToQString(unit->spawn_type));
    }
    return by_owner;
  }

  [[nodiscard]] auto force_count() const -> std::size_t {
    const auto& mission = m_campaign.current_mission_definition();
    return mission.has_value() ? 1 + mission->ai_setups.size() : 0;
  }

private:
  struct Engagement {

    bool defender = false;

    bool anything = false;

    bool barrier = false;
  };

  [[nodiscard]] auto current_engagement() -> Engagement {
    Engagement engagement;
    for (const auto id : m_spawned) {
      auto* entity = m_world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
      if (attack == nullptr) {
        continue;
      }
      auto* target = m_world.get_entity(attack->target_id);
      const auto* target_unit =
          target == nullptr ? nullptr : target->get_component<UnitComponent>();
      if (target_unit == nullptr || target_unit->owner_id != k_local_owner) {
        continue;
      }
      engagement.anything = true;
      engagement.defender = engagement.defender ||
                            !target->has_component<Engine::Core::BuildingComponent>();
      engagement.barrier =
          engagement.barrier ||
          (target_unit->spawn_type == Game::Units::SpawnType::WallSegment ||
           target_unit->spawn_type == Game::Units::SpawnType::WallGate);
    }
    return engagement;
  }

  Engine::Core::World m_world;
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Mission::MissionSetupCoordinator m_coordinator;
  Game::Mission::MissionWaves m_waves;
  Game::Systems::LevelSnapshot m_level;
  std::vector<Game::Mission::PendingMissionWave> m_pending_waves;
  std::vector<EntityID> m_spawned;
  QString m_error;
};

auto campaign_mission_ids() -> std::vector<QString> {
  Game::Campaign::CampaignDefinition campaign;
  QString error;
  if (!Game::Campaign::CampaignLoader::load_from_json_file(
          QString::fromLatin1(k_campaign_path), campaign, &error)) {
    return {};
  }
  auto missions = campaign.missions;
  std::stable_sort(missions.begin(),
                   missions.end(),
                   [](const Game::Campaign::CampaignMission& lhs,
                      const Game::Campaign::CampaignMission& rhs) {
                     return lhs.order_index < rhs.order_index;
                   });
  std::vector<QString> ids;
  ids.reserve(missions.size());
  for (const auto& mission : missions) {
    ids.push_back(mission.mission_id);
  }
  return ids;
}

} // namespace

TEST_F(CampaignWaveAssaultTest, FirstMissionWaveChargesThePlayerCamp) {
  const auto ids = campaign_mission_ids();
  ASSERT_FALSE(ids.empty()) << "the campaign must list its missions";

  MissionUnderTest mission;
  ASSERT_TRUE(mission.load(ids.front())) << mission.error().toStdString();
  ASSERT_FALSE(mission.waves().empty()) << "the first mission authors an assault wave";

  const auto& wave = mission.waves().front();
  const auto march = mission.march(wave, 60);
  ASSERT_FALSE(mission.spawned().empty()) << "the wave spawned nothing";
  ASSERT_GT(march.spawn_distance, 20.0F)
      << "the wave is supposed to start away from the camp it marches on";

  EXPECT_LT(march.closest_approach, march.spawn_distance - 1.0F)
      << "the wave never closed on the camp: spawned " << march.spawn_distance
      << " away, got no nearer than " << march.closest_approach;
  EXPECT_LT(march.furthest_from_camp, march.spawn_distance + 5.0F)
      << "the wave wandered away from the camp instead of marching on it";
  EXPECT_TRUE(march.engaged_the_camp) << "the wave never reached the camp's defences";
  EXPECT_TRUE(march.engaged_a_barrier)
      << "the wave ignored the intact wall between it and the camp";
  EXPECT_LE(march.first_barrier_engagement_second, 12)
      << "the wave waited at the wall instead of committing to a breach";
}

TEST_F(CampaignWaveAssaultTest, EveryCampaignWaveAnnouncesItselfBeforeItLands) {
  const auto ids = campaign_mission_ids();
  ASSERT_FALSE(ids.empty()) << "the campaign must list its missions";

  int announced = 0;
  for (const auto& mission_id : ids) {
    MissionUnderTest mission;
    ASSERT_TRUE(mission.load(mission_id)) << mission.error().toStdString();
    auto waves = mission.waves();
    if (waves.empty()) {
      continue;
    }

    Engine::Core::World world;
    Game::Mission::MissionWaveDirector director;
    director.bind(&waves, &world);

    float earliest = std::numeric_limits<float>::max();
    for (const auto& wave : waves) {
      if (wave.trigger == Game::Mission::WaveTriggerMode::Time) {
        earliest = std::min(earliest, wave.trigger_time);
      }
    }
    if (earliest == std::numeric_limits<float>::max()) {
      continue;
    }

    director.set_elapsed(earliest - waves.front().warning_seconds + 1.0F);
    const auto warned = director.advance();

    ASSERT_FALSE(warned.audio_cues.isEmpty())
        << mission_id.toStdString() << " warns of its first wave in silence";
    EXPECT_TRUE(warned.audio_cues.contains(
        QString::fromLatin1(Game::Audio::Cue::k_alert_enemy_reinforcements)))
        << mission_id.toStdString() << " announced a wave without the enemy horn";
    EXPECT_TRUE(warned.waves_to_spawn.empty())
        << mission_id.toStdString()
        << " spawns the wave in the same breath as the warning, so the horn is "
           "pointless";
    ++announced;
  }

  EXPECT_GT(announced, 0) << "no campaign mission authors a timed wave any more";
}

TEST_F(CampaignWaveAssaultTest, EveryCampaignMissionWaveClosesOnThePlayerCamp) {
  const auto ids = campaign_mission_ids();
  ASSERT_FALSE(ids.empty()) << "the campaign must list its missions";

  for (const auto& mission_id : ids) {
    SCOPED_TRACE(mission_id.toStdString());

    MissionUnderTest mission;
    ASSERT_TRUE(mission.load(mission_id)) << mission.error().toStdString();
    if (mission.waves().empty()) {
      continue;
    }

    const auto& wave = mission.waves().front();
    const auto march = mission.march(wave, 30);
    ASSERT_FALSE(mission.spawned().empty()) << "the wave spawned nothing";
    ASSERT_GT(march.spawn_distance, 1.0F);

    constexpr float k_min_ground_gained = 25.0F;
    EXPECT_TRUE(march.closest_approach <= march.spawn_distance - k_min_ground_gained ||
                march.engaged_the_camp)
        << "the wave neither closed on the camp nor found the player: spawned "
        << march.spawn_distance << " away, got no nearer than "
        << march.closest_approach;
    EXPECT_LT(march.furthest_from_camp, march.spawn_distance + 10.0F)
        << "the wave drifted away from the camp instead of marching on it";
  }
}

TEST_F(CampaignWaveAssaultTest, EveryCampaignForceLoadsWithExactlyOneCommander) {
  const auto ids = campaign_mission_ids();
  ASSERT_FALSE(ids.empty()) << "the campaign must list its missions";

  for (const auto& mission_id : ids) {
    SCOPED_TRACE(mission_id.toStdString());

    MissionUnderTest mission;
    ASSERT_TRUE(mission.load(mission_id)) << mission.error().toStdString();

    const auto commanders = mission.living_commanders_by_owner();
    const std::size_t forces = mission.force_count();
    ASSERT_GT(forces, 0U);

    for (std::size_t index = 0; index < forces; ++index) {
      const int owner_id = static_cast<int>(index) + 1;
      const auto it = commanders.find(owner_id);
      ASSERT_NE(it, commanders.end())
          << "owner " << owner_id << " went into battle with no commander";
      EXPECT_EQ(it->second.size(), 1U) << "owner " << owner_id << " fielded "
                                       << it->second.size() << " commanders at once";
    }

    EXPECT_EQ(commanders.size(), forces)
        << "the world holds commanders for owners the mission never declared";
  }
}

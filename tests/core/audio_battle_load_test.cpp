#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "app/audio/audio_resource_loader.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/cue_trace.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/unit.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::World;
using Game::Audio::CueTrace;

constexpr int k_player = 1;
constexpr int k_enemy = 2;

class AudioBattleLoadTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& audio = AudioSystem::get_instance();
    audio.shutdown();
    if (!audio.initialize()) {
      GTEST_SKIP() << "Audio backend is unavailable in this environment";
    }
    m_saved_master = audio.get_master_volume();
    m_saved_sound = audio.get_sound_volume();
    audio.set_master_volume(1.0F);
    audio.set_sound_volume(1.0F);

    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
    AudioResourceLoader::load_audio_cues();

    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = 96;
    map_definition.grid.height = 96;
    map_definition.grid.tile_size = 1.0F;
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::NavGrid::initialize(map_definition.grid.width,
                                       map_definition.grid.height);

    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "Rome");
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "Carthage");
    owners.set_owner_team(k_player, 1);
    owners.set_owner_team(k_enemy, 2);
    owners.set_local_player_id(k_player);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(k_player, Game::Systems::NationID::RomanRepublic);
    nations.set_player_nation(k_enemy, Game::Systems::NationID::Carthage);

    m_units = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_units);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CueTrace::instance().reset();

    m_handler = std::make_unique<Game::Audio::AudioEventHandler>(&m_world);
    ASSERT_TRUE(m_handler->initialize());
    m_handler->set_local_owner_id(k_player);
  }

  void TearDown() override {
    m_handler.reset();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    CueTrace::instance().reset();

    auto& audio = AudioSystem::get_instance();
    audio.set_master_volume(m_saved_master);
    audio.set_sound_volume(m_saved_sound);
    audio.shutdown();
  }

  auto spawn(Game::Units::SpawnType type,
             int owner_id,
             const QVector3D& position,
             Game::Systems::NationID nation) -> Entity* {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.nation_id = nation;
    params.is_initial_spawn = true;
    auto unit = m_units->create(type, m_world, params);
    if (!unit) {
      return nullptr;
    }
    return m_world.get_entity(unit->id());
  }

  static void order_attack(Entity& attacker, const Entity& target) {
    auto* order = attacker.add_component<Engine::Core::AttackTargetComponent>();
    order->target_id = target.get_id();
    order->should_chase = true;
  }

  World m_world;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_units;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_handler;
  float m_saved_master{1.0F};
  float m_saved_sound{1.0F};
};

TEST_F(AudioBattleLoadTest, AnImpactStormStillLetsTheAlarmThrough) {
  Game::Systems::register_runtime_systems(m_world);
  m_world.set_presentation_enabled(false);

  constexpr int k_per_side = 10;
  std::vector<Entity*> defenders;
  std::vector<Entity*> attackers;
  for (int index = 0; index < k_per_side; ++index) {
    const float offset = static_cast<float>(index) * 1.6F;
    auto* defender = spawn(Game::Units::SpawnType::Knight,
                           k_player,
                           QVector3D(-1.2F, 0.0F, offset),
                           Game::Systems::NationID::RomanRepublic);
    auto* attacker = spawn(Game::Units::SpawnType::Knight,
                           k_enemy,
                           QVector3D(1.2F, 0.0F, offset),
                           Game::Systems::NationID::Carthage);
    ASSERT_NE(defender, nullptr);
    ASSERT_NE(attacker, nullptr);
    defenders.push_back(defender);
    attackers.push_back(attacker);
  }

  for (int index = 0; index < k_per_side; ++index) {
    order_attack(*attackers[index], *defenders[index]);
    order_attack(*defenders[index], *attackers[index]);
  }

  constexpr float k_step = 1.0F / 30.0F;
  constexpr int k_seconds = 8;
  constexpr int k_ticks = k_seconds * 30;
  for (int tick = 0; tick < k_ticks; ++tick) {
    m_world.update(k_step);
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  const auto records = CueTrace::instance().records();
  std::uint64_t requests = 0;
  std::uint64_t accepted = 0;
  for (const auto& record : records) {
    requests += record.requests;
    accepted += record.accepted;
    std::string drops;
    for (std::size_t index = 0; index < Game::Audio::k_cue_outcome_count; ++index) {
      const auto outcome = static_cast<Game::Audio::CueOutcome>(index);
      if (outcome == Game::Audio::CueOutcome::Accepted ||
          record.outcomes.at(index) == 0U) {
        continue;
      }
      drops += std::string(" ") + Game::Audio::cue_outcome_name(outcome) + "=" +
               std::to_string(record.outcomes.at(index));
    }
    std::cout << "  " << record.cue_id << " requested " << record.requests << ", heard "
              << record.accepted << drops << "\n";
  }
  std::cout << "  " << k_seconds << "s of battle: " << requests << " requests ("
            << (requests / k_seconds) << "/s), " << accepted << " heard ("
            << (accepted / k_seconds) << "/s)\n";

  const auto hits =
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_hit_sword);
  ASSERT_GT(hits.requests, 20U)
      << "no sustained fighting happened, so this measures nothing";

  const auto deaths = CueTrace::instance().record_for(Game::Audio::Cue::k_combat_death);
  if (deaths.requests > 0U) {
    EXPECT_GT(deaths.accepted, 0U)
        << "men died through a wall of impacts and not one death was heard";
  }

  const auto lost =
      CueTrace::instance().record_for(Game::Audio::Cue::k_alert_unit_lost);
  if (lost.requests > 0U) {
    EXPECT_GT(lost.accepted, 0U)
        << "the player lost soldiers and the alert never got through the impacts";
  }

  EXPECT_GT(hits.accepted, 8U)
      << "a melee this size should be a steady rhythm of blows, not a trickle";
  EXPECT_LT(hits.accepted, hits.requests)
      << "every single blow was played, which at this density is a wall of noise "
         "rather than a battle";
}

TEST_F(AudioBattleLoadTest, ABattleOutOfEarshotBecomesOneDistantMass) {
  Game::Systems::register_runtime_systems(m_world);
  m_world.set_presentation_enabled(false);

  AudioSystem::get_instance().set_listener({.position = {400.0F, 60.0F, 400.0F},
                                            .right_x = 1.0F,
                                            .right_z = 0.0F,
                                            .valid = true});

  constexpr float k_far = 0.0F;
  constexpr int k_per_side = 10;
  std::vector<Entity*> defenders;
  std::vector<Entity*> attackers;
  for (int index = 0; index < k_per_side; ++index) {
    const float offset = static_cast<float>(index) * 1.6F;
    auto* defender = spawn(Game::Units::SpawnType::Knight,
                           k_player,
                           QVector3D(k_far - 1.2F, 0.0F, offset),
                           Game::Systems::NationID::RomanRepublic);
    auto* attacker = spawn(Game::Units::SpawnType::Knight,
                           k_enemy,
                           QVector3D(k_far + 1.2F, 0.0F, offset),
                           Game::Systems::NationID::Carthage);
    ASSERT_NE(defender, nullptr);
    ASSERT_NE(attacker, nullptr);
    defenders.push_back(defender);
    attackers.push_back(attacker);
  }
  for (int index = 0; index < k_per_side; ++index) {
    order_attack(*attackers[index], *defenders[index]);
    order_attack(*defenders[index], *attackers[index]);
  }

  constexpr float k_step = 1.0F / 30.0F;
  for (int tick = 0; tick < 6 * 30; ++tick) {
    m_world.update(k_step);
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  const auto impacts =
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_hit_sword);
  const auto bed =
      CueTrace::instance().record_for(Game::Audio::Cue::k_combat_distant_battle);
  std::cout << "  far battle: " << impacts.requests << " impacts, " << impacts.accepted
            << " heard, distant bed requested " << bed.requests << ", heard "
            << bed.accepted << "\n";

  ASSERT_GT(impacts.requests, 20U) << "no sustained fighting happened";
  EXPECT_EQ(impacts.accepted, 0U) << "blows 300 metres away were played at the camera";
  EXPECT_GT(bed.accepted, 0U)
      << "a whole battle out of earshot produced no sound at all";

  AudioSystem::get_instance().set_listener({});
}

} // namespace

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

#include "game/core/component.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/wall_network_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_player = 1;
constexpr int k_wave_ai = 2;
constexpr int k_map_size = 64;

constexpr int k_camp_grid_x = 20;
constexpr int k_wave_grid_x = 58;
constexpr int k_gate_grid_z = 30;

constexpr int k_ring_west_x = 4;
constexpr int k_ring_east_x = 40;
constexpr int k_ring_north_z = 10;
constexpr int k_ring_south_z = 50;

class MissionWaveAssaultTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  auto make_match() -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);
    auto& owners = session.owners();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "carth");
    owners.register_owner_with_id(k_wave_ai, Game::Systems::OwnerType::AI, "rome");
    owners.set_owner_team(k_player, 0);
    owners.set_owner_team(k_wave_ai, 1);
    Game::Systems::initialize_default_content(session.nations());
    Game::Systems::register_runtime_systems(session.world());

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    session.terrain().initialize(map_definition);
    return session;
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
  }

  static void run_for(SessionContext& session, double seconds) {
    const double step = session.clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
      }
    }
  }

  auto spawn(SessionContext& session,
             Game::Units::SpawnType type,
             int owner_id,
             QVector3D position,
             bool ai_controlled = false,
             float rotation_y = 0.0F) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.ai_controlled = ai_controlled;
    params.rotation_y = rotation_y;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, session.world(), params);
    return unit ? unit->id() : 0;
  }

  static auto position_of(SessionContext& session, EntityID id) -> QVector3D {
    auto* entity = session.world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    auto* transform = entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return Game::Systems::NavGrid::grid_to_world(Game::Systems::Point(grid_x, grid_z));
  }

  void build_camp_ring(SessionContext& session) {
    constexpr int k_pitch = Game::Systems::WallNetworkService::k_segment_spacing;
    auto place = [&](int grid_x, int grid_z) {
      const bool is_gate = grid_x == k_ring_east_x && grid_z == k_gate_grid_z;
      spawn(session,
            is_gate ? Game::Units::SpawnType::WallGate
                    : Game::Units::SpawnType::WallSegment,
            k_player,
            world_of(grid_x, grid_z),
            false,
            is_gate ? 90.0F : 0.0F);
    };
    for (int grid_x = k_ring_west_x; grid_x <= k_ring_east_x; grid_x += k_pitch) {
      place(grid_x, k_ring_north_z);
      place(grid_x, k_ring_south_z);
    }
    for (int grid_z = k_ring_north_z + k_pitch; grid_z < k_ring_south_z;
         grid_z += k_pitch) {
      place(k_ring_west_x, grid_z);
      place(k_ring_east_x, grid_z);
    }
    Game::Systems::WallNetworkService::refresh_world(session.world());
  }

  static auto is_inside_ring(const QVector3D& position) -> bool {
    const QVector3D north_west = world_of(k_ring_west_x, k_ring_north_z);
    const QVector3D south_east = world_of(k_ring_east_x, k_ring_south_z);
    return position.x() > north_west.x() + 1.0F &&
           position.x() < south_east.x() - 1.0F &&
           position.z() > north_west.z() + 1.0F && position.z() < south_east.z() - 1.0F;
  }

  auto spawn_wave(SessionContext& session, int count) -> std::vector<EntityID> {
    const QVector3D entry = world_of(k_wave_grid_x, k_gate_grid_z - 2);
    std::vector<EntityID> wave;
    for (int i = 0; i < count; ++i) {
      const EntityID id =
          spawn(session,
                Game::Units::SpawnType::Knight,
                k_wave_ai,
                entry + QVector3D(static_cast<float>(i) * 2.0F, 0.0F, 0.0F),
                true);
      if (id == 0) {
        continue;
      }
      session.world()
          .get_entity(id)
          ->add_component<Engine::Core::AssaultWaveComponent>();
      wave.push_back(id);
    }
    return wave;
  }

  static void make_defensive(SessionContext& session) {
    auto* ai_system = session.world().get_system<Game::Systems::AISystem>();
    ASSERT_NE(ai_system, nullptr);
    Game::Systems::AI::AIPlayerProfile profile;
    profile.strategy = Game::Systems::AI::AIStrategy::Defensive;
    profile.posture = Game::Systems::AI::AIPosture::Garrison;
    profile.personality.aggression = 0.35F;
    profile.personality.defense = 0.85F;
    profile.personality.harassment = 0.15F;
    profile.difficulty = "medium";
    ai_system->set_ai_profile(k_wave_ai, profile);
  }

  static auto closest_wave_distance_to(SessionContext& session,
                                       const std::vector<EntityID>& wave,
                                       const QVector3D& point) -> float {
    float best = std::numeric_limits<float>::max();
    for (const auto id : wave) {
      if (session.world().get_entity(id) == nullptr) {
        continue;
      }
      best = std::min(best, (position_of(session, id) - point).length());
    }
    return best;
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(MissionWaveAssaultTest, WaveUnitsMarchOnAnOpenCamp) {
  auto& session = make_match();

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);
  spawn(session,
        Game::Units::SpawnType::Spearman,
        k_player,
        camp + QVector3D(3.0F, 0.0F, 0.0F));

  make_defensive(session);
  const auto wave = spawn_wave(session, 2);
  ASSERT_EQ(wave.size(), 2U);

  const float start = closest_wave_distance_to(session, wave, camp);
  run_for(session, 40.0);
  const float end = closest_wave_distance_to(session, wave, camp);

  EXPECT_LT(end, start * 0.5F) << "start " << start << " end " << end;
}

TEST_F(MissionWaveAssaultTest, WaveUnitsAttackTheRampartInTheirWay) {
  auto& session = make_match();

  build_camp_ring(session);

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);
  spawn(session,
        Game::Units::SpawnType::Spearman,
        k_player,
        world_of(k_ring_east_x - 6, k_gate_grid_z));

  make_defensive(session);
  const auto wave = spawn_wave(session, 6);
  ASSERT_EQ(wave.size(), 6U);

  run_for(session, 240.0);

  int damage_dealt = 0;
  std::unordered_set<EntityID> barriers;
  for (auto* entity :
       session.world().collect_entities_with<Engine::Core::WallSegmentComponent>()) {
    const auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    barriers.insert(entity->get_id());
    damage_dealt += unit->max_health - unit->health;
  }

  int attacking_the_rampart = 0;
  for (const auto id : wave) {
    auto* entity = session.world().get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* attack_target =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target != nullptr && barriers.contains(attack_target->target_id)) {
      attacking_the_rampart++;
    }
  }

  EXPECT_GT(attacking_the_rampart, 0)
      << "no wave unit was working on the rampart between it and the camp";
  EXPECT_GT(damage_dealt, 0) << "the wave stood at the wall without ever striking it";
}

TEST_F(MissionWaveAssaultTest, WaveReachesACampBehindAFlankableRampart) {
  auto& session = make_match();

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);

  constexpr int k_pitch = Game::Systems::WallNetworkService::k_segment_spacing;
  for (int grid_z = k_gate_grid_z - 8; grid_z <= k_gate_grid_z + 8; grid_z += k_pitch) {
    spawn(session,
          Game::Units::SpawnType::WallSegment,
          k_player,
          world_of(k_ring_east_x, grid_z));
  }
  Game::Systems::WallNetworkService::refresh_world(session.world());

  make_defensive(session);
  const auto wave = spawn_wave(session, 3);
  ASSERT_EQ(wave.size(), 3U);

  const float start = closest_wave_distance_to(session, wave, camp);
  run_for(session, 90.0);
  const float end = closest_wave_distance_to(session, wave, camp);

  EXPECT_LT(end, start * 0.5F)
      << "the wave never closed on a camp it had a route into: start " << start
      << " end " << end;
}

TEST_F(MissionWaveAssaultTest, WaveWalksPastNeutralPropertyOnTheWayIn) {
  auto& session = make_match();

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);

  const EntityID temple =
      spawn(session,
            Game::Units::SpawnType::Temple,
            Game::Core::NEUTRAL_OWNER_ID,
            world_of((k_camp_grid_x + k_wave_grid_x) / 2, k_gate_grid_z - 2));
  ASSERT_NE(temple, 0U);

  make_defensive(session);
  const auto wave = spawn_wave(session, 3);
  ASSERT_EQ(wave.size(), 3U);

  const float start = closest_wave_distance_to(session, wave, camp);
  run_for(session, 90.0);

  EXPECT_LT(closest_wave_distance_to(session, wave, camp), start * 0.5F)
      << "the wave stopped for a neutral building instead of marching on the camp";

  auto* temple_entity = session.world().get_entity(temple);
  ASSERT_NE(temple_entity, nullptr);
  const auto* temple_unit = temple_entity->get_component<UnitComponent>();
  ASSERT_NE(temple_unit, nullptr);
  EXPECT_EQ(temple_unit->health, temple_unit->max_health)
      << "the wave spent itself on scenery nobody owns";
}

TEST_F(MissionWaveAssaultTest, WoundedWaveUnitsPressOnInsteadOfRunningHome) {
  auto& session = make_match();

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);

  const QVector3D ai_home = world_of(k_map_size - 4, k_map_size - 4);
  spawn(session, Game::Units::SpawnType::Barracks, k_wave_ai, ai_home);

  make_defensive(session);
  const auto wave = spawn_wave(session, 3);
  ASSERT_EQ(wave.size(), 3U);

  for (const auto id : wave) {
    auto* unit = session.world().get_entity(id)->get_component<UnitComponent>();
    unit->health = std::max(1, unit->max_health / 10);
  }

  const float start_to_camp = closest_wave_distance_to(session, wave, camp);
  const float start_to_home = closest_wave_distance_to(session, wave, ai_home);
  run_for(session, 60.0);

  EXPECT_LT(closest_wave_distance_to(session, wave, camp), start_to_camp * 0.5F)
      << "a bloodied wave must keep coming, not turn around";
  EXPECT_GT(closest_wave_distance_to(session, wave, ai_home), start_to_home)
      << "the wave retreated toward its own base";
}

TEST_F(MissionWaveAssaultTest, WaveCannotWalkThroughAnIntactRampart) {
  auto& session = make_match();

  build_camp_ring(session);

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);

  make_defensive(session);
  const auto wave = spawn_wave(session, 6);
  ASSERT_EQ(wave.size(), 6U);

  std::unordered_set<EntityID> rampart;
  for (auto* entity :
       session.world().collect_entities_with<Engine::Core::WallSegmentComponent>()) {
    rampart.insert(entity->get_id());
  }
  ASSERT_FALSE(rampart.empty());

  const auto rampart_is_intact = [&]() {
    return std::all_of(rampart.begin(), rampart.end(), [&](EntityID id) {
      return session.world().get_entity(id) != nullptr;
    });
  };

  for (int second = 1; second <= 240 && rampart_is_intact(); ++second) {
    run_for(session, 1.0);
    for (const auto id : wave) {
      if (session.world().get_entity(id) == nullptr) {
        continue;
      }
      const QVector3D position = position_of(session, id);
      ASSERT_FALSE(is_inside_ring(position))
          << "a wave unit stood inside the camp at t=" << second
          << "s without a single rampart section coming down: (" << position.x() << ", "
          << position.z() << ")";
    }
  }
}

TEST_F(MissionWaveAssaultTest, GarrisonAnswersAScoutWithAFewUnitsAndHoldsTheRest) {
  auto& session = make_match();

  const QVector3D camp = world_of(k_camp_grid_x, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Barracks, k_player, camp);

  const QVector3D garrison_home = world_of(k_wave_grid_x - 10, k_gate_grid_z);
  spawn(session, Game::Units::SpawnType::Home, k_wave_ai, garrison_home, true);

  std::vector<EntityID> garrison;
  for (int i = 0; i < 8; ++i) {
    const QVector3D position =
        garrison_home + QVector3D(-8.0F - static_cast<float>(i % 4),
                                  0.0F,
                                  -2.0F + static_cast<float>(i / 4) * 4.0F);
    const EntityID id =
        spawn(session, Game::Units::SpawnType::Spearman, k_wave_ai, position, true);
    ASSERT_NE(id, 0U);
    garrison.push_back(id);
  }

  make_defensive(session);

  const EntityID scout = spawn(session,
                               Game::Units::SpawnType::Builder,
                               k_player,
                               garrison_home + QVector3D(-24.0F, 0.0F, 6.0F));
  ASSERT_NE(scout, 0U);
  auto* scout_unit = session.world().get_entity(scout)->get_component<UnitComponent>();
  ASSERT_NE(scout_unit, nullptr);
  scout_unit->health = scout_unit->max_health = 100000;

  run_for(session, 30.0);

  int responders = 0;
  for (const auto id : garrison) {
    auto* entity = session.world().get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* attack_target =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target != nullptr && attack_target->target_id == scout) {
      responders++;
    }
  }

  EXPECT_GE(responders, 1) << "no garrison unit answered a scout inside its reach";
  EXPECT_LT(responders, static_cast<int>(garrison.size()))
      << "the whole garrison mobilised against a single scout";
}

} // namespace

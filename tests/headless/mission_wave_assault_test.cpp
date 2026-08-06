#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
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
    Game::Systems::CommandService::initialize(k_map_size, k_map_size);
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
    session.nations().initialize_defaults();
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
    return Game::Systems::CommandService::grid_to_world(
        Game::Systems::Point(grid_x, grid_z));
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
    ai_system->set_ai_strategy(k_wave_ai,
                               Game::Systems::AI::AIStrategy::Defensive,
                               0.35F,
                               0.85F,
                               0.15F,
                               "medium");
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
       session.world().get_entities_with<Engine::Core::WallSegmentComponent>()) {
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

} // namespace

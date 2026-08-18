#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/structure_combat.h"
#include "game/systems/default_content.h"
#include "game/systems/gate_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/wall_network_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

constexpr int k_defender = 1;
constexpr int k_raider = 2;
constexpr int k_map_size = 64;
constexpr int k_wall_grid_z = 32;
constexpr int k_wall_pitch = Game::Systems::WallNetworkService::k_segment_spacing;
constexpr float k_wall_half_thickness = 1.0F;

class WallSiegeTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  auto make_match() -> std::unique_ptr<SessionContext> {
    auto session = std::make_unique<SessionContext>();
    auto& owners = session->owners();
    owners.register_owner_with_id(k_defender, Game::Systems::OwnerType::Player, "blue");
    owners.register_owner_with_id(k_raider, Game::Systems::OwnerType::AI, "red");
    owners.set_owner_team(k_defender, 1);
    owners.set_owner_team(k_raider, 2);
    Game::Systems::initialize_default_content(session->nations());
    Game::Systems::register_runtime_systems(session->world());

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    session->terrain().initialize(map_definition);
    return session;
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

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return Game::Systems::NavGrid::grid_to_world(Game::Systems::Point(grid_x, grid_z));
  }

  static auto wall_line_z() -> float { return world_of(0, k_wall_grid_z).z(); }
  static auto besieged_face_z() -> float {
    return wall_line_z() - k_wall_half_thickness;
  }
  static auto defended_face_z() -> float {
    return wall_line_z() + k_wall_half_thickness;
  }

  static auto add_wall_piece(SessionContext& session,
                             Game::Units::SpawnType spawn_type,
                             int grid_x,
                             int grid_z) -> EntityID {
    const Game::Systems::WallGridPosition snapped{
        .x = Game::Systems::WallNetworkService::snap_grid_coordinate(grid_x),
        .z = Game::Systems::WallNetworkService::snap_grid_coordinate(grid_z)};
    const auto center = world_of(snapped.x, snapped.z);

    auto* entity = session.world().create_entity();
    auto* transform = entity->add_component<TransformComponent>();
    transform->position.x = center.x();
    transform->position.z = center.z();
    entity->add_component<Engine::Core::RenderableComponent>();
    auto* unit = entity->add_component<UnitComponent>(4000, 4000, 0.0F, 0.0F);
    unit->owner_id = k_defender;
    unit->spawn_type = spawn_type;
    entity->add_component<Engine::Core::BuildingComponent>();

    auto* wall = entity->add_component<Engine::Core::WallSegmentComponent>();
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;

    const bool is_gate = spawn_type == Game::Units::SpawnType::WallGate;
    if (is_gate) {
      entity->add_component<Engine::Core::GateComponent>();
    }

    session.building_collision().register_building(entity->get_id(),
                                                   is_gate ? "wall_gate"
                                                           : "wall_segment",
                                                   center.x(),
                                                   center.z(),
                                                   k_defender);
    if (is_gate) {
      session.building_collision().set_building_navigation_blocking(entity->get_id(),
                                                                    false);
    }
    return entity->get_id();
  }

  static auto
  add_wall_segment(SessionContext& session, int grid_x, int grid_z) -> EntityID {
    return add_wall_piece(session, Game::Units::SpawnType::WallSegment, grid_x, grid_z);
  }

  static void demolish(SessionContext& session, EntityID entity_id) {
    session.building_collision().unregister_building(entity_id);
    session.world().destroy_entity(entity_id);
  }

  static auto build_wall(SessionContext& session) -> EntityID {
    EntityID center_segment = 0;
    for (int grid_x = -8; grid_x <= k_map_size + 8; grid_x += k_wall_pitch) {
      const EntityID segment = add_wall_segment(session, grid_x, k_wall_grid_z);
      if (grid_x == k_map_size / 2) {
        center_segment = segment;
      }
    }
    Game::Systems::WallNetworkService::refresh_world(session.world());
    return center_segment;
  }

  auto spawn_troop(SessionContext& session,
                   Game::Units::SpawnType type,
                   int owner_id,
                   float x,
                   float z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = owner_id;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    params.is_initial_spawn = true;
    auto unit = m_factory->create(type, session.world(), params);
    if (!unit) {
      return 0;
    }
    auto* entity = session.world().get_entity(unit->id());
    return entity != nullptr ? entity->get_id() : 0;
  }

  static void order_attack(SessionContext& session, EntityID unit, EntityID target) {
    Game::Command::submit(
        session.world(),
        Game::Command::Source::Script,
        session.world().get_entity(unit)->get_component<UnitComponent>()->owner_id,
        Game::Command::AttackTarget{
            .units = {unit}, .target = target, .should_chase = true});
  }

  static auto position_of(SessionContext& session, EntityID unit) -> QVector3D {
    auto* entity = session.world().get_entity(unit);
    const auto* transform =
        entity != nullptr ? entity->get_component<TransformComponent>() : nullptr;
    return transform != nullptr
               ? QVector3D(transform->position.x, 0.0F, transform->position.z)
               : QVector3D();
  }

  static auto rotation_of(SessionContext& session, EntityID entity_id) -> float {
    auto* entity = session.world().get_entity(entity_id);
    const auto* transform =
        entity != nullptr ? entity->get_component<TransformComponent>() : nullptr;
    return transform != nullptr ? transform->rotation.y : 0.0F;
  }

  static auto health_of(SessionContext& session, EntityID unit) -> int {
    auto* entity = session.world().get_entity(unit);
    const auto* unit_comp =
        entity != nullptr ? entity->get_component<UnitComponent>() : nullptr;
    return unit_comp != nullptr ? unit_comp->health : 0;
  }

  static auto melee_locked(SessionContext& session, EntityID unit) -> bool {
    auto* entity = session.world().get_entity(unit);
    const auto* attack =
        entity != nullptr ? entity->get_component<AttackComponent>() : nullptr;
    return attack != nullptr && attack->in_melee_lock;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(WallSiegeTest, WallKeepsItsOrientationWhileBeingHacked) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID wall = build_wall(*session);
  ASSERT_NE(wall, 0U);

  const float rotation_before = rotation_of(*session, wall);
  const EntityID raider = spawn_troop(*session,
                                      Game::Units::SpawnType::Knight,
                                      k_raider,
                                      0.0F,
                                      besieged_face_z() - 3.0F);
  ASSERT_NE(raider, 0U);
  order_attack(*session, raider, wall);

  run_for(*session, 12.0);

  ASSERT_LT(health_of(*session, wall), 4000) << "the wall was never actually attacked";
  EXPECT_NEAR(rotation_of(*session, wall), rotation_before, 0.01F)
      << "the wall turned to face its attacker, breaking the line of the wall";
}

TEST_F(WallSiegeTest, WallKeepsItsOrientationWhenStruckFromAnAngle) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID wall = build_wall(*session);
  ASSERT_NE(wall, 0U);

  const float rotation_before = rotation_of(*session, wall);
  const EntityID raider = spawn_troop(*session,
                                      Game::Units::SpawnType::Knight,
                                      k_raider,
                                      -3.0F,
                                      besieged_face_z() - 2.0F);
  ASSERT_NE(raider, 0U);
  order_attack(*session, raider, wall);

  run_for(*session, 15.0);

  EXPECT_NEAR(rotation_of(*session, wall), rotation_before, 0.01F);
  const float contact_clearance = Game::Systems::Combat::structure_attack_profile(
                                      session->world().get_entity(raider))
                                      .contact_clearance;
  EXPECT_LE(position_of(*session, raider).z(),
            besieged_face_z() - contact_clearance + 0.05F)
      << "the raider embedded in the wall while trying to strike it";
}

TEST_F(WallSiegeTest, GateKeepsItsAxisAfterBeingHackedAndLosingANeighbour) {
  auto session = make_match();
  const ScopedSession scope(*session);

  constexpr int k_gate_grid_x = k_map_size / 2;
  EntityID west_neighbour = 0;
  EntityID east_neighbour = 0;
  for (int grid_x = -8; grid_x <= k_map_size + 8; grid_x += k_wall_pitch) {
    if (grid_x == k_gate_grid_x) {
      continue;
    }
    const EntityID segment = add_wall_segment(*session, grid_x, k_wall_grid_z);
    if (grid_x == k_gate_grid_x - k_wall_pitch) {
      west_neighbour = segment;
    } else if (grid_x == k_gate_grid_x + k_wall_pitch) {
      east_neighbour = segment;
    }
  }
  const EntityID gate = add_wall_piece(
      *session, Game::Units::SpawnType::WallGate, k_gate_grid_x, k_wall_grid_z);
  Game::Systems::WallNetworkService::refresh_world(session->world());
  ASSERT_NE(gate, 0U);
  ASSERT_NE(west_neighbour, 0U);
  ASSERT_NE(east_neighbour, 0U);

  const QVector3D breach = position_of(*session, west_neighbour);
  demolish(*session, west_neighbour);
  Game::Systems::WallNetworkService::refresh_world(session->world());

  const EntityID raider = spawn_troop(
      *session, Game::Units::SpawnType::Knight, k_raider, breach.x(), breach.z());
  ASSERT_NE(raider, 0U);
  order_attack(*session, raider, gate);
  run_for(*session, 8.0);

  demolish(*session, east_neighbour);
  Game::Systems::WallNetworkService::refresh_world(session->world());

  EXPECT_NEAR(rotation_of(*session, gate), 0.0F, 0.01F)
      << "the gate turned across the wall instead of standing in it";
  const auto* footprint = session->building_collision().find_building(gate);
  ASSERT_NE(footprint, nullptr);
  EXPECT_GT(footprint->width, footprint->depth)
      << "the gate's footprint no longer spans the line of the wall";
}

TEST_F(WallSiegeTest, TroopsPressedAgainstOppositeFacesCannotFightThroughTheWall) {
  auto session = make_match();
  const ScopedSession scope(*session);
  ASSERT_NE(build_wall(*session), 0U);

  const EntityID raider = spawn_troop(*session,
                                      Game::Units::SpawnType::Spearman,
                                      k_raider,
                                      0.0F,
                                      besieged_face_z() - 0.5F);
  const EntityID defender = spawn_troop(*session,
                                        Game::Units::SpawnType::Spearman,
                                        k_defender,
                                        0.0F,
                                        defended_face_z() + 0.5F);
  ASSERT_NE(raider, 0U);
  ASSERT_NE(defender, 0U);

  const int raider_health = health_of(*session, raider);
  const int defender_health = health_of(*session, defender);
  order_attack(*session, raider, defender);
  order_attack(*session, defender, raider);

  bool raider_locked = false;
  bool defender_locked = false;
  const double step = session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 12.0; elapsed += step) {
    run_for(*session, step);
    raider_locked = raider_locked || melee_locked(*session, raider);
    defender_locked = defender_locked || melee_locked(*session, defender);
  }

  EXPECT_FALSE(raider_locked) << "raider locked into melee through the wall";
  EXPECT_FALSE(defender_locked) << "defender locked into melee through the wall";
  EXPECT_EQ(health_of(*session, raider), raider_health);
  EXPECT_EQ(health_of(*session, defender), defender_health);
  EXPECT_LT(position_of(*session, raider).z(), besieged_face_z());
  EXPECT_GT(position_of(*session, defender).z(), defended_face_z());
}

TEST_F(WallSiegeTest, AnAssaultOnAnUnbreachedWallNeverGetsBehindIt) {
  auto session = make_match();
  const ScopedSession scope(*session);
  ASSERT_NE(build_wall(*session), 0U);

  std::vector<EntityID> raiders;
  std::vector<EntityID> defenders;
  for (int i = -2; i <= 2; ++i) {
    const auto lane = static_cast<float>(i) * 2.0F;
    const EntityID raider = spawn_troop(*session,
                                        Game::Units::SpawnType::Spearman,
                                        k_raider,
                                        lane,
                                        besieged_face_z() - 4.0F);
    const EntityID defender = spawn_troop(*session,
                                          Game::Units::SpawnType::Knight,
                                          k_defender,
                                          lane,
                                          defended_face_z() + 1.5F);
    ASSERT_NE(raider, 0U);
    ASSERT_NE(defender, 0U);
    raiders.push_back(raider);
    defenders.push_back(defender);
  }

  std::vector<int> defender_health;
  defender_health.reserve(defenders.size());
  for (const EntityID defender : defenders) {
    defender_health.push_back(health_of(*session, defender));
  }

  for (const EntityID raider : raiders) {
    order_attack(*session, raider, defenders.front());
  }

  float deepest_z = -1000.0F;
  const double step = session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 30.0; elapsed += step) {
    run_for(*session, step);
    for (const EntityID raider : raiders) {
      deepest_z = std::max(deepest_z, position_of(*session, raider).z());
    }
  }

  EXPECT_LT(deepest_z, besieged_face_z()) << "an attacker crossed an unbreached wall";
  for (std::size_t i = 0; i < defenders.size(); ++i) {
    EXPECT_EQ(health_of(*session, defenders[i]), defender_health[i])
        << "defender " << i << " was fought through the wall";
  }
}

TEST_F(WallSiegeTest, ShutGateKeepsRaidersOffTheTroopsBehindIt) {
  auto session = make_match();
  const ScopedSession scope(*session);

  constexpr int k_gate_grid_x = k_map_size / 2;
  for (int grid_x = -8; grid_x <= k_map_size + 8; grid_x += k_wall_pitch) {
    if (grid_x == k_gate_grid_x) {
      continue;
    }
    add_wall_segment(*session, grid_x, k_wall_grid_z);
  }
  const EntityID gate = add_wall_piece(
      *session, Game::Units::SpawnType::WallGate, k_gate_grid_x, k_wall_grid_z);
  Game::Systems::WallNetworkService::refresh_world(session->world());
  ASSERT_NE(gate, 0U);

  Game::Systems::GateService::set_manual_mode(
      *session->world().get_entity(gate),
      Engine::Core::GateComponent::ManualMode::ForcedClosed);

  const QVector3D gate_position = position_of(*session, gate);
  const EntityID raider = spawn_troop(*session,
                                      Game::Units::SpawnType::Spearman,
                                      k_raider,
                                      gate_position.x(),
                                      besieged_face_z() - 0.5F);
  const EntityID defender = spawn_troop(*session,
                                        Game::Units::SpawnType::Spearman,
                                        k_defender,
                                        gate_position.x(),
                                        defended_face_z() + 1.0F);
  ASSERT_NE(raider, 0U);
  ASSERT_NE(defender, 0U);

  Game::Command::submit(session->world(),
                        Game::Command::Source::Script,
                        k_defender,
                        Game::Command::SetHold{.units = {defender}, .active = true});
  order_attack(*session, raider, defender);

  int defender_health = health_of(*session, defender);
  bool raider_locked = false;
  bool defender_wounded = false;
  const double step = session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 12.0; elapsed += step) {
    run_for(*session, step);
    raider_locked = raider_locked || melee_locked(*session, raider);
    const int health = health_of(*session, defender);
    defender_wounded = defender_wounded || health < defender_health;
    defender_health = health;
  }

  EXPECT_FALSE(raider_locked) << "raider locked into melee through a shut gate";
  EXPECT_FALSE(defender_wounded) << "defender was hit through a shut gate";
  EXPECT_LT(position_of(*session, raider).z(), besieged_face_z())
      << "raider walked in through a shut gate";
}

TEST_F(WallSiegeTest, RaiderCannotReachATargetBehindAnIntactWall) {
  auto session = make_match();
  const ScopedSession scope(*session);
  ASSERT_NE(build_wall(*session), 0U);

  const EntityID raider = spawn_troop(*session,
                                      Game::Units::SpawnType::Knight,
                                      k_raider,
                                      0.0F,
                                      besieged_face_z() - 3.0F);
  const EntityID defender = spawn_troop(*session,
                                        Game::Units::SpawnType::Knight,
                                        k_defender,
                                        0.0F,
                                        defended_face_z() + 0.4F);
  ASSERT_NE(raider, 0U);
  ASSERT_NE(defender, 0U);
  const int defender_health = health_of(*session, defender);
  order_attack(*session, raider, defender);

  float deepest_z = -1000.0F;
  const double step = session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 15.0; elapsed += step) {
    run_for(*session, step);
    deepest_z = std::max(deepest_z, position_of(*session, raider).z());
  }

  EXPECT_LT(deepest_z, besieged_face_z()) << "raider walked through the wall";
  EXPECT_EQ(health_of(*session, defender), defender_health)
      << "defender was hit through a wall that was never breached";
}

} // namespace

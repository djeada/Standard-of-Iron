#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "game/core/component_structures.h"
#include "game/core/movement_trace.h"
#include "game/core/movement_trace_analysis.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/movement_route.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/route_corridor_planner.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "tests/support/movement_test_access.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::MovementFactsComponent;
using Engine::Core::MovementOrderState;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::MovementRoute;
using Game::Systems::NavGrid;
using Game::Systems::Point;
using Game::Systems::RouteCorridorPlanner;

auto corner_route() -> MovementRoute {
  MovementRoute route;
  const std::vector<std::pair<float, float>> waypoints{{5.0F, 0.0F}, {5.0F, 5.0F}};
  EXPECT_TRUE(route.build(1U, 0U, 0.0F, 0.0F, waypoints, 0U, 0.0F, 0.0F));
  return route;
}

} // namespace

TEST(MovementRouteTest, ArclengthIsTheSumOfTheSegments) {
  const auto route = corner_route();
  EXPECT_EQ(route.point_count(), 3U);
  EXPECT_FLOAT_EQ(route.length(), 10.0F);
  EXPECT_FLOAT_EQ(route.remaining(), 10.0F);
}

TEST(MovementRouteTest, ProgressNeverRunsBackwards) {
  auto route = corner_route();

  route.advance_to(route.project(4.0F, 0.0F, 6.0F).s);
  const float advanced = route.travelled();
  EXPECT_NEAR(advanced, 4.0F, 1.0e-3F);

  route.advance_to(route.project(1.0F, 0.0F, 6.0F).s);
  EXPECT_GE(route.travelled(), advanced);
}

TEST(MovementRouteTest, ProjectionOnlySearchesForwardWithinItsWindow) {
  auto route = corner_route();
  route.advance_to(route.project(5.0F, 4.0F, 12.0F).s);
  EXPECT_NEAR(route.travelled(), 9.0F, 1.0e-3F);

  const auto projection = route.project(1.0F, 0.0F, 1.0F);
  EXPECT_GE(projection.s, 9.0F);
}

TEST(MovementRouteTest, LateralErrorIsTheDistanceOffTheLine) {
  auto route = corner_route();
  const auto projection = route.project(2.0F, 1.5F, 6.0F);
  EXPECT_NEAR(projection.s, 2.0F, 1.0e-3F);
  EXPECT_NEAR(projection.lateral, 1.5F, 1.0e-3F);
}

TEST(MovementRouteTest, SteeringNeverAimsPastTheCorner) {
  const auto route = corner_route();

  EXPECT_NEAR(route.next_vertex_s(2.0F), 5.0F, 1.0e-3F);
  EXPECT_NEAR(route.next_vertex_s(5.0F), 10.0F, 1.0e-3F);

  const auto tangent = route.tangent_at(2.0F);
  EXPECT_NEAR(tangent.first, 1.0F, 1.0e-3F);
  EXPECT_NEAR(tangent.second, 0.0F, 1.0e-3F);
}

TEST(MovementRouteTest, ADirectTargetIsAOneSegmentRoute) {
  MovementRoute route;
  ASSERT_TRUE(route.build(1U, 0U, 0.0F, 0.0F, {}, 0U, 3.0F, 4.0F));
  EXPECT_EQ(route.point_count(), 2U);
  EXPECT_FLOAT_EQ(route.length(), 5.0F);
}

TEST(MovementRouteTest, ADegenerateBuildStillRecordsItsRevision) {
  MovementRoute route;
  EXPECT_FALSE(route.build(7U, 0U, 1.0F, 1.0F, {}, 0U, 1.0F, 1.0F));
  EXPECT_FALSE(route.valid());

  EXPECT_EQ(route.route_revision(), 7U);
}

TEST(MovementRouteTest, AMovingGoalKeepsTheArclengthAlreadyTravelled) {
  auto route = corner_route();
  route.advance_to(route.project(5.0F, 2.0F, 8.0F).s);
  const float travelled = route.travelled();

  route.update_final_point(5.0F, 6.0F);
  EXPECT_FLOAT_EQ(route.length(), 11.0F);
  EXPECT_FLOAT_EQ(route.travelled(), travelled);
}

TEST(RouteCorridorPlannerTest, MembersReceiveDistinctLanesWithValidConnectors) {
  NavGrid::initialize(64, 64);
  auto* pathfinder = NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  QVector3D const center_start = NavGrid::grid_to_world({12, 32});
  QVector3D const center_destination = NavGrid::grid_to_world({52, 32});
  auto const corridor =
      RouteCorridorPlanner::plan(*pathfinder,
                                 center_start,
                                 center_destination,
                                 Game::Systems::Pathfinding::Passability::Light,
                                 0.25F);
  ASSERT_TRUE(corridor.reachable());

  QVector3D const left_start = center_start + QVector3D(-2.0F, 0.0F, -3.0F);
  QVector3D const right_start = center_start + QVector3D(-2.0F, 0.0F, 3.0F);
  QVector3D const left_destination = center_destination + QVector3D(2.0F, 0.0F, -3.0F);
  QVector3D const right_destination = center_destination + QVector3D(2.0F, 0.0F, 3.0F);

  auto const left =
      RouteCorridorPlanner::fit_lane(*pathfinder,
                                     corridor,
                                     left_start,
                                     left_destination,
                                     3.0F,
                                     Game::Systems::Pathfinding::Passability::Light,
                                     0.25F);
  auto const right =
      RouteCorridorPlanner::fit_lane(*pathfinder,
                                     corridor,
                                     right_start,
                                     right_destination,
                                     -3.0F,
                                     Game::Systems::Pathfinding::Passability::Light,
                                     0.25F);
  ASSERT_TRUE(left.valid());
  ASSERT_TRUE(right.valid());

  EXPECT_EQ(left.waypoints.front(), left_start);
  EXPECT_EQ(right.waypoints.front(), right_start);
  EXPECT_EQ(left.waypoints.back(), left_destination);
  EXPECT_EQ(right.waypoints.back(), right_destination);
  ASSERT_GT(left.waypoints.size(), 3U);
  ASSERT_GT(right.waypoints.size(), 3U);
  EXPECT_LT(left.waypoints[left.waypoints.size() / 2U].z(),
            right.waypoints[right.waypoints.size() / 2U].z());

  auto expect_route_clear = [&](auto const& lane) {
    for (std::size_t index = 1; index < lane.waypoints.size(); ++index) {
      EXPECT_TRUE(pathfinder->is_world_segment_walkable(
          lane.waypoints[index - 1U],
          lane.waypoints[index],
          Game::Systems::Pathfinding::Passability::Light,
          0.25F));
    }
  };
  expect_route_clear(left);
  expect_route_clear(right);
}

TEST(RouteCorridorPlannerTest, NarrowPortalDeclaresControlledLaneCompression) {
  NavGrid::initialize(64, 64);
  auto* pathfinder = NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  for (int x = 29; x <= 35; ++x) {
    for (int z = 0; z < 64; ++z) {
      if (z >= 29 && z <= 35) {
        continue;
      }
      pathfinder->set_obstacle(x, z, true);
    }
  }

  QVector3D const start = NavGrid::grid_to_world({12, 32});
  QVector3D const destination = NavGrid::grid_to_world({52, 32});
  EXPECT_FALSE(pathfinder->is_world_position_walkable(
      NavGrid::grid_to_world({32, 38}),
      Game::Systems::Pathfinding::Passability::Light,
      0.25F));
  EXPECT_FALSE(pathfinder->is_world_segment_walkable(
      start + QVector3D(0.0F, 0.0F, 12.0F),
      destination + QVector3D(0.0F, 0.0F, 12.0F),
      Game::Systems::Pathfinding::Passability::Light,
      0.25F));
  Game::Systems::RouteCorridorPlan corridor;
  corridor.id = 1U;
  corridor.centerline = {start, destination};
  ASSERT_TRUE(corridor.reachable());

  auto const lane =
      RouteCorridorPlanner::fit_lane(*pathfinder,
                                     corridor,
                                     start + QVector3D(0.0F, 0.0F, 12.0F),
                                     destination + QVector3D(0.0F, 0.0F, 12.0F),
                                     -12.0F,
                                     Game::Systems::Pathfinding::Passability::Light,
                                     0.25F);
  ASSERT_TRUE(lane.valid());
  EXPECT_TRUE(lane.requires_controlled_break());
  EXPECT_LT(lane.minimum_lateral_scale, 1.0F);
  EXPECT_TRUE(lane.opening_point.has_value());
  EXPECT_TRUE(lane.reform_point.has_value());
}

TEST(RouteCorridorPlannerTest, LaneScaleReformsAfterTheExitWaypoint) {
  Engine::Core::MovementComponent movement;
  MovementTestAccess::set_path(
      movement, {{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}, {3.0F, 0.0F}});
  MovementTestAccess::set_route_lane_state(movement, 0.25F, 1U, 3U);

  MovementTestAccess::set_path_index(movement, 0U);
  EXPECT_FLOAT_EQ(movement.get_route_lane_scale(), 1.0F);
  MovementTestAccess::set_path_index(movement, 1U);
  EXPECT_FLOAT_EQ(movement.get_route_lane_scale(), 0.25F);
  MovementTestAccess::set_path_index(movement, 2U);
  EXPECT_FLOAT_EQ(movement.get_route_lane_scale(), 0.25F);
  MovementTestAccess::set_path_index(movement, 3U);
  EXPECT_FLOAT_EQ(movement.get_route_lane_scale(), 1.0F);
}

namespace {

class MovementMotorTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_owner, 1);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(k_map, k_map);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void reset_session() {
    m_scope.reset();
    m_session.reset();
    SetUp();
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  auto spawn(Game::Units::SpawnType type, const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void block_cell(int grid_x, int grid_z) {
    const auto position = world_of(grid_x, grid_z);
    auto* entity = m_session->world().create_entity();
    entity->add_component<TransformComponent>(position.x(), 0.0F, position.z());
    auto* unit = entity->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_owner;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "wall_segment",
        position.x(),
        position.z(),
        k_owner,
        {.width = 1.0F, .depth = 1.0F});
  }

  void seal_column(int grid_x) {
    for (int grid_z = 0; grid_z < k_map; ++grid_z) {
      block_cell(grid_x, grid_z);
    }
    auto* pathfinder = NavGrid::get_pathfinder();
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  void run_for(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    const auto* transform =
        entity == nullptr ? nullptr : entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  auto facts_of(EntityID id) -> const MovementFactsComponent* {
    auto* entity = m_session->world().get_entity(id);
    return entity == nullptr ? nullptr
                             : entity->get_component<MovementFactsComponent>();
  }

  static constexpr int k_map = 48;
  static constexpr int k_owner = 1;

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

} // namespace

TEST_F(MovementMotorTest, AnUnreachableGoalEndsInADeclaredOutcome) {
  seal_column(30);
  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(20, 24));
  ASSERT_NE(id, 0U);

  CommandService::move_unit(m_session->world(), id, world_of(40, 24));
  run_for(20.0);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_FALSE(Engine::Core::is_active_movement_state(facts->progress.state))
      << "the order is still "
      << Engine::Core::movement_state_name(facts->progress.state)
      << " twenty seconds after the goal proved unreachable";

  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_FALSE(movement->get_has_target());
}

TEST_F(MovementMotorTest, AnElephantWindsUpInsteadOfLeapingToSpeed) {
  const EntityID id = spawn(Game::Units::SpawnType::Elephant, world_of(10, 24));
  ASSERT_NE(id, 0U);

  CommandService::move_unit(m_session->world(), id, world_of(50, 24));
  const double step = m_session->clock().tick_seconds();
  QVector3D last = position_of(id);
  float last_speed = 0.0F;
  float peak_gain_per_second = 0.0F;
  float peak_speed = 0.0F;
  for (int tick = 0; tick < 240; ++tick) {
    run_for(step);
    const QVector3D now = position_of(id);
    const float speed =
        std::hypot(now.x() - last.x(), now.z() - last.z()) / static_cast<float>(step);
    peak_gain_per_second =
        std::max(peak_gain_per_second, (speed - last_speed) / static_cast<float>(step));
    peak_speed = std::max(peak_speed, speed);
    last_speed = speed;
    last = now;
  }
  EXPECT_LE(peak_gain_per_second,
            Game::Units::body_acceleration(Game::Units::SpawnType::Elephant) + 0.3F)
      << "the elephant leapt to speed instead of winding up";
  EXPECT_GT(peak_speed, 1.4F) << "the ramp never let the elephant reach travel speed";
}

TEST_F(MovementMotorTest, AFootSoldierStillStepsOffAtOnce) {
  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(10, 24));
  ASSERT_NE(id, 0U);

  CommandService::move_unit(m_session->world(), id, world_of(50, 24));
  run_for(0.6);
  const QVector3D start = position_of(id);
  run_for(0.25);
  const QVector3D now = position_of(id);
  const float speed = std::hypot(now.x() - start.x(), now.z() - start.z()) / 0.25F;
  EXPECT_GT(speed, 1.0F) << "an unramped body type should be at travel speed "
                            "once it has turned to face its goal";
}

TEST_F(MovementMotorTest, AWallDoesNotStopTravelAlongIt) {
  seal_column(30);
  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(28, 10));
  ASSERT_NE(id, 0U);

  CommandService::move_unit(m_session->world(), id, world_of(29, 40));
  run_for(12.0);

  EXPECT_GT(position_of(id).z(), world_of(28, 24).z())
      << "the body stopped at the wall instead of travelling along it";
}

TEST_F(MovementMotorTest, AnOpenGroundOrderProducesNoFindings) {
  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(10, 24));
  ASSERT_NE(id, 0U);

  Engine::Core::MovementTraceManifest manifest;
  manifest.scenario = "open_ground_single_troop";
  manifest.fixed_step_seconds = static_cast<float>(m_session->clock().tick_seconds());
  const Engine::Core::ScopedMovementTrace trace(manifest);

  CommandService::move_unit(m_session->world(), id, world_of(38, 24));
  run_for(30.0);

  Engine::Core::MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = manifest.fixed_step_seconds;
  const auto analysis = Engine::Core::analyze_active_movement_trace(thresholds);
  EXPECT_TRUE(analysis.passed()) << Engine::Core::format_movement_findings(analysis)
                                 << Engine::Core::format_movement_summary(analysis);

  EXPECT_GT(position_of(id).x(), world_of(36, 24).x());
}

TEST_F(MovementMotorTest, CrossingStreamsBothClearWithoutFindings) {
  Engine::Core::MovementTraceManifest manifest;
  manifest.scenario = "crossing_streams";
  manifest.fixed_step_seconds = static_cast<float>(m_session->clock().tick_seconds());
  const Engine::Core::ScopedMovementTrace trace(manifest);

  std::vector<EntityID> eastbound;
  std::vector<EntityID> northbound;
  for (int index = 0; index < 4; ++index) {
    eastbound.push_back(
        spawn(Game::Units::SpawnType::Spearman, world_of(10, 18 + index * 5)));
    northbound.push_back(
        spawn(Game::Units::SpawnType::Spearman, world_of(18 + index * 5, 10)));
  }

  std::vector<QVector3D> east_targets;
  std::vector<QVector3D> north_targets;
  for (int index = 0; index < 4; ++index) {
    east_targets.push_back(world_of(38, 18 + index * 5));
    north_targets.push_back(world_of(18 + index * 5, 38));
  }
  CommandService::move_units(m_session->world(), eastbound, east_targets);
  CommandService::move_units(m_session->world(), northbound, north_targets);

  run_for(45.0);

  Engine::Core::MovementGateThresholds thresholds;
  thresholds.fixed_step_seconds = manifest.fixed_step_seconds;
  const auto analysis = Engine::Core::analyze_active_movement_trace(thresholds);
  EXPECT_TRUE(analysis.passed()) << Engine::Core::format_movement_findings(analysis)
                                 << Engine::Core::format_movement_summary(analysis);

  for (const auto id : eastbound) {
    EXPECT_GT(position_of(id).x(), world_of(34, 24).x())
        << "an eastbound member never crossed";
  }
  for (const auto id : northbound) {
    EXPECT_GT(position_of(id).z(), world_of(24, 34).z())
        << "a northbound member never crossed";
  }
}

TEST_F(MovementMotorTest, OpenGroundTravelIsUnconstrainedBesideACrowd) {
  std::vector<EntityID> crowd;
  for (int index = 0; index < 8; ++index) {
    crowd.push_back(spawn(Game::Units::SpawnType::Spearman, world_of(20 + index, 8)));
  }
  const EntityID lone = spawn(Game::Units::SpawnType::Spearman, world_of(10, 30));
  ASSERT_NE(lone, 0U);

  std::vector<QVector3D> crowd_targets(crowd.size(), world_of(24, 16));
  CommandService::move_units(m_session->world(), crowd, crowd_targets);
  CommandService::move_unit(m_session->world(), lone, world_of(38, 30));

  run_for(6.0);

  const auto* facts = facts_of(lone);
  ASSERT_NE(facts, nullptr);
  EXPECT_EQ(facts->steering.result, Engine::Core::SteeringResult::Unconstrained)
      << "an untroubled body was steered by a crowd it will never meet";
  EXPECT_NEAR(facts->steering.correction_x, 0.0F, 1.0e-4F);
  EXPECT_NEAR(facts->steering.correction_z, 0.0F, 1.0e-4F);
}

TEST_F(MovementMotorTest, HeadOnBodiesPassEachOtherInsteadOfJamming) {
  const EntityID west = spawn(Game::Units::SpawnType::Spearman, world_of(16, 24));
  const EntityID east = spawn(Game::Units::SpawnType::Spearman, world_of(32, 24));
  ASSERT_NE(west, 0U);
  ASSERT_NE(east, 0U);

  CommandService::move_unit(m_session->world(), west, world_of(34, 24));
  CommandService::move_unit(m_session->world(), east, world_of(14, 24));

  float closest_approach = std::numeric_limits<float>::max();
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 12.0; elapsed += step) {
    m_session->clock().advance(step);
    while (m_session->clock().consume_tick()) {
      m_session->world().update(static_cast<float>(step));
    }
    const QVector3D separation = position_of(west) - position_of(east);
    closest_approach =
        std::min(closest_approach, std::hypot(separation.x(), separation.z()));
  }

  EXPECT_GT(position_of(west).x(), world_of(30, 24).x())
      << "the westward body never got past the one walking at it";
  EXPECT_LT(position_of(east).x(), world_of(18, 24).x())
      << "the eastward body never got past the one walking at it";
  EXPECT_GT(closest_approach, 0.1F) << "the two bodies passed through the same ground";
}

TEST_F(MovementMotorTest, RepeatedRunsAgreeOnTheMovementDigest) {
  auto capture = [this]() {
    reset_session();
    Engine::Core::MovementTraceManifest manifest;
    manifest.scenario = "crowd_digest";
    const Engine::Core::ScopedMovementTrace trace(manifest);

    std::vector<EntityID> crowd;
    for (int index = 0; index < 9; ++index) {
      crowd.push_back(spawn(Game::Units::SpawnType::Spearman,
                            world_of(14 + (index % 3), 22 + (index / 3))));
    }
    std::vector<QVector3D> targets;
    for (std::size_t index = 0; index < crowd.size(); ++index) {
      targets.push_back(
          world_of(34 + static_cast<int>(index % 3), 22 + static_cast<int>(index / 3)));
    }
    CommandService::move_units(m_session->world(), crowd, targets);
    run_for(20.0);

    auto& sink = Engine::Core::MovementTrace::instance();
    const auto digest =
        Engine::Core::movement_digest(sink.troop_samples(), sink.soldier_samples());
    return digest;
  };

  const auto first = capture();
  const auto second = capture();
  EXPECT_EQ(first, second) << "the same command stream produced two behaviours";
}

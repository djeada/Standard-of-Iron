#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

#include "game/core/component.h"
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
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

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

// An L: five metres east, then five metres north.
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

  // Pushed back down the route by a crowd: the body is nearer the start, but
  // the route it has consumed is not.
  route.advance_to(route.project(1.0F, 0.0F, 6.0F).s);
  EXPECT_GE(route.travelled(), advanced);
}

TEST(MovementRouteTest, ProjectionOnlySearchesForwardWithinItsWindow) {
  auto route = corner_route();
  route.advance_to(route.project(5.0F, 4.0F, 12.0F).s);
  EXPECT_NEAR(route.travelled(), 9.0F, 1.0e-3F);

  // A point sitting on the first leg is outside the window, so it cannot
  // re-acquire a part of the route the body already left.
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
  // Two metres in, a two-metre lookahead would otherwise cut the corner and
  // walk the body through whatever the corner is bending around.
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
  // Otherwise the follower rebuilds it every tick, and a rebuild resets the
  // progress measurement -- which is how an order stayed Following for
  // seventeen seconds without moving.
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

  // A determinism comparison needs two runs whose entity identities line up, so
  // each capture gets its own world rather than reusing one with fresh handles.
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

// Gate 2: an accepted order never stays active forever. A goal behind a sealed
// wall must reach a declared terminal outcome, not idle with the order alive.
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

// The old motor tried global X, then global Z, and zeroed the other axis'
// velocity. A body walking diagonally into a wall therefore stopped dead
// instead of sliding along it.
TEST_F(MovementMotorTest, AWallDoesNotStopTravelAlongIt) {
  seal_column(30);
  const EntityID id = spawn(Game::Units::SpawnType::Spearman, world_of(28, 10));
  ASSERT_NE(id, 0U);

  // A goal on the far side and well up the wall: the route hugs the wall, and
  // the body must keep making ground along it.
  CommandService::move_unit(m_session->world(), id, world_of(29, 40));
  run_for(12.0);

  EXPECT_GT(position_of(id).z(), world_of(28, 24).z())
      << "the body stopped at the wall instead of travelling along it";
}

// Gate 2 again: a whole run of an ordinary open-ground order must produce no
// findings at all.
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

// Gate 3: two crossing streams must both clear without starvation, without an
// unresolved overlap, and without either side alternating its passing choice.
TEST_F(MovementMotorTest, CrossingStreamsBothClearWithoutFindings) {
  Engine::Core::MovementTraceManifest manifest;
  manifest.scenario = "crossing_streams";
  manifest.fixed_step_seconds = static_cast<float>(m_session->clock().tick_seconds());
  const Engine::Core::ScopedMovementTrace trace(manifest);

  // A troop's body is its whole formation, several metres across, so both the
  // spawn line and the destination slots are spaced wider than that. Slots
  // closer together than the bodies are wide are a group-placement defect, and
  // Milestone 4 owns it; this scenario is about the crossing.
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

// A body already following a route on open ground must not wobble merely
// because a crowd is armed somewhere nearby.
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

// A head-on pair commits to one side and holds it. The old solver averaged
// every overlap correction, so a symmetric encounter could alternate between
// two equally plausible escapes for as long as it lasted.
TEST_F(MovementMotorTest, APassingSideIsHeldOnceCommitted) {
  const EntityID west = spawn(Game::Units::SpawnType::Spearman, world_of(16, 24));
  const EntityID east = spawn(Game::Units::SpawnType::Spearman, world_of(32, 24));
  ASSERT_NE(west, 0U);
  ASSERT_NE(east, 0U);

  CommandService::move_unit(m_session->world(), west, world_of(34, 24));
  CommandService::move_unit(m_session->world(), east, world_of(14, 24));

  std::vector<std::int8_t> west_sides;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 12.0; elapsed += step) {
    m_session->clock().advance(step);
    while (m_session->clock().consume_tick()) {
      m_session->world().update(static_cast<float>(step));
    }
    if (const auto* facts = facts_of(west); facts != nullptr) {
      west_sides.push_back(facts->steering.passing_side);
    }
  }

  int reversals = 0;
  std::int8_t committed = 0;
  for (const auto side : west_sides) {
    if (side == 0) {
      continue;
    }
    if (committed != 0 && side != committed) {
      ++reversals;
    }
    committed = side;
  }
  EXPECT_LE(reversals, 1) << "the passing side alternated " << reversals << " times";

  // And both bodies get past each other rather than deadlocking nose to nose.
  EXPECT_GT(position_of(west).x(), world_of(30, 24).x());
  EXPECT_LT(position_of(east).x(), world_of(18, 24).x());
}

// Gate 3: repeated runs of the same commands in one binary produce the same
// movement digest.
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

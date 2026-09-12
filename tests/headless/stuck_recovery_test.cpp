

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component.h"
#include "game/core/movement_facts.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/walkability.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::MovementFactsComponent;
using Engine::Core::MovementOrderState;
using Engine::Core::MovementRecoveryRung;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_owner = 1;
constexpr int k_map = 48;

constexpr double k_recovery_budget_seconds = 16.0;

class StuckRecoveryTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    open_field();
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void open_field() {
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
    NavGrid::initialize(map.grid.width, map.grid.height);
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  auto spawn(const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = Game::Units::SpawnType::Spearman;
    params.rotation_y = 90.0F;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit =
        m_factory->create(Game::Units::SpawnType::Spearman, m_session->world(), params);
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

  void refresh_grid() {
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  void seal_a_pen(int centre_x, int centre_z, int half_extent) {
    for (int offset = -half_extent; offset <= half_extent; ++offset) {
      block_cell(centre_x + offset, centre_z - half_extent);
      block_cell(centre_x + offset, centre_z + half_extent);
      block_cell(centre_x - half_extent, centre_z + offset);
      block_cell(centre_x + half_extent, centre_z + offset);
    }
    refresh_grid();
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

  auto facts_of(EntityID id) -> const MovementFactsComponent* {
    auto* entity = m_session->world().get_entity(id);
    return entity == nullptr ? nullptr
                             : entity->get_component<MovementFactsComponent>();
  }

  auto movement_of(EntityID id) -> const MovementComponent* {
    auto* entity = m_session->world().get_entity(id);
    return entity == nullptr ? nullptr : entity->get_component<MovementComponent>();
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    const auto* transform = entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(StuckRecoveryTest, AnOrderIntoASealedPenIsGivenUpOnWithinTheRecoveryBudget) {
  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 3);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));

  ASSERT_NE(movement_of(id), nullptr);
  ASSERT_TRUE(movement_of(id)->get_has_target())
      << "the order was refused outright, so there is no recovery to test";

  run_for(k_recovery_budget_seconds);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(facts->progress.stall.objective_abandoned)
      << "a unit walled in on every side still believed it was going somewhere";
  EXPECT_EQ(facts->progress.stall.rung, MovementRecoveryRung::Abandoned);
  EXPECT_EQ(facts->progress.state, MovementOrderState::Unreachable);
  EXPECT_FALSE(movement_of(id)->get_has_target())
      << "the objective was abandoned but the unit was left holding it";
}

TEST_F(StuckRecoveryTest, ANoRouteOrderWaitsWithoutShufflingThenEndsOnce) {

  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 3);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));

  std::vector<MovementRecoveryRung> seen;
  bool held = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < k_recovery_budget_seconds; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    if (facts == nullptr) {
      continue;
    }
    held = held || facts->progress.holding_at_obstruction;
    if (seen.empty() || seen.back() != facts->progress.stall.rung) {
      seen.push_back(facts->progress.stall.rung);
    }
  }

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(held) << "the unit never waited at the obstruction";
  EXPECT_TRUE(facts->progress.stall.objective_abandoned)
      << "the order was never given up";
  EXPECT_EQ(facts->progress.stall.recovery_attempts, 0U)
      << "the recovery ladder ran for a unit the planner had no route for";
  EXPECT_LE(facts->progress.repath_count, 1U);
  for (auto const rung : seen) {
    EXPECT_TRUE(rung == MovementRecoveryRung::None ||
                rung == MovementRecoveryRung::Abandoned)
        << "a waiting unit climbed recovery rung " << static_cast<int>(rung);
  }
  EXPECT_LT((position_of(id) - world_of(k_pen_x, k_pen_z)).length(), 2.0F)
      << "the unit shuffled about inside the pen";
}

TEST_F(StuckRecoveryTest, AnOrderFromACellWithNoWayOutIsGivenUpNotHeldForever) {

  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 1);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));
  ASSERT_NE(movement_of(id), nullptr);
  ASSERT_TRUE(movement_of(id)->get_has_target())
      << "the order was refused outright, so there is nothing to give up";

  run_for(k_recovery_budget_seconds + 4.0);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_FALSE(movement_of(id)->get_has_target())
      << "the unit was still holding an order it can never carry out; state "
      << Engine::Core::movement_state_name(facts->progress.state) << " holding "
      << facts->progress.holding_at_obstruction << " held for "
      << facts->progress.holding_seconds << " s";
  EXPECT_TRUE(facts->progress.stall.objective_abandoned);
  EXPECT_FALSE(facts->progress.holding_at_obstruction);
}

TEST_F(StuckRecoveryTest, GivingUpDoesNotTurnIntoARepathLoop) {
  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 3);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));

  run_for(60.0);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(facts->progress.stall.objective_abandoned);
  EXPECT_LE(facts->progress.stall.recovery_attempts, 8U)
      << "the recovery ladder kept climbing after the objective was dropped";
  EXPECT_LE(facts->progress.stall.abandon_count, 1U)
      << "the same objective was abandoned over and over";
}

TEST_F(StuckRecoveryTest, AnObjectiveInsideASealedPenWaitsAtTheWallWithoutShuffling) {

  constexpr int k_pen_x = 30;
  constexpr int k_pen_z = 24;
  constexpr int k_half = 5;
  seal_a_pen(k_pen_x, k_pen_z, k_half);

  const EntityID id = spawn(world_of(8, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(k_pen_x, k_pen_z));
  ASSERT_TRUE(movement_of(id)->get_has_target());

  const float wall_x = world_of(k_pen_x - k_half, k_pen_z).x();
  double reached_wall_at = -1.0;
  double waiting_at = -1.0;
  float wait_position_x = 0.0F;
  float drift_while_waiting = 0.0F;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 30.0; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    if (reached_wall_at < 0.0 && position_of(id).x() > wall_x - 2.5F) {
      reached_wall_at = elapsed;
    }
    if (facts != nullptr && facts->progress.holding_at_obstruction) {
      if (waiting_at < 0.0) {
        waiting_at = elapsed;
        wait_position_x = position_of(id).x();
      }
      drift_while_waiting = std::max(drift_while_waiting,
                                     std::fabs(position_of(id).x() - wait_position_x));
    }
  }

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  ASSERT_GE(reached_wall_at, 0.0) << "the unit never walked up to the compound";
  ASSERT_GE(waiting_at, 0.0) << "the unit never settled into waiting at the wall";
  EXPECT_LT(waiting_at - reached_wall_at, 2.5)
      << "the unit took " << waiting_at - reached_wall_at << " s to stop at the wall";
  EXPECT_LT(drift_while_waiting, 0.6F) << "the unit shuffled while it waited";
  EXPECT_EQ(facts->progress.stall.recovery_attempts, 0U)
      << "the recovery ladder ran for a unit that had nowhere further to go";
  EXPECT_LE(facts->progress.repath_count, 1U)
      << "arriving at the wall turned into a repath loop";
}

TEST_F(StuckRecoveryTest, StandingOnTheResolvedGoalIsArrivedNotBlocked) {

  constexpr int k_here_x = 12;
  constexpr int k_z = 24;
  for (int grid_x = k_here_x + 1; grid_x <= k_here_x + 4; ++grid_x) {
    for (int grid_z = k_z - 2; grid_z <= k_z + 2; ++grid_z) {
      block_cell(grid_x, grid_z);
    }
  }
  refresh_grid();

  const EntityID id = spawn(world_of(k_here_x, k_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(k_here_x + 2, k_z));

  bool ever_blocked = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 6.0; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    if (facts != nullptr &&
        (facts->progress.state == MovementOrderState::LocallyBlocked ||
         facts->progress.state == MovementOrderState::Repathing ||
         facts->progress.state == MovementOrderState::Recovering ||
         facts->progress.state == MovementOrderState::Unreachable)) {
      ever_blocked = true;
    }
  }

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_FALSE(movement_of(id)->get_has_target());
  EXPECT_EQ(facts->progress.state, MovementOrderState::Arrived);
  EXPECT_FALSE(ever_blocked) << "a unit standing on its goal was declared blocked";
  EXPECT_EQ(facts->progress.stall.rung, MovementRecoveryRung::None);
  EXPECT_LT((position_of(id) - world_of(k_here_x, k_z)).length(), 0.6F)
      << "the unit wandered off a goal it was already standing on";
}

TEST_F(StuckRecoveryTest, AUnitSetDownOnAFootprintStepsOffOnceAndStandsThere) {

  constexpr int k_x = 20;
  constexpr int k_z = 24;
  for (int grid_x = k_x - 2; grid_x <= k_x + 2; ++grid_x) {
    for (int grid_z = k_z - 1; grid_z <= k_z + 1; ++grid_z) {
      block_cell(grid_x, grid_z);
    }
  }
  refresh_grid();

  const EntityID id = spawn(world_of(k_x, k_z) + QVector3D(0.3F, 0.0F, 0.0F));
  ASSERT_NE(id, 0U);

  int recovery_orders = 0;
  bool was_recovering = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 4.0; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    ASSERT_NE(facts, nullptr);
    const bool recovering = facts->progress.state == MovementOrderState::Recovering;
    if (recovering && !was_recovering) {
      ++recovery_orders;
    }
    was_recovering = recovering;
  }

  const Game::Systems::BodyProfile point_body;
  EXPECT_TRUE(Game::Systems::Walkability::can_stand(position_of(id), point_body))
      << "the unit is still standing on the footprint at (" << position_of(id).x()
      << ", " << position_of(id).z() << ")";
  EXPECT_LE(recovery_orders, 1) << "the unit was sent off the footprint "
                                << recovery_orders << " times in four seconds";
  EXPECT_FALSE(movement_of(id)->get_has_target());
}

TEST_F(StuckRecoveryTest, RecoveryIsIssuedByOneLadder) {

  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 3);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));

  std::uint64_t last_revision = movement_of(id)->get_route_revision();
  int reroutes = 0;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < k_recovery_budget_seconds; elapsed += step) {
    run_for(step);
    const auto* movement = movement_of(id);
    if (movement == nullptr) {
      continue;
    }
    if (movement->get_route_revision() != last_revision) {
      last_revision = movement->get_route_revision();
      ++reroutes;
    }
    if (!movement->get_has_target()) {
      break;
    }
  }

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(facts->progress.stall.objective_abandoned);
  EXPECT_LE(reroutes, 2)
      << "more re-routes than the one confirming replan: a second ladder is running";
  EXPECT_LE(facts->progress.repath_count, 1U);
}

TEST_F(StuckRecoveryTest, AClickInsideABuildingStopsOnTheSideTheUnitCameFrom) {

  constexpr int k_block_x = 24;
  constexpr int k_block_z = 26;
  for (int grid_x = k_block_x - 3; grid_x <= k_block_x + 3; ++grid_x) {
    for (int grid_z = k_block_z - 3; grid_z <= k_block_z + 3; ++grid_z) {
      block_cell(grid_x, grid_z);
    }
  }
  refresh_grid();

  const EntityID id = spawn(world_of(k_block_x, 12));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(k_block_x, k_block_z));
  run_for(20.0);

  const QVector3D where = position_of(id);
  const QVector3D south_face = world_of(k_block_x, k_block_z - 4);
  EXPECT_LT(where.z(), world_of(k_block_x, k_block_z - 3).z())
      << "the unit ended at (" << where.x() << ", " << where.z()
      << "), not on the south face it approached";
  EXPECT_LT(std::fabs(where.x() - south_face.x()), 1.5F)
      << "the unit walked along the block instead of stopping in front of it";
  EXPECT_GT(where.z(), south_face.z() - 2.5F)
      << "the unit stopped well short of the block";
}

TEST_F(StuckRecoveryTest, ABlockTooWideForTheGateStillWalksThroughIt) {
  constexpr int k_wall_x = 24;
  constexpr int k_gap_z = 24;
  for (int grid_z = 4; grid_z < k_map - 4; ++grid_z) {
    if (grid_z != k_gap_z) {
      block_cell(k_wall_x, grid_z);
    }
  }
  refresh_grid();

  const EntityID id = spawn(world_of(8, k_gap_z));
  ASSERT_NE(id, 0U);
  const auto destination = world_of(40, k_gap_z);
  CommandService::move_unit(m_session->world(), id, destination);

  run_for(60.0);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_FALSE(facts->progress.stall.objective_abandoned)
      << "a gap the unit could have walked through was declared unreachable";
  EXPECT_GT(position_of(id).x(), world_of(k_wall_x + 2, k_gap_z).x())
      << "the unit never got past the gap";
}

TEST_F(StuckRecoveryTest, AMarchAcrossOpenGroundNeverLooksStuck) {
  const EntityID id = spawn(world_of(6, 24));
  ASSERT_NE(id, 0U);
  const auto destination = world_of(40, 24);
  CommandService::move_unit(m_session->world(), id, destination);

  float worst_stall = 0.0F;
  auto worst_rung = MovementRecoveryRung::None;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 40.0; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    if (facts == nullptr) {
      continue;
    }
    worst_stall = std::max(worst_stall, facts->progress.stall.stalled_seconds);
    worst_rung = std::max(worst_rung, facts->progress.stall.rung);
    if (!movement_of(id)->get_has_target()) {
      break;
    }
  }

  EXPECT_LT((position_of(id) - destination).length(), 2.0F)
      << "the march did not finish, so the rest of this proves nothing";
  EXPECT_EQ(worst_rung, MovementRecoveryRung::None)
      << "a clean march was put on the recovery ladder, worst stall " << worst_stall
      << " s";
}

} // namespace

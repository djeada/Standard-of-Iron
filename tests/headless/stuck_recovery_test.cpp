

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

TEST_F(StuckRecoveryTest, GivingUpClimbsTheLadderInsteadOfJumpingToTheEnd) {
  constexpr int k_pen_x = 12;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 3);

  const EntityID id = spawn(world_of(k_pen_x, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(40, k_pen_z));

  std::vector<MovementRecoveryRung> seen;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < k_recovery_budget_seconds; elapsed += step) {
    run_for(step);
    const auto* facts = facts_of(id);
    if (facts == nullptr) {
      continue;
    }
    const auto rung = facts->progress.stall.rung;
    if (seen.empty() || seen.back() != rung) {
      seen.push_back(rung);
    }
  }

  EXPECT_NE(std::find(seen.begin(), seen.end(), MovementRecoveryRung::Replan),
            seen.end())
      << "the unit was given up on without ever being replanned";
  EXPECT_EQ(seen.back(), MovementRecoveryRung::Abandoned);
  EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()))
      << "the recovery ladder went backwards instead of escalating";
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

TEST_F(StuckRecoveryTest, AnObjectiveInsideASealedPenIsGivenUpOnFromOutside) {
  constexpr int k_pen_x = 30;
  constexpr int k_pen_z = 24;
  seal_a_pen(k_pen_x, k_pen_z, 5);

  const EntityID id = spawn(world_of(8, k_pen_z));
  ASSERT_NE(id, 0U);
  CommandService::move_unit(m_session->world(), id, world_of(k_pen_x, k_pen_z));

  ASSERT_TRUE(movement_of(id)->get_has_target());

  run_for(60.0);

  const auto* facts = facts_of(id);
  ASSERT_NE(facts, nullptr);
  EXPECT_TRUE(facts->progress.stall.objective_abandoned)
      << "the unit never gave up on a destination walled off from every side; "
         "worst standstill "
      << facts->progress.stall.stalled_seconds << " s, worst no-closer "
      << facts->progress.stall.no_closer_seconds << " s";
  EXPECT_FALSE(movement_of(id)->get_has_target());
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

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/movement_trace.h"
#include "game/core/system_schedule.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/local_avoidance_system.h"
#include "game/systems/movement_pipeline.h"
#include "game/systems/movement_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/route_follow_system.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/unit_traversal_layout_system.h"

namespace {

using Engine::Core::MovementComponent;
using Engine::Core::MovementFactsComponent;
using Engine::Core::MovementOrderState;
using Engine::Core::MovementTrace;
using Engine::Core::MovementTraceManifest;
using Engine::Core::ScopedMovementTrace;
using Engine::Core::SystemPhase;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

template <typename ComponentType>
auto writes(const Engine::Core::SystemAccess& access) -> bool {
  const auto id = Engine::Core::component_type_id<ComponentType>();
  return std::find(access.writes.begin(), access.writes.end(), id) !=
         access.writes.end();
}

auto find_repo_root() -> std::filesystem::path {
  std::filesystem::path current = std::filesystem::current_path();
  for (int depth = 0; depth < 8; ++depth) {
    if (std::filesystem::exists(current / "CMakeLists.txt") &&
        std::filesystem::exists(current / "game")) {
      return current;
    }
    if (!current.has_parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::filesystem::current_path();
}

auto read_source(const std::string& relative) -> std::string {
  std::ifstream stream(find_repo_root() / relative);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

template <typename SystemType>
auto index_of_system(World& world) -> std::size_t {
  const auto& systems = world.systems();
  for (std::size_t index = 0; index < systems.size(); ++index) {
    if (dynamic_cast<const SystemType*>(systems[index].get()) != nullptr) {
      return index;
    }
  }
  return std::string::npos;
}

} // namespace

TEST(MovementStageOwnershipTest, TheRouteFollowerNeverMovesABody) {
  const auto access = Game::Systems::RouteFollowSystem{}.access();
  EXPECT_TRUE(writes<MovementFactsComponent>(access));
  EXPECT_TRUE(writes<MovementComponent>(access));
  EXPECT_FALSE(writes<TransformComponent>(access))
      << "the route follower published an intent and also moved the body";
}

TEST(MovementStageOwnershipTest, SteeringWritesOnlyItsOwnResult) {
  const auto access = Game::Systems::LocalAvoidanceSystem{}.access();
  EXPECT_TRUE(writes<MovementFactsComponent>(access));
  EXPECT_FALSE(writes<MovementComponent>(access))
      << "avoidance still writes the velocity the motor integrates";
  EXPECT_FALSE(writes<TransformComponent>(access));
}

TEST(MovementStageOwnershipTest, OnlyTheMotorWritesTheTransform) {
  const auto access = Game::Systems::MovementSystem{}.access();
  EXPECT_TRUE(writes<TransformComponent>(access));
  EXPECT_TRUE(writes<MovementFactsComponent>(access));
}

TEST(MovementStageOwnershipTest, SteeringDoesNotTouchTheIntegratedVelocity) {
  const auto source = read_source("game/systems/local_avoidance_system.cpp");
  ASSERT_FALSE(source.empty());
  EXPECT_EQ(source.find("set_manual_velocity"), std::string::npos)
      << "the steering stage overwrites the motor's velocity again";
  EXPECT_EQ(source.find("->vx ="), std::string::npos);
  EXPECT_EQ(source.find("->vz ="), std::string::npos);
}

TEST(MovementStageOwnershipTest, TheRegistryOrdersFollowThenSteerThenMotor) {
  SessionContext session;
  const ScopedSession scope(session);
  Game::Systems::register_runtime_systems(session.world());

  const auto follow =
      index_of_system<Game::Systems::RouteFollowSystem>(session.world());
  const auto steer =
      index_of_system<Game::Systems::LocalAvoidanceSystem>(session.world());
  const auto motor = index_of_system<Game::Systems::MovementSystem>(session.world());
  const auto traversal =
      index_of_system<Game::Systems::UnitTraversalLayoutSystem>(session.world());

  ASSERT_NE(follow, std::string::npos);
  ASSERT_NE(steer, std::string::npos);
  ASSERT_NE(motor, std::string::npos);
  ASSERT_NE(traversal, std::string::npos);
  EXPECT_LT(follow, steer) << "steering ran before there was an intent to steer";
  EXPECT_LT(steer, motor) << "the motor ran before steering could correct it";
  EXPECT_LT(motor, traversal)
      << "traversal layout ran before the motor published its accepted pose";

  const auto phases = session.world().system_phases();
  EXPECT_EQ(phases[follow], SystemPhase::Movement);
  EXPECT_EQ(phases[steer], SystemPhase::Movement);
  EXPECT_EQ(phases[motor], SystemPhase::Movement);
  EXPECT_EQ(phases[traversal], SystemPhase::Movement);
}

TEST(MovementStageOwnershipTest, TheCompositePipelineKeepsTheSameOrder) {
  const auto source = read_source("game/systems/movement_pipeline.cpp");
  ASSERT_FALSE(source.empty());
  const auto follow = source.find("m_route_follow.update");
  const auto steer = source.find("m_avoidance.update");
  const auto motor = source.find("m_motor.update");
  const auto traversal = source.find("m_traversal_layout.update");
  ASSERT_NE(follow, std::string::npos);
  ASSERT_NE(steer, std::string::npos);
  ASSERT_NE(motor, std::string::npos);
  ASSERT_NE(traversal, std::string::npos);
  EXPECT_LT(follow, steer);
  EXPECT_LT(steer, motor);
  EXPECT_LT(motor, traversal);
}

TEST(MovementStageOwnershipTest, OnlyTheOrderPipelineBeginsOrdersAndRoutes) {
  for (const char* relative : {"game/systems/movement_system.cpp",
                               "game/systems/route_follow_system.cpp",
                               "game/systems/local_avoidance_system.cpp",
                               "game/systems/movement_pipeline.cpp"}) {
    const auto source = read_source(relative);
    ASSERT_FALSE(source.empty()) << relative;
    EXPECT_EQ(source.find("begin_order()"), std::string::npos)
        << relative << " bumps the order sequence; only the order pipeline may";
    EXPECT_EQ(source.find("begin_route("), std::string::npos)
        << relative << " bumps the route revision; only the order pipeline may";
  }

  const auto orders = read_source("game/systems/movement_orders.cpp");
  ASSERT_FALSE(orders.empty());
  EXPECT_NE(orders.find("begin_order()"), std::string::npos);
  EXPECT_NE(orders.find("begin_route("), std::string::npos);
}

TEST(MovementStageOwnershipTest, ANewOrderSupersedesADeferredRoute) {
  Game::Systems::NavGrid::initialize(64, 64);
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();

  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 2.0F, 12.0F);
  unit->owner_id = 1;
  auto* movement = entity->add_component<MovementComponent>();

  Game::Systems::CommandService::move_unit(world, entity->get_id(), {8.0F, 0.0F, 0.0F});
  const auto first = movement->get_order_sequence();
  const auto first_route = movement->get_route_revision();
  EXPECT_GT(first, 0U);
  EXPECT_GT(first_route, 0U);

  Game::Systems::CommandService::move_unit(world, entity->get_id(), {0.0F, 0.0F, 9.0F});
  EXPECT_GT(movement->get_order_sequence(), first)
      << "a second accepted order reused the first order's sequence";
  EXPECT_GT(movement->get_route_revision(), first_route);
}

TEST(MovementStageOwnershipTest, OneTickOfTraceAccountsForTheWholeChain) {
  Game::Systems::NavGrid::initialize(64, 64);
  SessionContext session;
  const ScopedSession scope(session);
  World& world = session.world();

  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 2.0F, 12.0F);
  unit->owner_id = 1;
  entity->add_component<MovementComponent>();
  Game::Systems::CommandService::move_unit(world, entity->get_id(), {8.0F, 0.0F, 0.0F});

  MovementTraceManifest manifest;
  manifest.scenario = "movement_stage_ownership";
  manifest.fixed_step_seconds = 1.0F / 60.0F;
  const ScopedMovementTrace trace(manifest);

  world.add_system(std::make_unique<Game::Systems::MovementPipeline>(),
                   SystemPhase::Movement);

  for (int tick = 0; tick < 90; ++tick) {
    world.update(1.0F / 60.0F);
  }

  const auto& samples = MovementTrace::instance().troop_samples();
  ASSERT_FALSE(samples.empty());

  bool saw_moving_tick = false;
  for (const auto& sample : samples) {
    if (sample.state != MovementOrderState::Following) {
      continue;
    }
    if (std::hypot(sample.desired_vx, sample.desired_vz) < 0.01F) {
      continue;
    }
    saw_moving_tick = true;
    EXPECT_NE(sample.route_id, 0U)
        << "the trace lost the order pipeline's authoritative route identity";
    EXPECT_GE(sample.lane_scale, 0.0F);
    EXPECT_LE(sample.lane_scale, 1.0F);

    if (sample.neighbor_count > 0U) {
      EXPECT_NEAR(sample.steered_vx, sample.desired_vx + sample.avoidance_dx, 1.0e-4F);
      EXPECT_NEAR(sample.steered_vz, sample.desired_vz + sample.avoidance_dz, 1.0e-4F);
    }
    EXPECT_GT(std::hypot(sample.accepted_dx, sample.accepted_dz), 0.0F)
        << "tick " << sample.tick << " was Following with no accepted displacement";
    EXPECT_NEAR(sample.root_x - sample.previous_root_x, sample.accepted_dx, 1.0e-3F);
    EXPECT_NEAR(sample.root_z - sample.previous_root_z, sample.accepted_dz, 1.0e-3F);
  }
  EXPECT_TRUE(saw_moving_tick) << "no tick of the trace showed the unit following";
}

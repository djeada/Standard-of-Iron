#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/wall_network_service.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

constexpr int k_defender = 1;
constexpr int k_raider = 2;
constexpr float k_gate_x = 0.0F;
constexpr float k_gate_z = 0.0F;

auto make_match() -> std::unique_ptr<SessionContext> {
  auto session = std::make_unique<SessionContext>();
  auto& owners = session->owners();
  owners.register_owner_with_id(k_defender, Game::Systems::OwnerType::Player, "blue");
  owners.register_owner_with_id(k_raider, Game::Systems::OwnerType::AI, "red");
  owners.set_owner_team(k_defender, 1);
  owners.set_owner_team(k_raider, 2);
  Game::Systems::register_runtime_systems(session->world());
  return session;
}

void run_for(SessionContext& session, double seconds) {
  const double step = session.clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
    session.clock().advance(step);
    while (session.clock().consume_tick()) {
      session.world().update(static_cast<float>(step));
    }
  }
}

auto run_watching_gate(SessionContext& session,
                       EntityID gate,
                       double seconds) -> float {
  const double step = session.clock().tick_seconds();
  float peak = 0.0F;
  for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
    run_for(session, step);
    auto* entity = session.world().get_entity(gate);
    const auto* component = entity != nullptr
                                ? entity->get_component<Engine::Core::GateComponent>()
                                : nullptr;
    if (component != nullptr) {
      peak = std::max(peak, component->open_amount);
    }
  }
  return peak;
}

auto add_wall_piece(SessionContext& session,
                    Game::Units::SpawnType spawn_type,
                    float x,
                    float z) -> EntityID {
  const auto snapped = Game::Systems::WallNetworkService::snap_world_position(x, z);
  const auto center = Game::Systems::CommandService::grid_to_world(
      Game::Systems::Point(snapped.x, snapped.z));

  auto* entity = session.world().create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = center.x();
  transform->position.z = center.z();
  entity->add_component<Engine::Core::RenderableComponent>("", "");
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(800, 800, 0.0F, 0.0F);
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
                                                 is_gate ? "wall_gate" : "wall_segment",
                                                 center.x(),
                                                 center.z(),
                                                 k_defender);
  if (is_gate) {
    session.building_collision().set_building_navigation_blocking(entity->get_id(),
                                                                  false);
  }
  return entity->get_id();
}

auto build_gated_wall(SessionContext& session) -> EntityID {
  for (float x = -30.0F; x <= 30.0F; x += 2.0F) {
    if (std::fabs(x - k_gate_x) < 0.01F) {
      continue;
    }
    add_wall_piece(session, Game::Units::SpawnType::WallSegment, x, k_gate_z);
  }
  const EntityID gate =
      add_wall_piece(session, Game::Units::SpawnType::WallGate, k_gate_x, k_gate_z);
  Game::Systems::WallNetworkService::refresh_world(session.world());
  return gate;
}

auto spawn_troop(SessionContext& session, int owner_id, float x, float z) -> EntityID {
  auto* entity = session.world().create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(100, 100, 3.0F, 8.0F);
  unit->owner_id = owner_id;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  return entity->get_id();
}

void order_move(SessionContext& session, EntityID unit, const QVector3D& target) {
  Game::Command::submit(session.world(),
                        Game::Command::Source::Script,
                        session.world()
                            .get_entity(unit)
                            ->get_component<Engine::Core::UnitComponent>()
                            ->owner_id,
                        Game::Command::Move{.units = {unit}, .targets = {target}});
}

auto entity_position(SessionContext& session, EntityID unit) -> QVector3D {
  auto* entity = session.world().get_entity(unit);
  const auto* transform =
      entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  return transform != nullptr
             ? QVector3D(transform->position.x, 0.0F, transform->position.z)
             : QVector3D();
}

auto position_z(SessionContext& session, EntityID unit) -> float {
  auto* entity = session.world().get_entity(unit);
  const auto* transform =
      entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  return transform != nullptr ? transform->position.z : 0.0F;
}

class GateTraversalTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Systems::CommandService::initialize(64, 64); }
};

TEST_F(GateTraversalTest, OwnerTroopWalksThroughItsOwnGate) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID gate = build_gated_wall(*session);

  const auto gate_position = entity_position(*session, gate);
  const EntityID troop =
      spawn_troop(*session, k_defender, gate_position.x(), gate_position.z() - 6.0F);
  order_move(
      *session, troop, QVector3D(gate_position.x(), 0.0F, gate_position.z() + 6.0F));

  const float peak_open = run_watching_gate(*session, gate, 12.0);

  const auto* gate_component =
      session->world().get_entity(gate)->get_component<Engine::Core::GateComponent>();
  ASSERT_NE(gate_component, nullptr);
  EXPECT_GE(peak_open, Engine::Core::GateComponent::k_passable_open_amount);
  EXPECT_GT(position_z(*session, troop), gate_position.z() + 2.0F);

  EXPECT_FLOAT_EQ(gate_component->open_amount, 0.0F);
}

TEST_F(GateTraversalTest, GroupMembersIndividuallyUseTheGateCenterline) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID gate = build_gated_wall(*session);
  const QVector3D gate_position = entity_position(*session, gate);

  std::vector<EntityID> troops;
  std::vector<QVector3D> targets;
  for (int index = 0; index < 8; ++index) {
    const float offset = (static_cast<float>(index) - 3.5F) * 0.6F;
    troops.push_back(spawn_troop(*session,
                                 k_defender,
                                 gate_position.x() + offset,
                                 gate_position.z() - 8.0F - float(index / 4)));
    targets.emplace_back(gate_position.x(), 0.0F, gate_position.z() + 8.0F);
  }
  Game::Systems::CommandService::move_units(session->world(), troops, targets);

  std::vector<bool> used_centerline(troops.size(), false);
  const double step = session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 20.0; elapsed += step) {
    run_for(*session, step);
    for (std::size_t index = 0; index < troops.size(); ++index) {
      const QVector3D position = entity_position(*session, troops[index]);
      if (std::abs(position.z() - gate_position.z()) <= 1.0F) {
        used_centerline[index] = true;
        EXPECT_LE(std::abs(position.x() - gate_position.x()), 0.75F)
            << "unit " << index << " crossed off the gate centerline";
      }
    }
  }

  EXPECT_TRUE(std::all_of(
      used_centerline.begin(), used_centerline.end(), [](bool used) { return used; }))
      << "not every group member traversed the gate independently";
  for (const auto troop : troops) {
    EXPECT_GT(position_z(*session, troop), gate_position.z() + 2.0F);
  }
}

TEST_F(GateTraversalTest, AlliedTroopIsAdmittedOnTheSameTerms) {
  auto session = make_match();
  const ScopedSession scope(*session);

  session->owners().register_owner_with_id(3, Game::Systems::OwnerType::AI, "green");
  session->owners().set_owner_team(3, 1);
  const EntityID gate = build_gated_wall(*session);

  const auto gate_position = entity_position(*session, gate);
  const EntityID troop =
      spawn_troop(*session, 3, gate_position.x(), gate_position.z() - 6.0F);
  order_move(
      *session, troop, QVector3D(gate_position.x(), 0.0F, gate_position.z() + 6.0F));

  const float peak_open = run_watching_gate(*session, gate, 12.0);

  EXPECT_GE(peak_open, Engine::Core::GateComponent::k_passable_open_amount);
  EXPECT_GT(position_z(*session, troop), gate_position.z() + 2.0F);
}

TEST_F(GateTraversalTest, HostileTroopIsHeldOnItsOwnSideOfTheWall) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID gate = build_gated_wall(*session);

  const auto gate_position = entity_position(*session, gate);
  const EntityID raider =
      spawn_troop(*session, k_raider, gate_position.x(), gate_position.z() - 6.0F);
  order_move(
      *session, raider, QVector3D(gate_position.x(), 0.0F, gate_position.z() + 6.0F));

  const float peak_open = run_watching_gate(*session, gate, 12.0);

  const auto* gate_component =
      session->world().get_entity(gate)->get_component<Engine::Core::GateComponent>();
  ASSERT_NE(gate_component, nullptr);
  EXPECT_FLOAT_EQ(peak_open, 0.0F);
  EXPECT_FLOAT_EQ(gate_component->open_amount, 0.0F);
  EXPECT_LT(position_z(*session, raider), gate_position.z() - 0.5F);
}

TEST_F(GateTraversalTest, DestroyingTheGateOpensAPassableBreach) {
  auto session = make_match();
  const ScopedSession scope(*session);
  const EntityID gate = build_gated_wall(*session);

  const auto gate_position = entity_position(*session, gate);
  const QVector3D far_side(gate_position.x(), 0.0F, gate_position.z() + 6.0F);
  const EntityID raider =
      spawn_troop(*session, k_raider, gate_position.x(), gate_position.z() - 6.0F);
  order_move(*session, raider, far_side);
  run_for(*session, 4.0);
  ASSERT_LT(position_z(*session, raider), gate_position.z() - 0.5F);

  session->building_collision().unregister_building(gate);
  session->world().destroy_entity(gate);
  Game::Systems::WallNetworkService::refresh_world(session->world());

  order_move(*session, raider, far_side);
  run_for(*session, 12.0);

  EXPECT_GT(position_z(*session, raider), gate_position.z() + 2.0F);
}

} // namespace

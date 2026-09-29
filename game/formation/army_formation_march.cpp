#include "army_formation_march.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/navigation/nav_grid.h"
#include "../systems/navigation/pathfinding.h"
#include "../systems/navigation/route_corridor_planner.h"
#include "../util/planar_math.h"
#include "army_formation_tuning.h"

namespace Game::Formation::March {

namespace {

constexpr float k_corridor_waypoint_tolerance = 1.25F;
constexpr float k_corridor_max_anchor_lead = 6.0F;
constexpr float k_corridor_min_leg_length = 1.5F;

constexpr float k_max_wheel_degrees_per_second = 60.0F;
constexpr float k_wheel_in_place_degrees = 0.5F;
constexpr float k_final_sidestep_metres = 3.0F;

struct GroupTraversal {
  Game::Systems::Pathfinding::Passability passability{
      Game::Systems::Pathfinding::Passability::Light};
  float clearance{0.0F};
};

struct SlotSpread {
  float half_span{0.0F};
  float max_slot_error{0.0F};
};

auto group_traversal(Engine::Core::World& world,
                     const ArmyFormation& formation) -> GroupTraversal {
  GroupTraversal traversal;
  float widest = 0.0F;
  for (auto const member : formation.members) {
    const auto* movement = world.try_get<Engine::Core::MovementComponent>(member);
    if (movement == nullptr) {
      continue;
    }
    if (!movement->get_can_enter_forest()) {
      traversal.passability = Game::Systems::Pathfinding::Passability::Heavy;
    }
    widest = std::max(widest, movement->get_navigation_clearance());
  }
  traversal.clearance = Game::Systems::Pathfinding::routing_clearance(widest);
  return traversal;
}

auto build_corridor(const QVector3D& start,
                    const QVector3D& destination,
                    const GroupTraversal& traversal) -> std::vector<QVector3D> {
  std::vector<QVector3D> corridor;
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    corridor.push_back(destination);
    return corridor;
  }

  if (pathfinder->is_world_segment_walkable(
          start, destination, traversal.passability, traversal.clearance)) {
    return {destination};
  }

  auto const planned = Game::Systems::RouteCorridorPlanner::plan(
      *pathfinder, start, destination, traversal.passability, traversal.clearance);
  if (!planned.reachable()) {
    return corridor;
  }

  QVector3D previous = start;
  for (auto const& point : planned.centerline) {
    QVector3D const step(point.x() - previous.x(), 0.0F, point.z() - previous.z());
    if (step.length() < k_corridor_min_leg_length) {
      continue;
    }
    corridor.push_back(point);
    previous = point;
  }

  QVector3D const tail = corridor.empty() ? start : corridor.back();
  QVector3D const to_destination(
      destination.x() - tail.x(), 0.0F, destination.z() - tail.z());
  if (corridor.empty() || to_destination.length() > 0.1F) {
    corridor.push_back(destination);
  }
  return corridor;
}

auto seat_centroid(Engine::Core::World& world,
                   const ArmyFormation& formation,
                   std::optional<float> marching_facing) -> std::optional<QVector3D> {
  QVector3D centroid;
  int count = 0;
  for (auto const member : formation.members) {
    auto* entity = world.get_entity(member);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    centroid +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    if (marching_facing.has_value()) {
      if (const auto* slot = formation.find_slot_for(member)) {
        float const radians = *marching_facing * std::numbers::pi_v<float> / 180.0F;
        const auto& offset = slot->local_offset;
        centroid -=
            QVector3D(offset.x() * std::cos(radians) + offset.z() * std::sin(radians),
                      0.0F,
                      -offset.x() * std::sin(radians) + offset.z() * std::cos(radians));
      }
    }
    ++count;
  }
  if (count == 0) {
    return std::nullopt;
  }
  return centroid / static_cast<float>(count);
}

void face_first_leg(ArmyFormation& formation, std::optional<float> marching_facing) {
  QVector3D heading = formation.move_plan.next_waypoint() - formation.anchor;
  heading.setY(0.0F);
  if (heading.lengthSquared() > 1.0e-4F) {
    formation.move_plan.facing_direction = heading.normalized();
  }

  if (marching_facing.has_value()) {
    formation.facing = *marching_facing;
  } else if (heading.lengthSquared() > 1.0e-4F) {
    formation.facing =
        Game::Systems::yaw_degrees_from_direction(heading.x(), heading.z());
  }
}

auto pack_centroid(Engine::Core::World& world,
                   const ArmyFormation& formation) -> std::optional<QVector3D> {
  QVector3D centroid;
  int count = 0;
  for (auto const member : formation.members) {
    auto* entity = world.get_entity(member);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    centroid +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    if (const auto* slot = formation.find_slot_for(member)) {
      centroid -= slot->world_position - formation.anchor;
    }
    ++count;
  }
  if (count == 0) {
    return std::nullopt;
  }
  return centroid / static_cast<float>(count);
}

auto measure_spread(Engine::Core::World& world,
                    const ArmyFormation& formation) -> SlotSpread {
  SlotSpread spread;
  spread.half_span = std::max(formation.spacing, 0.5F);
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    spread.half_span = std::max(spread.half_span,
                                slot.local_offset.length() +
                                    std::hypot(slot.half_width, slot.half_depth));
    if (const auto* transform =
            world.try_get<Engine::Core::TransformComponent>(slot.occupant)) {
      spread.max_slot_error =
          std::max(spread.max_slot_error,
                   std::hypot(transform->position.x - slot.world_position.x(),
                              transform->position.z - slot.world_position.z()));
    }
  }
  return spread;
}

auto wheel_step_degrees(float pace, float half_span, float delta_time) -> float {
  return std::clamp(pace / std::max(half_span, 0.5F) * 180.0F /
                        std::numbers::pi_v<float>,
                    0.0F,
                    k_max_wheel_degrees_per_second) *
         delta_time;
}

void skip_reached_waypoints(FormationMovePlan& plan, const QVector3D& anchor) {
  while (plan.has_corridor()) {
    QVector3D const waypoint = plan.next_waypoint();
    QVector3D const to_waypoint(
        waypoint.x() - anchor.x(), 0.0F, waypoint.z() - anchor.z());
    float const tolerance = plan.corridor_index + 1U == plan.corridor.size()
                                ? 0.05F
                                : k_corridor_waypoint_tolerance;
    if (to_waypoint.length() > tolerance) {
      break;
    }
    ++plan.corridor_index;
  }
}

auto reopen_final_leg(ArmyFormation& formation, float wheel_step) -> bool {
  auto& plan = formation.move_plan;
  QVector3D const to_destination(formation.destination.x() - formation.anchor.x(),
                                 0.0F,
                                 formation.destination.z() - formation.anchor.z());
  if (to_destination.length() <= 0.05F) {
    plan.clear();
    if (!facing_settled(formation)) {
      formation.facing = Game::Systems::turn_yaw_toward(
          formation.facing, formation.destination_facing, wheel_step);
      formation.needs_replan = true;
    }
    return false;
  }
  plan.corridor.push_back(formation.destination);
  return true;
}

void steer_and_step(ArmyFormation& formation,
                    float declared_pace,
                    float wheel_step,
                    float delta_time) {
  auto& plan = formation.move_plan;
  QVector3D heading = plan.next_waypoint() - formation.anchor;
  heading.setY(0.0F);
  float const leg = heading.length();
  if (leg <= 1.0e-4F) {
    return;
  }
  heading /= leg;
  plan.facing_direction = heading;

  bool const final_step = plan.corridor_index + 1U >= plan.corridor.size() &&
                          leg <= k_final_sidestep_metres;
  float const target_facing =
      final_step ? formation.destination_facing
                 : Game::Systems::yaw_degrees_from_direction(heading.x(), heading.z());
  formation.facing =
      Game::Systems::turn_yaw_toward(formation.facing, target_facing, wheel_step);
  formation.needs_replan = true;
  if (std::abs(Game::Systems::signed_yaw_delta(formation.facing, target_facing)) >
      k_wheel_in_place_degrees) {
    return;
  }

  float const step = std::min(leg, std::max(0.05F, declared_pace * delta_time));
  formation.anchor += heading * step;
  formation.advance_progress += step;
}

} // namespace

auto facing_settled(const ArmyFormation& formation) -> bool {
  constexpr float k_settled_degrees = 0.5F;
  return !formation.maintains_formation() ||
         std::abs(Game::Systems::signed_yaw_delta(
             formation.facing, formation.destination_facing)) <= k_settled_degrees;
}

auto is_advancing(const ArmyFormation& formation) -> bool {
  return formation.has_destination && formation.maintains_formation() &&
         !formation.morph.active && formation.move_plan.active &&
         !formation.members.empty();
}

void begin(Engine::Core::World& world,
           ArmyFormation& formation,
           std::optional<float> marching_facing) {
  if (auto const centroid = seat_centroid(world, formation, marching_facing)) {
    formation.anchor = *centroid;
  }

  formation.move_plan.corridor = build_corridor(
      formation.anchor, formation.destination, group_traversal(world, formation));
  formation.move_plan.corridor_index = 0;
  formation.move_plan.formation_center = formation.anchor;
  formation.move_plan.active = !formation.move_plan.corridor.empty();
  face_first_leg(formation, marching_facing);
}

void advance_group(Engine::Core::World& world,
                   ArmyFormation& formation,
                   float delta_time) {
  auto const centroid = pack_centroid(world, formation);
  if (!centroid.has_value()) {
    return;
  }

  float const declared_pace = formation.cohesion_pace > 0.0F
                                  ? formation.cohesion_pace
                                  : Tuning::k_maintain_speed_multiplier;
  auto const spread = measure_spread(world, formation);
  if (!formation.compressed && spread.max_slot_error > 1.0F) {
    formation.moves_pending = true;
    return;
  }
  float const wheel_step =
      wheel_step_degrees(declared_pace, spread.half_span, delta_time);

  auto& plan = formation.move_plan;
  if (!plan.active) {
    plan.corridor = build_corridor(
        formation.anchor, formation.destination, group_traversal(world, formation));
    plan.corridor_index = 0;
    plan.active = !plan.corridor.empty();
  }
  plan.formation_center = *centroid;

  QVector3D const anchor_lead(
      formation.anchor.x() - centroid->x(), 0.0F, formation.anchor.z() - centroid->z());
  if (anchor_lead.length() > k_corridor_max_anchor_lead) {
    return;
  }

  skip_reached_waypoints(plan, formation.anchor);
  if (!plan.has_corridor() && !reopen_final_leg(formation, wheel_step)) {
    return;
  }
  steer_and_step(formation, declared_pace, wheel_step, delta_time);
}

} // namespace Game::Formation::March

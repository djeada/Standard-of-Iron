#include "attack_chase.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../formation_combat_geometry.h"
#include "combat_random.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "structure_combat.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

struct ChasePlan {
  QVector3D desired_pos;
  bool hold_position = false;
  bool goal_follows_attacker = false;
};

struct ChaseGeometry {
  QVector3D attacker_pos;
  QVector3D target_pos;
  float distance = 0.0F;
  float spread_angle = 0.0F;
};

auto chase_spread_angle(Engine::Core::EntityID attacker_id) -> float {
  std::uint32_t const seed =
      static_cast<std::uint32_t>(attacker_id * 2654435761U) ^ 0x85EBCA6BU;
  return (hash_to_unit(seed) - 0.5F) * Constants::k_chase_spread_arc;
}

auto rotate_xz(const QVector3D& vec, float angle) -> QVector3D {
  float const cos_a = std::cos(angle);
  float const sin_a = std::sin(angle);
  return {vec.x() * cos_a - vec.z() * sin_a, 0.0F, vec.x() * sin_a + vec.z() * cos_a};
}

auto single_body_chase_distance(Engine::Core::Entity& attacker,
                                Engine::Core::Entity& target,
                                const FormationCombat::ContactGeometry& geometry)
    -> float {
  auto const* target_attack = target.get_component<Engine::Core::AttackComponent>();
  bool const target_already_engaged =
      target_attack != nullptr && target_attack->in_melee_lock &&
      target_attack->melee_lock_target_id != 0 &&
      !target_attack->melee_locked_on(attacker.get_id());

  if (target_already_engaged) {
    return std::max(0.2F,
                    geometry.engagement_center_distance > 0.0F
                        ? geometry.engagement_center_distance
                        : geometry.contact_center_distance);
  }
  return geometry.contact_center_distance * 0.35F;
}

auto chase_destination(const QVector3D& attacker_pos,
                       const QVector3D& target_pos,
                       float desired_distance,
                       float spread_angle) -> QVector3D {
  QVector3D approach = attacker_pos - target_pos;
  approach.setY(0.0F);
  float const length_sq = approach.lengthSquared();
  if (length_sq <= 0.000001F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    approach /= std::sqrt(length_sq);
  }
  approach = rotate_xz(approach, spread_angle);
  return target_pos + approach * desired_distance;
}

auto should_queue_chase_command(Engine::Core::MovementComponent* movement,
                                const QVector3D& target_pos,
                                const QVector3D& desired_pos,
                                bool goal_follows_attacker,
                                float delta_time) -> bool {
  if (movement == nullptr) {
    return false;
  }

  if (!movement->get_has_target() && !movement->has_waypoints()) {
    return true;
  }

  QVector3D const planned_target =
      movement->get_has_requested_goal()
          ? QVector3D(movement->get_requested_goal_x(),
                      0.0F,
                      movement->get_requested_goal_z())
          : QVector3D(movement->get_goal_x(), 0.0F, movement->get_goal_y());

  float const frame_scale =
      std::clamp(delta_time * Constants::k_reference_frames_per_second,
                 1.0F,
                 Constants::k_max_chase_threshold_scale);
  float const threshold = Constants::k_new_command_threshold * frame_scale;
  if (!goal_follows_attacker) {
    return (planned_target - desired_pos).lengthSquared() > threshold * threshold;
  }

  float const planned_standoff = (planned_target - target_pos).length();
  float const desired_standoff = (desired_pos - target_pos).length();
  return std::abs(planned_standoff - desired_standoff) > threshold;
}

auto plan_structure_chase(const ChaseInputs& in,
                          const ChaseGeometry& geo) -> ChasePlan {
  ChasePlan plan{.desired_pos = geo.target_pos};
  if (in.ranged_unit) {
    auto const surface = closest_structure_surface(*in.target, geo.attacker_pos);
    float const optimal_range = in.range * Constants::k_optimal_range_factor;
    if (surface.distance > optimal_range + Constants::k_optimal_range_buffer) {
      plan.desired_pos = surface.point + surface.outward_normal * optimal_range;
      plan.desired_pos.setY(0.0F);
    } else {
      plan.hold_position = true;
    }
  } else {
    auto const approach = structure_navigation_melee_approach(*in.attacker, *in.target);
    plan.desired_pos = approach.destination;
    plan.desired_pos.setY(0.0F);
    plan.hold_position = approach.reached;
  }
  return plan;
}

auto plan_elephant_chase(const ChaseInputs& in, const ChaseGeometry& geo) -> ChasePlan {
  ChasePlan plan{.desired_pos = geo.target_pos};
  float const target_radius = combat_radius(in.target);
  if (geo.distance > 0.0F) {
    float const desired_distance = target_radius + std::max(in.range - 0.2F, 0.2F);
    if (geo.distance > desired_distance + 0.15F) {
      plan.desired_pos = chase_destination(
          geo.attacker_pos, geo.target_pos, desired_distance, geo.spread_angle);
      plan.goal_follows_attacker = true;
    } else {
      plan.hold_position = true;
    }
  }
  return plan;
}

auto plan_ranged_chase(const ChaseInputs& in, const ChaseGeometry& geo) -> ChasePlan {
  ChasePlan plan{.desired_pos = geo.target_pos};
  if (geo.distance > 0.0F) {
    float const optimal_range = in.range * Constants::k_optimal_range_factor;
    if (geo.distance > optimal_range + Constants::k_optimal_range_buffer) {
      float const ranged_approach_angle =
          FormationCombat::has_formation_slots(*in.attacker) ? 0.0F : geo.spread_angle;
      plan.desired_pos = chase_destination(
          geo.attacker_pos, geo.target_pos, optimal_range, ranged_approach_angle);
      plan.goal_follows_attacker = true;
    } else {
      plan.hold_position = true;
    }
  }
  return plan;
}

auto plan_melee_chase(const ChaseInputs& in, const ChaseGeometry& geo) -> ChasePlan {
  ChasePlan plan{.desired_pos = geo.target_pos};
  if (geo.distance <= 0.0F) {
    return plan;
  }
  auto const geometry = FormationCombat::contact_geometry(*in.attacker, *in.target);
  auto const elephant_penetration =
      elephant_formation_penetration_distance(*in.attacker, *in.target, geometry);
  float const desired_distance =
      geometry.uses_formation_slots
          ? elephant_penetration.value_or(
                std::max(0.0F,
                         geometry.engagement_center_distance -
                             (geometry.formation_overlap_required
                                  ? geometry.contact_tolerance * 2.0F
                                  : 0.0F)))
          : single_body_chase_distance(*in.attacker, *in.target, geometry);
  if (melee_contact_reached(*in.attacker, *in.target, geometry)) {
    plan.hold_position = true;
    return plan;
  }

  auto const* slot =
      in.attacker->get_component<Engine::Core::EngagementSlotComponent>();
  QVector3D const anchor =
      (slot != nullptr && slot->valid && slot->target_id == in.target->get_id())
          ? QVector3D(slot->anchor_offset_x, 0.0F, slot->anchor_offset_z)
          : QVector3D();
  if (anchor.lengthSquared() > 0.000001F) {
    plan.desired_pos = geo.target_pos + anchor.normalized() * desired_distance;
  } else {
    plan.desired_pos =
        chase_destination(geo.attacker_pos,
                          geo.target_pos,
                          desired_distance,
                          geometry.uses_formation_slots ? 0.0F : geo.spread_angle);
    plan.goal_follows_attacker = true;
  }
  return plan;
}

void route_around_separating_structure(const ChaseInputs& in,
                                       const ChaseGeometry& geo,
                                       ChasePlan& plan) {
  if (in.ranged_unit || !structure_separates_combatants(in.attacker, in.target)) {
    return;
  }
  auto const bypass_geometry =
      FormationCombat::contact_geometry(*in.attacker, *in.target);
  auto const bypass = melee_bypass_destination(
      geo.attacker_pos,
      geo.target_pos,
      bypass_geometry.contact_center_distance,
      std::max(k_min_bypass_clearance,
               FormationCombat::formation_navigation_clearance(*in.attacker)));
  if (bypass.has_value()) {
    plan.desired_pos = *bypass;
    plan.hold_position = false;
    plan.goal_follows_attacker = false;
  }
}

auto plan_chase(const ChaseInputs& in, const ChaseGeometry& geo) -> ChasePlan {
  ChasePlan plan;
  if (is_building(in.target)) {
    plan = plan_structure_chase(in, geo);
  } else if (in.target->has_component<Engine::Core::ElephantComponent>()) {
    plan = plan_elephant_chase(in, geo);
  } else if (in.ranged_unit) {
    plan = plan_ranged_chase(in, geo);
  } else {
    plan = plan_melee_chase(in, geo);
  }
  route_around_separating_structure(in, geo, plan);
  return plan;
}

} // namespace

void steer_toward_target(const ChaseInputs& in,
                         std::vector<CommandService::MoveIntent>& chase_move_intents) {
  Engine::Core::Entity* attacker = in.attacker;
  ChaseGeometry geo;
  geo.attacker_pos = QVector3D(
      in.attacker_transform->position.x, 0.0F, in.attacker_transform->position.z);
  geo.target_pos =
      QVector3D(in.target_transform->position.x, 0.0F, in.target_transform->position.z);
  geo.spread_angle = chase_spread_angle(attacker->get_id());
  float const distance_sq = (geo.target_pos - geo.attacker_pos).lengthSquared();
  geo.distance = distance_sq > 0.000001F ? std::sqrt(distance_sq) : 0.0F;

  ChasePlan const plan = plan_chase(in, geo);

  auto* movement =
      Engine::Core::get_or_add_component<Engine::Core::MovementComponent>(attacker);
  if (movement == nullptr) {
    return;
  }
  if (is_building(in.target) && !in.ranged_unit) {
    movement->set_structure_approach_target(in.target->get_id());
  } else {
    movement->clear_structure_approach_target();
  }
  if (plan.hold_position) {
    movement->stop();
    movement->set_rest_position(in.attacker_transform->position.x,
                                in.attacker_transform->position.z);
  } else if (should_queue_chase_command(movement,
                                        geo.target_pos,
                                        plan.desired_pos,
                                        plan.goal_follows_attacker,
                                        in.delta_time)) {
    chase_move_intents.push_back({attacker->get_id(), plan.desired_pos});
  }
}

} // namespace Game::Systems::Combat

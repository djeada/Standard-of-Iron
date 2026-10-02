#include "formation_slot_directive.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "../../util/planar_math.h"
#include "../navigation/wall_walk_path.h"
#include "combat_utils.h"
#include "core/wall_walk_geometry.h"
#include "formation_combat_roles.h"
#include "formation_local_frame.h"
#include "formation_reform_walk.h"
#include "formation_soldier_walk.h"
#include "structure_combat.h"
#include "target_rules.h"

namespace Game::Systems::Combat {
namespace {

using Soldier = Engine::Core::FormationSoldierPresentation;

constexpr float k_engage_close_speed = 3.2F;
constexpr float k_contact_turn_degrees = 180.0F;
constexpr float k_contact_yaw_hold_seconds = 0.6F;
constexpr float k_disengage_turn_degrees = 120.0F;
constexpr float k_weapon_contact_distance = 0.72F;

struct SlotContext {
  const FormationCombat::SoldierSlot& original;
  const FormationCombat::SoldierSlot* live{nullptr};
  const Soldier* previous{nullptr};
  const Engine::Core::UnitTraversalSlotState* traversal_slot{nullptr};
  SoldierAssignment assignment;
  float anchor_local_x{0.0F};
  float anchor_local_z{0.0F};
};

struct FacadeRank {
  StructureSurfaceContact surface{};
  bool ranked{false};
};

void keep_directive_off_facade(const EntityFrame& frame, Soldier& directive) {
  auto const& structure = frame.structure;
  if (!directive.alive || !structure.attacks_structure || frame.actor == nullptr ||
      structure.facade.outward_normal.lengthSquared() <= 0.000001F) {
    return;
  }
  auto const rendered =
      local_to_world(*frame.actor, directive.local_x, directive.local_z);
  float const facade_gap = QVector3D::dotProduct(rendered - structure.facade.point,
                                                 structure.facade.outward_normal);
  if (facade_gap < 0.0F) {
    QVector3D const correction = structure.facade.outward_normal * (-facade_gap);
    auto const [correction_x, correction_z] =
        world_vector_to_local(*frame.actor, correction.x(), correction.z());
    directive.local_x += correction_x;
    directive.local_z += correction_z;
  }
}

auto seed_directive(const EntityFrame& frame, const SlotContext& slot) -> Soldier {
  auto const& original = slot.original;
  auto const* live = slot.live;
  auto const& layout = frame.layout;
  Soldier directive;
  directive.slot_index = original.index;
  directive.row = live != nullptr ? live->row : original.row;
  directive.col = live != nullptr ? live->col : original.col;
  directive.local_x =
      frame.frame_sign * (live != nullptr ? live->local_x : original.local_x);
  directive.local_z =
      frame.frame_sign * (live != nullptr ? live->local_z : original.local_z);
  if (live != nullptr && frame.traversal != nullptr && slot.traversal_slot != nullptr) {
    auto const* traversal_slot = slot.traversal_slot;
    directive.row = traversal_slot->row;
    directive.col = traversal_slot->col;
    directive.previous_local_x = traversal_slot->previous_local_x;
    directive.previous_local_z = traversal_slot->previous_local_z;
    directive.local_x = traversal_slot->current_local_x;
    directive.local_z = traversal_slot->current_local_z;
    directive.relocation_velocity_x = traversal_slot->velocity_x;
    directive.relocation_velocity_z = traversal_slot->velocity_z;
    directive.relocation_blocked = traversal_slot->blocked;
  }
  if (live != nullptr && frame.structure.render_shift > 0.0F) {
    directive.local_x += frame.structure.shift_local_x;
    directive.local_z += frame.structure.shift_local_z;
  }
  directive.local_yaw = live != nullptr ? live->local_yaw : original.local_yaw;
  directive.alive = live != nullptr;
  directive.combat_speed_scale =
      0.94F + hash_unit_float(layout.seed, original.index * 73U + 19U) * 0.12F;
  directive.combat_phase_bias =
      (hash_unit_float(layout.seed, original.index * 131U + 41U) - 0.5F) * 0.56F;
  return directive;
}

void assign_target(const EntityFrame& frame,
                   const SlotContext& slot,
                   const RetainedTarget& retained,
                   Soldier& directive) {
  auto const* previous = slot.previous;
  auto const& assignment = slot.assignment;
  directive.opponent_id = assignment.front->opponent_id;
  directive.target_slot = retained.slot;
  bool const same_target = previous != nullptr &&
                           previous->opponent_id == directive.opponent_id &&
                           previous->target_slot == directive.target_slot;
  directive.target_held_seconds =
      same_target
          ? std::min(previous->target_held_seconds + std::max(0.0F, frame.delta_time),
                     k_target_hold_seconds + 1.0F)
          : 0.0F;
  directive.engagement_surface_gap =
      assignment.pair->surface_gap +
      (retained.root_distance - assignment.pair->root_distance);
}

auto rank_against_structure(const EntityFrame& frame,
                            Engine::Core::Entity* opponent,
                            const Soldier& directive) -> FacadeRank {
  FacadeRank rank;
  bool const opponent_is_structure = opponent != nullptr && is_building(opponent);
  if (!opponent_is_structure || frame.actor == nullptr) {
    return rank;
  }
  rank.surface = closest_structure_surface(
      *opponent, local_to_world(*frame.actor, directive.local_x, directive.local_z));
  rank.ranked = opponent != frame.display_opponent ||
                !std::isfinite(frame.structure.nearest_anchor) ||
                rank.surface.distance <= frame.structure.nearest_anchor +
                                             frame.structure.render_shift +
                                             frame.layout.spacing * 0.55F;
  return rank;
}

void press_toward_target(const EntityFrame& frame,
                         const SlotContext& slot,
                         Engine::Core::Entity* opponent,
                         const FormationCombat::FormationLayout& opponent_layout,
                         const RetainedTarget& retained,
                         const FacadeRank& rank,
                         Soldier& directive) {
  auto const* target_slot = find_live_slot(opponent_layout, retained.slot);
  auto const* opponent_transform =
      opponent != nullptr
          ? frame.world.try_get<Engine::Core::TransformComponent>(opponent->get_id())
          : nullptr;
  if (frame.actor == nullptr || opponent_transform == nullptr) {
    return;
  }
  auto const& layout = frame.layout;
  float const target_x = rank.ranked              ? rank.surface.point.x()
                         : target_slot != nullptr ? target_slot->world_x
                                                  : opponent_transform->position.x;
  float const target_z = rank.ranked              ? rank.surface.point.z()
                         : target_slot != nullptr ? target_slot->world_z
                                                  : opponent_transform->position.z;
  auto const contact_vector = local_contact_vector(
      *frame.actor, directive.local_x, directive.local_z, target_x, target_z);
  float const desired_yaw = contact_vector.yaw;
  float const prior_yaw =
      slot.previous != nullptr ? slot.previous->local_yaw : directive.local_yaw;
  directive.local_yaw =
      turn_yaw_toward(prior_yaw,
                      desired_yaw,
                      k_contact_turn_degrees * std::max(0.0F, frame.delta_time));

  float const pull_distance =
      rank.ranked
          ? std::clamp(rank.surface.distance -
                           structure_attack_profile(&frame.entity).contact_clearance,
                       0.0F,
                       layout.spacing * 1.6F)
          : std::clamp(contact_vector.distance - k_weapon_contact_distance,
                       0.0F,
                       layout.spacing * 0.20F);
  if (contact_vector.distance > 0.0001F) {
    directive.local_x += contact_vector.x / contact_vector.distance * pull_distance;
    directive.local_z += contact_vector.z / contact_vector.distance * pull_distance;
  }

  float const yaw_rad = desired_yaw * std::numbers::pi_v<float> / 180.0F;
  float const forward_x = std::sin(yaw_rad);
  float const forward_z = std::cos(yaw_rad);
  float const right_x = std::cos(yaw_rad);
  float const right_z = -std::sin(yaw_rad);
  std::uint32_t const soldier_seed =
      layout.seed ^ (static_cast<std::uint32_t>(slot.original.index) * 0x9e3779b9U);

  float const lateral = (hash_unit_float(soldier_seed, 0x4f1bbcdcU) - 0.5F) *
                        std::min(0.08F, layout.spacing * 0.08F);
  float const depth = (hash_unit_float(soldier_seed, 0x94d049bbU) - 0.5F) *
                      std::min(0.06F, layout.spacing * 0.06F);
  directive.local_x += right_x * lateral + forward_x * depth;
  directive.local_z += right_z * lateral + forward_z * depth;
}

void mark_damage_carrier(const EntityFrame& frame,
                         const SoldierAssignment& assignment,
                         Soldier& directive) {
  if (!assignment.front->outgoing) {
    return;
  }
  auto const carrier = std::find_if(frame.damage_carriers.begin(),
                                    frame.damage_carriers.end(),
                                    [&assignment](auto const& candidate) {
                                      return candidate.front == assignment.front;
                                    });
  directive.damage_carrier = carrier != frame.damage_carriers.end() &&
                             carrier->attacker_slot.has_value() &&
                             *carrier->attacker_slot == directive.slot_index;
}

void apply_assigned_engagement(const EntityFrame& frame,
                               const SlotContext& slot,
                               Soldier& directive) {
  auto const& assignment = slot.assignment;
  auto* opponent = frame.world.get_entity(assignment.front->opponent_id);
  static const FormationCombat::FormationLayout k_empty_layout;
  auto const& opponent_layout =
      opponent != nullptr ? frame.layouts.for_entity(*opponent) : k_empty_layout;
  auto const retained = retained_target_slot(
      opponent_layout, slot.live, slot.previous, assignment, frame.layout.spacing);

  assign_target(frame, slot, retained, directive);
  FacadeRank const rank = rank_against_structure(frame, opponent, directive);
  bool const opponent_is_structure = opponent != nullptr && is_building(opponent);
  bool const opponent_within_reach =
      opponent_is_structure
          ? rank.ranked
          : directive.engagement_surface_gap <= frame.layout.spacing * 0.65F;
  directive.combat_role =
      brawls_as_a_crowd(frame.world, frame.entity.get_id())
          ? crowd_brawl_role(slot.original.index)
      : opponent_within_reach
          ? combat_role_for(frame.layout.seed, slot.original.index, true)
          : Engine::Core::FormationSoldierCombatRole::Guard;
  directive.action = action_for_role(directive.combat_role);

  press_toward_target(
      frame, slot, opponent, opponent_layout, retained, rank, directive);
  mark_damage_carrier(frame, assignment, directive);
}

void face_crowd_opponent(const EntityFrame& frame, Soldier& directive) {
  bool const crowd = brawls_as_a_crowd(frame.world, frame.entity.get_id());
  bool const faces_an_animal = frame.display_opponent != nullptr &&
                               frame.world.has<Engine::Core::WildlifeComponent>(
                                   frame.display_opponent->get_id());
  if (!(crowd || faces_an_animal) || frame.structure.attacks_structure ||
      frame.actor == nullptr || frame.display_opponent == nullptr) {
    return;
  }
  if (auto const* opponent_transform =
          frame.world.try_get<Engine::Core::TransformComponent>(
              frame.display_opponent->get_id())) {
    auto const toward = local_contact_vector(*frame.actor,
                                             directive.local_x,
                                             directive.local_z,
                                             opponent_transform->position.x,
                                             opponent_transform->position.z);
    if (toward.distance > 0.0001F) {
      directive.local_yaw = toward.yaw;
    }
  }
}

void press_against_attacked_structure(const EntityFrame& frame,
                                      const SlotContext& slot,
                                      Soldier& directive) {
  auto const& structure = frame.structure;
  if (!structure.attacks_structure || frame.actor == nullptr) {
    return;
  }
  QVector3D const anchor_world =
      local_to_world(*frame.actor, directive.local_x, directive.local_z);
  auto const surface = closest_structure_surface(*frame.display_opponent, anchor_world);
  float const facade_gap = QVector3D::dotProduct(anchor_world - structure.facade.point,
                                                 structure.facade.outward_normal);
  bool const facade_rank = facade_gap <= structure.closest_gap +
                                             structure.render_shift +
                                             frame.layout.spacing * 0.55F;
  if (!facade_rank) {
    return;
  }
  auto const contact_vector = local_contact_vector(*frame.actor,
                                                   directive.local_x,
                                                   directive.local_z,
                                                   surface.point.x(),
                                                   surface.point.z());
  float const prior_yaw =
      slot.previous != nullptr ? slot.previous->local_yaw : directive.local_yaw;
  directive.local_yaw =
      turn_yaw_toward(prior_yaw,
                      contact_vector.yaw,
                      k_contact_turn_degrees * std::max(0.0F, frame.delta_time));

  float const desired_gap = structure_attack_profile(&frame.entity).contact_clearance;
  float const pull_distance =
      std::clamp(surface.distance - desired_gap, 0.0F, frame.layout.spacing * 1.6F);
  if (contact_vector.distance > 0.0001F) {
    directive.local_x += contact_vector.x / contact_vector.distance * pull_distance;
    directive.local_z += contact_vector.z / contact_vector.distance * pull_distance;
  }
}

void continue_follow_through(const EntityFrame& frame,
                             const Soldier* previous,
                             Soldier& directive) {
  using Action = Engine::Core::FormationSoldierAction;
  if (previous != nullptr && frame.presentation.target_id == frame.display_target &&
      (previous->action == Action::MeleeEngaged ||
       previous->action == Action::MeleeFollowThrough)) {
    directive.action = Action::MeleeFollowThrough;
    directive.combat_role = previous->combat_role;
    directive.target_slot = previous->target_slot;
    directive.engagement_surface_gap = previous->engagement_surface_gap;
  }
}

void apply_unassigned_melee(const EntityFrame& frame,
                            const SlotContext& slot,
                            Soldier& directive) {
  directive.combat_role =
      brawls_as_a_crowd(frame.world, frame.entity.get_id())
          ? crowd_brawl_role(slot.original.index)
          : combat_role_for(frame.layout.seed, slot.original.index, false);
  directive.action = action_for_role(directive.combat_role);
  face_crowd_opponent(frame, directive);
  press_against_attacked_structure(frame, slot, directive);
  continue_follow_through(frame, slot.previous, directive);
}

void apply_engagement(const EntityFrame& frame,
                      const SlotContext& slot,
                      Soldier& directive) {
  bool const assigned =
      slot.assignment.front != nullptr && slot.assignment.pair != nullptr;
  if (assigned) {
    apply_assigned_engagement(frame, slot, directive);
  } else if (directive.alive && frame.melee_ordered) {
    apply_unassigned_melee(frame, slot, directive);
  } else {
    directive.action = Engine::Core::FormationSoldierAction::FollowUnit;
  }
}

void hold_unassigned_yaw(const EntityFrame& frame,
                         const SlotContext& slot,
                         Soldier& directive) {
  bool const assigned =
      slot.assignment.front != nullptr && slot.assignment.pair != nullptr;
  auto const* previous = slot.previous;
  if (assigned || previous == nullptr || !directive.alive) {
    return;
  }
  directive.unassigned_seconds =
      std::min(previous->unassigned_seconds + std::max(0.0F, frame.delta_time),
               k_contact_yaw_hold_seconds + 1.0F);
  if (directive.unassigned_seconds < k_contact_yaw_hold_seconds) {
    directive.local_yaw = previous->local_yaw;
  } else {
    directive.local_yaw =
        turn_yaw_toward(previous->local_yaw,
                        directive.local_yaw,
                        k_disengage_turn_degrees * std::max(0.0F, frame.delta_time));
  }
}

void carry_fallen_soldier_offset(const EntityFrame& frame,
                                 const SlotContext& slot,
                                 Soldier& directive) {
  auto const* previous = slot.previous;
  if (directive.alive || previous == nullptr ||
      !(previous->alive || previous->contact_offset_x != 0.0F ||
        previous->contact_offset_z != 0.0F)) {
    return;
  }
  directive.contact_offset_x = previous->contact_offset_x;
  directive.contact_offset_z = previous->contact_offset_z;
  directive.local_x = slot.anchor_local_x + previous->contact_offset_x;
  directive.local_z = slot.anchor_local_z + previous->contact_offset_z;
  directive.local_yaw = previous->local_yaw;

  directive.crowd_offset_x = previous->crowd_offset_x;
  directive.crowd_offset_z = previous->crowd_offset_z;
  if (frame.actor != nullptr) {
    auto const [crowd_local_x, crowd_local_z] = world_vector_to_local(
        *frame.actor, previous->crowd_offset_x, previous->crowd_offset_z);
    directive.local_x += crowd_local_x;
    directive.local_z += crowd_local_z;
  }
}

void limit_contact_close_speed(const EntityFrame& frame,
                               const SlotContext& slot,
                               Soldier& directive) {
  if (!directive.alive) {
    return;
  }
  auto const* previous = slot.previous;
  float const desired_offset_x = directive.local_x - slot.anchor_local_x;
  float const desired_offset_z = directive.local_z - slot.anchor_local_z;
  bool const continues = previous != nullptr && previous->alive;
  float const prior_offset_x = continues ? previous->contact_offset_x : 0.0F;
  float const prior_offset_z = continues ? previous->contact_offset_z : 0.0F;
  float const step_x = desired_offset_x - prior_offset_x;
  float const step_z = desired_offset_z - prior_offset_z;
  float const step = std::hypot(step_x, step_z);
  float const allowed = k_engage_close_speed * std::max(0.0F, frame.delta_time);
  float offset_x = desired_offset_x;
  float offset_z = desired_offset_z;
  if (step > allowed && step > 0.0001F) {
    offset_x = prior_offset_x + step_x / step * allowed;
    offset_z = prior_offset_z + step_z / step * allowed;
  }
  directive.contact_offset_x = offset_x;
  directive.contact_offset_z = offset_z;
  directive.local_x = slot.anchor_local_x + offset_x;
  directive.local_z = slot.anchor_local_z + offset_z;
}

void track_relocation(const EntityFrame& frame,
                      const SlotContext& slot,
                      Soldier& directive) {
  if (slot.traversal_slot != nullptr) {
    return;
  }
  auto const* previous = slot.previous;
  if (previous != nullptr && directive.alive) {
    float const step_time = std::max(0.0F, frame.delta_time);
    directive.previous_local_x = previous->local_x;
    directive.previous_local_z = previous->local_z;
    directive.relocation_velocity_x =
        step_time > 0.0F ? (directive.local_x - directive.previous_local_x) / step_time
                         : 0.0F;
    directive.relocation_velocity_z =
        step_time > 0.0F ? (directive.local_z - directive.previous_local_z) / step_time
                         : 0.0F;
  } else {
    directive.previous_local_x = directive.local_x;
    directive.previous_local_z = directive.local_z;
  }
}

auto works_a_site(const EntityFrame& frame) -> bool {
  auto const* builder = frame.world.try_get<Engine::Core::BuilderProductionComponent>(
      frame.entity.get_id());
  return builder != nullptr && builder->in_progress && builder->at_construction_site;
}

void walk_slot(const EntityFrame& frame,
               const SlotContext& slot,
               const std::vector<Soldier>& previous_soldiers,
               Soldier& directive) {
  if (!directive.alive || frame.actor == nullptr ||
      frame.layout.all_slots.size() <= 1U) {
    return;
  }
  auto const* previous = slot.previous;
  if (!frame.melee_ordered) {
    directive.local_yaw =
        slot.live != nullptr ? slot.live->local_yaw : slot.original.local_yaw;
  }
  auto& foreign = frame.foreign;
  foreign.scratch.clear();
  if (frame.in_melee_contact && previous != nullptr && previous->world_motion_valid) {
    foreign.grid.gather(frame.entity.get_id(),
                        slot.original.index,
                        previous->world_x,
                        previous->world_z,
                        foreign_gather_radius(frame.layout.spacing, frame.mounted),
                        foreign.scratch);
  }
  float const contact_step =
      previous != nullptr
          ? std::hypot(directive.contact_offset_x - previous->contact_offset_x,
                       directive.contact_offset_z - previous->contact_offset_z)
          : 0.0F;
  float const crowd_step_budget =
      k_engage_close_speed * std::max(0.0F, frame.delta_time) - contact_step;
  walk_formation_slot(
      {.actor = *frame.actor,
       .formation = frame.presentation,
       .neighbors = previous_soldiers,
       .foreign_neighbors = foreign.scratch,
       .crowd_step_budget = crowd_step_budget,
       .squad_speed = frame.squad_speed,
       .march_speed = frame.unit.speed,
       .spacing = frame.layout.spacing,
       .body_radius = frame.layout.body_radius,
       .rows = frame.layout.rows,
       .seed = frame.layout.seed,
       .mounted = frame.mounted,
       .engaged = frame.melee_ordered,
       .external_reform = frame.reform != nullptr,
       .position_is_authored = slot.traversal_slot != nullptr && !frame.mounted,
       .walking_to_work_posts = works_a_site(frame),
       .passability = frame.passability,
       .delta_time = frame.delta_time},
      previous,
      directive);
}

// Soldiers of a troop on a wall stay on its walk: the balcony, the stair they
// are using, or the bridge of the tower they are leaving. Each heads for the
// spot its slot asks for, at a run, and is held to the nearest point of the
// walk with that point's height. A tower's company files out of the door one
// after another, nearest the landing first.
void pin_to_wall_walk(const EntityFrame& frame,
                      const SlotContext& slot,
                      Soldier& directive) {
  auto const* walker = frame.wall_walker;
  if (walker == nullptr ||
      walker->phase == Engine::Core::WallWalkerComponent::Phase::Approaching ||
      walker->path.empty()) {
    directive.elevation = 0.0F;
    return;
  }
  if (!directive.alive) {
    // The fallen lie where they fell, on the planks.
    if (slot.previous != nullptr && slot.previous->world_motion_valid) {
      directive.world_x = slot.previous->world_x;
      directive.world_z = slot.previous->world_z;
      directive.world_motion_valid = true;
    }
    directive.elevation = slot.previous != nullptr ? slot.previous->elevation : 0.0F;
    return;
  }
  constexpr float k_wall_run_speed = 2.4F;
  constexpr float k_bridge_speed = 1.7F;
  auto const* previous = slot.previous;
  bool const boarding =
      walker->phase == Engine::Core::WallWalkerComponent::Phase::Boarding;
  float const target_x = directive.world_x;
  float const target_z = directive.world_z;

  auto const index = static_cast<float>(slot.original.index);
  float const door_x = walker->door_x + 0.16F * std::sin(index * 2.39F);
  float const door_z = walker->door_z + 0.16F * std::cos(index * 2.39F);
  bool const fresh = previous == nullptr || !previous->world_motion_valid ||
                     (boarding && previous->elevation <= 0.0F);
  bool const released =
      !boarding || walker->boarding_seconds >=
                       index * Engine::Core::WallWalkerComponent::k_file_out_interval;
  if (boarding && (!released || fresh)) {
    directive.world_x = door_x;
    directive.world_z = door_z;
    directive.elevation = walker->door_y;
    directive.world_velocity_x = 0.0F;
    directive.world_velocity_z = 0.0F;
    directive.world_motion_valid = true;
    return;
  }
  if (boarding &&
      previous->elevation > Game::Systems::WallWalk::k_deck_height + 0.005F) {
    // Still on the bridge: cross it in file, door to lip to crest to balcony.
    std::array<Engine::Core::WallWalkSegment, 3> const chain{{
        {walker->door_x,
         walker->door_z,
         walker->door_y,
         walker->lip_x,
         walker->lip_z,
         walker->door_y},
        {walker->lip_x,
         walker->lip_z,
         walker->door_y,
         walker->crest_x,
         walker->crest_z,
         Game::Systems::WallWalk::k_crest_height},
        {walker->crest_x,
         walker->crest_z,
         Game::Systems::WallWalk::k_crest_height,
         walker->landing_x,
         walker->landing_z,
         Game::Systems::WallWalk::k_deck_height},
    }};
    float travelled = 0.0F;
    float best_d2 = std::numeric_limits<float>::max();
    float arc = 0.0F;
    for (auto const& link : chain) {
      float const dx = link.bx - link.ax;
      float const dz = link.bz - link.az;
      float const length = std::hypot(dx, dz);
      float t = 0.0F;
      if (length > 1.0e-4F) {
        t = std::clamp(
            ((previous->world_x - link.ax) * dx + (previous->world_z - link.az) * dz) /
                (length * length),
            0.0F,
            1.0F);
      }
      float const px = link.ax + dx * t - previous->world_x;
      float const pz = link.az + dz * t - previous->world_z;
      if (px * px + pz * pz < best_d2) {
        best_d2 = px * px + pz * pz;
        arc = travelled + length * t;
      }
      travelled += length;
    }
    arc = std::min(travelled, arc + k_bridge_speed * std::max(0.0F, frame.delta_time));
    float remaining = arc;
    for (auto const& link : chain) {
      float const length = std::hypot(link.bx - link.ax, link.bz - link.az);
      if (remaining <= length || &link == &chain.back()) {
        float const t =
            length > 1.0e-4F ? std::clamp(remaining / length, 0.0F, 1.0F) : 1.0F;
        float const x = link.ax + (link.bx - link.ax) * t;
        float const z = link.az + (link.bz - link.az) * t;
        float const step_time = std::max(frame.delta_time, 1.0e-4F);
        directive.world_velocity_x = (x - previous->world_x) / step_time;
        directive.world_velocity_z = (z - previous->world_z) / step_time;
        directive.world_x = x;
        directive.world_z = z;
        directive.elevation = link.ay + (link.by - link.ay) * t;
        if (directive.elevation <= Game::Systems::WallWalk::k_deck_height + 0.005F) {
          directive.elevation = Game::Systems::WallWalk::k_deck_height;
        }
        float const heading = std::atan2(link.bx - link.ax, link.bz - link.az);
        directive.world_yaw = heading * 180.0F / std::numbers::pi_v<float>;
        directive.world_motion_valid = true;
        return;
      }
      remaining -= length;
    }
  }

  float const from_x = fresh ? target_x : previous->world_x;
  float const from_z = fresh ? target_z : previous->world_z;
  float const dx = target_x - from_x;
  float const dz = target_z - from_z;
  float const want = std::hypot(dx, dz);
  float const step_time = std::max(0.0F, frame.delta_time);
  float const allowed = k_wall_run_speed * step_time;
  float x = target_x;
  float z = target_z;
  if (want > allowed && want > 1.0e-4F) {
    x = from_x + dx / want * allowed;
    z = from_z + dz / want * allowed;
  }
  auto const point = Game::Systems::WallWalk::project_onto_path(walker->path, x, z);
  if (!point.valid) {
    directive.elevation = 0.0F;
    return;
  }
  directive.world_x = point.x;
  directive.world_z = point.z;
  directive.elevation = point.y;
  directive.world_motion_valid = true;
  bool const settled = std::hypot(point.x - from_x, point.z - from_z) < 0.01F;
  if (walker->watching && settled && directive.opponent_id == 0U) {
    directive.world_yaw = walker->watch_yaw;
  }
  if (step_time > 0.0F && !fresh) {
    directive.world_velocity_x = (point.x - from_x) / step_time;
    directive.world_velocity_z = (point.z - from_z) / step_time;
  }
}

auto build_slot_directive(const EntityFrame& frame,
                          SlotContext& slot,
                          const std::vector<Soldier>& previous_soldiers) -> Soldier {
  Soldier directive = seed_directive(frame, slot);
  slot.anchor_local_x = directive.local_x;
  slot.anchor_local_z = directive.local_z;
  slot.assignment =
      directive.alive
          ? assignment_for_slot(frame.contact, slot.previous, slot.original.index)
          : SoldierAssignment{};

  apply_engagement(frame, slot, directive);
  hold_unassigned_yaw(frame, slot, directive);
  carry_fallen_soldier_offset(frame, slot, directive);
  limit_contact_close_speed(frame, slot, directive);
  if (frame.reform != nullptr && directive.alive && frame.actor != nullptr) {
    directive.reforming = walk_to_new_slot(*frame.reform,
                                           *frame.actor,
                                           frame.squad_speed,
                                           frame.delta_time,
                                           slot.previous,
                                           directive);
  }
  track_relocation(frame, slot, directive);
  keep_directive_off_facade(frame, directive);
  walk_slot(frame, slot, previous_soldiers, directive);
  pin_to_wall_walk(frame, slot, directive);
  return directive;
}

} // namespace

auto publish_soldiers(const EntityFrame& frame) -> bool {
  auto& directives = frame.presentation.soldiers;
  auto const& layout = frame.layout;

  const auto previous_soldiers = directives;
  std::size_t const previous_directive_count = directives.size();
  bool soldiers_changed = previous_directive_count != layout.all_slots.size();
  directives.resize(layout.all_slots.size());
  for (auto const& original_slot : layout.all_slots) {
    std::optional<Soldier> previous_value;
    if (original_slot.index < previous_directive_count) {
      previous_value = directives[original_slot.index];
    }
    SlotContext slot{
        .original = original_slot,
        .live = find_live_slot(layout, original_slot.index),
        .previous = previous_value.has_value() ? &*previous_value : nullptr,
        .traversal_slot = frame.traversal != nullptr
                              ? frame.traversal->slot_for(original_slot.index)
                              : nullptr,
    };
    Soldier const directive = build_slot_directive(frame, slot, previous_soldiers);
    soldiers_changed =
        soldiers_changed || slot.previous == nullptr || *slot.previous != directive;
    directives[original_slot.index] = directive;
  }
  return soldiers_changed;
}

} // namespace Game::Systems::Combat

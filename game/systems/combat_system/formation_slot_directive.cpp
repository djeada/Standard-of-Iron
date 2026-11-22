#include "formation_slot_directive.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "../../core/ambient_session.h"
#include "../../map/terrain_service.h"
#include "../../util/planar_math.h"
#include "../navigation/nav_grid.h"
#include "../navigation/wall_walk_path.h"
#include "combat_utils.h"
#include "core/wall_walk_geometry.h"
#include "formation_combat_roles.h"
#include "formation_local_frame.h"
#include "formation_reform_walk.h"
#include "formation_soldier_walk.h"
#include "raft_deck_fit.h"
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
// Against another formation a soldier keeps to his slot (0.20 spacing of give) unless
// his way to his foe is clear of his own comrades: then he steps into the gap, up to
// this many spacings, so the men behind a fallen front rank and the files on a shrunken
// enemy's flank close up and fight instead of standing in place. Close in, each side
// covers half the gap (the foe steps too); further out he walks all but a stand-off.
constexpr float k_blocked_press_spacings = 0.20F;
constexpr float k_front_line_press_spacings = 1.0F;
constexpr float k_mutual_press_share = 0.5F;
constexpr float k_mutual_press_gap = 1.2F;
constexpr float k_facade_body_gap_fraction = 0.5F;
constexpr float k_contact_slot_range = 10.0F;

struct SlotContext {
  const FormationCombat::SoldierSlot& original;
  const FormationCombat::SoldierSlot* live{nullptr};
  const Soldier* previous{nullptr};
  const Engine::Core::UnitTraversalSlotState* traversal_slot{nullptr};
  const SurroundSpot* surround{nullptr};
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
  float const body_gap = structure_attack_profile(&frame.entity).contact_clearance *
                         k_facade_body_gap_fraction;
  if (facade_gap < body_gap) {
    QVector3D const correction =
        structure.facade.outward_normal * (body_gap - facade_gap);
    auto const [correction_x, correction_z] =
        world_vector_to_local(*frame.actor, correction.x(), correction.z());
    directive.local_x += correction_x;
    directive.local_z += correction_z;
  }
}

void keep_walk_off_facade(const EntityFrame& frame, Soldier& directive) {
  auto const& structure = frame.structure;
  if (!directive.alive || !directive.world_motion_valid ||
      !structure.attacks_structure || frame.actor == nullptr) {
    return;
  }
  QVector3D const position(
      directive.world_x, frame.actor->position.y, directive.world_z);
  auto const& normal = structure.facade.outward_normal;
  float const gap = QVector3D::dotProduct(position - structure.facade.point, normal);
  float const body_gap = structure_attack_profile(&frame.entity).contact_clearance *
                         k_facade_body_gap_fraction;
  if (gap >= body_gap) {
    return;
  }
  QVector3D const correction = normal * (body_gap - gap);
  directive.world_x += correction.x();
  directive.world_z += correction.z();
  auto const [local_x, local_z] =
      world_vector_to_local(*frame.actor, correction.x(), correction.z());
  directive.local_x += local_x;
  directive.local_z += local_z;
  float const inward =
      directive.world_velocity_x * normal.x() + directive.world_velocity_z * normal.z();
  if (inward < 0.0F) {
    directive.world_velocity_x -= normal.x() * inward;
    directive.world_velocity_z -= normal.z() * inward;
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

auto front_line_is_clear(const FormationCombat::FormationLayout& layout,
                         std::uint16_t own_index,
                         float own_x,
                         float own_z,
                         const LocalContactVector& toward) -> bool {
  if (toward.distance <= 0.0001F) {
    return true;
  }
  float const dir_x = toward.x / toward.distance;
  float const dir_z = toward.z / toward.distance;
  float const shoulder = layout.spacing * 0.5F;
  for (auto const& comrade : layout.live_slots) {
    if (comrade.index == own_index) {
      continue;
    }
    float const rel_x = comrade.local_x - own_x;
    float const rel_z = comrade.local_z - own_z;
    float const along = rel_x * dir_x + rel_z * dir_z;
    if (along <= 0.15F || along >= toward.distance) {
      continue;
    }
    if (std::abs(rel_x * dir_z - rel_z * dir_x) < shoulder) {
      return false;
    }
  }
  return true;
}

// A step beyond a slot's give must not carry a man onto ground his unit cannot stand
// on (a slope, a wall, a river bank): fighting at the foot of a hill, a front rank that
// stepped into the gap left by the fallen walked up the slope.
auto press_stays_walkable(const EntityFrame& frame,
                          const Soldier& directive,
                          const LocalContactVector& toward,
                          float distance) -> bool {
  if (frame.actor == nullptr || toward.distance <= 0.0001F || distance <= 0.0F) {
    return true;
  }
  QVector3D const from =
      local_to_world(*frame.actor, directive.local_x, directive.local_z);
  QVector3D const to =
      local_to_world(*frame.actor,
                     directive.local_x + toward.x / toward.distance * distance,
                     directive.local_z + toward.z / toward.distance * distance);
  if (auto* pathfinder = NavGrid::get_pathfinder();
      pathfinder != nullptr &&
      !pathfinder->is_world_segment_walkable(from, to, frame.passability)) {
    return false;
  }
  auto const* terrain = Game::Session::services_for(frame.world).terrain;
  if (terrain == nullptr) {
    return true;
  }
  constexpr float k_probe_step = 0.5F;
  int const probes = std::max(1, static_cast<int>(std::ceil(distance / k_probe_step)));
  for (int i = 1; i <= probes; ++i) {
    QVector3D const at = from + (to - from) * (static_cast<float>(i) / probes);
    auto const cell = NavGrid::world_to_grid(at.x(), at.z());
    if (!terrain->is_walkable(cell.x, cell.y)) {
      return false;
    }
  }
  return true;
}

// Returns how far the soldier stepped toward his foe this frame.
auto press_toward_target(const EntityFrame& frame,
                         const SlotContext& slot,
                         Engine::Core::Entity* opponent,
                         const FormationCombat::FormationLayout& opponent_layout,
                         const RetainedTarget& retained,
                         const FacadeRank& rank,
                         Soldier& directive) -> float {
  auto const* target_slot = find_live_slot(opponent_layout, retained.slot);
  auto const* opponent_transform =
      opponent != nullptr
          ? frame.world.try_get<Engine::Core::TransformComponent>(opponent->get_id())
          : nullptr;
  if (frame.actor == nullptr || opponent_transform == nullptr) {
    return 0.0F;
  }
  auto const& layout = frame.layout;
  float const target_x = rank.ranked              ? rank.surface.point.x()
                         : target_slot != nullptr ? target_slot->world_x
                                                  : opponent_transform->position.x;
  float const target_z = rank.ranked              ? rank.surface.point.z()
                         : target_slot != nullptr ? target_slot->world_z
                                                  : opponent_transform->position.z;
  auto contact_vector = local_contact_vector(
      *frame.actor, directive.local_x, directive.local_z, target_x, target_z);
  bool const single_opponent = opponent != nullptr && !is_building(opponent) &&
                               !FormationCombat::has_formation_slots(*opponent);
  bool front_clear = rank.ranked || single_opponent ||
                     front_line_is_clear(frame.layout,
                                         slot.original.index,
                                         directive.local_x,
                                         directive.local_z,
                                         contact_vector);
  if (!front_clear) {
    // His nearest foe is screened by his own comrades: go for the nearest foe he can
    // reach with a clear path instead, which wraps the files round the enemy's flank.
    float const reach_limit = frame.layout.spacing * k_front_line_press_spacings +
                              k_weapon_contact_distance + frame.layout.spacing;
    // Only a foe about as near as his own: in a full-strength clash the rear ranks keep
    // formation instead of wrapping round the enemy's flanks.
    float const near_enough = contact_vector.distance + frame.layout.spacing * 0.5F;
    float best = std::numeric_limits<float>::max();
    for (auto const& foe : opponent_layout.live_slots) {
      auto const toward = local_contact_vector(
          *frame.actor, directive.local_x, directive.local_z, foe.world_x, foe.world_z);
      if (toward.distance >= best || toward.distance > reach_limit ||
          toward.distance > near_enough ||
          !front_line_is_clear(frame.layout,
                               slot.original.index,
                               directive.local_x,
                               directive.local_z,
                               toward)) {
        continue;
      }
      best = toward.distance;
      contact_vector = toward;
      directive.target_slot = foe.index;
      directive.engagement_surface_gap =
          toward.distance - (frame.layout.body_radius + opponent_layout.body_radius);
      front_clear = true;
    }
  }
  float const desired_yaw = contact_vector.yaw;
  float const prior_yaw =
      slot.previous != nullptr ? slot.previous->local_yaw : directive.local_yaw;
  directive.local_yaw =
      turn_yaw_toward(prior_yaw,
                      desired_yaw,
                      k_contact_turn_degrees * std::max(0.0F, frame.delta_time));

  auto const* attack =
      frame.world.try_get<Engine::Core::AttackComponent>(frame.entity.get_id());
  float const weapon_reach = attack != nullptr ? attack->melee_range : 1.5F;
  float const contact_distance =
      single_opponent
          ? layout.body_radius + opponent_layout.body_radius + weapon_reach * 0.35F
          : k_weapon_contact_distance;
  float const gap_to_close = std::max(0.0F, contact_vector.distance - contact_distance);
  float pull_distance = 0.0F;
  if (rank.ranked) {
    pull_distance =
        std::clamp(rank.surface.distance -
                       structure_attack_profile(&frame.entity).contact_clearance,
                   0.0F,
                   layout.spacing * 1.6F);
  } else if (single_opponent) {
    pull_distance = std::min(gap_to_close, layout.spacing * 1.6F);
  } else if (front_clear &&
             press_stays_walkable(
                 frame,
                 directive,
                 contact_vector,
                 std::min(gap_to_close,
                          layout.spacing * k_front_line_press_spacings))) {
    // Half the gap, or all but the stand-off when that is more: a man walking up to a
    // distant foe stops short enough that the foe can still step in without the two
    // bodies meeting inside each other, and close enough to reach him.
    float const own_share = std::max(gap_to_close * k_mutual_press_share,
                                     gap_to_close - k_mutual_press_gap);
    pull_distance = std::min(own_share, layout.spacing * k_front_line_press_spacings);
  } else {
    pull_distance = std::min(gap_to_close, layout.spacing * k_blocked_press_spacings);
  }
  bool const waits_behind = slot.surround != nullptr && slot.surround->ring > 0U;
  if (contact_vector.distance > 0.0001F && !waits_behind) {
    directive.local_x += contact_vector.x / contact_vector.distance * pull_distance;
    directive.local_z += contact_vector.z / contact_vector.distance * pull_distance;
  } else {
    pull_distance = 0.0F;
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
  return pull_distance;
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
  auto const* attack =
      frame.world.try_get<Engine::Core::AttackComponent>(frame.entity.get_id());
  float const weapon_reach =
      attack != nullptr ? attack->melee_range : frame.layout.spacing * 0.65F;
  // Reach is judged from where the soldier stands after stepping toward his foe.
  float const stepped = press_toward_target(
      frame, slot, opponent, opponent_layout, retained, rank, directive);
  bool const opponent_within_reach =
      opponent_is_structure
          ? rank.ranked
          : directive.engagement_surface_gap - stepped <= weapon_reach;
  directive.combat_role =
      brawls_as_a_crowd(frame.world, frame.entity.get_id())
          ? crowd_brawl_role(slot.original.index)
      : opponent_within_reach
          ? combat_role_for(
                frame.layout.seed, slot.original.index, true, frame.combat_motion_time)
          : Engine::Core::FormationSoldierCombatRole::Ready;
  directive.action = action_for_role(directive.combat_role);
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
  directive.combat_role = brawls_as_a_crowd(frame.world, frame.entity.get_id())
                              ? crowd_brawl_role(slot.original.index)
                              : combat_role_for(frame.layout.seed,
                                                slot.original.index,
                                                false,
                                                frame.combat_motion_time);
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
  directive.local_yaw = previous->local_yaw;
  directive.crowd_offset_x = previous->crowd_offset_x;
  directive.crowd_offset_z = previous->crowd_offset_z;
  // He falls where he stood: a man still stepping toward his foe had not reached the
  // spot his slot and offsets describe.
  directive.local_x = previous->local_x;
  directive.local_z = previous->local_z;
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
  float const step = Game::Systems::planar_length(step_x, step_z);
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

auto closed_with_target(const EntityFrame& frame) -> bool {
  if (frame.in_melee_contact) {
    return true;
  }
  if (!frame.melee_ordered || frame.actor == nullptr ||
      frame.display_opponent == nullptr) {
    return false;
  }
  auto const* target = frame.world.try_get<Engine::Core::TransformComponent>(
      frame.display_opponent->get_id());
  return target != nullptr &&
         Game::Systems::planar_length(target->position.x - frame.actor->position.x,
                                      target->position.z - frame.actor->position.z) <=
             k_contact_slot_range;
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
      previous != nullptr ? Game::Systems::planar_length(
                                directive.contact_offset_x - previous->contact_offset_x,
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
       .closed_with_target = closed_with_target(frame),
       .external_reform = frame.reform != nullptr,
       .position_is_authored =
           slot.traversal_slot != nullptr && !frame.mounted && slot.surround == nullptr,
       .walking_to_work_posts = works_a_site(frame),
       .worker = frame.unit.spawn_type == Game::Units::SpawnType::Builder,
       .solid_bodies = frame.solid_bodies,
       .props = frame.props,
       .passability = frame.passability,
       .delta_time = frame.delta_time},
      previous,
      directive);
}

auto arc_along(const Engine::Core::WallWalkSegment* chain,
               std::size_t links,
               float x,
               float z,
               float y) -> std::optional<float> {
  constexpr float k_on_chain = 0.3F;
  float travelled = 0.0F;
  float best_d2 = k_on_chain * k_on_chain;
  std::optional<float> arc;
  for (std::size_t i = 0; i < links; ++i) {
    auto const& link = chain[i];
    float const dx = link.bx - link.ax;
    float const dz = link.bz - link.az;
    float const dy = link.by - link.ay;
    float const length = std::sqrt(dx * dx + dz * dz + dy * dy);
    float t = 0.0F;
    if (length > 1.0e-4F) {
      t = std::clamp(((x - link.ax) * dx + (z - link.az) * dz + (y - link.ay) * dy) /
                         (length * length),
                     0.0F,
                     1.0F);
    }
    float const px = link.ax + dx * t - x;
    float const pz = link.az + dz * t - z;
    float const py = link.ay + dy * t - y;
    float const d2 = px * px + pz * pz + py * py;
    if (d2 <= best_d2) {
      best_d2 = d2;
      arc = travelled + length * t;
    }
    travelled += length;
  }
  return arc;
}

auto room_ahead(const Engine::Core::WallWalkSegment* chain,
                std::size_t links,
                float my_arc,
                std::uint16_t my_index,
                const std::vector<Soldier>& others) -> float {
  constexpr float k_file_gap = 0.75F;
  float room = std::numeric_limits<float>::max();
  for (auto const& other : others) {

    if (other.slot_index == my_index || !other.alive || !other.world_motion_valid ||
        std::abs(other.elevation - Game::Systems::WallWalk::k_deck_height) < 1.0e-4F ||
        other.elevation <= 5.0e-4F) {
      continue;
    }
    auto const arc =
        arc_along(chain, links, other.world_x, other.world_z, other.elevation);
    if (!arc.has_value()) {
      continue;
    }
    bool const ahead = *arc > my_arc || (*arc == my_arc && other.slot_index < my_index);
    if (ahead) {
      room = std::min(room, *arc - my_arc - k_file_gap);
    }
  }
  return std::max(0.0F, room);
}

void pin_to_wall_walk(const EntityFrame& frame,
                      const SlotContext& slot,
                      const std::vector<Soldier>& others,
                      Soldier& directive) {
  auto const* walker = frame.wall_walker;
  directive.climbing = false;
  if (walker == nullptr ||
      walker->phase == Engine::Core::WallWalkerComponent::Phase::Approaching ||
      walker->path.empty()) {
    directive.elevation = 0.0F;
    return;
  }
  if (!directive.alive) {

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
  constexpr float k_deck_band = 0.004F;
  namespace WW = Game::Systems::WallWalk;
  auto const* previous = slot.previous;
  bool const boarding =
      walker->phase == Engine::Core::WallWalkerComponent::Phase::Boarding;
  bool const tower_climb = boarding && walker->tower_id != 0;
  float const target_x = directive.world_x;
  float const target_z = directive.world_z;
  directive.climbing = false;

  std::array<Engine::Core::WallWalkSegment, 4> chain{};
  std::size_t links = 0;
  if (tower_climb) {
    chain[links++] = {walker->base_x,
                      walker->base_z,
                      0.0F,
                      walker->door_x,
                      walker->door_z,
                      walker->door_y};
  }
  chain[links++] = {walker->door_x,
                    walker->door_z,
                    walker->door_y,
                    walker->lip_x,
                    walker->lip_z,
                    walker->door_y};
  chain[links++] = {walker->lip_x,
                    walker->lip_z,
                    walker->door_y,
                    walker->crest_x,
                    walker->crest_z,
                    WW::k_crest_height};
  chain[links++] = {walker->crest_x,
                    walker->crest_z,
                    WW::k_crest_height,
                    walker->landing_x,
                    walker->landing_z,
                    WW::k_deck_height};
  WW::PathPoint const start{chain[0].ax, chain[0].az, chain[0].ay, true};

  auto const index = static_cast<float>(slot.original.index);
  bool const fresh = previous == nullptr || !previous->world_motion_valid;
  bool const released =
      !boarding || walker->boarding_seconds >=
                       index * Engine::Core::WallWalkerComponent::k_file_out_interval;
  float const chain_step_time = std::max(frame.delta_time, 1.0e-4F);
  auto settle = [&](float x, float z, float y) {
    if (!fresh) {
      directive.world_velocity_x = (x - previous->world_x) / chain_step_time;
      directive.world_velocity_z = (z - previous->world_z) / chain_step_time;
    } else {
      directive.world_velocity_x = 0.0F;
      directive.world_velocity_z = 0.0F;
    }
    directive.world_x = x;
    directive.world_z = z;
    directive.elevation = y;
    directive.world_motion_valid = true;
  };

  bool const coming_down =
      (walker->phase == Engine::Core::WallWalkerComponent::Phase::Descending ||
       walker->phase == Engine::Core::WallWalkerComponent::Phase::Leaving) &&
      !walker->exit_chain.empty() && !fresh && previous->elevation > 0.0005F;
  if (coming_down) {

    auto const& down = walker->exit_chain;
    auto const& top = down.front();
    float const to_top = Game::Systems::planar_length(top.ax - previous->world_x,
                                                      top.az - previous->world_z);
    bool const on_planks = std::abs(previous->elevation - WW::k_deck_height) < 1.0e-4F;
    if (on_planks && to_top > 0.12F) {
      float allowed = k_wall_run_speed * std::max(0.0F, frame.delta_time);

      if (to_top < 0.9F &&
          room_ahead(down.data(), down.size(), -to_top, slot.original.index, others) <
              to_top) {
        allowed = 0.0F;
      }
      float const t = std::min(1.0F, allowed / to_top);
      float const x = previous->world_x + (top.ax - previous->world_x) * t;
      float const z = previous->world_z + (top.az - previous->world_z) * t;
      settle(x, z, WW::k_deck_height);
      directive.world_yaw = Game::Systems::yaw_degrees_from_direction(
          top.ax - previous->world_x, top.az - previous->world_z);
      return;
    }
    float travelled = 0.0F;
    float best_d2 = std::numeric_limits<float>::max();
    float arc = 0.0F;
    std::size_t on_link = 0;
    for (std::size_t i = 0; i < down.size(); ++i) {
      auto const& link = down[i];
      float const dx = link.bx - link.ax;
      float const dz = link.bz - link.az;
      float const dy = link.by - link.ay;
      float const length = std::sqrt(dx * dx + dz * dz + dy * dy);
      float t = 0.0F;
      if (length > 1.0e-4F) {
        t = std::clamp(((previous->world_x - link.ax) * dx +
                        (previous->world_z - link.az) * dz +
                        (previous->elevation - link.ay) * dy) /
                           (length * length),
                       0.0F,
                       1.0F);
      }
      float const px = link.ax + dx * t - previous->world_x;
      float const pz = link.az + dz * t - previous->world_z;
      float const py = link.ay + dy * t - previous->elevation;
      float const d2 = px * px + pz * pz + py * py;
      if (d2 < best_d2) {
        best_d2 = d2;
        arc = travelled + length * t;
        on_link = i;
      }
      travelled += length;
    }
    bool const ladder = WW::segment_is_steep(down[on_link]);
    float const speed = ladder ? WW::k_ladder_climb_speed : 1.4F;
    float const room =
        room_ahead(down.data(), down.size(), arc, slot.original.index, others);
    arc = std::min(
        {travelled, arc + speed * std::max(0.0F, frame.delta_time), arc + room});
    float remaining = arc;
    for (std::size_t i = 0; i < down.size(); ++i) {
      auto const& link = down[i];
      float const dx = link.bx - link.ax;
      float const dz = link.bz - link.az;
      float const dy = link.by - link.ay;
      float const length = std::sqrt(dx * dx + dz * dz + dy * dy);
      if (remaining <= length || i + 1 == down.size()) {
        float const t =
            length > 1.0e-4F ? std::clamp(remaining / length, 0.0F, 1.0F) : 1.0F;
        float y = link.ay + dy * t;
        if (std::abs(y - WW::k_deck_height) < 1.0e-3F) {

          y = WW::k_deck_height - 1.0e-3F;
        }
        bool const steep = WW::segment_is_steep(link);
        settle(link.ax + dx * t, link.az + dz * t, std::max(y, 0.0F));
        directive.climbing = steep && y > 0.0F;
        directive.world_yaw = steep
                                  ? Game::Systems::yaw_degrees_from_direction(-dx, -dz)
                                  : Game::Systems::yaw_degrees_from_direction(dx, dz);
        return;
      }
      remaining -= length;
    }
  }
  if (walker->phase == Engine::Core::WallWalkerComponent::Phase::Leaving) {

    directive.elevation = 0.0F;
    return;
  }

  if (boarding) {

    bool const on_chain = !fresh && std::abs(previous->elevation - WW::k_deck_height) >
                                        k_deck_band * 0.5F;
    if (!released || (fresh && !tower_climb)) {

      if (tower_climb && !fresh) {
        settle(previous->world_x, previous->world_z, previous->elevation);
      } else {
        settle(start.x + 0.16F * std::sin(index * 2.39F),
               start.z + 0.16F * std::cos(index * 2.39F),
               start.y);
      }
      return;
    }
    if (tower_climb && (fresh || previous->elevation <= 0.0F)) {

      float const from_x = fresh ? start.x : previous->world_x;
      float const from_z = fresh ? start.z : previous->world_z;
      float const dx = start.x - from_x;
      float const dz = start.z - from_z;
      float const gap = Game::Systems::planar_length(dx, dz);
      float const allowed = k_wall_run_speed * std::max(0.0F, frame.delta_time);
      if (gap > allowed) {
        settle(from_x + dx / gap * allowed, from_z + dz / gap * allowed, 0.0F);
        directive.world_yaw = Game::Systems::yaw_degrees_from_direction(dx, dz);
        return;
      }
      if (room_ahead(chain.data(), links, 0.0F, slot.original.index, others) <= 0.0F) {

        settle(start.x, start.z, 0.0F);
        return;
      }
      settle(start.x, start.z, 0.001F);
      directive.climbing = true;
      directive.world_yaw = Game::Systems::yaw_degrees_from_direction(
          walker->lip_x - walker->base_x, walker->lip_z - walker->base_z);
      return;
    }
    if (on_chain) {

      float travelled = 0.0F;
      float best_d2 = std::numeric_limits<float>::max();
      float arc = 0.0F;
      std::size_t on_link = 0;
      for (std::size_t i = 0; i < links; ++i) {
        auto const& link = chain[i];
        float const dx = link.bx - link.ax;
        float const dz = link.bz - link.az;
        float const dy = link.by - link.ay;
        float const length = std::sqrt(dx * dx + dz * dz + dy * dy);
        float t = 0.0F;
        if (length > 1.0e-4F) {
          t = std::clamp(((previous->world_x - link.ax) * dx +
                          (previous->world_z - link.az) * dz +
                          (previous->elevation - link.ay) * dy) /
                             (length * length),
                         0.0F,
                         1.0F);
        }
        float const px = link.ax + dx * t - previous->world_x;
        float const pz = link.az + dz * t - previous->world_z;
        float const py = link.ay + dy * t - previous->elevation;
        float const d2 = px * px + pz * pz + py * py;
        if (d2 < best_d2) {
          best_d2 = d2;
          arc = travelled + length * t;
          on_link = i;
        }
        travelled += length;
      }
      bool const climbing_link = WW::segment_is_steep(chain[on_link]);
      float const speed = climbing_link ? WW::k_tower_climb_speed : k_bridge_speed;
      float const room =
          room_ahead(chain.data(), links, arc, slot.original.index, others);
      arc = std::min(
          {travelled, arc + speed * std::max(0.0F, frame.delta_time), arc + room});
      float remaining = arc;
      for (std::size_t i = 0; i < links; ++i) {
        auto const& link = chain[i];
        float const dx = link.bx - link.ax;
        float const dz = link.bz - link.az;
        float const dy = link.by - link.ay;
        float const length = std::sqrt(dx * dx + dz * dz + dy * dy);
        bool const last = i + 1 == links;
        if (remaining <= length || last) {
          float const t =
              length > 1.0e-4F ? std::clamp(remaining / length, 0.0F, 1.0F) : 1.0F;
          float y = link.ay + dy * t;
          bool const landed = last && t >= 1.0F;
          if (landed) {
            y = WW::k_deck_height;
          } else if (std::abs(y - WW::k_deck_height) <= k_deck_band) {
            y = WW::k_deck_height + (dy >= 0.0F ? k_deck_band : -k_deck_band) * 1.5F;
          }
          settle(link.ax + dx * t, link.az + dz * t, y);
          bool const steep = WW::segment_is_steep(link);
          directive.climbing = steep && !landed;
          directive.world_yaw =
              steep
                  ? Game::Systems::yaw_degrees_from_direction(
                        walker->lip_x - walker->base_x, walker->lip_z - walker->base_z)
                  : Game::Systems::yaw_degrees_from_direction(dx, dz);
          return;
        }
        remaining -= length;
      }
    }
  }

  float const from_x = fresh ? target_x : previous->world_x;
  float const from_z = fresh ? target_z : previous->world_z;
  float const dx = target_x - from_x;
  float const dz = target_z - from_z;
  float const want = Game::Systems::planar_length(dx, dz);
  float const step_time = std::max(0.0F, frame.delta_time);
  float const allowed = k_wall_run_speed * step_time;
  float x = target_x;
  float z = target_z;
  if (want > allowed && want > 1.0e-4F) {
    x = from_x + dx / want * allowed;
    z = from_z + dz / want * allowed;
  }

  std::vector<Engine::Core::WallWalkSegment> deck_only;
  auto const* walk = &walker->path;
  if (boarding && walker->path.size() > links) {
    deck_only.assign(walker->path.begin(),
                     walker->path.end() - static_cast<std::ptrdiff_t>(links));
    walk = &deck_only;
  }
  auto const point = Game::Systems::WallWalk::project_onto_path(*walk, x, z);
  if (!point.valid) {
    directive.elevation = 0.0F;
    return;
  }
  directive.world_x = point.x;
  directive.world_z = point.z;
  directive.elevation = point.y;
  directive.world_motion_valid = true;
  if (point.steep) {

    directive.climbing = true;
    if (std::abs(point.face_x) + std::abs(point.face_z) > 1.0e-4F) {
      directive.world_yaw =
          Game::Systems::yaw_degrees_from_direction(point.face_x, point.face_z);
    }
  }
  bool const settled =
      Game::Systems::planar_length(point.x - from_x, point.z - from_z) < 0.01F;
  if (walker->watching && settled && directive.opponent_id == 0U) {
    directive.world_yaw = walker->watch_yaw;
  }
  if (step_time > 0.0F && !fresh) {
    directive.world_velocity_x = (point.x - from_x) / step_time;
    directive.world_velocity_z = (point.z - from_z) / step_time;

    directive.angular_speed =
        std::abs(signed_yaw_delta(previous->world_yaw, directive.world_yaw)) /
        step_time;
  }
}

void pin_to_raft(const EntityFrame& frame,
                 const SlotContext& slot,
                 Soldier& directive) {
  auto const* rider = frame.raft_rider;

  if (rider == nullptr || frame.actor == nullptr || !directive.alive) {
    return;
  }
  auto const fit = Game::Systems::raft_deck_fit(frame.layout);
  directive.local_x = fit.fit_x(slot.anchor_local_x);
  directive.local_z = fit.fit_z(slot.anchor_local_z);
  QVector3D const deck =
      local_to_world(*frame.actor, directive.local_x, directive.local_z);
  float const step_time = std::max(0.0F, frame.delta_time);
  auto const* previous = slot.previous;
  bool const fresh = previous == nullptr || !previous->world_motion_valid;
  directive.world_velocity_x =
      !fresh && step_time > 0.0F ? (deck.x() - previous->world_x) / step_time : 0.0F;
  directive.world_velocity_z =
      !fresh && step_time > 0.0F ? (deck.z() - previous->world_z) / step_time : 0.0F;
  directive.world_x = deck.x();
  directive.world_z = deck.z();
  directive.world_yaw = frame.actor->rotation.y + directive.local_yaw;
  directive.world_motion_valid = true;
  directive.climbing = false;
  directive.carried = rider->afloat;
  auto const* terrain = Game::Session::services_for(frame.world).terrain;
  float const ground =
      terrain != nullptr
          ? terrain->resolve_surface_world_y(deck.x(), deck.z(), 0.0F, rider->deck_y)
          : rider->deck_y;
  directive.elevation = std::max(0.0F, rider->deck_y - ground);
}

void take_surround_spot(const SlotContext& slot, Soldier& directive) {
  if (slot.surround == nullptr || !directive.alive) {
    return;
  }
  directive.local_x = slot.surround->local_x;
  directive.local_z = slot.surround->local_z;
}

void wait_in_the_outer_rings(const EntityFrame& frame,
                             const SlotContext& slot,
                             Soldier& directive) {
  if (slot.surround == nullptr || !directive.alive || frame.actor == nullptr ||
      frame.display_opponent == nullptr) {
    return;
  }
  auto const* opponent_transform =
      frame.world.try_get<Engine::Core::TransformComponent>(
          frame.display_opponent->get_id());
  if (opponent_transform == nullptr) {
    return;
  }
  auto const toward = local_contact_vector(*frame.actor,
                                           directive.local_x,
                                           directive.local_z,
                                           opponent_transform->position.x,
                                           opponent_transform->position.z);
  float const prior_yaw =
      slot.previous != nullptr ? slot.previous->local_yaw : directive.local_yaw;
  if (toward.distance > 0.0001F) {
    directive.local_yaw =
        turn_yaw_toward(prior_yaw,
                        toward.yaw,
                        k_contact_turn_degrees * std::max(0.0F, frame.delta_time));
  }
  if (slot.surround->ring == 0U ||
      brawls_as_a_crowd(frame.world, frame.entity.get_id())) {
    return;
  }
  directive.combat_role = Engine::Core::FormationSoldierCombatRole::Ready;
  directive.action = action_for_role(directive.combat_role);
}

auto build_slot_directive(const EntityFrame& frame,
                          SlotContext& slot,
                          const std::vector<Soldier>& previous_soldiers) -> Soldier {
  Soldier directive = seed_directive(frame, slot);
  take_surround_spot(slot, directive);
  slot.anchor_local_x = directive.local_x;
  slot.anchor_local_z = directive.local_z;
  slot.assignment =
      directive.alive
          ? assignment_for_slot(frame.contact, slot.previous, slot.original.index)
          : SoldierAssignment{};

  apply_engagement(frame, slot, directive);
  hold_unassigned_yaw(frame, slot, directive);
  wait_in_the_outer_rings(frame, slot, directive);
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
  keep_walk_off_facade(frame, directive);
  pin_to_wall_walk(frame, slot, previous_soldiers, directive);
  pin_to_raft(frame, slot, directive);
  return directive;
}

} // namespace

auto publish_soldiers(const EntityFrame& frame) -> bool {
  auto& directives = frame.presentation.soldiers;
  auto const& layout = frame.layout;

  thread_local std::vector<Soldier> previous_soldiers;
  previous_soldiers.assign(directives.begin(), directives.end());
  std::size_t const previous_directive_count = directives.size();
  bool soldiers_changed = previous_directive_count != layout.all_slots.size();
  directives.resize(layout.all_slots.size());
  for (auto const& original_slot : layout.all_slots) {
    SlotContext slot{
        .original = original_slot,
        .live = find_live_slot(layout, original_slot.index),
        .previous = original_slot.index < previous_directive_count
                        ? &previous_soldiers[original_slot.index]
                        : nullptr,
        .traversal_slot = frame.traversal != nullptr
                              ? frame.traversal->slot_for(original_slot.index)
                              : nullptr,
    };
    if (frame.surround != nullptr && original_slot.index < frame.surround->size() &&
        (*frame.surround)[original_slot.index].active) {
      slot.surround = &(*frame.surround)[original_slot.index];
    }
    Soldier const directive = build_slot_directive(frame, slot, previous_soldiers);
    soldiers_changed =
        soldiers_changed || slot.previous == nullptr || *slot.previous != directive;
    directives[original_slot.index] = directive;
  }
  return soldiers_changed;
}

} // namespace Game::Systems::Combat

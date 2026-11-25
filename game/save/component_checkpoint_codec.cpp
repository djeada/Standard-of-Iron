#include <QJsonArray>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../util/checkpoint_values.h"
#include "entity_codec.h"

namespace Engine::Core::EntityCodec {
namespace {

using Game::CheckpointValues::read;
using Game::CheckpointValues::read_tuple;
using Game::CheckpointValues::write;
using Game::CheckpointValues::write_tuple;

template <typename Component>
auto fields(Component& component) {
  using Type = std::remove_const_t<Component>;
  if constexpr (std::is_same_v<Type, CommanderComponent>) {
    return std::tie(component.signature_move,
                    component.signature_cooldown,
                    component.signature_cooldown_remaining,
                    component.signature_damage_multiplier,
                    component.signature_bonus_reach,
                    component.signature_stagger_seconds,
                    component.signature_max_targets,
                    component.signature_strike_active,
                    component.advanced_combat_enabled,
                    component.combo_step,
                    component.combo_action_id,
                    component.combo_window_remaining,
                    component.power_strike_active,
                    component.shield_bash_cooldown_remaining,
                    component.vanguard_rush_cooldown_remaining,
                    component.second_wind_cooldown_remaining,
                    component.just_struck_enemy,
                    component.last_strike_combo_step,
                    component.jump_active,
                    component.jump_phase,
                    component.jump_height_offset,
                    component.airborne_velocity,
                    component.dodge_active,
                    component.dodge_phase,
                    component.dodge_dir_x,
                    component.dodge_dir_z,
                    component.dive_attack_active,
                    component.fpv_motion_vx,
                    component.fpv_motion_vz,
                    component.fpv_motion_requested,
                    component.posture,
                    component.posture_max,
                    component.punish_window_remaining,
                    component.riposte_window_remaining,
                    component.riposte_target_id,
                    component.close_camera_mode);
  } else if constexpr (std::is_same_v<Type, CommanderBodyControlComponent>) {
    return std::tie(component.steered_intent.windup_dir_x,
                    component.steered_intent.windup_dir_y,
                    component.steered_intent.strike_dir_x,
                    component.steered_intent.strike_dir_y,
                    component.steered_intent.thrust_amount,
                    component.steered_intent.elevation,
                    component.steered_intent.charge,
                    component.steered_intent.swing_speed,
                    component.steered_intent.follow_through,
                    component.steered_intent.weapon_target_x,
                    component.steered_intent.weapon_target_y,
                    component.steered_intent.weapon_target_z,
                    component.steered_intent.blade_dir_x,
                    component.steered_intent.blade_dir_y,
                    component.steered_intent.blade_dir_z,
                    component.steer_x,
                    component.steer_y,
                    component.steer_rate,
                    component.swing_in_flight,
                    component.rest_dir_x,
                    component.rest_dir_y,
                    component.rest_valid,
                    component.last_strike_dir_x,
                    component.last_strike_dir_y,
                    component.chain_window_remaining);
  } else if constexpr (std::is_same_v<Type, CommanderGuardComponent>) {
    return std::tie(component.guard_dir_x,
                    component.guard_dir_y,
                    component.guard_turn_rate,
                    component.active,
                    component.frontal_arc_dot,
                    component.damage_multiplier,
                    component.perfect_guard_remaining,
                    component.guard_break_remaining,
                    component.rearm_requires_release);
  } else if constexpr (std::is_same_v<Type, RpgHealthComponent>) {
    return std::tie(component.active,
                    component.dodge_grace_remaining,
                    component.dodge_dir_x,
                    component.dodge_dir_z,
                    component.blocked_contacts,
                    component.perfect_guard_contacts,
                    component.dodged_contacts,
                    component.damaging_contacts,
                    component.guard_broken_contacts,
                    component.last_contact_outcome);
  } else if constexpr (std::is_same_v<Type, RpgCommanderAimComponent>) {
    return std::tie(component.view_yaw_degrees,
                    component.view_pitch_degrees,
                    component.eye_height,
                    component.move_speed,
                    component.running,
                    component.camera_origin_x,
                    component.camera_origin_y,
                    component.camera_origin_z,
                    component.camera_origin_valid,
                    component.camera_forward_x,
                    component.camera_forward_y,
                    component.camera_forward_z,
                    component.camera_forward_valid,
                    component.camera_fov_degrees,
                    component.stance,
                    component.stance_resolved,
                    component.draw_held,
                    component.draw_stage,
                    component.draw_progress,
                    component.full_draw_hold,
                    component.spread_degrees,
                    component.shot_power,
                    component.relaxed_from_overhold,
                    component.shot_sequence);
  } else if constexpr (std::is_same_v<Type, RpgCommanderTargetComponent>) {
    return std::tie(component.explicit_lock_target_id,
                    component.explicit_lock_soldier_slot,
                    component.aim_candidate_id,
                    component.aim_candidate_soldier_slot,
                    component.aim_candidate_in_range,
                    component.recent_hit_target_id,
                    component.recent_hit_soldier_slot,
                    component.recent_hit_timer,
                    component.hit_confirm_sequence,
                    component.recent_hit_killed);
  } else if constexpr (std::is_same_v<Type, EngagementSlotComponent>) {
    return std::tie(component.target_id,
                    component.slot_index,
                    component.max_slots,
                    component.anchor_offset_x,
                    component.anchor_offset_z,
                    component.valid,
                    component.lease_remaining);
  } else if constexpr (std::is_same_v<Type, TargetCommitmentComponent>) {
    return std::tie(component.committed_target_id,
                    component.cooldown_remaining,
                    component.in_committed_phase);
  } else if constexpr (std::is_same_v<Type, CohortMembershipComponent>) {
    return std::tie(component.cohort_id, component.cohort_activated);
  } else if constexpr (std::is_same_v<Type, DeathAnimationComponent>) {
    return std::tie(component.profile,
                    component.state,
                    component.state_time,
                    component.state_duration,
                    component.dead_hold_duration,
                    component.sink_duration,
                    component.sequence_variant);
  } else if constexpr (std::is_same_v<Type, BloodStainComponent>) {
    return std::tie(component.radius,
                    component.elapsed_time,
                    component.lifetime,
                    component.rotation,
                    component.aspect_ratio,
                    component.seed);
  } else if constexpr (std::is_same_v<Type, CombatLaunchComponent>) {
    return std::tie(component.velocity_y,
                    component.velocity_x,
                    component.velocity_z,
                    component.ground_y);
  } else if constexpr (std::is_same_v<Type, AttackComponent>) {
    return std::tie(component.min_range,
                    component.melee_lock_separation_time,
                    component.melee_footwork_offset);
  } else if constexpr (std::is_same_v<Type, CombatStateComponent>) {
    return std::tie(component.intent.windup_dir_x,
                    component.intent.windup_dir_y,
                    component.intent.strike_dir_x,
                    component.intent.strike_dir_y,
                    component.intent.thrust_amount,
                    component.intent.elevation,
                    component.intent.charge,
                    component.intent.swing_speed,
                    component.intent.follow_through,
                    component.intent.weapon_target_x,
                    component.intent.weapon_target_y,
                    component.intent.weapon_target_z,
                    component.intent.blade_dir_x,
                    component.intent.blade_dir_y,
                    component.intent.blade_dir_z,
                    component.damage_dealt_this_swing,
                    component.tired_swing,
                    component.input_buffered,
                    component.telegraph_intensity,
                    component.telegraph_cue);
  } else if constexpr (std::is_same_v<Type, StaminaComponent>) {
    return std::tie(component.regen_delay_remaining);
  } else if constexpr (std::is_same_v<Type, WildlifeComponent>) {
    return std::tie(component.flinch_timer,
                    component.held_timer,
                    component.dazed_timer,
                    component.watched_health,
                    component.stall_timer,
                    component.last_x,
                    component.last_z,
                    component.stalled,
                    component.orbit);
  } else if constexpr (std::is_same_v<Type, ForestCoverComponent>) {
    return std::tie(component.in_forest, component.concealed, component.seen_by);
  } else if constexpr (std::is_same_v<Type, BuilderProductionComponent>) {
    return std::tie(component.site_approach_seconds,
                    component.site_closest_approach,
                    component.site_settle_seconds,
                    component.work_animation_seconds,
                    component.has_site_approach,
                    component.site_approach_x,
                    component.site_approach_z,
                    component.fault,
                    component.fault_display_remaining);
  } else if constexpr (std::is_same_v<Type, CaptureComponent>) {
    return std::tie(component.capture_blocked);
  } else if constexpr (std::is_same_v<Type, WallSegmentComponent>) {
    return std::tie(component.connection_mask,
                    component.inner_x,
                    component.inner_z,
                    component.has_stair,
                    component.ladder,
                    component.ladder_id);
  } else if constexpr (std::is_same_v<Type, ConstructionPreviewComponent>) {
    return std::tie(component.owner_id,
                    component.nation_id,
                    component.grid_x,
                    component.grid_z,
                    component.valid,
                    component.site_ghost,
                    component.progress);
  } else if constexpr (std::is_same_v<Type, UnitComponent>) {
    return std::tie(component.formation_files_override);
  } else if constexpr (std::is_same_v<Type, UnitLayoutStateComponent>) {
    return std::tie(component.transition_seconds, component.previous_layout_id);
  } else if constexpr (std::is_same_v<Type, UnitTraversalLayoutStateComponent>) {
    return std::tie(component.route_id,
                    component.portal_id,
                    component.normal_layout_id,
                    component.mode,
                    component.target_mode,
                    component.normal_files,
                    component.current_files,
                    component.target_files,
                    component.slot_states_revision,
                    component.about_faced,
                    component.about_face_cooldown_seconds,
                    component.entry_progress,
                    component.exit_progress,
                    component.transition_progress,
                    component.transition_curve,
                    component.transition_seconds,
                    component.transition_total_distance,
                    component.transition_remaining_distance,
                    component.mode_dwell_seconds,
                    component.tail_clear_seconds,
                    component.blocked_slot_count,
                    component.lateral_scale,
                    component.target_lateral_scale,
                    component.available_half_width,
                    component.desired_half_width,
                    component.constriction_distance,
                    component.soldier_body_radius,
                    component.file_spacing,
                    component.rank_spacing,
                    component.authored_file_spacing,
                    component.minimum_lateral_scale,
                    component.active);
  } else if constexpr (std::is_same_v<Type, UnitTraversalSlotState>) {
    return std::tie(component.slot_index,
                    component.row,
                    component.col,
                    component.start_local_x,
                    component.start_local_z,
                    component.previous_local_x,
                    component.previous_local_z,
                    component.current_local_x,
                    component.current_local_z,
                    component.target_local_x,
                    component.target_local_z,
                    component.velocity_x,
                    component.velocity_z,
                    component.blocked_seconds,
                    component.alive,
                    component.blocked);
  } else if constexpr (std::is_same_v<Type, FormationContactComponent>) {
    return std::tie(component.target_id,
                    component.surface_gap,
                    component.in_contact,
                    component.revision);
  } else if constexpr (std::is_same_v<Type, FormationContactFront>) {
    return std::tie(component.opponent_id,
                    component.surface_gap,
                    component.in_contact,
                    component.outgoing);
  } else if constexpr (std::is_same_v<Type, FormationEngagementPair>) {
    return std::tie(component.attacker_slot,
                    component.target_slot,
                    component.root_distance,
                    component.surface_gap);
  } else if constexpr (std::is_same_v<Type, FormationPresentationComponent>) {
    return std::tie(component.formation_seed,
                    component.rows,
                    component.cols,
                    component.spacing,
                    component.target_id,
                    component.target_alive,
                    component.melee_ordered,
                    component.allow_full_body_hit_reaction,
                    component.combat_motion_time,
                    component.motion_root_x,
                    component.motion_root_z,
                    component.motion_root_yaw,
                    component.motion_root_valid,
                    component.revision);
  } else if constexpr (std::is_same_v<Type, FormationSoldierPresentation>) {
    return std::tie(component.slot_index,
                    component.row,
                    component.col,
                    component.local_x,
                    component.local_z,
                    component.previous_local_x,
                    component.previous_local_z,
                    component.relocation_velocity_x,
                    component.relocation_velocity_z,
                    component.relocation_blocked,
                    component.local_yaw,
                    component.contact_offset_x,
                    component.contact_offset_z,
                    component.target_held_seconds,
                    component.alive,
                    component.action,
                    component.combat_role,
                    component.opponent_id,
                    component.target_slot,
                    component.engagement_surface_gap,
                    component.combat_phase_bias,
                    component.combat_speed_scale,
                    component.damage_carrier,
                    component.unassigned_seconds,
                    component.world_x,
                    component.world_z,
                    component.world_yaw,
                    component.world_velocity_x,
                    component.world_velocity_z,
                    component.angular_speed,
                    component.turn_response_remaining,
                    component.world_motion_valid,
                    component.turning,
                    component.reforming,
                    component.carried,
                    component.crowd_offset_x,
                    component.crowd_offset_z,
                    component.elevation,
                    component.climbing,
                    component.stranded_seconds);
  } else if constexpr (std::is_same_v<Type, FormationRosterPresentationComponent>) {
    return std::tie(component.total_count, component.live_count, component.revision);
  } else if constexpr (std::is_same_v<Type, SoldierCasualtyAnimationComponent::Entry>) {
    return std::tie(component.slot_index,
                    component.has_local_anchor,
                    component.local_x,
                    component.local_z,
                    component.local_yaw,
                    component.profile,
                    component.state,
                    component.state_time,
                    component.state_duration,
                    component.dead_hold_duration,
                    component.sink_duration,
                    component.sequence_variant,
                    component.launched,
                    component.launch_velocity_x,
                    component.launch_velocity_y,
                    component.launch_velocity_z,
                    component.launch_pitch_speed,
                    component.launch_roll_speed);
  } else if constexpr (std::is_same_v<Type, SquadReformComponent>) {
    return std::tie(component.remaining_seconds);
  } else if constexpr (std::is_same_v<Type, SquadReformSoldier>) {
    return std::tie(component.slot_index, component.world_x, component.world_z);
  } else if constexpr (std::is_same_v<Type, RpgEngagementComponent>) {
    return std::tie(
        component.ring_radius, component.pressure_clock, component.fight_context);
  } else if constexpr (std::is_same_v<Type, RpgEngagementComponent::Slot>) {
    return std::tie(component.entity_id,
                    component.distance,
                    component.signed_angle_degrees,
                    component.obstructed,
                    component.pressing);
  } else if constexpr (std::is_same_v<Type, RpgEngagementComponent::PressTenure>) {
    return std::tie(component.entity_id, component.started_at);
  } else if constexpr (std::is_same_v<Type, ThreatAlertComponent>) {
    return std::tie(component.aggressor_id, component.kind, component.cooldown);
  } else if constexpr (std::is_same_v<Type, FormationHitPresentationComponent>) {
    return std::tie(component.attacker_id,
                    component.soldier_slot,
                    component.remaining,
                    component.duration,
                    component.intensity,
                    component.reaction_kind,
                    component.hit_direction_x,
                    component.hit_direction_z,
                    component.revision);
  } else if constexpr (std::is_same_v<Type, MotionPresentationComponent>) {
    return std::tie(component.snapshot_valid,
                    component.initialized,
                    component.classified,
                    component.previous_x,
                    component.previous_y,
                    component.previous_z,
                    component.previous_rotation_y,
                    component.displacement_x,
                    component.displacement_z,
                    component.velocity_x,
                    component.velocity_z,
                    component.speed,
                    component.direction_x,
                    component.direction_z,
                    component.has_velocity,
                    component.has_navigation_intent,
                    component.has_chase_intent,
                    component.attack_target_in_range,
                    component.has_movement_target,
                    component.movement_target_x,
                    component.movement_target_z,
                    component.source,
                    component.state,
                    component.previous_state,
                    component.state_changed,
                    component.state_time,
                    component.seconds_since_motion,
                    component.stalled_seconds,
                    component.tick_delta_time);
  } else if constexpr (std::is_same_v<Type, AssaultWaveComponent>) {
    return std::tie(component.active,
                    component.wave_phase,
                    component.has_march_target,
                    component.march_target_x,
                    component.march_target_z);
  }
}

template <typename T>
void write_component(const Entity& entity, QJsonObject& out, const char* key) {
  if (const auto* component = entity.registry()->try_get<const T>(entity.get_id())) {
    out[QLatin1String(key)] = write_tuple(fields(*component));
  }
}

template <typename T>
auto read_component(Entity& entity, const QJsonObject& in, const char* key) -> bool {
  const auto value = in.value(QLatin1String(key));
  if (value.isUndefined()) {
    return true;
  }
  auto* component = get_or_add_component<T>(&entity);
  return component != nullptr && read_tuple(value, fields(*component));
}

template <typename Intent>
auto swing_fields(Intent& swing) {
  return std::tie(swing.windup_dir_x,
                  swing.windup_dir_y,
                  swing.strike_dir_x,
                  swing.strike_dir_y,
                  swing.thrust_amount,
                  swing.elevation,
                  swing.charge,
                  swing.swing_speed,
                  swing.follow_through,
                  swing.weapon_target_x,
                  swing.weapon_target_y,
                  swing.weapon_target_z,
                  swing.blade_dir_x,
                  swing.blade_dir_y,
                  swing.blade_dir_z);
}

template <typename Action>
auto action_fields(Action& action) {
  return std::tie(action.phase,
                  action.combat_action_id,
                  action.melee_attack_sequence,
                  action.exchange_outcome,
                  action.active_target_id,
                  action.active_target_soldier_slot,
                  action.last_hit_target_id,
                  action.last_hit_soldier_slot,
                  action.hit_target_count,
                  action.last_damage,
                  action.requested_damage,
                  action.last_contact_speed,
                  action.action_elapsed_time,
                  action.action_duration,
                  action.previous_normalized_action_time,
                  action.normalized_action_time,
                  action.next_event_index,
                  action.last_event_type,
                  action.last_event_valid,
                  action.action_active,
                  action.weapon_trace_active,
                  action.action_running,
                  action.action_completed);
}

void write_active_action(const Entity& entity, QJsonObject& out) {
  if (const auto* action =
          entity.registry()->try_get<const RpgCommanderActionComponent>(
              entity.get_id())) {
    QJsonArray hits;
    for (std::size_t index = 0; index < action->hit_target_ids.size(); ++index) {
      hits.append(write(action->hit_target_ids[index],
                        action->hit_target_soldier_slots[index]));
    }
    out["commander_action"] =
        QJsonObject{{"state", write_tuple(action_fields(*action))}, {"hits", hits}};
  }
}

auto read_active_action(Entity& entity, const QJsonObject& in) -> bool {
  if (!in.contains("commander_action")) {
    return true;
  }
  const auto value = in.value("commander_action").toObject();
  const auto hits = value.value("hits").toArray();
  auto* action = get_or_add_component<RpgCommanderActionComponent>(&entity);
  if (action == nullptr || !read_tuple(value.value("state"), action_fields(*action)) ||
      hits.size() != static_cast<qsizetype>(action->hit_target_ids.size()) ||
      action->hit_target_count > action->hit_target_ids.size()) {
    return false;
  }
  for (std::size_t index = 0; index < action->hit_target_ids.size(); ++index) {
    if (!read(hits.at(static_cast<qsizetype>(index)),
              action->hit_target_ids[index],
              action->hit_target_soldier_slots[index])) {
      return false;
    }
  }
  return true;
}

template <typename Queue>
auto queue_fields(Queue& queue) {
  return std::tie(queue.clock,
                  queue.last_outcome,
                  queue.last_outcome_age,
                  queue.accepted_intents,
                  queue.buffered_intents,
                  queue.refused_intents,
                  queue.expired_intents,
                  queue.overflow_intents);
}

void write_intents(const Entity& entity, QJsonObject& out) {
  if (const auto* queue = entity.registry()->try_get<const CombatIntentQueueComponent>(
          entity.get_id())) {
    QJsonArray entries;
    for (std::size_t index = 0; index < queue->count; ++index) {
      const auto& intent = queue->entries[index];
      entries.append(QJsonObject{
          {"intent",
           write(
               intent.type, intent.has_swing, intent.pressed_at, intent.held_duration)},
          {"swing", write_tuple(swing_fields(intent.swing))}});
    }
    out["combat_intents"] =
        QJsonObject{{"state", write_tuple(queue_fields(*queue))}, {"entries", entries}};
  }
}

auto read_intents(Entity& entity, const QJsonObject& in) -> bool {
  if (!in.contains("combat_intents")) {
    return true;
  }
  const auto value = in.value("combat_intents").toObject();
  const auto entries = value.value("entries").toArray();
  CombatIntentQueueComponent queue;
  if (!value.value("entries").isArray() ||
      entries.size() > static_cast<qsizetype>(queue.entries.size()) ||
      !read_tuple(value.value("state"), queue_fields(queue))) {
    return false;
  }
  for (const auto record : entries) {
    const auto row = record.toObject();
    auto& intent = queue.entries[queue.count++];
    if (!read(row.value("intent"),
              intent.type,
              intent.has_swing,
              intent.pressed_at,
              intent.held_duration) ||
        !read_tuple(row.value("swing"), swing_fields(intent.swing))) {
      return false;
    }
  }
  entity.registry()->emplace<CombatIntentQueueComponent>(entity.get_id(),
                                                         std::move(queue));
  return true;
}

template <typename Entry>
auto write_list(const std::vector<Entry>& entries) -> QJsonArray {
  QJsonArray rows;
  for (const auto& entry : entries) {
    rows.append(write_tuple(fields(entry)));
  }
  return rows;
}

template <typename Entry>
auto read_list(const QJsonValue& value, std::vector<Entry>& entries) -> bool {
  if (!value.isArray()) {
    return false;
  }
  for (const auto row : value.toArray()) {
    if (!read_tuple(row, fields(entries.emplace_back()))) {
      return false;
    }
  }
  return true;
}

auto write_contact(const FormationContactComponent& contact) -> QJsonObject {
  QJsonArray fronts;
  for (const auto& front : contact.fronts) {
    fronts.append(QJsonObject{{"state", write_tuple(fields(front))},
                              {"pairs", write_list(front.engagement_pairs)}});
  }
  QJsonArray indices;
  for (const auto index : contact.engaged_soldier_indices) {
    indices.append(index);
  }
  return {{"state", write_tuple(fields(contact))},
          {"indices", indices},
          {"pairs", write_list(contact.engagement_pairs)},
          {"fronts", fronts}};
}

auto read_contact(const QJsonObject& data, FormationContactComponent& contact) -> bool {
  if (!read_tuple(data.value("state"), fields(contact)) ||
      !read_list(data.value("pairs"), contact.engagement_pairs) ||
      !data.value("indices").isArray() || !data.value("fronts").isArray()) {
    return false;
  }
  for (const auto row : data.value("indices").toArray()) {
    std::uint16_t index = 0;
    if (!Game::CheckpointValues::decode(row, index)) {
      return false;
    }
    contact.engaged_soldier_indices.push_back(index);
  }
  for (const auto row : data.value("fronts").toArray()) {
    const auto front_data = row.toObject();
    auto& front = contact.fronts.emplace_back();
    if (!read_tuple(front_data.value("state"), fields(front)) ||
        !read_list(front_data.value("pairs"), front.engagement_pairs)) {
      return false;
    }
  }
  return true;
}

void write_construction_ghost(const Entity& entity, QJsonObject& out) {
  if (const auto* ghost =
          entity.registry()->try_get<const ConstructionPreviewComponent>(
              entity.get_id())) {
    out["construction_ghost"] =
        QJsonObject{{"state", write_tuple(fields(*ghost))},
                    {"product", QString::fromStdString(ghost->product_type)}};
  }
}

auto read_construction_ghost(Entity& entity, const QJsonObject& in) -> bool {
  if (!in.contains("construction_ghost")) {
    return true;
  }
  const auto data = in.value("construction_ghost").toObject();
  ConstructionPreviewComponent ghost;
  if (!read_tuple(data.value("state"), fields(ghost)) ||
      !data.value("product").isString()) {
    return false;
  }
  ghost.product_type = data.value("product").toString().toStdString();
  entity.registry()->emplace<ConstructionPreviewComponent>(entity.get_id(),
                                                           std::move(ghost));
  return true;
}

void write_traversal(const Entity& entity, QJsonObject& out) {
  if (const auto* traversal =
          entity.registry()->try_get<const UnitTraversalLayoutStateComponent>(
              entity.get_id())) {
    QJsonArray mapping;
    for (const auto index : traversal->stable_slot_mapping) {
      mapping.append(index);
    }
    out["traversal"] = QJsonObject{{"state", write_tuple(fields(*traversal))},
                                   {"mapping", mapping},
                                   {"soldiers", write_list(traversal->slot_states)}};
  }
}

auto read_traversal(Entity& entity, const QJsonObject& in) -> bool {
  if (!in.contains("traversal")) {
    return true;
  }
  const auto data = in.value("traversal").toObject();
  UnitTraversalLayoutStateComponent traversal;
  if (!read_tuple(data.value("state"), fields(traversal)) ||
      !data.value("mapping").isArray() ||
      !read_list(data.value("soldiers"), traversal.slot_states)) {
    return false;
  }
  for (const auto row : data.value("mapping").toArray()) {
    std::uint16_t index = 0;
    if (!Game::CheckpointValues::decode(row, index)) {
      return false;
    }
    traversal.stable_slot_mapping.push_back(index);
  }
  entity.registry()->emplace<UnitTraversalLayoutStateComponent>(entity.get_id(),
                                                                std::move(traversal));
  return true;
}

void write_formation(const Entity& entity, QJsonObject& out) {
  if (const auto* formation =
          entity.registry()->try_get<const FormationPresentationComponent>(
              entity.get_id())) {
    out["formation_motion"] =
        QJsonObject{{"state", write_tuple(fields(*formation))},
                    {"soldiers", write_list(formation->soldiers)}};
  }
  if (const auto* contact = entity.registry()->try_get<const FormationContactComponent>(
          entity.get_id())) {
    out["formation_contact"] = write_contact(*contact);
  }
  if (const auto* roster =
          entity.registry()->try_get<const FormationRosterPresentationComponent>(
              entity.get_id())) {
    QJsonArray alive;
    for (const auto member : roster->alive) {
      alive.append(member);
    }
    out["formation_roster"] =
        QJsonObject{{"state", write_tuple(fields(*roster))}, {"alive", alive}};
  }
  if (const auto* casualties =
          entity.registry()->try_get<const SoldierCasualtyAnimationComponent>(
              entity.get_id())) {
    out["soldier_casualties"] = write_list(casualties->entries);
  }
  if (const auto* reform =
          entity.registry()->try_get<const SquadReformComponent>(entity.get_id())) {
    out["squad_reform"] = QJsonObject{{"state", write_tuple(fields(*reform))},
                                      {"soldiers", write_list(reform->soldiers)}};
  }
}

auto read_formation(Entity& entity, const QJsonObject& in) -> bool {
  if (in.contains("formation_motion")) {
    const auto data = in.value("formation_motion").toObject();
    FormationPresentationComponent formation;
    if (!read_tuple(data.value("state"), fields(formation)) ||
        !read_list(data.value("soldiers"), formation.soldiers)) {
      return false;
    }
    entity.registry()->emplace<FormationPresentationComponent>(entity.get_id(),
                                                               std::move(formation));
  }
  if (in.contains("formation_contact")) {
    FormationContactComponent contact;
    if (!read_contact(in.value("formation_contact").toObject(), contact)) {
      return false;
    }
    entity.registry()->emplace<FormationContactComponent>(entity.get_id(),
                                                          std::move(contact));
  }
  if (in.contains("formation_roster")) {
    const auto data = in.value("formation_roster").toObject();
    FormationRosterPresentationComponent roster;
    if (!read_tuple(data.value("state"), fields(roster)) ||
        !data.value("alive").isArray()) {
      return false;
    }
    for (const auto row : data.value("alive").toArray()) {
      std::uint8_t alive = 0;
      if (!Game::CheckpointValues::decode(row, alive) || alive > 1) {
        return false;
      }
      roster.alive.push_back(alive);
    }
    entity.registry()->emplace<FormationRosterPresentationComponent>(entity.get_id(),
                                                                     std::move(roster));
  }
  if (in.contains("soldier_casualties")) {
    SoldierCasualtyAnimationComponent casualties;
    if (!read_list(in.value("soldier_casualties"), casualties.entries)) {
      return false;
    }
    entity.registry()->emplace<SoldierCasualtyAnimationComponent>(
        entity.get_id(), std::move(casualties));
  }
  if (in.contains("squad_reform")) {
    const auto data = in.value("squad_reform").toObject();
    SquadReformComponent reform;
    if (!read_tuple(data.value("state"), fields(reform)) ||
        !read_list(data.value("soldiers"), reform.soldiers)) {
      return false;
    }
    entity.registry()->emplace<SquadReformComponent>(entity.get_id(),
                                                     std::move(reform));
  }
  return true;
}

void write_engagement(const Entity& entity, QJsonObject& out) {
  if (const auto* engagement =
          entity.registry()->try_get<const RpgEngagementComponent>(entity.get_id())) {
    out["rpg_engagement"] =
        QJsonObject{{"state", write_tuple(fields(*engagement))},
                    {"slots", write_list(engagement->engagement_slots)},
                    {"tenure", write_list(engagement->press_tenure)}};
  }
}

auto read_engagement(Entity& entity, const QJsonObject& in) -> bool {
  if (!in.contains("rpg_engagement")) {
    return true;
  }
  const auto data = in.value("rpg_engagement").toObject();
  RpgEngagementComponent engagement;
  if (!read_tuple(data.value("state"), fields(engagement)) ||
      !read_list(data.value("slots"), engagement.engagement_slots) ||
      !read_list(data.value("tenure"), engagement.press_tenure)) {
    return false;
  }
  entity.registry()->emplace<RpgEngagementComponent>(entity.get_id(),
                                                     std::move(engagement));
  return true;
}

} // namespace

void write_checkpoint_components(const Entity& entity, QJsonObject& out) {
  QJsonObject components;
  write_component<CommanderComponent>(entity, components, "direct_control");
  write_component<CommanderBodyControlComponent>(entity, components, "commander_body");
  write_component<CommanderGuardComponent>(entity, components, "commander_guard");
  write_component<RpgHealthComponent>(entity, components, "commander_health");
  write_component<RpgCommanderAimComponent>(entity, components, "commander_aim");
  write_component<RpgCommanderTargetComponent>(entity, components, "commander_targets");
  write_component<EngagementSlotComponent>(entity, components, "engagement_slot");
  write_component<TargetCommitmentComponent>(entity, components, "target_commitment");
  write_component<CohortMembershipComponent>(entity, components, "cohort");
  write_component<DeathAnimationComponent>(entity, components, "death");
  write_component<BloodStainComponent>(entity, components, "blood_stain");
  write_component<CombatLaunchComponent>(entity, components, "combat_launch");
  write_component<AttackComponent>(entity, components, "attack_extras");
  write_component<CombatStateComponent>(entity, components, "combat_state_extras");
  write_component<StaminaComponent>(entity, components, "stamina_regen");
  write_component<WildlifeComponent>(entity, components, "wildlife_timers");
  write_component<ForestCoverComponent>(entity, components, "forest_cover");
  write_component<BuilderProductionComponent>(entity, components, "builder_work");
  write_component<CaptureComponent>(entity, components, "capture_lock");
  write_component<WallSegmentComponent>(entity, components, "wall_links");
  write_component<AssaultWaveComponent>(entity, components, "assault_wave");
  write_component<ThreatAlertComponent>(entity, components, "threat_alert");
  write_component<FormationHitPresentationComponent>(
      entity, components, "formation_hit");
  write_component<MotionPresentationComponent>(entity, components, "motion");
  write_component<UnitComponent>(entity, components, "unit_layout_override");
  write_component<UnitLayoutStateComponent>(
      entity, components, "unit_layout_transition");
  write_construction_ghost(entity, components);
  write_traversal(entity, components);
  write_formation(entity, components);
  write_engagement(entity, components);
  write_active_action(entity, components);
  write_intents(entity, components);
  if (!components.isEmpty()) {
    out["checkpoint_components"] = components;
  }
}

auto read_checkpoint_components(Entity& entity, const QJsonObject& snapshot) -> bool {
  const auto components = snapshot.value("checkpoint_components").toObject();
  return read_component<CommanderComponent>(entity, components, "direct_control") &&
         read_component<CommanderBodyControlComponent>(
             entity, components, "commander_body") &&
         read_component<CommanderGuardComponent>(
             entity, components, "commander_guard") &&
         read_component<RpgHealthComponent>(entity, components, "commander_health") &&
         read_component<RpgCommanderAimComponent>(
             entity, components, "commander_aim") &&
         read_component<RpgCommanderTargetComponent>(
             entity, components, "commander_targets") &&
         read_component<EngagementSlotComponent>(
             entity, components, "engagement_slot") &&
         read_component<TargetCommitmentComponent>(
             entity, components, "target_commitment") &&
         read_component<CohortMembershipComponent>(entity, components, "cohort") &&
         read_component<DeathAnimationComponent>(entity, components, "death") &&
         read_component<BloodStainComponent>(entity, components, "blood_stain") &&
         read_component<CombatLaunchComponent>(entity, components, "combat_launch") &&
         read_component<AttackComponent>(entity, components, "attack_extras") &&
         read_component<CombatStateComponent>(
             entity, components, "combat_state_extras") &&
         read_component<StaminaComponent>(entity, components, "stamina_regen") &&
         read_component<WildlifeComponent>(entity, components, "wildlife_timers") &&
         read_component<ForestCoverComponent>(entity, components, "forest_cover") &&
         read_component<BuilderProductionComponent>(
             entity, components, "builder_work") &&
         read_component<CaptureComponent>(entity, components, "capture_lock") &&
         read_component<WallSegmentComponent>(entity, components, "wall_links") &&
         read_component<AssaultWaveComponent>(entity, components, "assault_wave") &&
         read_component<ThreatAlertComponent>(entity, components, "threat_alert") &&
         read_component<FormationHitPresentationComponent>(
             entity, components, "formation_hit") &&
         read_component<MotionPresentationComponent>(entity, components, "motion") &&
         read_component<UnitComponent>(entity, components, "unit_layout_override") &&
         read_component<UnitLayoutStateComponent>(
             entity, components, "unit_layout_transition") &&
         read_construction_ghost(entity, components) &&
         read_traversal(entity, components) && read_formation(entity, components) &&
         read_engagement(entity, components) &&
         read_active_action(entity, components) && read_intents(entity, components);
}

} // namespace Engine::Core::EntityCodec

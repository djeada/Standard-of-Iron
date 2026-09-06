#include "arena_scenario.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <utility>

#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/formation/army_formation_service.h"
#include "game/map/terrain_service.h"
#include "game/systems/attack_range.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/combat_system/structure_combat.h"
#include "game/systems/combat_system/structure_fire.h"
#include "game/systems/command_service.h"
#include "game/systems/defensive_unit_layout_service.h"
#include "game/systems/food_targets.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/order_service.h"
#include "game/systems/pathfinding.h"
#include "game/systems/projectile_kind.h"
#include "game/systems/projectile_system.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "game/systems/selection_system.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/unit.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_species.h"
#include "render/graphics_settings.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/performance_report.h"

namespace Arena {
namespace {

constexpr float k_default_response_seconds = 0.45F;
constexpr float k_default_idle_seconds = 1.25F;
constexpr float k_default_engagement_distance = 7.0F;
constexpr float k_spawn_settle_seconds = 0.10F;

constexpr float k_default_root_step = 0.8F;
constexpr float k_default_frame_budget_ms = 33.34F;
constexpr float k_default_fall_up_y = 0.72F;

constexpr float k_default_foot_slide = 0.08F;
constexpr float k_boom_reversal_floor = 0.005F;
constexpr float k_presented_yaw_allowance_degrees = 0.5F;
constexpr float k_planted_root_step = 0.02F;
constexpr float k_stride_fade_presence = 0.05F;
constexpr float k_planted_foot_height = 0.05F;
constexpr float k_default_hand_step = 0.90F;
constexpr float k_default_pelvis_step = 70.0F;
constexpr float k_default_attack_torso_sweep = 6.0F;

auto shortest_degrees(float to_degrees, float from_degrees) -> float {
  float diff = to_degrees - from_degrees;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  return diff;
}

auto vector_from_transform(const Engine::Core::TransformComponent& transform)
    -> QVector3D {
  return {transform.position.x, transform.position.y, transform.position.z};
}

auto owner_of(Engine::Core::World& world, Engine::Core::EntityID entity_id) -> int {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr ? unit->owner_id : 0;
}

auto horizontal_distance(const QVector3D& lhs, const QVector3D& rhs) -> float {
  QVector3D delta = lhs - rhs;
  delta.setY(0.0F);
  return delta.length();
}

auto soldier_key(Engine::Core::EntityID entity_id, int soldier_index) -> std::uint64_t {
  return (static_cast<std::uint64_t>(entity_id) << 32U) |
         static_cast<std::uint32_t>(std::max(0, soldier_index));
}

auto command_name(ScenarioCommandKind kind) -> QString {
  switch (kind) {
  case ScenarioCommandKind::Stand:
    return QStringLiteral("Stand");
  case ScenarioCommandKind::Move:
    return QStringLiteral("Move");
  case ScenarioCommandKind::FormationMove:
    return QStringLiteral("FormationMove");
  case ScenarioCommandKind::Run:
    return QStringLiteral("Run");
  case ScenarioCommandKind::FormArmy:
    return QStringLiteral("FormArmy");
  case ScenarioCommandKind::Charge:
    return QStringLiteral("Charge");
  case ScenarioCommandKind::Attack:
    return QStringLiteral("Attack");
  case ScenarioCommandKind::AttackMove:
    return QStringLiteral("AttackMove");
  case ScenarioCommandKind::Hold:
    return QStringLiteral("Hold");
  case ScenarioCommandKind::Guard:
    return QStringLiteral("Guard");
  case ScenarioCommandKind::Stop:
    return QStringLiteral("Stop");
  case ScenarioCommandKind::SpawnAmbush:
    return QStringLiteral("SpawnAmbush");
  case ScenarioCommandKind::ReleaseReserve:
    return QStringLiteral("ReleaseReserve");
  case ScenarioCommandKind::SetCamera:
    return QStringLiteral("SetCamera");
  case ScenarioCommandKind::SetHealth:
    return QStringLiteral("SetHealth");
  case ScenarioCommandKind::ApplyDamage:
    return QStringLiteral("ApplyDamage");
  case ScenarioCommandKind::MeleeLock:
    return QStringLiteral("MeleeLock");
  case ScenarioCommandKind::SetFullCreatureLod:
    return QStringLiteral("SetFullCreatureLod");
  case ScenarioCommandKind::TriggerCommanderAura:
    return QStringLiteral("TriggerCommanderAura");
  case ScenarioCommandKind::TriggerFlagRally:
    return QStringLiteral("TriggerFlagRally");
  case ScenarioCommandKind::RpgPrimaryAttack:
    return QStringLiteral("RpgPrimaryAttack");
  case ScenarioCommandKind::RpgHeavyAttack:
    return QStringLiteral("RpgHeavyAttack");
  case ScenarioCommandKind::RpgAttackHold:
    return QStringLiteral("RpgAttackHold");
  case ScenarioCommandKind::RpgAim:
    return QStringLiteral("RpgAim");
  case ScenarioCommandKind::RpgGuard:
    return QStringLiteral("RpgGuard");
  case ScenarioCommandKind::RpgDodge:
    return QStringLiteral("RpgDodge");
  case ScenarioCommandKind::RpgJump:
    return QStringLiteral("RpgJump");
  case ScenarioCommandKind::RpgSpecial:
    return QStringLiteral("RpgSpecial");
  case ScenarioCommandKind::RpgWeaponSwitch:
    return QStringLiteral("RpgWeaponSwitch");
  case ScenarioCommandKind::RepairStructure:
    return QStringLiteral("RepairStructure");
  case ScenarioCommandKind::DeliverToStructure:
    return QStringLiteral("DeliverToStructure");
  case ScenarioCommandKind::HarvestResource:
    return QStringLiteral("HarvestResource");
  case ScenarioCommandKind::AbandonWork:
    return QStringLiteral("AbandonWork");
  case ScenarioCommandKind::SetFarmGrowth:
    return QStringLiteral("SetFarmGrowth");
  case ScenarioCommandKind::RpgMove:
    return QStringLiteral("RpgMove");
  case ScenarioCommandKind::RpgCycleLockOn:
    return QStringLiteral("RpgCycleLockOn");
  case ScenarioCommandKind::ReloadUndeadZoneState:
    return QStringLiteral("ReloadUndeadZoneState");
  }
  return QStringLiteral("Unknown");
}

auto json_vector(const QVector3D& value) -> QJsonArray {
  return {value.x(), value.y(), value.z()};
}

auto framing_state_name(App::Core::CommanderFramingState state) -> QString {
  switch (state) {
  case App::Core::CommanderFramingState::Explore:
    return QStringLiteral("explore");
  case App::Core::CommanderFramingState::Melee:
    return QStringLiteral("melee");
  case App::Core::CommanderFramingState::DuelLock:
    return QStringLiteral("duel_lock");
  case App::Core::CommanderFramingState::BowAim:
    return QStringLiteral("bow_aim");
  }
  return QStringLiteral("unknown");
}

auto commander_trace_json(const App::Core::CommanderPresentationTrace& trace)
    -> QJsonObject {
  auto const& input = trace.input;
  auto const& motor = trace.motor;
  auto const& camera = trace.camera;
  auto const& combat = trace.combat;

  QJsonObject input_json{
      {QStringLiteral("frame_index"), static_cast<qint64>(input.frame_index)},
      {QStringLiteral("primary_press_sequence"),
       static_cast<qint64>(input.primary_press_sequence)},
      {QStringLiteral("primary_release_sequence"),
       static_cast<qint64>(input.primary_release_sequence)},
      {QStringLiteral("primary_consumed_sequence"),
       static_cast<qint64>(input.primary_consumed_sequence)},
      {QStringLiteral("primary_dropped_sequence"),
       static_cast<qint64>(input.primary_dropped_sequence)},
      {QStringLiteral("guard_press_sequence"),
       static_cast<qint64>(input.guard_press_sequence)},
      {QStringLiteral("guard_release_sequence"),
       static_cast<qint64>(input.guard_release_sequence)},
      {QStringLiteral("dodge_request_sequence"),
       static_cast<qint64>(input.dodge_request_sequence)},
      {QStringLiteral("dodge_consumed_sequence"),
       static_cast<qint64>(input.dodge_consumed_sequence)},
      {QStringLiteral("dodge_refused_sequence"),
       static_cast<qint64>(input.dodge_refused_sequence)},
      {QStringLiteral("jump_request_sequence"),
       static_cast<qint64>(input.jump_request_sequence)},
      {QStringLiteral("jump_consumed_sequence"),
       static_cast<qint64>(input.jump_consumed_sequence)},
      {QStringLiteral("jump_refused_sequence"),
       static_cast<qint64>(input.jump_refused_sequence)},
      {QStringLiteral("move_axes"),
       QJsonArray{input.move_forward_axis, input.move_right_axis}},
      {QStringLiteral("run_held"), input.run_held},
      {QStringLiteral("primary_held"), input.primary_held},
      {QStringLiteral("guard_held"), input.guard_held},
      {QStringLiteral("primary_held_duration"), input.primary_held_duration},
      {QStringLiteral("look_delta"),
       QJsonArray{input.look_delta_yaw, input.look_delta_pitch}},
      {QStringLiteral("view_yaw"), input.view_yaw},
      {QStringLiteral("view_pitch"), input.view_pitch}};

  QJsonObject motor_json{
      {QStringLiteral("previous_position"), json_vector(motor.previous_position)},
      {QStringLiteral("position"), json_vector(motor.position)},
      {QStringLiteral("desired_velocity"), json_vector(motor.desired_velocity)},
      {QStringLiteral("actual_velocity"), json_vector(motor.actual_velocity)},
      {QStringLiteral("requested_speed"), motor.requested_speed},
      {QStringLiteral("smoothed_speed"), motor.smoothed_speed},
      {QStringLiteral("speed_error"), motor.speed_error},
      {QStringLiteral("grounded"), motor.grounded},
      {QStringLiteral("blocked"), motor.blocked},
      {QStringLiteral("slid"), motor.slid},
      {QStringLiteral("separation_push"), motor.separation_push},
      {QStringLiteral("movement_mode"),
       QString::fromLatin1(App::Core::movement_mode_name(motor.movement_mode))},
      {QStringLiteral("steering_source"), QString::fromLatin1(motor.steering_source)},
      {QStringLiteral("static_walkable"), motor.static_walkable},
      {QStringLiteral("dynamic_push"), json_vector(motor.dynamic_push)},
      {QStringLiteral("dynamic_neighbors"),
       static_cast<qint64>(motor.dynamic_neighbors)},
      {QStringLiteral("dynamic_overlap"), motor.dynamic_overlap},
      {QStringLiteral("accepted_displacement"), motor.accepted_displacement},
      {QStringLiteral("lunge_distance"), motor.lunge_distance},
      {QStringLiteral("snap_back_distance"), motor.snap_back_distance},
      {QStringLiteral("displacement_source"),
       QString::fromLatin1(
           App::Core::displacement_source_name(motor.displacement_source))},
      {QStringLiteral("dt"), motor.dt},
      {QStringLiteral("presented_position"), json_vector(motor.presented_position)},
      {QStringLiteral("presented_yaw"), motor.presented_yaw},
      {QStringLiteral("presentation_alpha"), motor.presentation_alpha},
      {QStringLiteral("presentation_extrapolated"), motor.presentation_extrapolated}};

  QJsonObject camera_json{
      {QStringLiteral("valid"), camera.valid},
      {QStringLiteral("commander_position"), json_vector(camera.commander_position)},
      {QStringLiteral("visual_anchor"), json_vector(camera.visual_anchor)},
      {QStringLiteral("anchor_lag"), camera.anchor_lag},
      {QStringLiteral("pivot"), json_vector(camera.pivot)},
      {QStringLiteral("eye_unconstrained"), json_vector(camera.eye_unconstrained)},
      {QStringLiteral("target_unconstrained"),
       json_vector(camera.target_unconstrained)},
      {QStringLiteral("eye_resolved"), json_vector(camera.eye_resolved)},
      {QStringLiteral("target_resolved"), json_vector(camera.target_resolved)},
      {QStringLiteral("boom_unconstrained"), camera.boom_unconstrained},
      {QStringLiteral("boom_resolved"), camera.boom_resolved},
      {QStringLiteral("boom_clear_fraction"), camera.boom_clear_fraction},
      {QStringLiteral("terrain_clear_fraction"), camera.terrain_clear_fraction},
      {QStringLiteral("sight_line_clear"), camera.sight_line_clear},
      {QStringLiteral("occlusion_fraction"), camera.occlusion_fraction},
      {QStringLiteral("terrain_lift"), camera.terrain_lift},
      {QStringLiteral("eye_clearance"), camera.eye_clearance},
      {QStringLiteral("fov"), camera.fov},
      {QStringLiteral("yaw"), camera.yaw},
      {QStringLiteral("pitch"), camera.pitch},
      {QStringLiteral("yaw_velocity"), camera.yaw_velocity},
      {QStringLiteral("pitch_velocity"), camera.pitch_velocity},
      {QStringLiteral("ground_y"), camera.ground_y},
      {QStringLiteral("framing_state"), framing_state_name(camera.framing_state)},
      {QStringLiteral("framing_changed"), camera.framing_changed},
      {QStringLiteral("dt"), camera.dt}};

  QJsonObject combat_json{
      {QStringLiteral("action_id"), combat.action_id},
      {QStringLiteral("action_phase"), combat.action_phase},
      {QStringLiteral("action_normalized_time"), combat.action_normalized_time},
      {QStringLiteral("action_running"), combat.action_running},
      {QStringLiteral("queued_intents"), combat.queued_intents},
      {QStringLiteral("guard_active"), combat.guard_active},
      {QStringLiteral("perfect_guard_remaining"), combat.perfect_guard_remaining},
      {QStringLiteral("dodge_state"), combat.dodge_state},
      {QStringLiteral("dodge_timer"), combat.dodge_timer},
      {QStringLiteral("dodge_grace_remaining"), combat.dodge_grace_remaining},
      {QStringLiteral("locked_target_id"),
       static_cast<qint64>(combat.locked_target_id)},
      {QStringLiteral("locked_target_slot"), combat.locked_target_slot},
      {QStringLiteral("soft_target_id"), static_cast<qint64>(combat.soft_target_id)},
      {QStringLiteral("soft_target_slot"), combat.soft_target_slot},
      {QStringLiteral("hit_confirm_sequence"),
       static_cast<qint64>(combat.hit_confirm_sequence)},
      {QStringLiteral("action_hit_count"), combat.action_hit_count},
      {QStringLiteral("health"), combat.health},
      {QStringLiteral("stamina"), combat.stamina},
      {QStringLiteral("queue_outcome"),
       QString::fromLatin1(
           App::Core::combat_intent_outcome_name(combat.queue_outcome))},
      {QStringLiteral("queue_outcome_age"), combat.queue_outcome_age},
      {QStringLiteral("queue_accepted"), static_cast<qint64>(combat.queue_accepted)},
      {QStringLiteral("queue_buffered"), static_cast<qint64>(combat.queue_buffered)},
      {QStringLiteral("queue_refused"), static_cast<qint64>(combat.queue_refused)},
      {QStringLiteral("queue_expired"), static_cast<qint64>(combat.queue_expired)},
      {QStringLiteral("queue_overflow"), static_cast<qint64>(combat.queue_overflow)},
      {QStringLiteral("action_window_start"), combat.action_window_start},
      {QStringLiteral("action_window_end"), combat.action_window_end},
      {QStringLiteral("blocked_contacts"),
       static_cast<qint64>(combat.blocked_contacts)},
      {QStringLiteral("perfect_guard_contacts"),
       static_cast<qint64>(combat.perfect_guard_contacts)},
      {QStringLiteral("dodged_contacts"), static_cast<qint64>(combat.dodged_contacts)},
      {QStringLiteral("damaging_contacts"),
       static_cast<qint64>(combat.damaging_contacts)},
      {QStringLiteral("guard_broken_contacts"),
       static_cast<qint64>(combat.guard_broken_contacts)}};

  QJsonObject const costs_json{
      {QStringLiteral("motor_ms"), trace.costs.motor_ms},
      {QStringLiteral("targeting_ms"), trace.costs.targeting_ms},
      {QStringLiteral("weapon_trace_ms"), trace.costs.weapon_trace_ms},
      {QStringLiteral("engagement_ms"), trace.costs.engagement_ms},
      {QStringLiteral("camera_ms"), trace.costs.camera_ms},
      {QStringLiteral("total_ms"), trace.costs.total_ms()}};

  return QJsonObject{{QStringLiteral("sequence"), static_cast<qint64>(trace.sequence)},
                     {QStringLiteral("time_seconds"), trace.time_seconds},
                     {QStringLiteral("input"), input_json},
                     {QStringLiteral("motor"), motor_json},
                     {QStringLiteral("camera"), camera_json},
                     {QStringLiteral("combat"), combat_json},
                     {QStringLiteral("costs"), costs_json}};
}

auto expectation_requires_zone(ArenaExpectationKind kind) -> bool {
  return kind == ArenaExpectationKind::UndeadZoneDormantBefore ||
         kind == ArenaExpectationKind::UndeadZoneAwakened ||
         kind == ArenaExpectationKind::UndeadZoneCleared ||
         kind == ArenaExpectationKind::UndeadZoneShrineStands ||
         kind == ArenaExpectationKind::UndeadZoneShrineDestroyed;
}

auto describe_window(const ArenaExpectation& expectation) -> QString {
  if (expectation.start_seconds <= 0.0F && expectation.end_seconds <= 0.0F) {
    return {};
  }
  return QStringLiteral(" between %1 s and %2 s")
      .arg(expectation.start_seconds, 0, 'f', 1)
      .arg(expectation.end_seconds > 0.0F
               ? QString::number(expectation.end_seconds, 'f', 1)
               : QStringLiteral("the end"));
}

auto expectation_requires_side(ArenaExpectationKind kind) -> bool {
  switch (kind) {
  case ArenaExpectationKind::SideSurvives:
  case ArenaExpectationKind::SideAdvanceAtLeast:
  case ArenaExpectationKind::SideAdvanceAtMost:
  case ArenaExpectationKind::SideProducesReinforcements:
  case ArenaExpectationKind::SideDoctrineIs:
  case ArenaExpectationKind::SideCommitsToAttack:
  case ArenaExpectationKind::SideHoldsPosition:
  case ArenaExpectationKind::SideBuildsAtLeast:
  case ArenaExpectationKind::SideKeepsGarrison:
  case ArenaExpectationKind::SideFieldsArmy:
    return true;
  default:
    return false;
  }
}

} // namespace

auto validate_scenario(const ArenaScenarioDefinition& definition)
    -> std::vector<ArenaScenarioValidationError> {
  std::vector<ArenaScenarioValidationError> errors;
  if (definition.id.trimmed().isEmpty()) {
    errors.push_back({QStringLiteral("id"), QStringLiteral("scenario id is empty")});
  }
  if (!(definition.duration_seconds > 0.0F)) {
    errors.push_back({QStringLiteral("duration_seconds"),
                      QStringLiteral("duration must be positive")});
  }
  if (definition.groups.empty()) {
    errors.push_back(
        {QStringLiteral("groups"), QStringLiteral("at least one group is required")});
  }
  if (definition.expectations.empty()) {
    errors.push_back({QStringLiteral("expectations"),
                      QStringLiteral("scenario must declare acceptance expectations")});
  }

  QSet<QString> group_names;
  for (std::size_t i = 0; i < definition.groups.size(); ++i) {
    auto const& group = definition.groups[i];
    QString const field = QStringLiteral("groups[%1]").arg(i);
    if (group.name.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("group name is empty")});
    } else if (group_names.contains(group.name)) {
      errors.push_back({field, QStringLiteral("duplicate group '%1'").arg(group.name)});
    } else {
      group_names.insert(group.name);
    }
    if (group.count <= 0) {
      errors.push_back({field, QStringLiteral("group count must be positive")});
    }
    if (group.individuals_per_unit < 0) {
      errors.push_back(
          {field, QStringLiteral("individuals_per_unit cannot be negative")});
    }
  }

  auto check_group = [&](const QString& value, const QString& field, bool required) {
    if (value.isEmpty()) {
      if (required) {
        errors.push_back({field, QStringLiteral("group reference is required")});
      }
      return;
    }
    if (!group_names.contains(value)) {
      errors.push_back(
          {field, QStringLiteral("unknown group reference '%1'").arg(value)});
    }
  };

  if (definition.rpg_mode) {
    check_group(
        definition.rpg_commander_group, QStringLiteral("rpg_commander_group"), true);
    auto const commander_group = std::find_if(
        definition.groups.begin(), definition.groups.end(), [&](auto const& group) {
          return group.name == definition.rpg_commander_group;
        });
    if (commander_group != definition.groups.end() && commander_group->count != 1) {
      errors.push_back(
          {QStringLiteral("rpg_commander_group"),
           QStringLiteral("RPG commander group must contain exactly one unit")});
    }
  } else if (!definition.rpg_commander_group.isEmpty()) {
    errors.push_back({QStringLiteral("rpg_commander_group"),
                      QStringLiteral("RPG commander group requires rpg_mode")});
  }

  QSet<QString> zone_ids;
  for (std::size_t i = 0; i < definition.undead_zones.size(); ++i) {
    auto const& zone = definition.undead_zones[i];
    QString const field = QStringLiteral("undead_zones[%1]").arg(i);
    if (zone.id.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("undead zone id is empty")});
    } else if (zone_ids.contains(zone.id)) {
      errors.push_back(
          {field, QStringLiteral("duplicate undead zone '%1'").arg(zone.id)});
    } else {
      zone_ids.insert(zone.id);
    }
    if (!(zone.radius > 0.0F)) {
      errors.push_back({field, QStringLiteral("undead zone radius must be positive")});
    }
  }

  for (std::size_t i = 0; i < definition.steps.size(); ++i) {
    auto const& step = definition.steps[i];
    QString const field = QStringLiteral("steps[%1]").arg(i);
    if (step.trigger.time_seconds < 0.0F) {
      errors.push_back({field, QStringLiteral("trigger time cannot be negative")});
    }
    bool const command_needs_group =
        step.zone_id.isEmpty() && step.command != ScenarioCommandKind::SetCamera &&
        step.command != ScenarioCommandKind::SetFullCreatureLod &&
        step.command != ScenarioCommandKind::ReloadUndeadZoneState;
    check_group(step.group, field + QStringLiteral(".group"), command_needs_group);
    if (!step.zone_id.isEmpty() && !zone_ids.contains(step.zone_id)) {
      errors.push_back(
          {field + QStringLiteral(".zone_id"),
           QStringLiteral("unknown undead zone reference '%1'").arg(step.zone_id)});
    }
    bool const command_needs_target =
        step.command == ScenarioCommandKind::Attack ||
        step.command == ScenarioCommandKind::AttackMove ||
        step.command == ScenarioCommandKind::Charge ||
        step.command == ScenarioCommandKind::ReleaseReserve ||
        step.command == ScenarioCommandKind::MeleeLock;
    check_group(step.target_group,
                field + QStringLiteral(".target_group"),
                command_needs_target);
    if (step.trigger.kind != ScenarioTriggerKind::AtTime &&
        step.trigger.kind != ScenarioTriggerKind::PreviousStepComplete) {
      check_group(step.trigger.group, field + QStringLiteral(".trigger.group"), true);
    }
    if (step.trigger.kind == ScenarioTriggerKind::FirstContact ||
        step.trigger.kind == ScenarioTriggerKind::GroupsWithinDistance) {
      check_group(step.trigger.target_group,
                  field + QStringLiteral(".trigger.target_group"),
                  true);
    }

    for (int member = 0; member < step.formation.groups.size(); ++member) {
      check_group(step.formation.groups.at(member),
                  field + QStringLiteral(".formation.groups[%1]").arg(member),
                  true);
    }
    if (step.command == ScenarioCommandKind::FormArmy &&
        step.formation.frontage < 0.0F) {
      errors.push_back(
          {field, QStringLiteral("formation frontage cannot be negative")});
    }
  }

  QSet<QString> battle_side_labels;
  for (std::size_t i = 0; i < definition.battle_sides.size(); ++i) {
    auto const& side = definition.battle_sides[i];
    QString const field = QStringLiteral("battle_sides[%1]").arg(i);
    if (side.label.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("battle side label is empty")});
    } else if (battle_side_labels.contains(side.label)) {
      errors.push_back(
          {field, QStringLiteral("duplicate battle side '%1'").arg(side.label)});
    } else {
      battle_side_labels.insert(side.label);
    }
  }

  auto check_side = [&](const QString& value, const QString& field) {
    if (value.isEmpty()) {
      errors.push_back({field, QStringLiteral("battle side reference is required")});
      return;
    }
    if (!battle_side_labels.contains(value)) {
      errors.push_back(
          {field, QStringLiteral("unknown battle side reference '%1'").arg(value)});
    }
  };

  for (std::size_t i = 0; i < definition.expectations.size(); ++i) {
    auto const& expectation = definition.expectations[i];
    QString const field = QStringLiteral("expectations[%1]").arg(i);
    if (expectation_requires_side(expectation.kind)) {
      check_side(expectation.side, field + QStringLiteral(".side"));
    }
    if (expectation.kind == ArenaExpectationKind::BattleReachesDecision &&
        definition.battle_sides.size() < 2U) {
      errors.push_back({field,
                        QStringLiteral("BattleReachesDecision requires at least two "
                                       "authored battle sides")});
    }
    if (expectation_requires_zone(expectation.kind)) {
      if (expectation.zone_id.isEmpty()) {
        errors.push_back({field + QStringLiteral(".zone_id"),
                          QStringLiteral("undead zone reference is required")});
      } else if (!zone_ids.contains(expectation.zone_id)) {
        errors.push_back({field + QStringLiteral(".zone_id"),
                          QStringLiteral("unknown undead zone reference '%1'")
                              .arg(expectation.zone_id)});
      }
    }
    check_group(expectation.group, field + QStringLiteral(".group"), false);
    check_group(
        expectation.target_group, field + QStringLiteral(".target_group"), false);
    if (expectation.end_seconds > 0.0F &&
        expectation.end_seconds < expectation.start_seconds) {
      errors.push_back({field, QStringLiteral("expectation end precedes its start")});
    }
  }
  return errors;
}

namespace {

auto battle_summary(const ArenaBattleOutcome& battle) -> QString {
  if (!battle.tracked || battle.sides.empty()) {
    return {};
  }
  QStringList parts;
  for (auto const& side : battle.sides) {
    parts.push_back(
        QStringLiteral("%1[%2] %3u/%4b peak %5 adv %6 atk %7s "
                       "built %8 home %9 fwd %10%11")
            .arg(side.label,
                 side.strategy.isEmpty()
                     ? QStringLiteral("?")
                     : side.strategy + QStringLiteral("/") + side.posture)
            .arg(side.living_units)
            .arg(side.living_buildings)
            .arg(side.peak_units)
            .arg(side.peak_advance, 0, 'f', 2)
            .arg(side.seconds_attacking, 0, 'f', 0)
            .arg(side.buildings_constructed)
            .arg(side.peak_home_units)
            .arg(side.peak_forward_units)
            .arg(side.eliminated_at >= 0.0F
                     ? QStringLiteral(" dead@%1s").arg(side.eliminated_at, 0, 'f', 1)
                     : QString()));
  }
  QString const verdict = battle.decided
                              ? QStringLiteral("victor %1 at %2 s")
                                    .arg(battle.victor_label)
                                    .arg(battle.decided_at_seconds, 0, 'f', 1)
                              : QStringLiteral("undecided");
  return QStringLiteral(" | battle: %1 [%2]")
      .arg(verdict, parts.join(QStringLiteral("; ")));
}

} // namespace

namespace {

auto movement_summary(const std::vector<ArenaGroupMovementDiagnostics>& rows)
    -> QString {
  if (rows.empty()) {
    return {};
  }
  QStringList parts;
  parts.reserve(static_cast<int>(rows.size()));
  for (auto const& row : rows) {
    QString const objective = row.has_objective ? QStringLiteral("(%1, %2)")
                                                      .arg(row.objective_x, 0, 'f', 1)
                                                      .arg(row.objective_z, 0, 'f', 1)
                                                : QStringLiteral("none");
    parts.push_back(
        QStringLiteral("%1 stall %2 s@%3 s (%4) for %5, repaths %6, "
                       "recoveries %7, abandoned %8, still wedged %9")
            .arg(row.group)
            .arg(row.worst_stalled_seconds, 0, 'f', 1)
            .arg(row.worst_stalled_at, 0, 'f', 1)
            .arg(row.worst_state.isEmpty() ? QStringLiteral("-") : row.worst_state,
                 objective)
            .arg(row.repaths)
            .arg(row.recovery_attempts)
            .arg(row.abandons)
            .arg(row.units_holding_a_stalled_objective));
  }
  return QStringLiteral(" | movement: ") + parts.join(QStringLiteral("; "));
}

} // namespace

auto narrow_layout_summary(const std::vector<ArenaNarrowLayoutOutcome>& outcomes)
    -> QString {
  if (outcomes.empty()) {
    return {};
  }
  QStringList parts;
  for (auto const& outcome : outcomes) {
    parts.push_back(
        QStringLiteral("%1 frontage %2m corridor %3m %4 %5/%6 files at %7m, "
                       "narrowest %8m over %9m deep, reform %10m")
            .arg(outcome.group)
            .arg(outcome.formation_half_width * 2.0F, 0, 'f', 2)
            .arg(outcome.narrowest_corridor_half_width * 2.0F, 0, 'f', 2)
            .arg(outcome.engaged ? outcome.narrowest_mode
                                 : QStringLiteral("Normal(idle)"))
            .arg(outcome.narrowest_files)
            .arg(outcome.normal_files)
            .arg(outcome.tightest_file_spacing, 0, 'f', 2)
            .arg(outcome.narrowest_frontage, 0, 'f', 2)
            .arg(outcome.deepest_column, 0, 'f', 2)
            .arg(outcome.worst_reform_error, 0, 'f', 2));
  }
  return QStringLiteral(" | narrow layout: ") + parts.join(QStringLiteral("; "));
}

auto ArenaScenarioReport::summary() const -> QString {
  if (passed()) {
    QString result = QStringLiteral("PASS %1: %2 frames, %3 s")
                         .arg(scenario_id)
                         .arg(rendered_frames)
                         .arg(elapsed_seconds, 0, 'f', 2);
    if (frame_time_samples > 0U && frame_time_p95_ms > 0.0) {
      result += QStringLiteral(", peak %1 visible soldiers, frame p50/p95/max "
                               "%2/%3/%4 ms (%5 FPS at p95), peak rigged "
                               "%6 commands/%7 instanced instances")
                    .arg(peak_visible_soldiers)
                    .arg(frame_time_p50_ms, 0, 'f', 2)
                    .arg(frame_time_p95_ms, 0, 'f', 2)
                    .arg(frame_time_max_ms, 0, 'f', 2)
                    .arg(1000.0 / frame_time_p95_ms, 0, 'f', 1)
                    .arg(peak_rigged_commands)
                    .arg(peak_rigged_instanced_instances);
    }
    result += battle_summary(battle);
    result += movement_summary(movement);
    result += narrow_layout_summary(narrow_layout);
    return result;
  }
  return QStringLiteral("FAIL %1: %2 issue(s); first: %3%4%5")
      .arg(scenario_id)
      .arg(issues.size())
      .arg(issues.front().message,
           battle_summary(battle),
           movement_summary(movement) + narrow_layout_summary(narrow_layout));
}

struct ArenaScenarioRunner::Impl {
  struct StepRuntime {
    bool executed{false};
    float executed_at{0.0F};
  };

  struct CommandResponse {
    float issued_at{0.0F};
    float deadline{0.0F};
    QVector3D initial_position;
    float initial_yaw{0.0F};
    QString command;
    bool observed{false};
    bool reported{false};
  };

  struct EntityState {
    QVector3D position;
    float observed_at{0.0F};
    int health{0};
    float yaw{0.0F};
    Engine::Core::EntityID melee_lock_target_id{0};
    bool melee_lock{false};
    bool initialized{false};
  };

  struct SoldierState {
    QVector3D root_position;
    QVector3D hand_l_world;
    QVector3D hand_r_world;
    QVector3D foot_l_world;
    QVector3D foot_r_world;
    float pelvis_yaw_degrees{0.0F};
    float locomotion_presence{0.0F};
    bool joints_valid{false};
    float attack_pelvis_yaw_min{0.0F};
    float attack_pelvis_yaw_max{0.0F};
    bool attack_yaw_tracked{false};
    float observed_at{0.0F};
    float fight_idle_since{-1.0F};
    float terminal_pose_since{-1.0F};
    bool initialized{false};
    bool culled{false};
    bool attacking{false};
    bool completed_attack_phase{false};
    bool ever_visible{false};
    bool alive{false};
  };

  struct TraceUnit {
    Engine::Core::EntityID entity_id{0};
    QString group;
    QVector3D position;
    int health{0};
    Engine::Core::EntityID target_id{0};
    QString motion;
    QString combat_mode;
    int mounted_charge_state{-1};
    int mounted_charge_cancel_reason{-1};
    int combat_action_id{0};
    bool melee_lock{false};
    Engine::Core::EntityID melee_lock_target_id{0};
    bool combat_indicator_submitted{false};
    float yaw{0.0F};
    bool movement_target{false};
    float movement_vx{0.0F};
    float movement_vz{0.0F};
    float movement_goal_x{0.0F};
    float movement_goal_z{0.0F};
    bool formation_contact{false};
    float formation_surface_gap{0.0F};
    std::vector<std::uint16_t> engaged_soldiers;
    std::vector<Engine::Core::FormationEngagementPair> engagement_pairs;
    QString construction_type;
    bool construction_site{false};
    bool construction_in_progress{false};
    float construction_time_remaining{0.0F};
    bool commander_aura_active{false};
    bool commander_aura_buffed{false};
    int rpg_health{-1};
    bool rpg_guard_active{false};
    bool rpg_dodge_grace{false};
    Engine::Core::EntityID rpg_aim_target_id{0};
    int rpg_aim_soldier_slot{-1};
    int rpg_action_phase{0};
    float rpg_action_normalized_time{0.0F};
  };

  struct TraceSoldier {
    Engine::Core::EntityID entity_id{0};
    int soldier_index{0};
    QVector3D root_position;
    float root_yaw_degrees{0.0F};
    float root_up_y{1.0F};
    float submitted_body_up_y{1.0F};
    float submitted_max_arm_reach{0.0F};
    bool submitted_body_pose_valid{false};
    QVector3D foot_l_world;
    QVector3D foot_r_world;
    float locomotion_blend{0.0F};
    float locomotion_presence{0.0F};
    float cycle_phase{0.0F};
    bool persistent_valid{false};
    float sample_time{0.0F};
    float persistent_last_sample_time{0.0F};
    QString declared_action;
    int declared_target_slot{-1};
    float declared_surface_gap{0.0F};
    QString animation;
    QString visual;
    bool swing_recoil{false};
    float hit_reaction_tilt_degrees{0.0F};
    float attack_phase{0.0F};
    std::uint32_t transitions{0};
    bool culled{false};
    QString cull_reason;
  };

  struct TraceAnimal {
    Engine::Core::EntityID entity_id{0};
    QVector3D position;
    int health{0};
    QString species;
    QString behavior;
    Engine::Core::EntityID focus_id{0};
    float yaw{0.0F};
    float desired_yaw{0.0F};
    bool has_desired_yaw{false};
    float vx{0.0F};
    float vz{0.0F};
    bool biting{false};
    float bite_phase{-1.0F};
    float flinch_phase{-1.0F};
    Engine::Core::EntityID bite_target_id{0};
    bool impact_pending{false};
    bool dying{false};
  };

  struct TraceFrame {
    float time_seconds{0.0F};
    float animation_time{0.0F};
    double frame_time_ms{0.0};
    ArenaRenderedFrameTimings timings;
    std::vector<TraceUnit> units;
    std::vector<TraceSoldier> soldiers;
    std::vector<TraceAnimal> animals;
    App::Core::CommanderPresentationTrace commander;
  };

  struct TravelObservation {
    bool has_start{false};
    bool has_end{false};
    QVector3D start;
    QVector3D end;
  };

  struct BridgeAlignmentObservation {
    bool sampled{false};
    float midpoint_distance{std::numeric_limits<float>::infinity()};
    float lateral_offset{std::numeric_limits<float>::infinity()};
  };

  struct BattleSideState {
    int owner_id{0};
    QString label;
    QVector3D home;
    QVector3D enemy_home;
    bool has_axis{false};
    float separation{0.0F};
    int living_units{0};
    int living_soldiers{0};
    int living_buildings{0};
    int peak_units{0};
    int peak_soldiers{0};
    int initial_units{0};
    int initial_soldiers{0};
    float peak_advance{0.0F};
    float final_advance{0.0F};
    bool has_advance{false};
    std::vector<std::pair<float, float>> advance_samples;
    QString strategy;
    QString posture;
    float seconds_attacking{0.0F};
    float seconds_observed{0.0F};
    float home_radius{16.0F};
    QSet<Engine::Core::EntityID> initial_buildings;
    QSet<Engine::Core::EntityID> seen_buildings;
    QHash<QString, int> building_census;
    int peak_buildings{0};
    int peak_home_units{0};
    int peak_forward_units{0};
    double home_share_sum{0.0};
    int home_share_samples{0};
    float eliminated_at{-1.0F};
    bool had_presence{false};
    QSet<Engine::Core::EntityID> seen_units;
    QString ai_state;
    bool wave_committed{false};
    int wave_size{0};
  };

  struct UndeadZoneObservation {
    int spawned_total{0};
    int peak_alive{0};
    int alive{0};
    float first_spawn_at{-1.0F};
    bool shrine_seen{false};
    bool shrine_standing{false};
    bool shrine_destroyed{false};
  };

  Engine::Core::World& world;
  ArenaScenarioHost host;
  ArenaScenarioDefinition scenario;
  QVector3D world_origin;
  QHash<QString, std::vector<Engine::Core::EntityID>> groups;
  QHash<Engine::Core::EntityID, QString> entity_groups;
  std::vector<StepRuntime> steps;
  QHash<Engine::Core::EntityID, CommandResponse> responses;
  QHash<Engine::Core::EntityID, EntityState> entity_states;
  QHash<std::uint64_t, SoldierState> soldier_states;
  QHash<Engine::Core::EntityID, QSet<int>> sampled_soldiers_by_entity;
  QSet<Engine::Core::EntityID> entities_with_render_samples;
  QHash<Engine::Core::EntityID, float> idle_since;
  QHash<QString, bool> visible_attacks;
  QHash<QString, bool> visible_movement;
  QHash<QString, QString> building_overlap_report;
  QHash<QString, bool> visible_attack_recoveries;
  QHash<QString, bool> visible_hit_reactions;
  QHash<QString, bool> visible_deaths;
  QHash<QString, bool> launched_casualties;
  QHash<QString, bool> charge_impacts;
  QHash<QString, bool> melee_locks_after_charge;
  QHash<QString, bool> paired_visible_attacks;
  QHash<QString, bool> projectile_flights;
  QHash<QString, bool> flaming_projectile_flights;
  QHash<QString, bool> plain_projectile_flights;
  QHash<QString, bool> structure_fires;
  QHash<QString, bool> projectile_contacts;
  QHash<QString, bool> projectile_impacts;
  QSet<std::uint64_t> observed_projectile_impacts;
  QHash<QString, QSet<std::uint64_t>> living_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> engaged_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> attacking_soldiers_by_group;
  QHash<std::uint64_t, int> attack_entries_by_soldier;
  QHash<QString, bool> staggered_attack_phases;
  QHash<QString, bool> damage_seen;
  QHash<QString, int> initial_health_by_group;
  QHash<QString, bool> structure_damage_cues;
  QHash<QString, bool> structure_facade_contacts;
  QHash<QString, float> minimum_formation_surface_gap;
  QHash<QString, bool> bridge_traversal_seen;
  QHash<QString, bool> gate_seen;
  QHash<QString, bool> gate_opened_seen;
  QHash<QString, BridgeAlignmentObservation> bridge_alignment;
  QHash<QString, float> initial_elevation;
  QHash<QString, float> maximum_elevation;
  struct ElevationLegState {
    bool seeded{false};
    float extreme{0.0F};
    float worst_reversal{0.0F};
    float worst_at{0.0F};
    bool suspended{false};
  };
  QHash<QString, ElevationLegState> elevation_climb_legs;
  QHash<QString, ElevationLegState> elevation_descent_legs;
  struct ElevationFloorState {
    bool seeded{false};
    float lowest{0.0F};
    float lowest_at{0.0F};
    QVector3D lowest_where;
  };
  QHash<QString, ElevationFloorState> elevation_floors;
  struct OffGroundState {
    int samples{0};
    QVector3D worst;
    float worst_at{0.0F};
  };
  QHash<QString, OffGroundState> off_walkable_ground;
  QHash<QString, OffGroundState> off_walkable_soldiers;
  struct StallObservation {
    float worst_stalled_seconds{0.0F};
    float worst_stalled_at{0.0F};
    std::uint32_t recovery_attempts{0};
    std::uint32_t repaths{0};
    std::uint32_t abandons{0};
    bool recovery_seen{false};
    QString worst_state;
    bool has_objective{false};
    float objective_x{0.0F};
    float objective_z{0.0F};
  };
  QHash<QString, StallObservation> stall_observations;

  struct NarrowLayoutState {
    bool seeded{false};
    bool engaged{false};
    float formation_half_width{0.0F};
    float narrowest_corridor{0.0F};
    std::uint32_t normal_files{0};
    std::uint32_t narrowest_files{0};
    float tightest_file_spacing{0.0F};
    float narrowest_frontage{0.0F};
    float deepest_column{0.0F};
    float engaged_from{-1.0F};
    float engaged_until{-1.0F};
    Engine::Core::TraversalLayoutMode narrowest_mode{
        Engine::Core::TraversalLayoutMode::Normal};
    Engine::Core::TraversalLayoutMode previous_mode{
        Engine::Core::TraversalLayoutMode::Normal};
    std::uint32_t mode_changes{0};
    float worst_reform_error{0.0F};
    bool active_at_end{false};
    std::uint32_t files_at_end{0};
  };
  QHash<QString, NarrowLayoutState> narrow_layout;
  QSet<QString> narrow_layout_groups;
  bool narrow_layout_groups_resolved{false};
  QHash<QString, bool> defensive_layout_locked;
  QHash<QString, bool> useful_bot_action;
  std::vector<BattleSideState> battle_sides;
  bool battle_decided{false};
  float battle_decided_at{-1.0F};
  QHash<QString, UndeadZoneObservation> undead_zone_states;
  QHash<QString, QSet<Engine::Core::EntityID>> undead_zone_entities;
  QHash<QString, float> range_ring_max_radius;
  QHash<QString, float> range_ring_min_radius;
  std::size_t max_range_ring_count{0};
  QHash<QString, bool> commander_aura_active_seen;
  QHash<QString, bool> commander_aura_expired_seen;
  QHash<QString, bool> commander_aura_buff_seen;
  QHash<QString, bool> exact_rpg_target_seen;
  QHash<QString, bool> rpg_damage_contact_seen;
  QHash<QString, bool> rpg_block_contact_seen;
  QHash<QString, bool> rpg_dodge_contact_seen;
  QHash<QString, bool> rpg_dodge_window_seen;
  QHash<QString, int> initial_rpg_health_by_group;
  QHash<QString, int> minimum_rpg_health_by_group;
  QHash<QString, bool> rpg_walk_seen;
  QHash<QString, bool> rpg_run_seen;
  QHash<QString, QString> rpg_locomotion_mismatch;
  QHash<QString, QString> rpg_strike_mismatch;
  QHash<QString, float> rpg_strike_mismatch_since;
  QHash<QString, int> rpg_action_phase_previous;
  QHash<QString, float> rpg_action_time_previous;
  QHash<QString, std::vector<float>> rpg_swing_starts;
  QHash<QString, std::vector<float>> rpg_swing_carry;
  QHash<QString, float> rpg_swing_carry_pending;
  QHash<QString, QVector3D> rpg_swing_carry_origin;
  QHash<QString, bool> rpg_swing_carry_open;
  QHash<QString, TravelObservation> rpg_travel_observations;
  QHash<QString, float> minimum_group_pair_distance;
  QHash<int, int> completed_construction_by_owner;
  QHash<int, int> completed_harvest_by_owner;
  QSet<Engine::Core::EntityID> latched_builder_completions;
  QSet<Engine::Core::EntityID> initial_building_ids;
  QSet<Engine::Core::EntityID> observed_constructed_building_ids;
  QHash<QString, std::uint64_t> rendered_by_group;
  QHash<QString, std::vector<float>> initial_formation_projection;
  QSet<QString> issue_keys;
  std::vector<TraceFrame> trace;
  App::Core::CommanderPresentationTrace commander_trace;
  float animation_time{0.0F};
  ArenaScenarioReport report;
  ArenaEnvironmentSnapshot environment_snapshot;
  float elapsed{0.0F};
  float duration_limit{0.0F};
  bool started{false};
  bool complete{false};
  bool end_expectations_checked{false};

  QString rpg_aim_shooter_group;
  QString rpg_aim_target_group;

  Impl(Engine::Core::World& world_value,
       ArenaScenarioHost host_value,
       const ArenaScenarioDefinition& definition,
       QVector3D origin)
      : world(world_value)
      , host(std::move(host_value))
      , scenario(definition)
      , world_origin(origin)
      , steps(definition.steps.size())
      , duration_limit(definition.duration_seconds) {
    report.scenario_id = definition.id;
  }

  [[nodiscard]] auto
  group_definition(const QString& name) const -> const ArenaScenarioGroup* {
    auto const found =
        std::find_if(scenario.groups.begin(),
                     scenario.groups.end(),
                     [&](auto const& candidate) { return candidate.name == name; });
    return found == scenario.groups.end() ? nullptr : &*found;
  }

  [[nodiscard]] auto
  ids(const QString& group) const -> const std::vector<Engine::Core::EntityID>& {
    static const std::vector<Engine::Core::EntityID> empty;
    auto const found = groups.constFind(group);
    return found == groups.cend() ? empty : found.value();
  }

  [[nodiscard]] auto step_entities(const ArenaScenarioStep& step) const
      -> std::vector<Engine::Core::EntityID> {
    if (step.zone_id.isEmpty()) {
      return ids(step.group);
    }
    auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
    auto const shrine_id = undead != nullptr ? undead->anchor_entity(step.zone_id) : 0U;
    if (shrine_id == 0U) {
      return {};
    }
    return {shrine_id};
  }

  [[nodiscard]] auto entity_alive(Engine::Core::EntityID entity_id) const -> bool {
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    return unit != nullptr && unit->health > 0 &&
           !world.has<Engine::Core::PendingRemovalComponent>(entity_id);
  }

  [[nodiscard]] auto group_health(const QString& group) const -> int {
    int total = 0;
    for (auto entity_id : ids(group)) {
      auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
      if (unit != nullptr) {
        total += std::max(0, unit->health);
      }
    }
    return total;
  }

  [[nodiscard]] auto commander_frames() const -> std::vector<const TraceFrame*> {
    std::vector<const TraceFrame*> frames;
    frames.reserve(trace.size());
    for (auto const& frame : trace) {
      if (frame.commander.valid) {
        frames.push_back(&frame);
      }
    }
    return frames;
  }

  [[nodiscard]] auto group_destroyed(const QString& group) const -> bool {
    auto const& group_ids = ids(group);
    return !group_ids.empty() &&
           std::none_of(group_ids.begin(), group_ids.end(), [&](auto entity_id) {
             return entity_alive(entity_id);
           });
  }

  [[nodiscard]] auto centroid(const QString& group) const -> std::optional<QVector3D> {
    QVector3D total;
    int count = 0;
    for (auto const entity_id : ids(group)) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      total += vector_from_transform(*transform);
      ++count;
    }
    return count > 0 ? std::optional<QVector3D>(total / static_cast<float>(count))
                     : std::nullopt;
  }

  void track_rpg_aim() {
    if (rpg_aim_shooter_group.isEmpty() || rpg_aim_target_group.isEmpty()) {
      return;
    }
    auto const target = centroid(rpg_aim_target_group);
    if (!target.has_value()) {

      rpg_aim_shooter_group.clear();
      rpg_aim_target_group.clear();
      return;
    }

    constexpr float k_chest_height = 1.15F;
    QVector3D const aim_point = *target + QVector3D(0.0F, k_chest_height, 0.0F);
    for (auto entity_id : ids(rpg_aim_shooter_group)) {
      if (host.aim_rpg_view_at) {
        host.aim_rpg_view_at(entity_id, aim_point);
        continue;
      }
      if (!host.set_rpg_view_yaw || !host.set_rpg_view_pitch) {
        continue;
      }
      auto const* shooter = world.get_entity(entity_id);
      auto const* shooter_transform =
          shooter != nullptr
              ? shooter->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      if (shooter_transform == nullptr) {
        continue;
      }
      constexpr float k_eye_height = 1.55F;
      float const dx = aim_point.x() - shooter_transform->position.x;
      float const dz = aim_point.z() - shooter_transform->position.z;
      float const flat = std::sqrt((dx * dx) + (dz * dz));
      host.set_rpg_view_yaw(entity_id,
                            std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>);
      host.set_rpg_view_pitch(
          entity_id,
          std::atan2(aim_point.y() - (shooter_transform->position.y + k_eye_height),
                     std::max(flat, 0.01F)) *
              180.0F / std::numbers::pi_v<float>);
    }
  }

  [[nodiscard]] auto groups_distance(const QString& lhs,
                                     const QString& rhs) const -> float {
    float closest = std::numeric_limits<float>::max();
    for (auto const lhs_id : ids(lhs)) {
      auto const* lhs_transform =
          world.try_get<Engine::Core::TransformComponent>(lhs_id);
      if (lhs_transform == nullptr || !entity_alive(lhs_id)) {
        continue;
      }
      for (auto const rhs_id : ids(rhs)) {
        auto const* rhs_transform =
            world.try_get<Engine::Core::TransformComponent>(rhs_id);
        if (rhs_transform == nullptr || !entity_alive(rhs_id)) {
          continue;
        }
        closest = std::min(closest,
                           horizontal_distance(vector_from_transform(*lhs_transform),
                                               vector_from_transform(*rhs_transform)));
      }
    }
    return closest;
  }

  void add_issue(QString code,
                 QString message,
                 Engine::Core::EntityID entity_id = 0,
                 int soldier_index = -1) {
    QString const key =
        QStringLiteral("%1:%2:%3").arg(code).arg(entity_id).arg(soldier_index);
    if (issue_keys.contains(key)) {
      return;
    }
    issue_keys.insert(key);
    report.issues.push_back(
        {std::move(code), std::move(message), elapsed, entity_id, soldier_index});
  }

  void spawn_group(const ArenaScenarioGroup& group) {
    if (!ids(group.name).empty() || !host.spawn_unit) {
      return;
    }
    auto& spawned = groups[group.name];
    spawned.reserve(static_cast<std::size_t>(group.count));
    float const center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
    for (int index = 0; index < group.count; ++index) {
      QVector3D const position = world_origin + group.origin +
                                 group.spacing * (static_cast<float>(index) - center);
      Engine::Core::EntityID const entity_id = host.spawn_unit(group, position);
      if (entity_id == 0U) {
        add_issue(
            QStringLiteral("spawn_failed"),
            QStringLiteral("%1: failed to spawn member %2").arg(group.name).arg(index));
        continue;
      }
      spawned.push_back(entity_id);
      entity_groups.insert(entity_id, group.name);
      auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
      if (unit != nullptr) {
        initial_health_by_group[group.name] += unit->health;
      }
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform != nullptr) {
        entity_states[entity_id] = {
            vector_from_transform(*transform), elapsed, 0, 1.0F};
      }
    }
    QVector3D axis = group.spacing;
    axis.setY(0.0F);
    if (axis.lengthSquared() < 0.0001F) {
      axis = QVector3D(1.0F, 0.0F, 0.0F);
    }
    axis.normalize();
    auto& projections = initial_formation_projection[group.name];
    for (auto entity_id : spawned) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      projections.push_back(
          transform != nullptr
              ? QVector3D::dotProduct(vector_from_transform(*transform), axis)
              : 0.0F);
    }
  }

  [[nodiscard]] auto trigger_ready(std::size_t index,
                                   const ArenaScenarioStep& step) const -> bool {
    auto const& trigger = step.trigger;
    switch (trigger.kind) {
    case ScenarioTriggerKind::AtTime:
      return elapsed + 1.0e-5F >= trigger.time_seconds;
    case ScenarioTriggerKind::GroupDestroyed:
      return group_destroyed(trigger.group);
    case ScenarioTriggerKind::FirstContact:
    case ScenarioTriggerKind::GroupsWithinDistance: {
      float const threshold = trigger.distance > 0.0F ? trigger.distance : 2.5F;
      return groups_distance(trigger.group, trigger.target_group) <= threshold;
    }
    case ScenarioTriggerKind::GroupEnteredArea: {
      auto const group_center = centroid(trigger.group);
      float const radius = trigger.distance > 0.0F ? trigger.distance : 1.0F;
      return group_center.has_value() &&
             horizontal_distance(*group_center, world_origin + trigger.position) <=
                 radius;
    }
    case ScenarioTriggerKind::PreviousStepComplete:
      return index == 0 || steps[index - 1].executed;
    }
    return false;
  }

  void arm_response(const QString& group, const QString& command) {
    float threshold = k_default_response_seconds;
    bool response_required = false;
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind == ArenaExpectationKind::AllGroupsRespondWithin &&
          (expectation.group.isEmpty() || expectation.group == group) &&
          expectation_active(expectation)) {
        response_required = true;
        if (expectation.threshold > 0.0F) {
          threshold = expectation.threshold;
        }
      }
    }
    if (!response_required) {
      return;
    }
    for (auto entity_id : ids(group)) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      responses[entity_id] = {elapsed,
                              elapsed + threshold,
                              transform != nullptr ? vector_from_transform(*transform)
                                                   : QVector3D{},
                              transform != nullptr ? transform->rotation.y : 0.0F,
                              command,
                              false,
                              false};
    }
  }

  void stop_group(const QString& group, bool clear_attack) {
    for (auto entity_id : ids(group)) {
      auto* entity = world.get_entity(entity_id);
      if (entity == nullptr) {
        continue;
      }
      if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
        movement->stop();
      }
      if (clear_attack) {
        entity->remove_component<Engine::Core::AttackTargetComponent>();
      }
    }
  }

  void attack_group(const QString& group, const QString& target_group, bool chase) {
    auto const& attackers = ids(group);
    auto const& targets = ids(target_group);
    if (targets.empty()) {
      add_issue(
          QStringLiteral("command_target_missing"),
          QStringLiteral("%1 cannot attack empty group %2").arg(group, target_group));
      return;
    }
    std::vector<Engine::Core::EntityID> living;
    living.reserve(targets.size());
    for (auto target : targets) {
      if (entity_alive(target)) {
        living.push_back(target);
      }
    }
    if (living.empty()) {
      add_issue(
          QStringLiteral("command_target_missing"),
          QStringLiteral("%1 cannot attack empty group %2").arg(group, target_group));
      return;
    }

    const std::size_t share = (attackers.size() + living.size() - 1U) / living.size();
    std::unordered_map<Engine::Core::EntityID, std::size_t> assigned;

    const auto position_of =
        [&](Engine::Core::EntityID entity_id) -> std::optional<QVector3D> {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform == nullptr) {
        return std::nullopt;
      }
      return vector_from_transform(*transform);
    };

    for (auto attacker : attackers) {
      auto const from = position_of(attacker);
      Engine::Core::EntityID chosen = living.front();
      float best = std::numeric_limits<float>::max();
      bool found = false;
      for (bool honour_share : {true, false}) {
        for (auto target : living) {
          if (honour_share && assigned[target] >= share) {
            continue;
          }
          auto const to = position_of(target);
          float const distance = (from.has_value() && to.has_value())
                                     ? horizontal_distance(*from, *to)
                                     : 0.0F;
          if (!found || distance < best) {
            best = distance;
            chosen = target;
            found = true;
          }
        }
        if (found) {
          break;
        }
      }
      Game::Systems::CommandService::attack_target(world, {attacker}, chosen, chase);
      ++assigned[chosen];
    }
  }

  void form_army(const ArenaScenarioStep& step) {
    std::vector<Engine::Core::EntityID> members;
    QStringList sources = step.formation.groups;
    if (sources.isEmpty()) {
      sources.append(step.group);
    }
    for (auto const& name : sources) {
      auto const& group_ids = ids(name);
      members.insert(members.end(), group_ids.begin(), group_ids.end());
    }
    members.erase(
        std::remove_if(members.begin(),
                       members.end(),
                       [&](auto entity_id) { return !entity_alive(entity_id); }),
        members.end());
    if (members.empty()) {
      add_issue(
          QStringLiteral("formation_members_missing"),
          QStringLiteral("FormArmy step '%1' resolved no living units").arg(step.name));
      return;
    }

    Game::Formation::ArmyFormationRequest request;
    request.members = members;
    request.anchor = world_origin + step.formation.anchor;
    request.facing = step.formation.facing_degrees;
    request.frontage = step.formation.frontage;
    request.intent = step.formation.intent;
    request.options = step.formation.options;
    if (!step.formation.doctrine.isEmpty()) {
      request.doctrine = step.formation.doctrine.toStdString();
      request.options.doctrine_locked = true;
    }
    if (step.formation.spacing > 0.0F) {
      request.spacing = step.formation.spacing;
    }

    auto const result = Game::Formation::ArmyFormationService::commit(world, request);
    if (!result.valid) {
      add_issue(QStringLiteral("formation_rejected"),
                QStringLiteral("FormArmy step '%1' was rejected: %2")
                    .arg(step.name, QString::fromStdString(result.rejection_reason)));
      return;
    }
    if (result.positions.size() != members.size()) {
      add_issue(QStringLiteral("formation_plan_mismatch"),
                QStringLiteral("FormArmy step '%1' planned %2 slots for %3 units")
                    .arg(step.name)
                    .arg(result.positions.size())
                    .arg(members.size()));
      return;
    }

    for (std::size_t i = 0; i < members.size(); ++i) {
      auto* entity = world.get_entity(members[i]);
      if (entity == nullptr) {
        continue;
      }
      if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
        if (i < result.facing_angles.size()) {
          transform->desired_yaw = result.facing_angles[i];
          transform->has_desired_yaw = true;
        }
      }
      auto* formation_mode =
          entity->get_component<Engine::Core::FormationModeComponent>();
      if (formation_mode != nullptr && i < result.stable_slot_ids.size()) {
        formation_mode->formation_id = result.group_id;
        formation_mode->stable_slot_id = result.stable_slot_ids[i];
        formation_mode->stable_rank = result.stable_ranks[i];
        formation_mode->stable_file = result.stable_files[i];
        formation_mode->stable_slot_x = result.positions[i].x();
        formation_mode->stable_slot_z = result.positions[i].z();
      }
    }

    Game::Systems::CommandService::MoveOptions options;
    options.kind = Game::Systems::MoveOrderKind::FormationMove;
    options.preserve_formation_mode = result.used_army_formation;
    Game::Systems::CommandService::move_units(
        world, members, result.positions, options);

    for (auto const& name : sources) {
      arm_response(name, command_name(step.command));
    }
  }

  void execute_step(std::size_t index, const ArenaScenarioStep& step) {
    auto& runtime = steps[index];
    runtime.executed = true;
    runtime.executed_at = elapsed;

    switch (step.command) {
    case ScenarioCommandKind::Stand:
    case ScenarioCommandKind::Stop:
      stop_group(step.group, true);
      break;
    case ScenarioCommandKind::Move: {
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination);
      if (plan.fully_placeable_for(group_ids)) {
        Game::Systems::CommandService::move_units(
            world, group_ids, plan.target_positions());
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::FormationMove: {
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination, true);
      Game::Systems::CommandService::issue_ground_move(world, group_ids, plan);
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::Run: {
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination, true);
      Game::Systems::CommandService::issue_ground_move(world, group_ids, plan);
      for (auto entity_id : group_ids) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_run_mode(step.enabled);
          }
        }
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::FormArmy:
      form_army(step);
      break;
    case ScenarioCommandKind::Charge:
      for (auto entity_id : ids(step.group)) {
        if (auto* entity = world.get_entity(entity_id)) {
          (void)Game::Systems::Combat::request_mounted_charge(
              *entity, Engine::Core::MountedChargeIntentSource::Player);
        }
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_run_mode(true);
          }
        }
      }
      attack_group(step.group, step.target_group, true);
      arm_response(step.group, command_name(step.command));
      break;
    case ScenarioCommandKind::Attack:
    case ScenarioCommandKind::AttackMove:
    case ScenarioCommandKind::ReleaseReserve:
      attack_group(step.group,
                   step.target_group,
                   step.command == ScenarioCommandKind::AttackMove ||
                       step.command == ScenarioCommandKind::ReleaseReserve ||
                       step.chase);
      arm_response(step.group, command_name(step.command));
      break;
    case ScenarioCommandKind::Hold:
      for (auto entity_id : ids(step.group)) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_hold_mode(step.enabled);
          }
        }
      }
      break;
    case ScenarioCommandKind::Guard: {
      auto const& targets = ids(step.target_group);
      auto const& subjects = ids(step.group);

      const int guard_owner_id =
          subjects.empty() ? 0 : owner_of(world, subjects.front());

      Game::Command::Command guard_command{};
      guard_command.source = Game::Command::Source::Script;
      guard_command.owner_id = guard_owner_id;
      Game::Command::SetGuard payload{};
      payload.units = subjects;
      payload.active = step.enabled;
      if (step.enabled && !targets.empty()) {
        if (auto const target_centre = centroid(step.target_group)) {
          payload.anchor = *target_centre;
          payload.has_anchor = true;
        }
      }
      guard_command.payload = std::move(payload);
      Game::Command::dispatch(world, guard_command);

      if (step.enabled && !targets.empty()) {
        for (auto entity_id : subjects) {
          if (host.find_unit) {
            if (auto* unit = host.find_unit(entity_id)) {
              unit->set_guard_target(targets.front());
            }
          }
        }
      } else if (!step.enabled) {
        for (auto entity_id : subjects) {
          if (host.find_unit) {
            if (auto* unit = host.find_unit(entity_id)) {
              unit->clear_guard_mode();
            }
          }
        }
      }
      break;
    }
    case ScenarioCommandKind::SpawnAmbush:
      if (auto const* group = group_definition(step.group)) {
        spawn_group(*group);
      }
      if (!step.target_group.isEmpty()) {
        attack_group(step.group, step.target_group, true);
        arm_response(step.group, command_name(step.command));
      }
      break;
    case ScenarioCommandKind::RepairStructure: {
      auto const& structures = ids(step.target_group);
      auto const workers = ids(step.group);
      if (structures.empty() || workers.empty()) {
        break;
      }
      Game::Command::dispatch(
          world,
          Game::Command::Command{
              .source = Game::Command::Source::Script,
              .owner_id = owner_of(world, workers.front()),
              .payload = Game::Command::RepairStructure{
                  .units = workers, .structure = structures.front()}});
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::DeliverToStructure: {
      auto const& structures = ids(step.target_group);
      auto const carriers = ids(step.group);
      if (structures.empty() || carriers.empty()) {
        break;
      }
      Game::Command::dispatch(
          world,
          Game::Command::Command{
              .source = Game::Command::Source::Script,
              .owner_id = owner_of(world, carriers.front()),
              .payload = Game::Command::DeliverCivilians{
                  .units = carriers, .barracks = structures.front()}});
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::SetFarmGrowth:
      for (auto entity_id : step_entities(step)) {
        auto* entity = world.get_entity(entity_id);
        auto* farm = entity != nullptr
                         ? entity->get_component<Engine::Core::FarmComponent>()
                         : nullptr;
        if (farm != nullptr) {
          farm->growth =
              std::clamp(static_cast<float>(step.value) / 100.0F, 0.0F, 1.0F);
        }
      }
      break;
    case ScenarioCommandKind::HarvestResource: {
      if (host.terrain == nullptr) {
        break;
      }
      auto& terrain = *host.terrain;
      const QString kind = step.resource_kind;
      if (kind == QStringLiteral("grain") || kind == QStringLiteral("sheep")) {
        const std::string_view product =
            kind == QStringLiteral("grain")
                ? Game::Systems::k_builder_product_harvest_grain
                : Game::Systems::k_builder_product_slaughter_sheep;
        for (auto entity_id : ids(step.group)) {
          auto* entity = world.get_entity(entity_id);
          auto* transform =
              entity != nullptr
                  ? entity->get_component<Engine::Core::TransformComponent>()
                  : nullptr;
          if (transform == nullptr) {
            continue;
          }
          const int owner_id = owner_of(world, entity_id);
          const auto target =
              Game::Systems::find_food_target_near(world,
                                                   product,
                                                   owner_id,
                                                   transform->position.x,
                                                   transform->position.z,
                                                   0.0F,
                                                   entity_id);
          if (!target.has_value()) {
            continue;
          }
          Game::Command::dispatch(
              world,
              Game::Command::Command{
                  .source = Game::Command::Source::Script,
                  .owner_id = owner_id,
                  .payload = Game::Command::StartHarvest{
                      .units = {entity_id},
                      .construction_type = std::string(product),
                      .resource_target = target->id,
                      .site = QVector3D(target->x, 0.0F, target->z)}});
        }
        arm_response(step.group, command_name(step.command));
        break;
      }
      const auto matches = [&kind](Game::Map::WorldProp::Type type) {
        if (kind == QStringLiteral("tree")) {
          return Game::Map::is_tree_world_prop_type(type);
        }
        if (kind == QStringLiteral("boulder")) {
          return Game::Map::is_boulder_world_prop_type(type);
        }
        if (kind == QStringLiteral("iron_ore")) {
          return Game::Map::is_iron_ore_world_prop_type(type);
        }
        return false;
      };
      const std::string product =
          kind == QStringLiteral("boulder")
              ? std::string(Game::Systems::k_builder_product_collect_stone)
          : kind == QStringLiteral("iron_ore")
              ? std::string(Game::Systems::k_builder_product_collect_iron_ore)
              : std::string(Game::Systems::k_builder_product_cut_tree);

      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* builder =
            entity != nullptr
                ? entity->get_component<Engine::Core::BuilderProductionComponent>()
                : nullptr;
        auto* transform =
            entity != nullptr
                ? entity->get_component<Engine::Core::TransformComponent>()
                : nullptr;
        if (builder == nullptr || transform == nullptr) {
          continue;
        }

        const Game::Map::WorldProp* best = nullptr;
        float best_distance_sq = std::numeric_limits<float>::infinity();
        for (const auto& candidate : terrain.world_props()) {
          if (!matches(candidate.type) ||
              terrain.is_world_prop_reserved(candidate.id)) {
            continue;
          }
          const QVector3D at = terrain.world_prop_world_position(candidate);
          const float dx = at.x() - transform->position.x;
          const float dz = at.z() - transform->position.z;
          const float distance_sq = dx * dx + dz * dz;
          if (distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best = &candidate;
          }
        }
        if (best == nullptr) {
          continue;
        }

        const QVector3D at = terrain.world_prop_world_position(*best);
        Game::Command::dispatch(
            world,
            Game::Command::Command{
                .source = Game::Command::Source::Script,
                .owner_id = owner_of(world, entity_id),
                .payload = Game::Command::StartHarvest{.units = {entity_id},
                                                       .construction_type = product,
                                                       .resource_target = best->id,
                                                       .site = at}});
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::AbandonWork: {

      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* builder =
            entity != nullptr
                ? entity->get_component<Engine::Core::BuilderProductionComponent>()
                : nullptr;
        if (builder == nullptr) {
          continue;
        }
        builder->report_fault(Engine::Core::BuilderTaskFault::Interrupted,
                              std::max(1.0F, static_cast<float>(step.value)));
        builder->in_progress = false;
        builder->at_construction_site = false;
      }
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination);
      if (plan.fully_placeable_for(group_ids)) {
        Game::Systems::CommandService::move_units(
            world, group_ids, plan.target_positions());
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::SetCamera:
      if (host.set_camera) {
        host.set_camera(all_entities(),
                        {step.camera_distance, step.camera_angle, step.camera_yaw});
      }
      break;
    case ScenarioCommandKind::SetHealth:
      for (auto entity_id : step_entities(step)) {
        auto* entity = world.get_entity(entity_id);
        auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
        if (unit != nullptr) {
          unit->health = std::clamp(step.value, 0, unit->max_health);
        }
      }
      break;
    case ScenarioCommandKind::ApplyDamage: {
      Engine::Core::EntityID const attacker_id =
          !ids(step.target_group).empty() ? ids(step.target_group).front() : 0U;
      for (auto entity_id : step_entities(step)) {
        auto* entity = world.get_entity(entity_id);
        (void)Game::Systems::Combat::apply_unit_damage(
            &world, entity, std::max(0, step.value), attacker_id);
      }
      break;
    }
    case ScenarioCommandKind::MeleeLock: {
      attack_group(step.group, step.target_group, false);
      auto const& targets = ids(step.target_group);
      if (!targets.empty()) {
        for (auto entity_id : ids(step.group)) {
          auto* entity = world.get_entity(entity_id);
          auto* attack = entity != nullptr
                             ? entity->get_component<Engine::Core::AttackComponent>()
                             : nullptr;
          if (attack != nullptr) {
            attack->in_melee_lock = true;
            attack->melee_lock_target_id = targets.front();
            attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
            attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
          }
        }
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::SetFullCreatureLod:
      if (host.set_force_full_creature_lod) {
        host.set_force_full_creature_lod(step.enabled);
      }
      break;
    case ScenarioCommandKind::TriggerCommanderAura:
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* commander =
            entity != nullptr
                ? entity->get_component<Engine::Core::CommanderComponent>()
                : nullptr;
        if (commander == nullptr) {
          add_issue(QStringLiteral("aura_commander_missing"),
                    QStringLiteral("%1 contains no commander for aura activation")
                        .arg(step.group),
                    entity_id);
          continue;
        }
        if (step.value > 0) {
          commander->aura_ability_duration = static_cast<float>(step.value);
        }
        commander->request_aura_ability();
      }
      break;
    case ScenarioCommandKind::TriggerFlagRally:
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* commander =
            entity != nullptr
                ? entity->get_component<Engine::Core::CommanderComponent>()
                : nullptr;
        if (commander == nullptr) {
          add_issue(QStringLiteral("flag_rally_commander_missing"),
                    QStringLiteral("%1 contains no commander for a flag rally")
                        .arg(step.group),
                    entity_id);
          continue;
        }
        auto const* transform =
            entity->get_component<Engine::Core::TransformComponent>();
        if (transform == nullptr) {
          add_issue(QStringLiteral("flag_rally_transform_missing"),
                    QStringLiteral("%1 has no transform to plant a standard on")
                        .arg(step.group),
                    entity_id);
          continue;
        }

        if (step.value < 0) {
          commander->cancel_flag_rally();
          continue;
        }

        commander->begin_flag_rally(transform->position.x, transform->position.z, true);
      }
      break;
    case ScenarioCommandKind::RpgPrimaryAttack:
      for (auto entity_id : ids(step.group)) {
        if (!host.rpg_primary_attack || !host.rpg_primary_attack(entity_id)) {
          add_issue(QStringLiteral("rpg_attack_failed"),
                    QStringLiteral("%1 could not start an RPG primary attack")
                        .arg(step.group),
                    entity_id);
        }
      }
      break;
    case ScenarioCommandKind::RpgHeavyAttack:
      for (auto entity_id : ids(step.group)) {
        if (!host.rpg_heavy_attack || !host.rpg_heavy_attack(entity_id)) {
          add_issue(
              QStringLiteral("rpg_heavy_attack_failed"),
              QStringLiteral("%1 could not start an RPG heavy attack").arg(step.group),
              entity_id);
        }
      }
      break;
    case ScenarioCommandKind::RpgAttackHold:
      for (auto entity_id : ids(step.group)) {
        if (!host.set_rpg_attack_held) {
          add_issue(
              QStringLiteral("rpg_attack_hold_unavailable"),
              QStringLiteral("%1 has no RPG attack-hold host callback").arg(step.group),
              entity_id);
          continue;
        }
        host.set_rpg_attack_held(entity_id, step.enabled);
      }
      break;
    case ScenarioCommandKind::RpgAim:
      if (!step.target_group.isEmpty()) {

        rpg_aim_shooter_group = step.group;
        rpg_aim_target_group = step.target_group;
        track_rpg_aim();
        break;
      }
      rpg_aim_shooter_group.clear();
      rpg_aim_target_group.clear();
      for (auto entity_id : ids(step.group)) {
        if (!host.set_rpg_view_yaw || !host.set_rpg_view_pitch) {
          add_issue(QStringLiteral("rpg_aim_unavailable"),
                    QStringLiteral("%1 has no RPG view host callbacks").arg(step.group),
                    entity_id);
          continue;
        }
        host.set_rpg_view_yaw(entity_id, step.rpg_view_yaw_degrees.value_or(0.0F));
        host.set_rpg_view_pitch(entity_id, step.rpg_view_pitch_degrees.value_or(0.0F));
      }
      break;
    case ScenarioCommandKind::RpgGuard:
      for (auto entity_id : ids(step.group)) {
        if (!host.set_rpg_guard) {
          add_issue(QStringLiteral("rpg_guard_unavailable"),
                    QStringLiteral("%1 has no RPG guard host callback").arg(step.group),
                    entity_id);
          continue;
        }
        host.set_rpg_guard(entity_id, step.enabled);
      }
      break;
    case ScenarioCommandKind::RpgDodge:
      for (auto entity_id : ids(step.group)) {
        if (!host.request_rpg_dodge) {
          add_issue(QStringLiteral("rpg_dodge_unavailable"),
                    QStringLiteral("%1 has no RPG dodge host callback").arg(step.group),
                    entity_id);
          continue;
        }
        host.request_rpg_dodge(entity_id, step.destination);
      }
      break;
    case ScenarioCommandKind::RpgJump:
      for (auto entity_id : ids(step.group)) {
        if (!host.request_rpg_jump) {
          add_issue(QStringLiteral("rpg_jump_unavailable"),
                    QStringLiteral("%1 has no RPG jump host callback").arg(step.group),
                    entity_id);
          continue;
        }
        host.request_rpg_jump(entity_id);
      }
      break;
    case ScenarioCommandKind::RpgSpecial:
      for (auto entity_id : ids(step.group)) {
        if (!host.request_rpg_special) {
          add_issue(
              QStringLiteral("rpg_special_unavailable"),
              QStringLiteral("%1 has no RPG special host callback").arg(step.group),
              entity_id);
          continue;
        }
        host.request_rpg_special(entity_id);
      }
      break;
    case ScenarioCommandKind::RpgWeaponSwitch:
      for (auto entity_id : ids(step.group)) {
        if (!host.request_rpg_weapon_switch) {
          add_issue(QStringLiteral("rpg_weapon_switch_unavailable"),
                    QStringLiteral("%1 has no RPG weapon-switch host callback")
                        .arg(step.group),
                    entity_id);
          continue;
        }
        host.request_rpg_weapon_switch(entity_id);
      }
      break;
    case ScenarioCommandKind::RpgCycleLockOn:
      for (auto entity_id : ids(step.group)) {
        if (!host.cycle_rpg_lock_on) {
          add_issue(
              QStringLiteral("rpg_lock_on_unavailable"),
              QStringLiteral("%1 has no RPG lock-on host callback").arg(step.group),
              entity_id);
          continue;
        }
        host.cycle_rpg_lock_on(entity_id);
      }
      break;
    case ScenarioCommandKind::RpgMove:
      for (auto entity_id : ids(step.group)) {
        if (!host.set_rpg_move_input) {
          add_issue(QStringLiteral("rpg_move_unavailable"),
                    QStringLiteral("%1 has no RPG move host callback").arg(step.group),
                    entity_id);
          continue;
        }
        if (step.rpg_view_yaw_degrees.has_value() && host.set_rpg_view_yaw) {
          host.set_rpg_view_yaw(entity_id, *step.rpg_view_yaw_degrees);
        }
        host.set_rpg_move_input(entity_id, step.destination, step.value != 0);
      }
      break;
    case ScenarioCommandKind::ReloadUndeadZoneState: {
      auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
      if (undead == nullptr) {
        add_issue(QStringLiteral("undead_zone_reload_unavailable"),
                  QStringLiteral("the scene has no undead awakening system to "
                                 "save and restore"));
        break;
      }
      undead->restore_state(undead->serialize_state());
      break;
    }
    }
  }

  struct WildlifeObservation {
    bool grazing_seen{false};
    bool flee_seen{false};
    bool hunt_seen{false};
    int min_population{-1};
    int peak_population{0};
    std::uint64_t bird_scatter_events{0U};
    std::uint64_t bird_flyovers{0U};
  };

  WildlifeObservation wildlife_observation;

  static void record_animal(Engine::Core::Entity& entity,
                            const Engine::Core::UnitComponent& unit,
                            const Engine::Core::WildlifeComponent& wildlife,
                            TraceFrame& frame) {
    auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return;
    }
    auto const behavior_name = [&]() -> QString {
      switch (wildlife.behavior) {
      case Game::Wildlife::Behavior::Graze:
        return QStringLiteral("graze");
      case Game::Wildlife::Behavior::Flee:
        return QStringLiteral("flee");
      case Game::Wildlife::Behavior::Stalk:
        return QStringLiteral("stalk");
      case Game::Wildlife::Behavior::Roam:
        return QStringLiteral("roam");
      default:
        return QStringLiteral("other");
      }
    }();

    TraceAnimal animal;
    animal.entity_id = entity.get_id();
    animal.position = vector_from_transform(*transform);
    animal.health = unit.health;
    animal.species = wildlife.species == Game::Wildlife::Species::Wolf
                         ? QStringLiteral("wolf")
                         : QStringLiteral("sheep");
    animal.behavior = behavior_name;
    animal.focus_id = wildlife.focus_id;
    animal.yaw = transform->rotation.y;
    animal.desired_yaw = transform->desired_yaw;
    animal.has_desired_yaw = transform->has_desired_yaw;
    if (auto const* movement =
            entity.get_component<Engine::Core::MovementComponent>()) {
      animal.vx = movement->get_vx();
      animal.vz = movement->get_vz();
    }
    animal.biting = wildlife.bite_timer > 0.0F;
    animal.bite_phase =
        animal.biting
            ? 1.0F - wildlife.bite_timer /
                         Engine::Core::WildlifeComponent::k_bite_animation_seconds
            : -1.0F;
    animal.flinch_phase =
        wildlife.flinch_timer > 0.0F
            ? 1.0F - wildlife.flinch_timer /
                         Engine::Core::WildlifeComponent::k_flinch_animation_seconds
            : -1.0F;
    animal.bite_target_id = wildlife.bite_target_id;
    animal.impact_pending = wildlife.bite_impact_pending;
    animal.dying = entity.has_component<Engine::Core::DeathAnimationComponent>();
    frame.animals.push_back(std::move(animal));
  }

  void record_animals(TraceFrame& frame) {
    for (auto* entity :
         world.collect_entities_with<Engine::Core::WildlifeComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto const* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
      if (unit == nullptr || wildlife == nullptr) {
        continue;
      }
      record_animal(*entity, *unit, *wildlife, frame);
    }
  }

  void observe_wildlife() {
    int population = 0;
    for (auto [id, unit, wildlife] :
         world.view<Engine::Core::UnitComponent, Engine::Core::WildlifeComponent>()) {
      if (unit.health <= 0) {
        continue;
      }
      ++population;
      switch (wildlife.behavior) {
      case Game::Wildlife::Behavior::Graze:
        wildlife_observation.grazing_seen = true;
        break;
      case Game::Wildlife::Behavior::Flee:
        wildlife_observation.flee_seen = true;
        break;
      case Game::Wildlife::Behavior::Stalk:
        wildlife_observation.hunt_seen = true;
        break;
      default:
        break;
      }
    }

    for (auto const& bird : Game::Wildlife::BirdFlockManager::instance().birds()) {
      ++population;
      if (bird.behavior == Game::Wildlife::Behavior::Scatter) {
        wildlife_observation.flee_seen = true;
      }
    }
    wildlife_observation.bird_scatter_events =
        Game::Wildlife::BirdFlockManager::instance().stats().scatter_events;
    wildlife_observation.bird_flyovers =
        Game::Wildlife::BirdFlockManager::instance().stats().flyovers_launched;

    wildlife_observation.peak_population =
        std::max(wildlife_observation.peak_population, population);
    wildlife_observation.min_population =
        wildlife_observation.min_population < 0
            ? population
            : std::min(wildlife_observation.min_population, population);
  }

  void initialize_battle_sides() {
    battle_sides.clear();
    if (scenario.battle_sides.size() < 2U) {
      return;
    }
    for (auto const& side : scenario.battle_sides) {
      BattleSideState state;
      state.owner_id = side.owner_id;
      state.home_radius = side.home_radius;
      state.label = side.label.isEmpty() ? QStringLiteral("owner_%1").arg(side.owner_id)
                                         : side.label;
      state.home = world_origin + side.home;
      battle_sides.push_back(std::move(state));
    }
    for (auto& state : battle_sides) {
      QVector3D enemy_sum;
      int enemy_count = 0;
      for (auto const& other : battle_sides) {
        if (other.owner_id == state.owner_id) {
          continue;
        }
        enemy_sum += other.home;
        ++enemy_count;
      }
      if (enemy_count == 0) {
        continue;
      }
      state.enemy_home = enemy_sum / static_cast<float>(enemy_count);
      state.separation = horizontal_distance(state.home, state.enemy_home);
      state.has_axis = state.separation > 1.0F;
    }
    report.battle.tracked = !battle_sides.empty();
    observe_battle();
    for (auto& state : battle_sides) {
      state.initial_units = state.living_units;
      state.initial_soldiers = state.living_soldiers;
      state.initial_buildings = state.seen_buildings;
    }
  }

  void observe_battle() {
    if (battle_sides.empty()) {
      return;
    }

    struct Accumulator {
      int units{0};

      int soldiers{0};
      int buildings{0};
      QVector3D army_sum;
      int army_count{0};
      int home_units{0};
      int forward_units{0};
    };
    std::vector<Accumulator> accumulators(battle_sides.size());

    for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
      auto const* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (unit == nullptr || unit->health <= 0) {
        continue;
      }
      std::size_t index = battle_sides.size();
      for (std::size_t i = 0; i < battle_sides.size(); ++i) {
        if (battle_sides[i].owner_id == unit->owner_id) {
          index = i;
          break;
        }
      }
      if (index >= battle_sides.size()) {
        continue;
      }

      auto& side = battle_sides[index];
      auto& accumulator = accumulators[index];
      if (Game::Units::is_building_spawn(unit->spawn_type)) {
        ++accumulator.buildings;
        if (!side.seen_buildings.contains(entity->get_id())) {
          side.seen_buildings.insert(entity->get_id());
          side.building_census[QString::fromStdString(
              Game::Units::spawn_typeToString(unit->spawn_type))] += 1;
        }
        continue;
      }

      ++accumulator.units;
      accumulator.soldiers += Game::Systems::FormationCombat::living_slot_count(
          *entity,
          Game::Systems::FormationCombat::resolve_definition(*unit).total_count);
      side.seen_units.insert(entity->get_id());
      if (unit->spawn_type == Game::Units::SpawnType::Builder ||
          entity->has_component<Engine::Core::CommanderComponent>()) {
        continue;
      }
      auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        QVector3D const position = vector_from_transform(*transform);
        accumulator.army_sum += position;
        ++accumulator.army_count;
        if (horizontal_distance(position, side.home) <= side.home_radius) {
          ++accumulator.home_units;
        }
        if (side.has_axis) {
          QVector3D axis = side.enemy_home - side.home;
          axis.setY(0.0F);
          QVector3D offset = position - side.home;
          offset.setY(0.0F);
          if (QVector3D::dotProduct(offset, axis.normalized()) / side.separation >
              0.5F) {
            ++accumulator.forward_units;
          }
        }
      }
    }

    for (std::size_t i = 0; i < battle_sides.size(); ++i) {
      auto& side = battle_sides[i];
      auto const& accumulator = accumulators[i];
      if (host.sample_ai_doctrine) {
        auto const doctrine = host.sample_ai_doctrine(side.owner_id);
        if (doctrine.valid) {
          side.strategy = doctrine.strategy;
          side.posture = doctrine.posture;
          side.ai_state = doctrine.state;
          side.wave_committed = doctrine.wave_committed;
          side.wave_size = doctrine.wave_size;
          float const step = std::max(0.0F, elapsed - side.seconds_observed);
          side.seconds_observed = elapsed;
          if (doctrine.state == QStringLiteral("attacking")) {
            side.seconds_attacking += step;
          }
        }
      }
      side.living_units = accumulator.units;
      side.living_soldiers = accumulator.soldiers;
      side.living_buildings = accumulator.buildings;
      side.peak_units = std::max(side.peak_units, accumulator.units);
      side.peak_soldiers = std::max(side.peak_soldiers, accumulator.soldiers);
      side.peak_buildings = std::max(side.peak_buildings, accumulator.buildings);
      side.peak_home_units = std::max(side.peak_home_units, accumulator.home_units);
      side.peak_forward_units =
          std::max(side.peak_forward_units, accumulator.forward_units);
      if (accumulator.army_count > 0) {
        side.home_share_sum += static_cast<double>(accumulator.home_units) /
                               static_cast<double>(accumulator.army_count);
        ++side.home_share_samples;
      }
      if (accumulator.units > 0 || accumulator.buildings > 0) {
        side.had_presence = true;
      } else if (side.had_presence && side.eliminated_at < 0.0F) {
        side.eliminated_at = elapsed;
      }

      if (side.has_axis && accumulator.army_count > 0) {
        QVector3D const centroid =
            accumulator.army_sum / static_cast<float>(accumulator.army_count);
        QVector3D axis = side.enemy_home - side.home;
        axis.setY(0.0F);
        QVector3D offset = centroid - side.home;
        offset.setY(0.0F);
        float const projected =
            QVector3D::dotProduct(offset, axis.normalized()) / side.separation;
        side.final_advance = projected;
        side.peak_advance =
            side.has_advance ? std::max(side.peak_advance, projected) : projected;
        side.has_advance = true;
        side.advance_samples.emplace_back(elapsed, projected);
      }
    }

    if (!battle_decided) {
      int survivors = 0;
      for (auto const& side : battle_sides) {
        if (side.eliminated_at < 0.0F) {
          ++survivors;
        }
      }
      if (survivors <= 1 && battle_sides.size() >= 2U) {
        battle_decided = true;
        battle_decided_at = elapsed;
      }
    }
  }

  [[nodiscard]] auto battle_decision_ends_scenario() const -> bool {
    if (!battle_decided) {
      return false;
    }
    constexpr float k_decision_settle_seconds = 2.0F;
    if (elapsed < battle_decided_at + k_decision_settle_seconds) {
      return false;
    }
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind == ArenaExpectationKind::BattleReachesDecision) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] static auto
  side_result(const BattleSideState& side) -> ArenaBattleSideResult {
    ArenaBattleSideResult result;
    result.owner_id = side.owner_id;
    result.label = side.label;
    result.living_units = side.living_units;
    result.living_soldiers = side.living_soldiers;
    result.living_buildings = side.living_buildings;
    result.peak_units = side.peak_units;
    result.peak_soldiers = std::max(side.peak_soldiers, side.initial_soldiers);
    result.units_produced =
        std::max(0, static_cast<int>(side.seen_units.size()) - side.initial_units);
    result.peak_advance = side.peak_advance;
    result.final_advance = side.final_advance;
    result.eliminated_at = side.eliminated_at;
    result.strategy = side.strategy;
    result.posture = side.posture;
    result.ai_state = side.ai_state;
    result.wave_committed = side.wave_committed;
    result.wave_size = side.wave_size;
    result.buildings_constructed =
        std::max(0,
                 static_cast<int>(side.seen_buildings.size()) -
                     static_cast<int>(side.initial_buildings.size()));
    result.peak_buildings = side.peak_buildings;
    result.peak_home_units = side.peak_home_units;
    result.peak_forward_units = side.peak_forward_units;
    result.mean_home_share =
        side.home_share_samples > 0
            ? static_cast<float>(side.home_share_sum /
                                 static_cast<double>(side.home_share_samples))
            : 0.0F;
    {
      QStringList census;
      auto keys = side.building_census.keys();
      std::sort(keys.begin(), keys.end());
      for (auto const& key : keys) {
        census.push_back(
            QStringLiteral("%1x%2").arg(key).arg(side.building_census.value(key)));
      }
      result.building_census = census.join(QStringLiteral(","));
    }
    result.seconds_attacking = side.seconds_attacking;
    result.seconds_observed = side.seconds_observed;
    return result;
  }

  [[nodiscard]] auto live_sides() const -> std::vector<ArenaBattleSideResult> {
    std::vector<ArenaBattleSideResult> sides;
    sides.reserve(battle_sides.size());
    for (auto const& side : battle_sides) {
      sides.push_back(side_result(side));
    }
    return sides;
  }

  void publish_battle_outcome() {
    if (battle_sides.empty()) {
      return;
    }
    report.battle.tracked = true;
    report.battle.decided = battle_decided;
    report.battle.decided_at_seconds = battle_decided_at;
    report.battle.sides.clear();
    for (auto const& side : battle_sides) {
      report.battle.sides.push_back(side_result(side));
      if (battle_decided && side.eliminated_at < 0.0F) {
        report.battle.victor_owner_id = side.owner_id;
        report.battle.victor_label = side.label;
      }
    }
  }

  [[nodiscard]] static auto
  windowed_peak_advance(const BattleSideState& side,
                        const ArenaExpectation& expectation) -> std::optional<float> {
    float const start = expectation.start_seconds;
    float const end = expectation.end_seconds > 0.0F
                          ? expectation.end_seconds
                          : std::numeric_limits<float>::infinity();
    std::optional<float> peak;
    for (auto const& [time, advance] : side.advance_samples) {
      if (time < start || time > end) {
        continue;
      }
      peak = peak.has_value() ? std::max(*peak, advance) : advance;
    }
    return peak;
  }

  [[nodiscard]] auto battle_side(const QString& label) const -> const BattleSideState* {
    for (auto const& side : battle_sides) {
      if (side.label == label) {
        return &side;
      }
    }
    return nullptr;
  }

  void observe_undead_zones() {
    if (scenario.undead_zones.empty()) {
      return;
    }
    auto const entities = world.collect_entities_with<Engine::Core::UnitComponent>();
    for (auto const& zone : scenario.undead_zones) {
      auto& state = undead_zone_states[zone.id];
      auto& seen = undead_zone_entities[zone.id];
      int alive = 0;
      for (auto* entity : entities) {
        auto const* unit = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;

        if (unit == nullptr || unit->owner_id != zone.owner_id || unit->health <= 0 ||
            !Game::Units::is_troop_spawn(unit->spawn_type)) {
          continue;
        }
        ++alive;
        if (!seen.contains(entity->get_id())) {
          seen.insert(entity->get_id());
          ++state.spawned_total;
        }
      }
      if (alive > 0 && state.first_spawn_at < 0.0F) {
        state.first_spawn_at = elapsed;
      }
      state.alive = alive;
      state.peak_alive = std::max(state.peak_alive, alive);
      observe_zone_shrine(zone, state);
    }
  }

  void observe_zone_shrine(const Game::Map::UndeadZone& zone,
                           UndeadZoneObservation& state) {
    auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
    if (undead == nullptr) {
      return;
    }

    auto const anchor_id = undead->anchor_entity(zone.id);
    auto* anchor = anchor_id != 0 ? world.get_entity(anchor_id) : nullptr;
    auto const* unit = anchor != nullptr
                           ? anchor->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    bool const standing = unit != nullptr && unit->health > 0;

    if (standing) {
      state.shrine_seen = true;
    }
    state.shrine_standing = standing;
    if (state.shrine_seen && !standing) {
      state.shrine_destroyed = true;
    }
  }

  [[nodiscard]] auto
  undead_zone_state(const QString& zone_id) const -> UndeadZoneObservation {
    auto const found = undead_zone_states.constFind(zone_id);
    return found == undead_zone_states.cend() ? UndeadZoneObservation{} : found.value();
  }

  void observe_range_rings() {
    auto* selection = world.get_system<Game::Systems::SelectionSystem>();
    if (selection == nullptr) {
      return;
    }
    const auto& selected = selection->get_selected_units();
    if (selected.empty()) {
      return;
    }

    Game::Systems::AttackRangeRingRequest request;
    request.world = &world;
    request.local_owner_id = 1;
    request.selection = selected;
    request.max_rings = Game::Systems::k_attack_range_max_rings;
    const auto rings = Game::Systems::collect_attack_range_rings(request);
    max_range_ring_count = std::max(max_range_ring_count, rings.size());

    for (auto const& group : scenario.groups) {
      auto const& group_ids = ids(group.name);
      for (auto const& ring : rings) {
        if (std::find(group_ids.begin(), group_ids.end(), ring.entity_id) ==
            group_ids.end()) {
          continue;
        }
        range_ring_max_radius[group.name] = ring.max_radius;
        range_ring_min_radius[group.name] = ring.min_radius;
      }
    }
  }

  void observe_commander_aura_state() {
    for (auto const& group : scenario.groups) {
      for (auto entity_id : ids(group.name)) {
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }
        if (auto const* commander =
                entity->get_component<Engine::Core::CommanderComponent>()) {
          if (commander->aura_ability_active) {
            commander_aura_active_seen[group.name] = true;
          } else if (commander_aura_active_seen.value(group.name, false) &&
                     commander->aura_ability_cooldown_remaining > 0.0F) {
            commander_aura_expired_seen[group.name] = true;
          }
        }
        if (auto const* buff =
                entity->get_component<Engine::Core::CommanderAuraBuffComponent>();
            buff != nullptr && buff->active) {
          commander_aura_buff_seen[group.name] = true;
        }
      }
    }
  }

  [[nodiscard]] auto all_entities() const -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> result;
    for (auto const& group : scenario.groups) {
      auto const& group_ids = ids(group.name);
      result.insert(result.end(), group_ids.begin(), group_ids.end());
    }
    return result;
  }

  void track_elevation_leg(ElevationLegState& leg,
                           float elevation,
                           bool climbing,
                           float ceiling) {
    if (ceiling > 0.0F && elevation > ceiling) {
      leg.suspended = true;
      return;
    }
    if (leg.suspended) {
      leg.suspended = false;
      leg.extreme = elevation;
    }
    if (!leg.seeded) {
      leg.seeded = true;
      leg.extreme = elevation;
      return;
    }
    float const reversal = climbing ? leg.extreme - elevation : elevation - leg.extreme;
    if (reversal > leg.worst_reversal) {
      leg.worst_reversal = reversal;
      leg.worst_at = elapsed;
    }
    leg.extreme =
        climbing ? std::max(leg.extreme, elevation) : std::min(leg.extreme, elevation);
  }

  [[nodiscard]] auto
  expectation_active(const ArenaExpectation& expectation) const -> bool {
    if (elapsed + 1.0e-5F < expectation.start_seconds) {
      return false;
    }
    return expectation.end_seconds <= 0.0F ||
           elapsed <= expectation.end_seconds + 1.0e-5F;
  }

  [[nodiscard]] auto rpg_health_protection_active(const QString& group) const -> bool {
    bool has_protection = false;
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::RpgHealthUnchanged ||
          expectation.group != group) {
        continue;
      }
      has_protection = true;
      if (expectation_active(expectation)) {
        return true;
      }
    }
    return !has_protection;
  }

  [[nodiscard]] auto
  projectile_pair_key(Engine::Core::EntityID attacker_id,
                      Engine::Core::EntityID target_id) const -> QString {
    auto const attacker = entity_groups.constFind(attacker_id);
    auto const target = entity_groups.constFind(target_id);
    if (attacker == entity_groups.cend() || target == entity_groups.cend()) {
      return {};
    }
    return attacker.value() + QChar(0x1f) + target.value();
  }

  [[nodiscard]] static auto
  projectile_pair_key(const QString& attacker_group,
                      const QString& target_group) -> QString {
    return attacker_group + QChar(0x1f) + target_group;
  }

  void observe_projectiles() {
    auto const* system = world.get_system<Game::Systems::ProjectileSystem>();
    if (system == nullptr) {
      return;
    }
    for (auto const& projectile : system->projectiles()) {
      if (projectile == nullptr || !projectile->is_active() ||
          projectile->get_progress() < 0.0F) {
        continue;
      }
      auto const key = projectile_pair_key(projectile->get_attacker_id(),
                                           projectile->get_target_id());
      if (!key.isEmpty()) {
        projectile_flights[key] = true;
        if (Game::Systems::is_incendiary_projectile_kind(projectile->get_kind())) {
          flaming_projectile_flights[key] = true;
        } else {
          plain_projectile_flights[key] = true;
        }
      }
    }
    for (auto const& impact : system->impacts()) {
      if (observed_projectile_impacts.contains(impact.sequence)) {
        continue;
      }
      observed_projectile_impacts.insert(impact.sequence);
      auto const key = projectile_pair_key(impact.attacker_id, impact.target_id);
      if (!key.isEmpty() && impact.hit_target) {
        projectile_contacts[key] = true;
      }
      if (!key.isEmpty() && impact.hit_target && impact.damage_applied) {
        projectile_impacts[key] = true;
      }
    }
  }

  [[nodiscard]] auto applies_to(const ArenaExpectation& expectation,
                                const QString& group) const -> bool {
    return expectation.group.isEmpty() || expectation.group == group;
  }

  void observe_building_clearance(Engine::Core::EntityID entity_id,
                                  const QString& group) {
    if (building_overlap_report.contains(group)) {
      return;
    }
    const bool wanted = std::any_of(
        scenario.expectations.begin(),
        scenario.expectations.end(),
        [&](const ArenaExpectation& expectation) {
          return expectation.kind == ArenaExpectationKind::UnitsClearOfBuildings &&
                 expectation_active(expectation) && applies_to(expectation, group);
        });
    if (!wanted) {
      return;
    }
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    if (transform == nullptr || unit == nullptr || unit->health <= 0) {
      return;
    }
    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      return;
    }
    constexpr float k_body_radius = 0.35F;
    if (host.building_collision == nullptr) {
      return;
    }
    auto const& registry = *host.building_collision;
    const float unit_x = transform->position.x;
    const float unit_z = transform->position.z;
    const bool in_gateway =
        std::any_of(registry.navigation_passages().begin(),
                    registry.navigation_passages().end(),
                    [&](const Game::Systems::NavigationPassage& passage) {
                      return std::abs(unit_x - passage.center_x) <=
                                 (passage.width * 0.5F) + k_body_radius &&
                             std::abs(unit_z - passage.center_z) <=
                                 (passage.depth * 0.5F) + k_body_radius;
                    });
    if (!in_gateway &&
        registry.is_circle_overlapping_building(unit_x, unit_z, k_body_radius)) {
      building_overlap_report.insert(
          group,
          QStringLiteral("entity %1 stood inside a building at (%2, %3) after %4 s")
              .arg(entity_id)
              .arg(unit_x, 0, 'f', 1)
              .arg(unit_z, 0, 'f', 1)
              .arg(elapsed, 0, 'f', 1));
    }
  }

  [[nodiscard]] auto tracks_narrow_layout(const QString& group) -> bool {
    if (!narrow_layout_groups_resolved) {
      narrow_layout_groups_resolved = true;
      for (auto const& expectation : scenario.expectations) {
        switch (expectation.kind) {
        case ArenaExpectationKind::NarrowLayoutEngaged:
        case ArenaExpectationKind::NarrowLayoutStaysWide:
        case ArenaExpectationKind::NarrowLayoutKeepsFiles:
        case ArenaExpectationKind::NarrowLayoutModeSettles:
        case ArenaExpectationKind::NarrowLayoutRestores:
          narrow_layout_groups.insert(expectation.group);
          break;
        default:
          break;
        }
      }
    }
    return narrow_layout_groups.contains(group);
  }

  void observe_narrow_layout(const QString& group, Engine::Core::EntityID entity_id) {
    if (!tracks_narrow_layout(group)) {
      return;
    }
    auto const* traversal =
        world.try_get<Engine::Core::UnitTraversalLayoutStateComponent>(entity_id);
    if (traversal == nullptr) {
      return;
    }
    auto& state = narrow_layout[group];
    if (!state.seeded) {
      state.seeded = true;
      state.narrowest_corridor = std::numeric_limits<float>::max();
      state.narrowest_files = traversal->normal_files;
      state.tightest_file_spacing = traversal->authored_file_spacing;
      state.previous_mode = traversal->mode;
    }
    state.formation_half_width =
        std::max(state.formation_half_width, traversal->desired_half_width);
    if (traversal->available_half_width > 0.0F) {
      state.narrowest_corridor =
          std::min(state.narrowest_corridor, traversal->available_half_width);
    }
    state.normal_files = std::max(state.normal_files, traversal->normal_files);
    if (traversal->mode != state.previous_mode) {
      ++state.mode_changes;
      state.previous_mode = traversal->mode;
    }
    state.active_at_end = traversal->active;
    state.files_at_end = traversal->target_files;

    float frontage = 0.0F;
    float front = 0.0F;
    float back = 0.0F;
    float reform_error = 0.0F;
    for (auto const& slot : traversal->slot_states) {
      if (!slot.alive) {
        continue;
      }
      frontage = std::max(frontage, std::abs(slot.current_local_x) * 2.0F);
      front = std::max(front, slot.current_local_z);
      back = std::min(back, slot.current_local_z);
      reform_error = std::max(reform_error,
                              std::hypot(slot.current_local_x - slot.target_local_x,
                                         slot.current_local_z - slot.target_local_z));
    }
    state.worst_reform_error = reform_error;
    if (!traversal->active) {
      return;
    }
    state.engaged = true;
    if (state.engaged_from < 0.0F) {
      state.engaged_from = elapsed;
    }
    state.engaged_until = elapsed;
    state.deepest_column = std::max(state.deepest_column, front - back);
    state.tightest_file_spacing =
        std::min(state.tightest_file_spacing,
                 traversal->authored_file_spacing * traversal->lateral_scale);
    if (frontage > 0.0F) {
      state.narrowest_frontage = state.narrowest_frontage > 0.0F
                                     ? std::min(state.narrowest_frontage, frontage)
                                     : frontage;
    }
    if (traversal->target_files < state.narrowest_files) {
      state.narrowest_files = traversal->target_files;
      state.narrowest_mode = traversal->target_mode;
    }
  }

  void observe_entity(Engine::Core::EntityID entity_id,
                      const QString& group,
                      TraceFrame& frame) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    if (transform == nullptr || unit == nullptr) {
      return;
    }
    auto* entity = world.get_entity(entity_id);
    if (entity == nullptr) {
      return;
    }
    if (auto const* gate = world.try_get<Engine::Core::GateComponent>(entity_id)) {
      gate_seen[group] = true;
      if (gate->open_amount >= Engine::Core::GateComponent::k_passable_open_amount) {
        gate_opened_seen[group] = true;
      }
    }
    if (auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
        rpg != nullptr && rpg->active) {
      auto const* rpg_unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
      int const health = rpg_unit != nullptr ? rpg_unit->health : 0;
      if (!initial_rpg_health_by_group.contains(group)) {
        initial_rpg_health_by_group[group] = health;
        minimum_rpg_health_by_group[group] = health;
      } else if (rpg_health_protection_active(group)) {

        minimum_rpg_health_by_group[group] =
            std::min(minimum_rpg_health_by_group.value(group), health);
      }
      if (rpg->dodge_grace_remaining > 0.0F) {
        rpg_dodge_window_seen[group] = true;
      }
    }
    if (auto const* targets =
            world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
        targets != nullptr && targets->aim_candidate_in_range &&
        targets->aim_candidate_id != 0) {
      auto* target = world.get_entity(targets->aim_candidate_id);
      bool const exact_slot_required =
          target != nullptr &&
          Game::Systems::FormationCombat::has_formation_slots(*target);
      bool const has_exact_slot =
          targets->aim_candidate_soldier_slot !=
          Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
      if (target != nullptr && (!exact_slot_required || has_exact_slot) &&
          Game::Systems::RpgCombat::resolve_soldier_target(
              *target, targets->aim_candidate_soldier_slot)
              .has_value()) {
        exact_rpg_target_seen[group] = true;
      }
    }
    if (auto const* contacts =
            world.try_get<Engine::Core::RpgContactPresentationComponent>(entity_id)) {
      for (auto const& contact : contacts->entries) {
        switch (contact.outcome) {
        case Engine::Core::RpgContactOutcome::Damage:
          rpg_damage_contact_seen[group] = true;
          break;
        case Engine::Core::RpgContactOutcome::Block:
        case Engine::Core::RpgContactOutcome::PerfectGuard:
          rpg_block_contact_seen[group] = true;
          break;
        case Engine::Core::RpgContactOutcome::Dodge:
          rpg_dodge_contact_seen[group] = true;
          break;
        }
      }
    }
    if (auto const* structure_damage =
            world.try_get<Engine::Core::StructureDamagePresentationComponent>(
                entity_id);
        structure_damage != nullptr && !structure_damage->impacts.empty()) {
      structure_damage_cues[group] = true;
    }
    if (Game::Systems::Combat::structure_fire_intensity(*entity) > 0.0F) {
      structure_fires[group] = true;
    }
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::StructureFacadeContactObserved ||
          !expectation_active(expectation) || expectation.group != group) {
        continue;
      }
      for (auto target_id : ids(expectation.target_group)) {
        auto* structure = world.get_entity(target_id);
        if (structure != nullptr &&
            structure->has_component<Engine::Core::BuildingComponent>() &&
            Game::Systems::Combat::structure_melee_contact_active(
                *entity,
                *structure,
                Engine::Core::AttackComponent::k_melee_contact_range_grace)) {
          structure_facade_contacts[projectile_pair_key(
              expectation.group, expectation.target_group)] = true;
          break;
        }
      }
    }
    if (Game::Systems::DefensiveUnitLayoutService::is_formed(*entity)) {
      defensive_layout_locked[group] = true;
    }
    if (auto const* movement_facts =
            world.try_get<Engine::Core::MovementFactsComponent>(entity_id)) {
      auto const& stall = movement_facts->progress.stall;
      auto& observation = stall_observations[group];
      float const going_nowhere =
          std::max(stall.stalled_seconds, stall.no_closer_seconds);
      if (going_nowhere > observation.worst_stalled_seconds) {
        observation.worst_stalled_seconds = going_nowhere;
        observation.worst_stalled_at = elapsed;
        observation.worst_state = QString::fromLatin1(
            Engine::Core::movement_state_name(movement_facts->progress.state));
        observation.has_objective = stall.objective_valid;
        observation.objective_x = stall.objective_x;
        observation.objective_z = stall.objective_z;
      }
      observation.recovery_attempts =
          std::max(observation.recovery_attempts, stall.recovery_attempts);
      observation.repaths =
          std::max(observation.repaths, movement_facts->progress.repath_count);
      observation.abandons = std::max(observation.abandons, stall.abandon_count);
      observation.recovery_seen =
          observation.recovery_seen ||
          stall.rung != Engine::Core::MovementRecoveryRung::None;
    }
    QVector3D const position = vector_from_transform(*transform);
    if (!initial_elevation.contains(group)) {
      initial_elevation[group] = position.y();
      maximum_elevation[group] = position.y();
    } else {
      maximum_elevation[group] = std::max(maximum_elevation.value(group), position.y());
    }
    if (host.terrain != nullptr &&
        host.terrain->is_on_bridge(position.x(), position.z())) {
      bridge_traversal_seen[group] = true;
    }
    for (auto const& expectation : scenario.expectations) {
      if (expectation.group != group || !expectation_active(expectation)) {
        continue;
      }
      switch (expectation.kind) {
      case ArenaExpectationKind::ElevationClimbIsMonotonic:
        track_elevation_leg(
            elevation_climb_legs[group], position.y(), true, expectation.distance);
        break;
      case ArenaExpectationKind::ElevationDescentIsMonotonic:
        track_elevation_leg(
            elevation_descent_legs[group], position.y(), false, expectation.distance);
        break;
      case ArenaExpectationKind::ElevationHeldAbove: {
        auto& floor = elevation_floors[group];
        if (!floor.seeded || position.y() < floor.lowest) {
          floor.seeded = true;
          floor.lowest = position.y();
          floor.lowest_at = elapsed;
          floor.lowest_where = position;
        }
        break;
      }
      case ArenaExpectationKind::UnitsStayOnWalkableGround: {
        if (Game::Systems::NavGrid::is_world_position_walkable(position)) {
          break;
        }
        auto& state = off_walkable_ground[group];
        state.samples += 1;
        if (state.samples == 1) {
          state.worst = position;
          state.worst_at = elapsed;
        }
        break;
      }
      case ArenaExpectationKind::SoldiersStayOnWalkableGround: {
        auto const* presentation =
            world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
        if (presentation == nullptr) {
          break;
        }
        float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
        float const sin_yaw = std::sin(yaw);
        float const cos_yaw = std::cos(yaw);
        for (auto const& soldier : presentation->soldiers) {
          if (!soldier.alive) {
            continue;
          }
          QVector3D const world(
              position.x() + (cos_yaw * soldier.local_x) + (sin_yaw * soldier.local_z),
              0.0F,
              position.z() - (sin_yaw * soldier.local_x) + (cos_yaw * soldier.local_z));
          auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
          if (pathfinder == nullptr) {
            continue;
          }
          auto const cell = pathfinder->world_to_grid(world.x(), world.z());
          if (pathfinder->is_terrain_walkable(cell.x, cell.y)) {
            continue;
          }
          auto& state = off_walkable_soldiers[group];
          state.samples += 1;
          if (state.samples == 1) {
            state.worst = world;
            state.worst_at = elapsed;
          }
        }
        break;
      }
      default:
        break;
      }
    }
    auto const* target = world.try_get<Engine::Core::AttackTargetComponent>(entity_id);
    auto const* motion =
        world.try_get<Engine::Core::MotionPresentationComponent>(entity_id);
    auto const* formation_contact =
        world.try_get<Engine::Core::FormationContactComponent>(entity_id);
    auto const* attack = world.try_get<Engine::Core::AttackComponent>(entity_id);
    auto const* movement = world.try_get<Engine::Core::MovementComponent>(entity_id);
    auto const* mounted_charge =
        world.try_get<Engine::Core::MountedChargeComponent>(entity_id);
    auto const* combat_action =
        world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
    if (auto const* casualties =
            world.try_get<Engine::Core::SoldierCasualtyAnimationComponent>(entity_id);
        casualties != nullptr &&
        std::any_of(casualties->entries.begin(),
                    casualties->entries.end(),
                    [](auto const& entry) { return entry.launched; })) {
      launched_casualties[group] = true;
    }
    auto const* builder =
        world.try_get<Engine::Core::BuilderProductionComponent>(entity_id);
    if (builder != nullptr && builder->construction_complete &&
        !latched_builder_completions.contains(entity_id)) {
      if (Game::Systems::is_gather_builder_product(builder->product_type)) {
        completed_harvest_by_owner[unit->owner_id]++;
      }
      latched_builder_completions.insert(entity_id);
    } else if (builder != nullptr && !builder->construction_complete) {
      latched_builder_completions.remove(entity_id);
    }
    bool const mounted_charge_impact =
        combat_action != nullptr &&
        combat_action->combat_action_id ==
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
    if (mounted_charge_impact) {
      charge_impacts[group] = true;
    }
    if (charge_impacts.value(group, false) && attack != nullptr &&
        attack->in_melee_lock && !mounted_charge_impact) {
      melee_locks_after_charge[group] = true;
    }
    bool const combat_indicator_submitted =
        Render::Profiling::CombatAnimationDiagnostics::instance()
            .mode_indicator_submitted(entity_id);
    QString combat_mode = QStringLiteral("none");
    if (attack != nullptr) {
      combat_mode =
          attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged
              ? QStringLiteral("ranged")
              : (attack->current_mode ==
                         Engine::Core::AttackComponent::CombatMode::Melee
                     ? QStringLiteral("melee")
                     : QStringLiteral("auto"));
    }
    QString motion_name = QStringLiteral("idle");
    if (motion != nullptr) {
      motion_name = motion->is_run_state()
                        ? QStringLiteral("run")
                        : (motion->is_walk_state() ? QStringLiteral("walk")
                                                   : QStringLiteral("idle"));
    }
    if (formation_contact != nullptr && formation_contact->in_contact) {
      auto const current = minimum_formation_surface_gap.find(group);
      if (current == minimum_formation_surface_gap.end() ||
          formation_contact->surface_gap < current.value()) {
        minimum_formation_surface_gap[group] = formation_contact->surface_gap;
      }
    }
    frame.units.push_back(
        {entity_id,
         group,
         position,
         unit->health,
         target != nullptr ? target->target_id : 0U,
         motion_name,
         combat_mode,
         mounted_charge != nullptr ? static_cast<int>(mounted_charge->state) : -1,
         mounted_charge != nullptr
             ? static_cast<int>(mounted_charge->last_cancel_reason)
             : -1,
         combat_action != nullptr ? combat_action->combat_action_id : 0,
         attack != nullptr && attack->in_melee_lock,
         attack != nullptr ? attack->melee_lock_target_id : 0U,
         combat_indicator_submitted,
         transform->rotation.y,
         movement != nullptr && movement->get_has_target(),
         movement != nullptr ? movement->get_vx() : 0.0F,
         movement != nullptr ? movement->get_vz() : 0.0F,
         movement != nullptr ? movement->get_goal_x() : 0.0F,
         movement != nullptr ? movement->get_goal_y() : 0.0F,
         formation_contact != nullptr && formation_contact->in_contact,
         formation_contact != nullptr ? formation_contact->surface_gap : 0.0F,
         formation_contact != nullptr ? formation_contact->engaged_soldier_indices
                                      : std::vector<std::uint16_t>{},
         formation_contact != nullptr
             ? formation_contact->engagement_pairs
             : std::vector<Engine::Core::FormationEngagementPair>{},
         builder != nullptr ? QString::fromStdString(builder->product_type) : QString{},
         builder != nullptr && builder->has_construction_site,
         builder != nullptr && builder->in_progress,
         builder != nullptr ? builder->time_remaining : 0.0F,
         [&]() {
           auto const* commander =
               world.try_get<Engine::Core::CommanderComponent>(entity_id);
           return commander != nullptr && commander->aura_ability_active;
         }(),
         [&]() {
           auto const* buff =
               world.try_get<Engine::Core::CommanderAuraBuffComponent>(entity_id);
           return buff != nullptr && buff->active;
         }(),
         [&]() {
           auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
           auto const* rpg_unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
           return rpg != nullptr && rpg->active && rpg_unit != nullptr
                      ? rpg_unit->health
                      : -1;
         }(),
         [&]() {
           auto const* guard =
               world.try_get<Engine::Core::CommanderGuardComponent>(entity_id);
           return guard != nullptr && guard->active;
         }(),
         [&]() {
           auto const* rpg = world.try_get<Engine::Core::RpgHealthComponent>(entity_id);
           return rpg != nullptr && rpg->active && rpg->dodge_grace_remaining > 0.0F;
         }(),
         [&]() {
           auto const* targets =
               world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
           return targets != nullptr ? targets->aim_candidate_id
                                     : Engine::Core::EntityID{0};
         }(),
         [&]() {
           auto const* targets =
               world.try_get<Engine::Core::RpgCommanderTargetComponent>(entity_id);
           if (targets == nullptr ||
               targets->aim_candidate_soldier_slot ==
                   Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot) {
             return -1;
           }
           return static_cast<int>(targets->aim_candidate_soldier_slot);
         }(),
         [&]() {
           auto const* action =
               world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
           return action != nullptr ? static_cast<int>(action->phase) : 0;
         }(),
         [&]() {
           auto const* action =
               world.try_get<Engine::Core::RpgCommanderActionComponent>(entity_id);
           return action != nullptr ? action->normalized_action_time : 0.0F;
         }()});

    auto& previous = entity_states[entity_id];

    if (attack != nullptr && attack->in_melee_lock && !combat_indicator_submitted) {
      for (auto const& expectation : scenario.expectations) {
        if (expectation.kind == ArenaExpectationKind::CombatIndicatorIsContinuous &&
            expectation_active(expectation) && applies_to(expectation, group)) {
          add_issue(QStringLiteral("missing_combat_indicator"),
                    QStringLiteral("%1 entity %2 had a melee lock but submitted no "
                                   "fight-mode indicator in the rendered frame")
                        .arg(group)
                        .arg(entity_id),
                    entity_id);
        }
      }
    }
    float const sample_dt =
        previous.initialized ? elapsed - previous.observed_at : 0.0F;
    if (previous.initialized && sample_dt > 0.0F) {
      float const step = horizontal_distance(position, previous.position);
      for (auto const& expectation : scenario.expectations) {
        if (!expectation_active(expectation) || !applies_to(expectation, group)) {
          continue;
        }
        if (expectation.kind == ArenaExpectationKind::MovementIsContinuous) {
          float const multiplier =
              expectation.threshold > 0.0F ? expectation.threshold : 2.5F;
          float const allowed = std::max(0.25F, unit->speed * sample_dt * multiplier);
          if (step > allowed) {
            add_issue(QStringLiteral("movement_discontinuity"),
                      QStringLiteral("%1 entity %2 moved %3 m in one rendered frame "
                                     "(allowed %4 m)")
                          .arg(group)
                          .arg(entity_id)
                          .arg(step, 0, 'f', 2)
                          .arg(allowed, 0, 'f', 2),
                      entity_id);
          }
        }
        if (expectation.kind == ArenaExpectationKind::FormationEngagementIsStable &&
            previous.melee_lock && entity_alive(previous.melee_lock_target_id)) {
          Engine::Core::EntityID const current_lock_target =
              attack != nullptr && attack->in_melee_lock ? attack->melee_lock_target_id
                                                         : 0U;
          if (current_lock_target != previous.melee_lock_target_id) {
            add_issue(QStringLiteral("melee_engagement_restarted"),
                      QStringLiteral("%1 entity %2 released or changed a living "
                                     "melee opponent during the engagement")
                          .arg(group)
                          .arg(entity_id),
                      entity_id);
          }
          bool const locomoting = motion != nullptr && motion->has_locomotion();
          bool const navigation_active =
              movement != nullptr &&
              (movement->get_has_target() || std::abs(movement->get_vx()) > 0.001F ||
               std::abs(movement->get_vz()) > 0.001F);
          if (locomoting || navigation_active || step > 0.005F) {
            add_issue(QStringLiteral("movement_during_melee_engagement"),
                      QStringLiteral("%1 entity %2 walked while its melee opponent "
                                     "was still alive")
                          .arg(group)
                          .arg(entity_id),
                      entity_id);
          }
          float const yaw_delta = std::abs(
              std::fmod(transform->rotation.y - previous.yaw + 540.0F, 360.0F) -
              180.0F);
          if (yaw_delta > 0.05F) {
            add_issue(QStringLiteral("rotation_during_melee_engagement"),
                      QStringLiteral("%1 entity %2 changed facing by %3 degrees "
                                     "while melee-locked")
                          .arg(group)
                          .arg(entity_id)
                          .arg(yaw_delta, 0, 'f', 2),
                      entity_id);
          }
        }
      }
      if (unit->health < previous.health) {
        damage_seen[group] = true;
      }
    }
    previous = {position,
                elapsed,
                unit->health,
                transform->rotation.y,
                attack != nullptr ? attack->melee_lock_target_id : 0U,
                attack != nullptr && attack->in_melee_lock,
                true};

    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::FormationEngagementIsStable ||
          !expectation_active(expectation) || !applies_to(expectation, group) ||
          formation_contact == nullptr || !formation_contact->in_contact ||
          attack == nullptr || !attack->in_melee_lock) {
        continue;
      }
      auto const* formation =
          world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
      std::size_t const living =
          formation != nullptr ? static_cast<std::size_t>(std::count_if(
                                     formation->soldiers.begin(),
                                     formation->soldiers.end(),
                                     [](auto const& soldier) { return soldier.alive; }))
                               : 0U;
      QSet<std::uint16_t> paired_slots;
      for (auto const& pair : formation_contact->engagement_pairs) {
        paired_slots.insert(pair.attacker_slot);
      }
      if (living > 0U && paired_slots.size() != static_cast<qsizetype>(living)) {
        add_issue(QStringLiteral("incomplete_formation_engagement"),
                  QStringLiteral("%1 entity %2 engaged %3 of %4 living soldiers")
                      .arg(group)
                      .arg(entity_id)
                      .arg(paired_slots.size())
                      .arg(living),
                  entity_id);
      }
      auto* lock_target = world.get_entity(attack->melee_lock_target_id);
      if (lock_target != nullptr) {
        auto const geometry =
            Game::Systems::FormationCombat::contact_geometry(*entity, *lock_target);
        if (!previous.melee_lock && geometry.formation_overlap_required &&
            geometry.center_distance > geometry.engagement_center_distance + 0.01F) {
          add_issue(QStringLiteral("insufficient_formation_overlap"),
                    QStringLiteral("%1 entity %2 locked at center distance %3 m "
                                   "instead of the %4 m overlap contract")
                        .arg(group)
                        .arg(entity_id)
                        .arg(geometry.center_distance, 0, 'f', 3)
                        .arg(geometry.engagement_center_distance, 0, 'f', 3),
                    entity_id);
        }
      }
    }

    if (auto response = responses.find(entity_id);
        response != responses.end() && !response->observed) {
      bool const moved =
          horizontal_distance(position, response->initial_position) > 0.03F;
      bool const turned =
          std::abs(std::fmod(transform->rotation.y - response->initial_yaw + 540.0F,
                             360.0F) -
                   180.0F) > 5.0F;
      bool const visually_active = motion != nullptr && motion->has_locomotion();
      auto const* combat = world.try_get<Engine::Core::CombatStateComponent>(entity_id);
      bool const combat_active =
          combat != nullptr &&
          combat->animation_state != Engine::Core::CombatAnimationState::Idle;
      auto const* hold = world.try_get<Engine::Core::HoldModeComponent>(entity_id);
      bool const stance_exit_accepted =
          response->command == QStringLiteral("ReleaseReserve") && hold != nullptr &&
          !hold->active;
      response->observed =
          moved || turned || visually_active || combat_active || stance_exit_accepted;
      if (!response->observed && elapsed > response->deadline && !response->reported) {
        response->reported = true;
        add_issue(QStringLiteral("command_response_timeout"),
                  QStringLiteral("%1 entity %2 did not visibly respond to %3 within "
                                 "%4 s")
                      .arg(group)
                      .arg(entity_id)
                      .arg(response->command)
                      .arg(response->deadline - response->issued_at, 0, 'f', 2),
                  entity_id);
      }
    }

    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::NoEligibleTroopIdleDuringCombat ||
          !expectation_active(expectation) || !applies_to(expectation, group) ||
          expectation.target_group.isEmpty() || unit->health <= 0) {
        continue;
      }
      float const distance = groups_distance(group, expectation.target_group);
      float const eligible_distance = expectation.distance > 0.0F
                                          ? expectation.distance
                                          : k_default_engagement_distance;
      bool const active_order = target != nullptr && target->target_id != 0U;
      bool const active_motion = motion != nullptr && motion->has_locomotion();
      bool const eligible_idle =
          distance <= eligible_distance && !active_order && !active_motion;
      if (!eligible_idle) {
        idle_since.remove(entity_id);
        continue;
      }
      if (!idle_since.contains(entity_id)) {
        idle_since.insert(entity_id, elapsed);
      }
      float const allowed_idle =
          expectation.threshold > 0.0F ? expectation.threshold : k_default_idle_seconds;
      if (elapsed - idle_since.value(entity_id) > allowed_idle) {
        add_issue(QStringLiteral("eligible_troop_idle"),
                  QStringLiteral("%1 entity %2 stayed idle for %3 s while an eligible "
                                 "enemy was %4 m away")
                      .arg(group)
                      .arg(entity_id)
                      .arg(elapsed - idle_since.value(entity_id), 0, 'f', 2)
                      .arg(distance, 0, 'f', 2),
                  entity_id);
      }
    }

    if (motion != nullptr && motion->has_locomotion()) {
      useful_bot_action[group] = true;
    }
    if (target != nullptr && target->target_id != 0U) {
      useful_bot_action[group] = true;
    }
  }

  void observe_bridge_centerline_alignment(const QString& group) {
    QVector3D centroid;
    int living = 0;
    for (auto entity_id : ids(group)) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      centroid += vector_from_transform(*transform);
      ++living;
    }
    if (living == 0) {
      return;
    }
    centroid /= static_cast<float>(living);

    auto const* height_map =
        host.terrain != nullptr ? host.terrain->get_height_map() : nullptr;
    if (height_map == nullptr) {
      return;
    }
    for (auto const& bridge : height_map->get_bridges()) {
      QVector3D direction = bridge.end - bridge.start;
      direction.setY(0.0F);
      float const length = direction.length();
      if (length < 0.01F) {
        continue;
      }
      direction /= length;
      QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
      QVector3D offset = centroid - bridge.start;
      offset.setY(0.0F);
      float const along = QVector3D::dotProduct(offset, direction);
      float const lateral = std::abs(QVector3D::dotProduct(offset, perpendicular));
      if (along < 0.0F || along > length || lateral > bridge.width * 0.5F) {
        continue;
      }

      float const midpoint_distance = std::abs(along - length * 0.5F);
      auto& observation = bridge_alignment[group];
      if (!observation.sampled || midpoint_distance < observation.midpoint_distance) {
        observation.sampled = true;
        observation.midpoint_distance = midpoint_distance;
        observation.lateral_offset = lateral;
      }
    }
  }

  void observe_rpg_locomotion_presentation(const TraceFrame& frame) {
    if (scenario.rpg_commander_group.isEmpty()) {
      return;
    }
    const QString& group = scenario.rpg_commander_group;
    for (auto const& unit : frame.units) {
      if (unit.group != group) {
        continue;
      }
      if (unit.motion == QStringLiteral("walk")) {
        rpg_walk_seen[group] = true;
      } else if (unit.motion == QStringLiteral("run")) {
        rpg_run_seen[group] = true;
      }
      for (auto const& soldier : frame.soldiers) {
        if (soldier.entity_id != unit.entity_id || soldier.culled) {
          continue;
        }

        constexpr float k_strike_sync_grace_seconds = 0.05F;
        const bool striking =
            unit.rpg_action_phase ==
            static_cast<int>(Engine::Core::RpgCommanderActionPhase::Strike);

        const bool strike_visual = soldier.visual == QStringLiteral("Attack") ||
                                   soldier.visual == QStringLiteral("Dying") ||
                                   soldier.visual == QStringLiteral("Dead");
        if (!striking || strike_visual) {
          rpg_strike_mismatch_since.remove(group);
        } else {
          if (!rpg_strike_mismatch_since.contains(group)) {
            rpg_strike_mismatch_since[group] = frame.time_seconds;
          }
          float const lagged =
              frame.time_seconds - rpg_strike_mismatch_since.value(group);
          if (lagged > k_strike_sync_grace_seconds &&
              !rpg_strike_mismatch.contains(group)) {
            rpg_strike_mismatch[group] =
                QStringLiteral("simulation resolved a strike for %1 s while the "
                               "renderer still showed %2 at %3 s")
                    .arg(lagged, 0, 'f', 3)
                    .arg(soldier.visual)
                    .arg(frame.time_seconds, 0, 'f', 2);
          }
        }

        const bool visual_moving = soldier.visual == QStringLiteral("Walk") ||
                                   soldier.visual == QStringLiteral("Run");
        const bool visual_running = soldier.visual == QStringLiteral("Run");
        const bool combat_visual = soldier.visual == QStringLiteral("Attack") ||
                                   soldier.visual == QStringLiteral("HitReaction") ||
                                   soldier.visual == QStringLiteral("Dying") ||
                                   soldier.visual == QStringLiteral("Dead");
        if (combat_visual || rpg_locomotion_mismatch.contains(group)) {
          continue;
        }
        if (unit.motion == QStringLiteral("run") && !visual_running) {
          rpg_locomotion_mismatch[group] =
              QStringLiteral(
                  "simulation reported run at %1 s while the renderer showed "
                  "%2")
                  .arg(frame.time_seconds, 0, 'f', 2)
                  .arg(soldier.visual);
        } else if (unit.motion == QStringLiteral("walk") && !visual_moving) {
          rpg_locomotion_mismatch[group] =
              QStringLiteral(
                  "simulation reported walk at %1 s while the renderer showed "
                  "%2")
                  .arg(frame.time_seconds, 0, 'f', 2)
                  .arg(soldier.visual);
        } else if (unit.motion == QStringLiteral("idle") && visual_moving) {
          rpg_locomotion_mismatch[group] =
              QStringLiteral(
                  "simulation reported idle at %1 s while the renderer showed "
                  "%2")
                  .arg(frame.time_seconds, 0, 'f', 2)
                  .arg(soldier.visual);
        }
      }
    }
  }

  void observe_rpg_swing_cadence(const TraceFrame& frame) {
    if (scenario.rpg_commander_group.isEmpty()) {
      return;
    }
    const QString& group = scenario.rpg_commander_group;
    for (auto const& unit : frame.units) {
      if (unit.group != group) {
        continue;
      }
      int const striking =
          static_cast<int>(Engine::Core::RpgCommanderActionPhase::Strike);
      int const previous_phase = rpg_action_phase_previous.value(group, 0);
      float const previous_time = rpg_action_time_previous.value(group, 1.0F);

      bool const swing_started = unit.rpg_action_phase == striking &&
                                 (previous_phase != striking ||
                                  unit.rpg_action_normalized_time < previous_time);

      auto const bank_carry = [&]() {
        if (rpg_swing_carry_open.value(group, false)) {
          rpg_swing_carry[group].push_back(rpg_swing_carry_pending.value(group, 0.0F));
          rpg_swing_carry_open[group] = false;
        }
      };
      if (swing_started) {
        bank_carry();
        rpg_swing_starts[group].push_back(frame.time_seconds);
        rpg_swing_carry_pending[group] = 0.0F;
        rpg_swing_carry_origin[group] = unit.position;
        rpg_swing_carry_open[group] = true;
      }
      if (rpg_swing_carry_open.value(group, false)) {
        if (unit.rpg_action_phase == striking) {
          rpg_swing_carry_pending[group] +=
              horizontal_distance(rpg_swing_carry_origin.value(group), unit.position);
          rpg_swing_carry_origin[group] = unit.position;
        } else {
          bank_carry();
        }
      }
      rpg_action_phase_previous[group] = unit.rpg_action_phase;
      rpg_action_time_previous[group] = unit.rpg_action_normalized_time;
    }
  }

  static auto travel_key(const ArenaExpectation& expectation) -> QString {
    return QStringLiteral("%1|%2|%3")
        .arg(expectation.group)
        .arg(expectation.start_seconds)
        .arg(expectation.end_seconds);
  }

  void observe_rpg_travel(const TraceFrame& frame) {
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::RpgTravelObserved) {
        continue;
      }
      for (auto const& unit : frame.units) {
        if (unit.group != expectation.group) {
          continue;
        }
        auto& observation = rpg_travel_observations[travel_key(expectation)];
        if (!observation.has_start && frame.time_seconds >= expectation.start_seconds) {
          observation.has_start = true;
          observation.start = unit.position;
        }
        if (observation.has_start && frame.time_seconds <= expectation.end_seconds) {
          observation.has_end = true;
          observation.end = unit.position;
        }
      }
    }
  }

  void observe_group_pair_proximity(const TraceFrame& frame) {
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::RpgApproachWithin) {
        continue;
      }
      float closest = std::numeric_limits<float>::infinity();
      for (auto const& unit : frame.units) {
        if (unit.group != expectation.group) {
          continue;
        }
        for (auto const& other : frame.units) {
          if (other.group != expectation.target_group) {
            continue;
          }
          closest =
              std::min(closest, horizontal_distance(unit.position, other.position));
        }
      }
      if (!std::isfinite(closest)) {
        continue;
      }
      const QString key =
          projectile_pair_key(expectation.group, expectation.target_group);
      auto const existing = minimum_group_pair_distance.constFind(key);
      if (existing == minimum_group_pair_distance.cend() ||
          closest < existing.value()) {
        minimum_group_pair_distance[key] = closest;
      }
    }
  }

  void observe_soldiers(Engine::Core::EntityID entity_id,
                        const QString& group,
                        TraceFrame& frame) {
    auto const* presentation =
        world.try_get<Engine::Core::FormationPresentationComponent>(entity_id);
    auto const* debug =
        Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(entity_id);
    const bool verify_render_continuity = std::any_of(
        scenario.expectations.begin(),
        scenario.expectations.end(),
        [&](const ArenaExpectation& expectation) {
          return expectation.kind == ArenaExpectationKind::NoRenderVisibilityChurn &&
                 expectation_active(expectation) && applies_to(expectation, group);
        });

    const bool verify_unit_submission =
        verify_render_continuity ||
        std::any_of(scenario.expectations.begin(),
                    scenario.expectations.end(),
                    [&](const ArenaExpectation& expectation) {
                      return expectation.kind ==
                                 ArenaExpectationKind::RpgFormationSurvivesLensGap &&
                             expectation_active(expectation) &&
                             applies_to(expectation, group);
                    });
    if (debug == nullptr) {
      const auto previous_samples = sampled_soldiers_by_entity.value(entity_id);
      const bool had_living_render_sample = std::any_of(
          previous_samples.begin(), previous_samples.end(), [&](int soldier_index) {
            const auto state =
                soldier_states.constFind(soldier_key(entity_id, soldier_index));
            return state != soldier_states.cend() && state->alive;
          });
      auto const unit_cull_reason =
          Render::Profiling::CombatAnimationDiagnostics::instance().unit_cull_reason(
              entity_id);

      const bool explained_by_visibility =
          unit_cull_reason == Render::Profiling::SoldierCullReason::Frustum ||
          unit_cull_reason == Render::Profiling::SoldierCullReason::Fog;
      const bool unit_continuity_required =
          verify_render_continuity ||
          (verify_unit_submission && !explained_by_visibility);
      if (unit_continuity_required && entity_alive(entity_id) &&
          entities_with_render_samples.contains(entity_id) &&
          had_living_render_sample) {
        add_issue(
            QStringLiteral("unit_submission_disappeared"),
            QStringLiteral("%1 entity %2 disappeared before per-soldier "
                           "submission diagnostics (%3)")
                .arg(group)
                .arg(entity_id)
                .arg(QString::fromLatin1(
                    Render::Profiling::soldier_cull_reason_name(unit_cull_reason))),
            entity_id);
      }
      return;
    }
    if (world.has<Engine::Core::ElephantComponent>(entity_id) &&
        debug->unit.is_attacking) {
      visible_attacks[group] = true;
      useful_bot_action[group] = true;
      auto const* contact =
          world.try_get<Engine::Core::FormationContactComponent>(entity_id);
      if (contact != nullptr &&
          std::any_of(contact->fronts.begin(),
                      contact->fronts.end(),
                      [](auto const& front) { return front.in_contact; })) {
        paired_visible_attacks[group] = true;
      }
    }
    entities_with_render_samples.insert(entity_id);
    QSet<int> sampled_this_frame;
    bool const formation_fight_active =
        presentation != nullptr && presentation->melee_ordered &&
        presentation->target_alive &&
        std::any_of(
            presentation->soldiers.begin(),
            presentation->soldiers.end(),
            [](auto const& soldier) {
              return soldier.alive &&
                     (soldier.action ==
                          Engine::Core::FormationSoldierAction::MeleeEngaged ||
                      soldier.action ==
                          Engine::Core::FormationSoldierAction::MeleeFollowThrough);
            });
    int living_soldier_samples = 0;
    int lens_gap_culled_samples = 0;
    float minimum_attack_phase = std::numeric_limits<float>::max();
    float maximum_attack_phase = std::numeric_limits<float>::lowest();
    QSet<int> attack_phase_bins;
    int visible_attack_count = 0;
    for (auto const& soldier : debug->soldiers) {
      sampled_this_frame.insert(soldier.soldier_index);
      Engine::Core::FormationSoldierPresentation const* directive = nullptr;
      if (presentation != nullptr && soldier.soldier_index >= 0) {
        auto const slot = static_cast<std::size_t>(soldier.soldier_index);
        if (slot < presentation->soldiers.size() &&
            presentation->soldiers[slot].slot_index == slot) {
          directive = &presentation->soldiers[slot];
        }
      }
      QString declared_action = QStringLiteral("single_body");
      if (directive != nullptr) {
        switch (directive->action) {
        case Engine::Core::FormationSoldierAction::FollowUnit:
          declared_action = QStringLiteral("follow_unit");
          break;
        case Engine::Core::FormationSoldierAction::MeleeReady:
          declared_action = QStringLiteral("melee_ready");
          break;
        case Engine::Core::FormationSoldierAction::MeleeEngaged:
          declared_action = QStringLiteral("melee_engaged");
          break;
        case Engine::Core::FormationSoldierAction::MeleeFollowThrough:
          declared_action = QStringLiteral("melee_follow_through");
          break;
        case Engine::Core::FormationSoldierAction::MeleeGuard:
          declared_action = QStringLiteral("melee_guard");
          break;
        case Engine::Core::FormationSoldierAction::MeleeReposition:
          declared_action = QStringLiteral("melee_reposition");
          break;
        }
      }
      ++report.rendered_soldier_samples;
      ++rendered_by_group[group];
      bool const culled =
          soldier.cull_reason != Render::Profiling::SoldierCullReason::None;
      bool const observed_attack =
          soldier.visual_state == Render::Profiling::SoldierVisualState::Attack &&
          !culled;
      bool const observed_movement =
          (soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
           soldier.visual_state == Render::Profiling::SoldierVisualState::Run) &&
          !culled;
      if (observed_movement) {
        visible_movement[group] = true;
      }
      if (!culled &&
          (soldier.visual_state == Render::Profiling::SoldierVisualState::HitReaction ||
           (soldier.is_swing_recoiling &&
            std::abs(soldier.hit_reaction_tilt_degrees) > 0.05F))) {

        visible_hit_reactions[group] = true;
      }
      if ((soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
           soldier.visual_state == Render::Profiling::SoldierVisualState::Dead) &&
          !culled) {
        visible_deaths[group] = true;
      }
      frame.soldiers.push_back(
          {entity_id,
           soldier.soldier_index,
           soldier.root_position,
           soldier.root_yaw_degrees,
           soldier.root_up_y,
           soldier.submitted_body_up_y,
           soldier.submitted_max_arm_reach,
           soldier.submitted_body_pose_valid,
           soldier.foot_l_world,
           soldier.foot_r_world,
           soldier.locomotion_blend,
           soldier.locomotion_presence,
           soldier.cycle_phase,
           soldier.persistent_valid,
           soldier.sample_time,
           soldier.persistent_last_sample_time,
           declared_action,
           directive != nullptr &&
                   (directive->action ==
                        Engine::Core::FormationSoldierAction::MeleeEngaged ||
                    directive->action ==
                        Engine::Core::FormationSoldierAction::MeleeFollowThrough)
               ? static_cast<int>(directive->target_slot)
               : -1,
           directive != nullptr ? directive->engagement_surface_gap : 0.0F,
           QString::fromLatin1(
               Render::Profiling::animation_state_name(soldier.animation_state)),
           QString::fromLatin1(
               Render::Profiling::soldier_visual_state_name(soldier.visual_state)),
           soldier.is_swing_recoiling,
           soldier.hit_reaction_tilt_degrees,
           soldier.attack_phase,
           soldier.transitions_last_second,
           culled,
           QString::fromLatin1(
               Render::Profiling::soldier_cull_reason_name(soldier.cull_reason))});

      std::uint64_t const key = soldier_key(entity_id, soldier.soldier_index);
      const bool continuity_alive =
          soldier.visual_state != Render::Profiling::SoldierVisualState::Dying &&
          soldier.visual_state != Render::Profiling::SoldierVisualState::Dead &&
          (directive == nullptr || directive->alive);
      if (directive != nullptr && directive->alive) {
        living_soldiers_by_group[group].insert(key);
        if (directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged) {
          engaged_soldiers_by_group[group].insert(key);
        }
      }
      if (continuity_alive) {
        ++living_soldier_samples;
        if (soldier.cull_reason == Render::Profiling::SoldierCullReason::LensGap) {
          ++lens_gap_culled_samples;
        }
      }
      auto& previous = soldier_states[key];
      if (verify_render_continuity && continuity_alive && culled &&
          previous.ever_visible && (!previous.initialized || !previous.culled)) {
        add_issue(
            QStringLiteral("soldier_submission_disappeared"),
            QStringLiteral("%1 entity %2 soldier %3 changed from submitted to "
                           "culled (%4)")
                .arg(group)
                .arg(entity_id)
                .arg(soldier.soldier_index)
                .arg(QString::fromLatin1(
                    Render::Profiling::soldier_cull_reason_name(soldier.cull_reason))),
            entity_id,
            soldier.soldier_index);
      }
      if (!culled) {
        previous.ever_visible = true;
      }
      if ((observed_attack && (!previous.initialized || !previous.attacking)) ||
          soldier.attack_phase_reset) {
        ++attack_entries_by_soldier[key];
      }
      bool const recovered_to_controlled_pose =
          soldier.visual_state == Render::Profiling::SoldierVisualState::Idle ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Hold ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Run;
      if (!culled && previous.completed_attack_phase && recovered_to_controlled_pose) {
        visible_attack_recoveries[group] = true;
      }
      if (observed_attack && soldier.attack_phase >= 0.85F) {
        previous.completed_attack_phase = true;
      }
      bool const living_formation_fighter =
          formation_fight_active && directive != nullptr && directive->alive && !culled;
      if (!living_formation_fighter) {
        previous.fight_idle_since = -1.0F;
        previous.terminal_pose_since = -1.0F;
      }
      float const step =
          previous.initialized
              ? horizontal_distance(soldier.root_position, previous.root_position)
              : 0.0F;
      for (auto const& expectation : scenario.expectations) {
        if (!expectation_active(expectation) || !applies_to(expectation, group)) {
          continue;
        }
        if (expectation.kind == ArenaExpectationKind::NoPoseOscillation &&
            soldier.churn_flagged) {
          add_issue(QStringLiteral("pose_oscillation"),
                    QStringLiteral("%1 entity %2 soldier %3 changed visual state %4 "
                                   "times in one second")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(soldier.transitions_last_second),
                    entity_id,
                    soldier.soldier_index);
        }
        if (expectation.kind == ArenaExpectationKind::FullCreatureDetailOnly &&
            continuity_alive) {
          const bool lod_shed =
              soldier.cull_reason == Render::Profiling::SoldierCullReason::Distance;
          const bool reduced_visible_mesh =
              !culled && soldier.lod != static_cast<std::uint8_t>(
                                            Render::Creature::CreatureLOD::Full);
          if (lod_shed || reduced_visible_mesh) {
            add_issue(QStringLiteral("ultra_creature_lod_used"),
                      QStringLiteral("%1 entity %2 soldier %3 used LOD %4 (%5) while "
                                     "full creature detail was required")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(static_cast<int>(soldier.lod))
                          .arg(QString::fromLatin1(
                              Render::Profiling::soldier_cull_reason_name(
                                  soldier.cull_reason))),
                      entity_id,
                      soldier.soldier_index);
          }
        }

        float const frame_budget_scale =
            std::max(1.0F, (elapsed - previous.observed_at) * 60.0F);

        if (expectation.kind == ArenaExpectationKind::NoRootTeleport &&
            previous.initialized && !previous.culled && !culled &&
            elapsed >= k_spawn_settle_seconds &&
            elapsed - previous.observed_at <= 0.05F) {
          float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                              : k_default_root_step) *
                                frame_budget_scale;
          if (step > allowed) {
            add_issue(QStringLiteral("render_root_teleport"),
                      QStringLiteral("%1 entity %2 soldier %3 render root jumped %4 m "
                                     "between frames")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(step, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::NoUnexpectedFallPose && !culled) {
          float const minimum_up = expectation.threshold > 0.0F ? expectation.threshold
                                                                : k_default_fall_up_y;
          bool const legitimate_fall =
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dead ||
              (soldier.visual_state ==
                   Render::Profiling::SoldierVisualState::HitReaction &&
               soldier.hit_reaction_kind == Engine::Core::HitReactionKind::Evade);
          float const observed_up = soldier.submitted_body_pose_valid
                                        ? soldier.submitted_body_up_y
                                        : soldier.root_up_y;
          if (observed_up < minimum_up && !legitimate_fall) {
            add_issue(
                QStringLiteral("unexpected_fall_pose"),
                QStringLiteral("%1 entity %2 soldier %3 submitted body up-vector was "
                               "%4 while alive")
                    .arg(group)
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(observed_up, 0, 'f', 2),
                entity_id,
                soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::NoLimbOverextension && !culled &&
            soldier.submitted_body_pose_valid) {
          float const maximum_reach =
              expectation.threshold > 0.0F ? expectation.threshold : 0.60F;
          if (soldier.submitted_max_arm_reach > maximum_reach) {
            add_issue(QStringLiteral("limb_overextension"),
                      QStringLiteral("%1 entity %2 soldier %3 submitted arm reach was "
                                     "%4 m")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(soldier.submitted_max_arm_reach, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        bool const joints_comparable = previous.initialized && previous.joints_valid &&
                                       soldier.joint_sample_valid && !previous.culled &&
                                       !culled &&
                                       elapsed - previous.observed_at <= 0.05F;

        if (expectation.kind == ArenaExpectationKind::NoPlantedFootSliding &&
            joints_comparable) {
          bool const locomoting =
              soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
              soldier.visual_state == Render::Profiling::SoldierVisualState::Run ||
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dead ||
              std::max(soldier.locomotion_presence, previous.locomotion_presence) >
                  k_stride_fade_presence;
          bool const root_planted = step <= k_planted_root_step;
          if (!locomoting && root_planted) {
            float const allowed =
                (expectation.threshold > 0.0F ? expectation.threshold
                                              : k_default_foot_slide) *
                frame_budget_scale;
            float const ground_y = soldier.root_position.y();
            float planted_slide = 0.0F;
            auto const consider_foot = [&](const QVector3D& now,
                                           const QVector3D& before) {
              bool const planted_now = now.y() - ground_y <= k_planted_foot_height;
              bool const planted_before =
                  before.y() - ground_y <= k_planted_foot_height;
              if (!planted_now || !planted_before) {
                return;
              }
              planted_slide = std::max(planted_slide, horizontal_distance(now, before));
            };
            consider_foot(soldier.foot_l_world, previous.foot_l_world);
            consider_foot(soldier.foot_r_world, previous.foot_r_world);

            if (planted_slide > allowed) {
              add_issue(QStringLiteral("planted_foot_slide"),
                        QStringLiteral("%1 entity %2 soldier %3 planted foot slid %4 m "
                                       "between frames while not walking")
                            .arg(group)
                            .arg(entity_id)
                            .arg(soldier.soldier_index)
                            .arg(planted_slide, 0, 'f', 3),
                        entity_id,
                        soldier.soldier_index);
            }
          }
        }

        if (expectation.kind == ArenaExpectationKind::NoWeaponTeleport &&
            joints_comparable) {
          float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                              : k_default_hand_step) *
                                frame_budget_scale;
          float const step_l = (soldier.hand_l_world - previous.hand_l_world).length();
          float const step_r = (soldier.hand_r_world - previous.hand_r_world).length();
          float const worst = std::max(step_l, step_r);
          if (worst > allowed) {
            add_issue(QStringLiteral("weapon_hand_teleport"),
                      QStringLiteral("%1 entity %2 soldier %3 weapon hand jumped %4 m "
                                     "between frames")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(worst, 0, 'f', 3),
                      entity_id,
                      soldier.soldier_index);
          }
        }

        if (expectation.kind == ArenaExpectationKind::NoPelvisSnap &&
            joints_comparable) {
          float const allowed = (expectation.threshold > 0.0F ? expectation.threshold
                                                              : k_default_pelvis_step) *
                                frame_budget_scale;
          float const turn = std::abs(shortest_degrees(soldier.pelvis_yaw_degrees,
                                                       previous.pelvis_yaw_degrees));
          if (turn > allowed) {
            add_issue(
                QStringLiteral("pelvis_snap"),
                QStringLiteral("%1 entity %2 soldier %3 pelvis rotated %4 degrees "
                               "between frames")
                    .arg(group)
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(turn, 0, 'f', 1),
                entity_id,
                soldier.soldier_index);
          }
        }

        if (expectation.kind == ArenaExpectationKind::AttackHasTorsoRotation &&
            soldier.joint_sample_valid && !culled) {
          if (observed_attack) {
            if (!previous.attack_yaw_tracked) {
              previous.attack_pelvis_yaw_min = soldier.pelvis_yaw_degrees;
              previous.attack_pelvis_yaw_max = soldier.pelvis_yaw_degrees;
              previous.attack_yaw_tracked = true;
            } else {
              float const relative = shortest_degrees(soldier.pelvis_yaw_degrees,
                                                      previous.attack_pelvis_yaw_min);
              previous.attack_pelvis_yaw_max =
                  std::max(previous.attack_pelvis_yaw_max,
                           previous.attack_pelvis_yaw_min + relative);
              previous.attack_pelvis_yaw_min =
                  std::min(previous.attack_pelvis_yaw_min,
                           previous.attack_pelvis_yaw_min + relative);
            }
          } else if (previous.attack_yaw_tracked) {
            float const swept =
                previous.attack_pelvis_yaw_max - previous.attack_pelvis_yaw_min;
            float const required = expectation.threshold > 0.0F
                                       ? expectation.threshold
                                       : k_default_attack_torso_sweep;
            if (swept < required) {
              add_issue(QStringLiteral("attack_without_torso_rotation"),
                        QStringLiteral("%1 entity %2 soldier %3 swung with only %4 "
                                       "degrees of torso rotation")
                            .arg(group)
                            .arg(entity_id)
                            .arg(soldier.soldier_index)
                            .arg(swept, 0, 'f', 1),
                        entity_id,
                        soldier.soldier_index);
            }
            previous.attack_yaw_tracked = false;
          }
        }

        if (expectation.kind == ArenaExpectationKind::AllLivingSoldiersFight &&
            living_formation_fighter) {
          if (observed_attack) {
            previous.fight_idle_since = -1.0F;
          } else if (previous.fight_idle_since < 0.0F) {
            previous.fight_idle_since = elapsed;
          } else {
            float const allowed_idle =
                expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
            if (elapsed - previous.fight_idle_since > allowed_idle) {
              add_issue(QStringLiteral("living_soldier_idle_in_fight"),
                        QStringLiteral("%1 entity %2 soldier %3 stayed out of its "
                                       "unit fight animation for %4 s")
                            .arg(group)
                            .arg(entity_id)
                            .arg(soldier.soldier_index)
                            .arg(elapsed - previous.fight_idle_since, 0, 'f', 2),
                        entity_id,
                        soldier.soldier_index);
            }
          }
          bool const terminal_attack_pose =
              observed_attack && soldier.attack_phase >= 0.99F;
          if (!terminal_attack_pose) {
            previous.terminal_pose_since = -1.0F;
          } else if (previous.terminal_pose_since < 0.0F) {
            previous.terminal_pose_since = elapsed;
          } else if (elapsed - previous.terminal_pose_since > 0.25F) {
            add_issue(QStringLiteral("fight_animation_terminal_stall"),
                      QStringLiteral("%1 entity %2 soldier %3 held its terminal "
                                     "fight pose for %4 s")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(elapsed - previous.terminal_pose_since, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::HoldPoseMaintained && !culled) {
          auto const* hold = world.try_get<Engine::Core::HoldModeComponent>(entity_id);
          if (hold != nullptr && hold->active && hold->kneel_entry_progress >= 0.999F &&
              soldier.animation_state != Render::Creature::AnimationStateId::Hold) {
            add_issue(
                QStringLiteral("hold_pose_replaced"),
                QStringLiteral("%1 entity %2 soldier %3 rendered %4 while Hold was "
                               "active")
                    .arg(group)
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(QString::fromLatin1(Render::Profiling::animation_state_name(
                        soldier.animation_state))),
                entity_id,
                soldier.soldier_index);
          }
        }
      }
      if (observed_attack) {
        attacking_soldiers_by_group[group].insert(key);
        minimum_attack_phase = std::min(minimum_attack_phase, soldier.attack_phase);
        maximum_attack_phase = std::max(maximum_attack_phase, soldier.attack_phase);
        attack_phase_bins.insert(
            std::clamp(static_cast<int>(soldier.attack_phase * 12.0F), 0, 11));
        ++visible_attack_count;
        visible_attacks[group] = true;
        useful_bot_action[group] = true;
        if (presentation != nullptr && presentation->melee_ordered) {
          bool const physical_strike =
              directive != nullptr &&
              directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged;
          bool const declared_follow_through =
              directive != nullptr &&
              directive->action ==
                  Engine::Core::FormationSoldierAction::MeleeFollowThrough;
          bool const continuing_visible_attack =
              declared_follow_through && previous.initialized && previous.attacking;

          if (physical_strike || continuing_visible_attack) {
            paired_visible_attacks[group] = true;
          }
        } else {

          paired_visible_attacks[group] = true;
        }
        if (auto response = responses.find(entity_id); response != responses.end()) {
          response->observed = true;
        }
      }
      previous.root_position = soldier.root_position;
      previous.hand_l_world = soldier.hand_l_world;
      previous.hand_r_world = soldier.hand_r_world;
      previous.foot_l_world = soldier.foot_l_world;
      previous.foot_r_world = soldier.foot_r_world;
      previous.pelvis_yaw_degrees = soldier.pelvis_yaw_degrees;
      previous.locomotion_presence = soldier.locomotion_presence;
      previous.joints_valid = soldier.joint_sample_valid;
      previous.observed_at = elapsed;
      previous.initialized = true;
      previous.culled = culled;
      previous.attacking = observed_attack;
      previous.alive = continuity_alive;
    }

    if (verify_render_continuity) {
      const auto previous_samples = sampled_soldiers_by_entity.value(entity_id);
      for (int const soldier_index : previous_samples) {
        if (sampled_this_frame.contains(soldier_index)) {
          continue;
        }
        const auto previous =
            soldier_states.constFind(soldier_key(entity_id, soldier_index));
        bool still_alive = previous != soldier_states.cend() && previous->alive;
        if (presentation != nullptr && soldier_index >= 0) {
          const auto slot = static_cast<std::size_t>(soldier_index);
          still_alive = still_alive && slot < presentation->soldiers.size() &&
                        presentation->soldiers[slot].alive;
        }
        if (still_alive) {
          add_issue(QStringLiteral("soldier_submission_missing"),
                    QStringLiteral("%1 entity %2 soldier %3 vanished from the "
                                   "render sample set")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier_index),
                    entity_id,
                    soldier_index);
        }
      }
    }
    check_lens_gap_readability(
        entity_id, group, living_soldier_samples, lens_gap_culled_samples);
    sampled_soldiers_by_entity[entity_id] = std::move(sampled_this_frame);
    bool const small_group_staggered =
        visible_attack_count >= 2 && visible_attack_count < 8 &&
        maximum_attack_phase - minimum_attack_phase >= 0.02F;
    bool const formation_staggered =
        visible_attack_count >= 8 && attack_phase_bins.size() >= 4 &&
        maximum_attack_phase - minimum_attack_phase >= 0.20F;
    if (small_group_staggered || formation_staggered) {
      staggered_attack_phases[group] = true;
    }
  }

  void check_lens_gap_readability(Engine::Core::EntityID entity_id,
                                  const QString& group,
                                  int living_samples,
                                  int lens_gap_culled) {
    if (living_samples <= 0 || lens_gap_culled <= 0) {
      return;
    }
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::RpgFormationSurvivesLensGap ||
          !expectation_active(expectation) || !applies_to(expectation, group)) {
        continue;
      }
      float const allowed_fraction =
          expectation.threshold > 0.0F ? expectation.threshold : 0.5F;
      float const culled_fraction =
          static_cast<float>(lens_gap_culled) / static_cast<float>(living_samples);
      if (culled_fraction > allowed_fraction) {
        add_issue(QStringLiteral("formation_erased_by_lens_gap"),
                  QStringLiteral("%1 entity %2 dropped %3 of %4 living soldiers to "
                                 "the chase-lens gap in one frame")
                      .arg(group)
                      .arg(entity_id)
                      .arg(lens_gap_culled)
                      .arg(living_samples),
                  entity_id);
      }
    }
  }

  void check_formation_order(const ArenaExpectation& expectation) {
    auto const* definition = group_definition(expectation.group);
    auto const& group_ids = ids(expectation.group);
    if (definition == nullptr || group_ids.size() < 2) {
      return;
    }
    QVector3D axis = definition->spacing;
    axis.setY(0.0F);
    if (axis.lengthSquared() < 0.0001F) {
      return;
    }
    axis.normalize();
    float const tolerance =
        expectation.threshold > 0.0F ? expectation.threshold : 0.75F;
    float previous_projection = -std::numeric_limits<float>::max();
    for (auto entity_id : group_ids) {
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      float const projection =
          QVector3D::dotProduct(vector_from_transform(*transform), axis);
      if (projection + tolerance < previous_projection) {
        add_issue(QStringLiteral("formation_order_inversion"),
                  QStringLiteral("%1 formation members crossed their stable order")
                      .arg(expectation.group),
                  entity_id);
        return;
      }
      previous_projection = projection;
    }
  }

  void publish_movement_diagnostics() {
    report.movement.clear();
    for (auto const& group : scenario.groups) {
      auto const found = stall_observations.constFind(group.name);
      if (found == stall_observations.constEnd()) {
        continue;
      }
      ArenaGroupMovementDiagnostics row;
      row.group = group.name;
      row.worst_stalled_seconds = found->worst_stalled_seconds;
      row.worst_stalled_at = found->worst_stalled_at;
      row.recovery_attempts = found->recovery_attempts;
      row.repaths = found->repaths;
      row.abandons = found->abandons;
      row.worst_state = found->worst_state;
      row.has_objective = found->has_objective;
      row.objective_x = found->objective_x;
      row.objective_z = found->objective_z;
      for (auto entity_id : ids(group.name)) {
        auto const* movement =
            world.try_get<Engine::Core::MovementComponent>(entity_id);
        auto const* movement_facts =
            world.try_get<Engine::Core::MovementFactsComponent>(entity_id);
        if (movement == nullptr || movement_facts == nullptr ||
            !entity_alive(entity_id)) {
          continue;
        }
        if (movement->get_has_target() &&
            movement_facts->progress.stall.rung !=
                Engine::Core::MovementRecoveryRung::None) {
          ++row.units_holding_a_stalled_objective;
        }
      }
      if (row.worst_stalled_seconds <= 0.0F && row.recovery_attempts == 0U &&
          row.repaths == 0U && row.abandons == 0U &&
          row.units_holding_a_stalled_objective == 0) {
        continue;
      }
      report.movement.push_back(std::move(row));
    }
  }

  void publish_narrow_layout_outcome() {
    report.narrow_layout.clear();
    for (auto const& group : scenario.groups) {
      if (!narrow_layout.contains(group.name)) {
        continue;
      }
      auto const& state = narrow_layout.value(group.name);
      ArenaNarrowLayoutOutcome outcome;
      outcome.group = group.name;
      outcome.engaged = state.engaged;
      outcome.formation_half_width = state.formation_half_width;
      outcome.narrowest_corridor_half_width = state.narrowest_corridor;
      outcome.normal_files = state.normal_files;
      outcome.narrowest_files = state.narrowest_files;
      outcome.tightest_file_spacing = state.tightest_file_spacing;
      outcome.narrowest_frontage = state.narrowest_frontage;
      outcome.deepest_column = state.deepest_column;
      outcome.narrowest_mode = QString::fromLatin1(
          Engine::Core::traversal_layout_mode_name(state.narrowest_mode));
      outcome.mode_changes = state.mode_changes;
      outcome.worst_reform_error = state.worst_reform_error;
      report.narrow_layout.push_back(std::move(outcome));
    }
  }

  void check_end_expectations() {
    if (end_expectations_checked) {
      return;
    }
    end_expectations_checked = true;
    observe_battle();
    publish_battle_outcome();
    publish_movement_diagnostics();
    publish_narrow_layout_outcome();
    for (auto const& expectation : scenario.expectations) {
      switch (expectation.kind) {
      case ArenaExpectationKind::BattleReachesDecision: {
        if (battle_sides.size() < 2U) {
          add_issue(QStringLiteral("battle_not_tracked"),
                    QStringLiteral("BattleReachesDecision needs at least two "
                                   "authored battle sides"));
          break;
        }
        if (!battle_decided) {
          QStringList standing;
          for (auto const& side : battle_sides) {
            if (side.eliminated_at < 0.0F) {
              standing.push_back(QStringLiteral("%1 (%2 units, %3 buildings)")
                                     .arg(side.label)
                                     .arg(side.living_units)
                                     .arg(side.living_buildings));
            }
          }
          add_issue(QStringLiteral("battle_undecided"),
                    QStringLiteral("no side was eliminated within %1 s; still "
                                   "standing: %2")
                        .arg(elapsed, 0, 'f', 1)
                        .arg(standing.join(QStringLiteral(", "))));
        }
        break;
      }
      case ArenaExpectationKind::SideSurvives: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else if (side->eliminated_at >= 0.0F) {
          add_issue(QStringLiteral("battle_side_eliminated"),
                    QStringLiteral("%1 was wiped out at %2 s")
                        .arg(side->label)
                        .arg(side->eliminated_at, 0, 'f', 1));
        }
        break;
      }
      case ArenaExpectationKind::SideAdvanceAtLeast: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else {
          auto const peak = windowed_peak_advance(*side, expectation);
          if (!peak.has_value()) {
            add_issue(QStringLiteral("side_advance_unsampled"),
                      QStringLiteral("%1 produced no army position samples in its "
                                     "measurement window")
                          .arg(side->label));
          } else if (*peak < expectation.threshold) {
            add_issue(QStringLiteral("side_advance_too_small"),
                      QStringLiteral("%1 pushed only %2 of the way to the enemy "
                                     "base%3, expected at least %4")
                          .arg(side->label)
                          .arg(*peak, 0, 'f', 3)
                          .arg(describe_window(expectation))
                          .arg(expectation.threshold, 0, 'f', 3));
          }
        }
        break;
      }
      case ArenaExpectationKind::SideAdvanceAtMost: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else {
          auto const peak = windowed_peak_advance(*side, expectation);
          if (peak.has_value() && *peak > expectation.threshold) {
            add_issue(QStringLiteral("side_advance_too_large"),
                      QStringLiteral("%1 pushed %2 of the way to the enemy base%3, "
                                     "expected at most %4")
                          .arg(side->label)
                          .arg(*peak, 0, 'f', 3)
                          .arg(describe_window(expectation))
                          .arg(expectation.threshold, 0, 'f', 3));
          }
        }
        break;
      }
      case ArenaExpectationKind::SideProducesReinforcements: {
        auto const* side = battle_side(expectation.side);
        int const required =
            std::max(1, static_cast<int>(std::lround(expectation.threshold)));
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else {
          int const produced =
              static_cast<int>(side->seen_units.size()) - side->initial_units;
          if (produced < required) {
            add_issue(QStringLiteral("side_produced_too_few"),
                      QStringLiteral("%1 fielded %2 new units, expected at least %3")
                          .arg(side->label)
                          .arg(produced)
                          .arg(required));
          }
        }
        break;
      }
      case ArenaExpectationKind::SideBuildsAtLeast: {
        auto const* side = battle_side(expectation.side);
        int const required =
            std::max(1, static_cast<int>(std::lround(expectation.threshold)));
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else {
          int const built = static_cast<int>(side->seen_buildings.size()) -
                            static_cast<int>(side->initial_buildings.size());
          if (built < required) {
            add_issue(QStringLiteral("side_built_too_little"),
                      QStringLiteral("%1 raised %2 new buildings, expected at "
                                     "least %3")
                          .arg(side->label)
                          .arg(built)
                          .arg(required));
          }
        }
        break;
      }
      case ArenaExpectationKind::SideKeepsGarrison: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else if (side->peak_home_units < expectation.threshold) {
          add_issue(QStringLiteral("side_left_home_open"),
                    QStringLiteral("%1 never held more than %2 units within %3 m of "
                                   "its own base, expected at least %4")
                        .arg(side->label)
                        .arg(side->peak_home_units)
                        .arg(side->home_radius, 0, 'f', 0)
                        .arg(expectation.threshold, 0, 'f', 0));
        }
        break;
      }
      case ArenaExpectationKind::SideFieldsArmy: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else if (side->peak_forward_units < expectation.threshold) {
          add_issue(QStringLiteral("side_never_fielded_army"),
                    QStringLiteral("%1 never pushed more than %2 units past the "
                                   "midpoint, expected at least %3")
                        .arg(side->label)
                        .arg(side->peak_forward_units)
                        .arg(expectation.threshold, 0, 'f', 0));
        }
        break;
      }
      case ArenaExpectationKind::SideDoctrineIs: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
          break;
        }
        if (side->strategy.isEmpty()) {
          add_issue(QStringLiteral("side_doctrine_unsampled"),
                    QStringLiteral("%1 never reported an AI strategy; the scenario "
                                   "host did not expose one")
                        .arg(side->label));
          break;
        }
        QString const expected = expectation.counter_key;
        QString const actual =
            side->posture.isEmpty()
                ? side->strategy
                : side->strategy + QStringLiteral(":") + side->posture;
        if (expected != side->strategy && expected != actual) {
          add_issue(QStringLiteral("side_doctrine_mismatch"),
                    QStringLiteral("%1 ran the %2 doctrine, expected %3")
                        .arg(side->label, actual, expected));
        }
        break;
      }
      case ArenaExpectationKind::SideCommitsToAttack: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else if (side->seconds_attacking < expectation.threshold) {
          add_issue(QStringLiteral("side_never_committed"),
                    QStringLiteral("%1 spent %2 s in an attacking state, expected "
                                   "at least %3 s")
                        .arg(side->label)
                        .arg(side->seconds_attacking, 0, 'f', 1)
                        .arg(expectation.threshold, 0, 'f', 1));
        }
        break;
      }
      case ArenaExpectationKind::SideHoldsPosition: {
        auto const* side = battle_side(expectation.side);
        if (side == nullptr) {
          add_issue(QStringLiteral("battle_side_unknown"),
                    QStringLiteral("%1 is not an authored battle side")
                        .arg(expectation.side));
        } else if (side->seconds_attacking > expectation.threshold) {
          add_issue(QStringLiteral("side_did_not_hold"),
                    QStringLiteral("%1 spent %2 s in an attacking state, expected "
                                   "at most %3 s")
                        .arg(side->label)
                        .arg(side->seconds_attacking, 0, 'f', 1)
                        .arg(expectation.threshold, 0, 'f', 1));
        }
        break;
      }
      case ArenaExpectationKind::AttackAnimationObserved:
        if (!visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack"),
                    QStringLiteral("%1 never produced a visible attack animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RepeatedAttackAnimationObserved: {
        int const required_cycles =
            expectation.threshold > 0.0F
                ? std::max(2, static_cast<int>(expectation.threshold))
                : 2;
        int missing = 0;
        for (auto const soldier : living_soldiers_by_group.value(expectation.group)) {
          if (attack_entries_by_soldier.value(soldier, 0) < required_cycles) {
            ++missing;
          }
        }
        if (missing > 0) {
          add_issue(QStringLiteral("soldiers_did_not_repeat_attack"),
                    QStringLiteral("%1 had %2 living soldiers with fewer than %3 "
                                   "visible attack cycles")
                        .arg(expectation.group)
                        .arg(missing)
                        .arg(required_cycles));
        }
        break;
      }
      case ArenaExpectationKind::MovementAnimationObserved:
        if (!visible_movement.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_movement"),
                    QStringLiteral("%1 never produced a visible movement animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AttackHasVisibleContact:
        if (!visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack"),
                    QStringLiteral("%1 never produced a visible attack animation")
                        .arg(expectation.group));
        } else if (!paired_visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_paired_visible_attack"),
                    QStringLiteral("%1 never attacked through a physical soldier "
                                   "engagement pair")
                        .arg(expectation.group));
        } else if (!expectation.target_group.isEmpty() &&
                   !damage_seen.value(expectation.target_group, false) &&
                   !group_destroyed(expectation.target_group)) {
          add_issue(QStringLiteral("no_visible_contact_result"),
                    QStringLiteral("%1 attacked but %2 never registered contact damage")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      case ArenaExpectationKind::ProjectileFlightObserved: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (!projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("projectile_flight_not_observed"),
                    QStringLiteral("%1 never showed an in-flight projectile toward %2")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::ProjectileImpactObserved: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (!projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("projectile_flight_not_observed"),
                    QStringLiteral("%1 never showed an in-flight projectile toward %2")
                        .arg(expectation.group, expectation.target_group));
        } else if (!projectile_contacts.value(key, false)) {
          add_issue(QStringLiteral("projectile_contact_not_observed"),
                    QStringLiteral("%1 produced no visible projectile contact on %2")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::ProjectileImpactSynchronized: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (!projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("projectile_flight_not_observed"),
                    QStringLiteral("%1 never showed an in-flight projectile toward %2")
                        .arg(expectation.group, expectation.target_group));
        } else if (!projectile_impacts.value(key, false)) {
          add_issue(
              QStringLiteral("projectile_impact_not_synchronized"),
              QStringLiteral("%1 produced no renderer-facing impact record in the "
                             "same simulation event that damaged %2")
                  .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::GroupHealthUnchanged: {
        int const initial = initial_health_by_group.value(expectation.group, -1);
        int const current = group_health(expectation.group);
        if (initial < 0 || current != initial) {
          add_issue(QStringLiteral("group_health_changed"),
                    QStringLiteral("%1 health changed from %2 to %3")
                        .arg(expectation.group)
                        .arg(initial)
                        .arg(current));
        }
        break;
      }
      case ArenaExpectationKind::GroupHealthReduced: {
        int const initial = initial_health_by_group.value(expectation.group, -1);
        int const current = group_health(expectation.group);
        int const required_drop = std::max(1, static_cast<int>(expectation.threshold));
        if (initial < 0 || current > initial - required_drop) {
          add_issue(QStringLiteral("group_health_not_reduced"),
                    QStringLiteral("%1 health changed from %2 to %3; expected at "
                                   "least %4 damage")
                        .arg(expectation.group)
                        .arg(initial)
                        .arg(current)
                        .arg(required_drop));
        }
        break;
      }
      case ArenaExpectationKind::StructureDamageCueObserved:
        if (!structure_damage_cues.value(expectation.group, false)) {
          add_issue(QStringLiteral("structure_damage_cue_not_observed"),
                    QStringLiteral("%1 never exposed a localized facade damage cue")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::StructureFireObserved:
        if (!structure_fires.value(expectation.group, false)) {
          add_issue(QStringLiteral("structure_fire_not_observed"),
                    QStringLiteral("%1 never caught fire").arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoStructureFireObserved:
        if (structure_fires.value(expectation.group, false)) {
          add_issue(QStringLiteral("unexpected_structure_fire"),
                    QStringLiteral("%1 burned without taking incendiary damage")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::FlamingProjectileObserved: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (!flaming_projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("flaming_projectile_not_observed"),
                    QStringLiteral("%1 never sent flaming shot at %2")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::NoFlamingProjectileObserved: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (flaming_projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("unexpected_flaming_projectile"),
                    QStringLiteral("%1 sent flaming shot at %2")
                        .arg(expectation.group, expectation.target_group));
        } else if (!plain_projectile_flights.value(key, false)) {
          add_issue(QStringLiteral("projectile_flight_not_observed"),
                    QStringLiteral("%1 never sent a shot at %2")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::StructureFacadeContactObserved: {
        auto const key =
            projectile_pair_key(expectation.group, expectation.target_group);
        if (!structure_facade_contacts.value(key, false)) {
          add_issue(QStringLiteral("structure_facade_contact_not_observed"),
                    QStringLiteral("%1 never reached the visible facade of %2")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::FormationBodyOverlapObserved: {
        float const required_overlap =
            expectation.threshold > 0.0F ? expectation.threshold : 0.20F;
        auto const observed = minimum_formation_surface_gap.find(expectation.group);
        if (observed == minimum_formation_surface_gap.end() ||
            observed.value() > -required_overlap) {
          add_issue(
              QStringLiteral("formation_body_overlap_not_observed"),
              observed == minimum_formation_surface_gap.end()
                  ? QStringLiteral("%1 never established visible formation contact")
                        .arg(expectation.group)
                  : QStringLiteral("%1 reached only %2 m of body overlap (required "
                                   "%3 m)")
                        .arg(expectation.group)
                        .arg(-observed.value(), 0, 'f', 2)
                        .arg(required_overlap, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::DeathAnimationObserved:
        if (!visible_deaths.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_death"),
                    QStringLiteral("%1 never produced a visible death animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::LaunchedCasualtyObserved:
        if (!launched_casualties.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_launched_casualty"),
                    QStringLiteral("%1 never produced an impact-launched casualty")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoLaunchedCasualtyObserved:
        if (launched_casualties.value(expectation.group, false)) {
          add_issue(QStringLiteral("unexpected_launched_casualty"),
                    QStringLiteral("%1 produced a launched casualty despite its "
                                   "braced hold")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::ChargeImpactPrecedesMeleeLock:
        if (!charge_impacts.value(expectation.group, false)) {
          add_issue(QStringLiteral("charge_impact_not_observed"),
                    QStringLiteral("%1 never entered its one-shot mounted charge "
                                   "impact action")
                        .arg(expectation.group));
        } else if (!melee_locks_after_charge.value(expectation.group, false)) {
          add_issue(QStringLiteral("melee_did_not_follow_charge"),
                    QStringLiteral("%1 did not enter ordinary melee lock after its "
                                   "mounted charge impact")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AttackRecoveryObserved:
        if (!visible_attack_recoveries.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack_recovery"),
                    QStringLiteral("%1 never completed an attack and recovered to a "
                                   "controlled movement or idle pose")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoActiveCombatAtEnd: {
        bool active_combat = false;
        for (auto entity_id : ids(expectation.group)) {
          auto* entity = world.get_entity(entity_id);
          if (entity == nullptr || !entity_alive(entity_id)) {
            continue;
          }
          auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
          auto const* target =
              entity->get_component<Engine::Core::AttackTargetComponent>();
          auto const* combat =
              entity->get_component<Engine::Core::CombatStateComponent>();
          auto const* action =
              entity->get_component<Engine::Core::RpgCommanderActionComponent>();
          auto const* presentation =
              entity->get_component<Engine::Core::CreaturePresentationComponent>();
          active_combat =
              active_combat || (attack != nullptr && attack->in_melee_lock) ||
              (target != nullptr && target->target_id != 0) ||
              (combat != nullptr &&
               combat->animation_state != Engine::Core::CombatAnimationState::Idle) ||
              (action != nullptr &&
               (action->action_running || action->action_active)) ||
              (presentation != nullptr &&
               (presentation->is_attacking || presentation->combat_active));
        }
        if (active_combat) {
          add_issue(QStringLiteral("combat_continued_without_opponent"),
                    QStringLiteral("%1 retained attack state at scenario end")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::HitReactionObserved:
        if (!visible_hit_reactions.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_hit_reaction"),
                    QStringLiteral("%1 never produced a visible hit reaction")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AllLivingSoldiersFight: {
        auto missing = living_soldiers_by_group.value(expectation.group);
        missing.subtract(attacking_soldiers_by_group.value(expectation.group));
        if (!missing.isEmpty()) {
          add_issue(QStringLiteral("soldiers_never_fought"),
                    QStringLiteral("%1 had %2 living soldiers that never "
                                   "produced a fight animation")
                        .arg(expectation.group)
                        .arg(missing.size()));
        }
        if (!engaged_soldiers_by_group.value(expectation.group).isEmpty() &&
            !staggered_attack_phases.value(expectation.group, false)) {
          add_issue(QStringLiteral("synchronized_formation_attacks"),
                    QStringLiteral("%1 never produced visibly staggered per-soldier "
                                   "fight phases")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::TargetRetakenAfterDeath: {
        bool retaken = false;
        auto const& valid_targets = ids(expectation.target_group);
        for (auto entity_id : ids(expectation.group)) {
          auto const* target =
              world.try_get<Engine::Core::AttackTargetComponent>(entity_id);
          retaken = retaken || (target != nullptr &&
                                std::find(valid_targets.begin(),
                                          valid_targets.end(),
                                          target->target_id) != valid_targets.end());
        }
        if (!retaken) {
          add_issue(QStringLiteral("target_not_retaken"),
                    QStringLiteral("%1 did not retarget %2 after its first target died")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::BotIssuesUsefulCommand:
        if (!useful_bot_action.value(expectation.group, false)) {
          add_issue(QStringLiteral("bot_inactive"),
                    QStringLiteral("AI group %1 never moved or acquired a target")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RangeIndicatorObserved: {
        if (!range_ring_max_radius.contains(expectation.group)) {
          add_issue(QStringLiteral("range_indicator_missing"),
                    QStringLiteral("%1 never produced a range indicator ring")
                        .arg(expectation.group));
          break;
        }
        float const observed = range_ring_max_radius.value(expectation.group, 0.0F);
        if (expectation.threshold > 0.0F && std::abs(observed - expectation.threshold) >
                                                expectation.threshold * 0.02F) {
          add_issue(QStringLiteral("range_indicator_radius_mismatch"),
                    QStringLiteral("%1 drew a %2 range ring, expected %3")
                        .arg(expectation.group)
                        .arg(observed, 0, 'f', 2)
                        .arg(expectation.threshold, 0, 'f', 2));
        }
        float const observed_min = range_ring_min_radius.value(expectation.group, 0.0F);
        if (expectation.distance > 0.0F &&
            std::abs(observed_min - expectation.distance) >
                expectation.distance * 0.02F) {
          add_issue(QStringLiteral("range_indicator_min_radius_mismatch"),
                    QStringLiteral("%1 drew a %2 minimum range ring, expected %3")
                        .arg(expectation.group)
                        .arg(observed_min, 0, 'f', 2)
                        .arg(expectation.distance, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::RangeIndicatorCountAtMost:
        if (expectation.threshold > 0.0F &&
            static_cast<float>(max_range_ring_count) > expectation.threshold) {
          add_issue(QStringLiteral("range_indicator_count_exceeded"),
                    QStringLiteral("range indicators peaked at %1 rings, cap is %2")
                        .arg(max_range_ring_count)
                        .arg(static_cast<int>(expectation.threshold)));
        }
        break;
      case ArenaExpectationKind::CommanderPresentedPoseAgrees: {
        float const allowed =
            expectation.threshold > 0.0F ? expectation.threshold : 0.01F;
        auto const frames = commander_frames();
        float worst = 0.0F;
        float worst_time = 0.0F;
        float worst_yaw = 0.0F;
        float worst_yaw_time = 0.0F;
        int compared = 0;
        for (auto const* frame : frames) {
          auto const& shot = frame->commander.camera;
          if (!shot.valid || frame->soldiers.empty()) {
            continue;
          }
          ++compared;
          float const gap = horizontal_distance(shot.commander_position,
                                                frame->soldiers[0].root_position);
          if (gap > worst) {
            worst = gap;
            worst_time = frame->time_seconds;
          }

          float const yaw_error =
              std::abs(shortest_degrees(frame->commander.motor.presented_yaw,
                                        frame->soldiers[0].root_yaw_degrees));
          if (yaw_error > worst_yaw) {
            worst_yaw = yaw_error;
            worst_yaw_time = frame->time_seconds;
          }
        }
        if (compared == 0) {
          add_issue(QStringLiteral("commander_pose_not_compared"),
                    QStringLiteral("%1 never had a camera and a rendered body in the "
                                   "same frame")
                        .arg(expectation.group));
          break;
        }
        if (worst > allowed) {
          add_issue(QStringLiteral("commander_presented_pose_disagreement"),
                    QStringLiteral("the camera framed a point %1 m from the body it "
                                   "was drawing at %2 s (allowed %3 m)")
                        .arg(worst, 0, 'f', 4)
                        .arg(worst_time, 0, 'f', 2)
                        .arg(allowed, 0, 'f', 4));
        }
        if (worst_yaw > k_presented_yaw_allowance_degrees) {
          add_issue(QStringLiteral("commander_presented_yaw_disagreement"),
                    QStringLiteral("the presented pose faced %1 degrees away from the "
                                   "body that was drawn at %2 s (allowed %3)")
                        .arg(worst_yaw, 0, 'f', 3)
                        .arg(worst_yaw_time, 0, 'f', 2)
                        .arg(k_presented_yaw_allowance_degrees, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::CommanderCameraClearanceAtLeast: {
        float const required =
            expectation.threshold > 0.0F ? expectation.threshold : 0.10F;
        auto const frames = commander_frames();
        float worst = std::numeric_limits<float>::max();
        float worst_time = 0.0F;
        int sampled = 0;
        for (auto const* frame : frames) {
          auto const& shot = frame->commander.camera;
          if (!shot.valid) {
            continue;
          }
          ++sampled;
          if (shot.eye_clearance < worst) {
            worst = shot.eye_clearance;
            worst_time = frame->time_seconds;
          }
        }
        if (sampled == 0) {
          add_issue(QStringLiteral("commander_camera_not_traced"),
                    QStringLiteral("%1 never published a camera trace")
                        .arg(expectation.group));
          break;
        }
        if (worst < 0.0F) {
          add_issue(QStringLiteral("commander_camera_penetrated"),
                    QStringLiteral("camera eye was %1 m inside an obstacle at %2 s")
                        .arg(-worst, 0, 'f', 3)
                        .arg(worst_time, 0, 'f', 2));
        } else if (worst < required) {
          add_issue(QStringLiteral("commander_camera_clearance"),
                    QStringLiteral("camera eye came within %1 m of an obstacle at %2 s "
                                   "(needs %3 m)")
                        .arg(worst, 0, 'f', 3)
                        .arg(worst_time, 0, 'f', 2)
                        .arg(required, 0, 'f', 3));
        }
        break;
      }
      case ArenaExpectationKind::CommanderCameraKeepsCommanderInSight: {

        float const allowed =
            expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
        auto const frames = commander_frames();
        float blocked_run = 0.0F;
        float worst_run = 0.0F;
        float worst_run_end = 0.0F;
        int sampled = 0;
        for (auto const* frame : frames) {
          auto const& shot = frame->commander.camera;
          if (!shot.valid) {
            continue;
          }
          ++sampled;
          if (shot.sight_line_clear) {
            blocked_run = 0.0F;
            continue;
          }
          blocked_run += std::max(shot.dt, 0.0F);
          if (blocked_run > worst_run) {
            worst_run = blocked_run;
            worst_run_end = frame->time_seconds;
          }
        }
        if (sampled == 0) {
          add_issue(QStringLiteral("commander_camera_not_traced"),
                    QStringLiteral("%1 never published a camera trace")
                        .arg(expectation.group));
          break;
        }
        if (worst_run > allowed) {
          add_issue(
              QStringLiteral("commander_camera_lost_sight"),
              QStringLiteral("geometry stood between the lens and the commander for "
                             "%1 s, ending at %2 s (allowed %3 s); the boom has to "
                             "shorten until he is in sight and recover on its own")
                  .arg(worst_run, 0, 'f', 3)
                  .arg(worst_run_end, 0, 'f', 2)
                  .arg(allowed, 0, 'f', 3));
        }
        break;
      }
      case ArenaExpectationKind::CommanderBoomIsContinuous: {
        float const budget =
            expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
        auto const frames = commander_frames();
        float previous_boom = 0.0F;
        float previous_step = 0.0F;
        bool have_previous = false;
        int reversals = 0;
        for (auto const* frame : frames) {
          auto const& shot = frame->commander.camera;
          if (!shot.valid) {
            continue;
          }
          if (have_previous) {
            float const step = shot.boom_resolved - previous_boom;

            float const allowed = budget * std::max(1.0F, shot.dt * 60.0F);
            if (step > allowed) {
              add_issue(QStringLiteral("commander_boom_discontinuity"),
                        QStringLiteral("camera boom extended %1 m in one frame at %2 s "
                                       "(allowed %3 m); retraction may be immediate "
                                       "but release has to be damped")
                            .arg(step, 0, 'f', 3)
                            .arg(frame->time_seconds, 0, 'f', 2)
                            .arg(allowed, 0, 'f', 3));
              break;
            }
            if (std::abs(step) > k_boom_reversal_floor &&
                std::abs(previous_step) > k_boom_reversal_floor &&
                ((step > 0.0F) != (previous_step > 0.0F)) &&
                shot.boom_clear_fraction < 1.0F) {
              ++reversals;
            }
            previous_step = step;
          }
          previous_boom = shot.boom_resolved;
          have_previous = true;
        }
        int const allowed_reversals =
            std::max(2, static_cast<int>(std::lround(expectation.distance)));
        if (reversals > allowed_reversals) {
          add_issue(QStringLiteral("commander_boom_pumping"),
                    QStringLiteral("camera boom reversed direction %1 times while an "
                                   "obstruction stayed active (allowed %2)")
                        .arg(reversals)
                        .arg(allowed_reversals));
        }
        break;
      }
      case ArenaExpectationKind::NoUncommandedViewRotation: {
        float const allowed =
            expectation.threshold > 0.0F ? expectation.threshold : 0.05F;
        for (auto const* frame : commander_frames()) {
          auto const& shot = frame->commander.camera;
          auto const& in = frame->commander.input;
          if (!shot.valid || shot.framing_changed ||
              frame->commander.combat.locked_target_id != 0) {
            continue;
          }
          if (std::abs(in.look_delta_yaw) > 1.0e-4F ||
              std::abs(in.look_delta_pitch) > 1.0e-4F) {
            continue;
          }
          float const yaw_step = std::abs(shot.yaw_velocity * shot.dt);
          float const pitch_step = std::abs(shot.pitch_velocity * shot.dt);
          if (yaw_step > allowed || pitch_step > allowed) {
            add_issue(
                QStringLiteral("commander_view_rotated_uncommanded"),
                QStringLiteral("view turned %1 deg yaw / %2 deg pitch at %3 s with no "
                               "look input, no lock and no framing change (allowed %4)")
                    .arg(yaw_step, 0, 'f', 3)
                    .arg(pitch_step, 0, 'f', 3)
                    .arg(frame->time_seconds, 0, 'f', 2)
                    .arg(allowed, 0, 'f', 3));
            break;
          }
        }
        break;
      }
      case ArenaExpectationKind::CommanderMotorCorrectionWithin: {
        float const budget =
            expectation.threshold > 0.0F ? expectation.threshold : 0.08F;
        for (auto const* frame : commander_frames()) {
          auto const& motor = frame->commander.motor;
          float const correction =
              std::max(motor.snap_back_distance, motor.separation_push);

          float const allowed = budget * std::max(1.0F, motor.dt * 60.0F);
          if (correction > allowed) {
            add_issue(
                QStringLiteral("commander_motor_correction"),
                QStringLiteral("motor corrected the body %1 m in one tick at %2 s via "
                               "%3 (allowed %4 m)")
                    .arg(correction, 0, 'f', 3)
                    .arg(frame->time_seconds, 0, 'f', 2)
                    .arg(QString::fromLatin1(
                        App::Core::displacement_source_name(motor.displacement_source)))
                    .arg(allowed, 0, 'f', 3));
            break;
          }
        }
        break;
      }
      case ArenaExpectationKind::CommanderSpeedIsContinuous: {
        float const allowed =
            expectation.threshold > 0.0F ? expectation.threshold : 4.0F;
        float previous_speed = 0.0F;
        bool have_previous = false;
        for (auto const* frame : commander_frames()) {
          auto const& motor = frame->commander.motor;
          float const speed = motor.actual_velocity.length();
          if (have_previous && motor.dt > 0.0F) {
            float const change = std::abs(speed - previous_speed) / motor.dt;
            if (change > allowed) {
              add_issue(
                  QStringLiteral("commander_speed_discontinuity"),
                  QStringLiteral("planar speed changed %1 m/s^2 at %2 s via %3 "
                                 "(allowed %4 m/s^2)")
                      .arg(change, 0, 'f', 2)
                      .arg(frame->time_seconds, 0, 'f', 2)
                      .arg(QString::fromLatin1(App::Core::displacement_source_name(
                          motor.displacement_source)))
                      .arg(allowed, 0, 'f', 2));
              break;
            }
          }
          previous_speed = speed;
          have_previous = true;
        }
        break;
      }
      case ArenaExpectationKind::CommanderInputEdgesAllConsumed: {
        auto const frames = commander_frames();
        if (frames.empty()) {
          add_issue(QStringLiteral("commander_input_not_traced"),
                    QStringLiteral("no commander presentation trace was recorded, so "
                                   "input edges cannot be accounted for"));
          break;
        }
        auto const& last = frames.back()->commander.input;
        if (last.primary_press_sequence !=
            last.primary_consumed_sequence + last.primary_dropped_sequence) {
          add_issue(
              QStringLiteral("commander_attack_edge_unaccounted"),
              QStringLiteral("%1 attack presses produced %2 consumed and %3 dropped; "
                             "every edge must land in exactly one of the two")
                  .arg(last.primary_press_sequence)
                  .arg(last.primary_consumed_sequence)
                  .arg(last.primary_dropped_sequence));
        }
        auto const allowed_drops =
            static_cast<std::uint64_t>(std::max(0.0F, expectation.threshold));
        if (last.primary_dropped_sequence > allowed_drops) {
          add_issue(QStringLiteral("commander_attack_edge_dropped"),
                    QStringLiteral("%1 attack presses were dropped without reaching "
                                   "the simulation (allowed %2)")
                        .arg(last.primary_dropped_sequence)
                        .arg(allowed_drops));
        }
        if (last.dodge_request_sequence !=
            last.dodge_consumed_sequence + last.dodge_refused_sequence) {
          add_issue(
              QStringLiteral("commander_dodge_edge_unaccounted"),
              QStringLiteral("%1 dodge requests produced %2 consumed and %3 refused")
                  .arg(last.dodge_request_sequence)
                  .arg(last.dodge_consumed_sequence)
                  .arg(last.dodge_refused_sequence));
        }
        break;
      }
      case ArenaExpectationKind::CommanderCombatCounterWithin: {
        auto const frames = commander_frames();
        if (frames.empty()) {
          add_issue(QStringLiteral("commander_combat_not_traced"),
                    QStringLiteral("no commander presentation trace was recorded, so "
                                   "%1 cannot be counted")
                        .arg(expectation.counter_key));
          break;
        }
        auto const counter_of =
            [&expectation](
                const App::Core::CommanderCombatTrace& combat) -> std::uint32_t {
          auto const& key = expectation.counter_key;
          if (key == QLatin1String("accepted")) {
            return combat.queue_accepted;
          }
          if (key == QLatin1String("buffered")) {
            return combat.queue_buffered;
          }
          if (key == QLatin1String("refused")) {
            return combat.queue_refused;
          }
          if (key == QLatin1String("expired")) {
            return combat.queue_expired;
          }
          if (key == QLatin1String("overflow")) {
            return combat.queue_overflow;
          }
          if (key == QLatin1String("block")) {
            return combat.blocked_contacts;
          }
          if (key == QLatin1String("perfect_guard")) {
            return combat.perfect_guard_contacts;
          }
          if (key == QLatin1String("dodge")) {
            return combat.dodged_contacts;
          }
          if (key == QLatin1String("damage")) {
            return combat.damaging_contacts;
          }
          if (key == QLatin1String("guard_break")) {
            return combat.guard_broken_contacts;
          }
          return 0U;
        };

        float const window_start = expectation.start_seconds;
        float const window_end = expectation.end_seconds > 0.0F
                                     ? expectation.end_seconds
                                     : std::numeric_limits<float>::max();
        std::optional<std::uint32_t> first;
        std::uint32_t last_value = 0U;
        bool have_last = false;
        for (auto const* frame : frames) {
          if (frame->time_seconds < window_start) {
            first = counter_of(frame->commander.combat);
            continue;
          }
          if (frame->time_seconds > window_end) {
            break;
          }
          if (!first.has_value()) {
            first = counter_of(frame->commander.combat);
          }
          last_value = counter_of(frame->commander.combat);
          have_last = true;
        }
        if (!have_last) {
          add_issue(QStringLiteral("commander_combat_counter_window_empty"),
                    QStringLiteral("no traced frame fell inside %1 s - %2 s for %3")
                        .arg(window_start, 0, 'f', 2)
                        .arg(expectation.end_seconds, 0, 'f', 2)
                        .arg(expectation.counter_key));
          break;
        }
        auto const observed = last_value - first.value_or(0U);
        auto const minimum =
            static_cast<std::uint32_t>(std::max(0.0F, expectation.threshold));
        if (observed < minimum) {
          add_issue(QStringLiteral("commander_combat_counter_too_low"),
                    QStringLiteral("%1 was counted %2 times between %3 s and %4 s but "
                                   "at least %5 were required")
                        .arg(expectation.counter_key)
                        .arg(observed)
                        .arg(window_start, 0, 'f', 2)
                        .arg(window_end, 0, 'f', 2)
                        .arg(minimum));
          break;
        }
        if (expectation.maximum >= 0.0F) {
          auto const maximum = static_cast<std::uint32_t>(expectation.maximum);
          if (observed > maximum) {
            add_issue(QStringLiteral("commander_combat_counter_too_high"),
                      QStringLiteral("%1 was counted %2 times between %3 s and %4 s "
                                     "but at most %5 are allowed")
                          .arg(expectation.counter_key)
                          .arg(observed)
                          .arg(window_start, 0, 'f', 2)
                          .arg(window_end, 0, 'f', 2)
                          .arg(maximum));
          }
        }
        break;
      }
      case ArenaExpectationKind::CommanderLockStateWithin: {
        auto const frames = commander_frames();
        if (frames.empty()) {
          add_issue(QStringLiteral("commander_lock_not_traced"),
                    QStringLiteral("no commander presentation trace was recorded, so "
                                   "the lock state cannot be read"));
          break;
        }
        float const window_start = expectation.start_seconds;
        float const window_end = expectation.end_seconds > 0.0F
                                     ? expectation.end_seconds
                                     : std::numeric_limits<float>::max();
        std::vector<std::uint64_t> locks;
        for (auto const* frame : frames) {
          if (frame->time_seconds < window_start || frame->time_seconds > window_end) {
            continue;
          }
          locks.push_back(frame->commander.combat.locked_target_id);
        }
        if (locks.empty()) {
          add_issue(QStringLiteral("commander_lock_window_empty"),
                    QStringLiteral("no traced frame fell inside %1 s - %2 s")
                        .arg(window_start, 0, 'f', 2)
                        .arg(expectation.end_seconds, 0, 'f', 2));
          break;
        }
        auto const& key = expectation.counter_key;
        if (key == QLatin1String("held")) {
          auto const lost = std::find(locks.begin(), locks.end(), 0U);
          if (lost != locks.end()) {
            add_issue(QStringLiteral("commander_lock_not_held"),
                      QStringLiteral("the lock was empty inside %1 s - %2 s")
                          .arg(window_start, 0, 'f', 2)
                          .arg(window_end, 0, 'f', 2));
          }
        } else if (key == QLatin1String("cleared")) {
          auto const still_locked = std::find_if(
              locks.begin(), locks.end(), [](std::uint64_t id) { return id != 0U; });
          if (still_locked != locks.end()) {
            add_issue(QStringLiteral("commander_lock_not_cleared"),
                      QStringLiteral("entity %1 was still locked inside %2 s - %3 s")
                          .arg(*still_locked)
                          .arg(window_start, 0, 'f', 2)
                          .arg(window_end, 0, 'f', 2));
          }
        } else if (key == QLatin1String("changed")) {
          if (locks.front() == locks.back()) {
            add_issue(QStringLiteral("commander_lock_did_not_change"),
                      QStringLiteral("the lock stayed on entity %1 across %2 s - %3 s")
                          .arg(locks.front())
                          .arg(window_start, 0, 'f', 2)
                          .arg(window_end, 0, 'f', 2));
          }
        }
        break;
      }
      case ArenaExpectationKind::CommanderContactCountAtMost: {
        int const allowed_floor =
            std::max(1,
                     static_cast<int>(std::lround(
                         expectation.threshold > 0.0F ? expectation.threshold : 1.0F)));
        for (auto const* frame : commander_frames()) {
          auto const& combat = frame->commander.combat;
          int allowed = allowed_floor;
          for (auto const& unit : frame->units) {
            if (unit.group != expectation.group) {
              continue;
            }
            auto const* definition =
                Game::Systems::CombatActions::find_combat_action_definition(
                    static_cast<Game::Systems::CombatActions::CombatActionId>(
                        unit.combat_action_id));
            if (definition != nullptr) {
              allowed = std::max(allowed, definition->max_targets);
            }
            break;
          }
          if (combat.action_running && combat.action_hit_count > allowed) {
            add_issue(QStringLiteral("commander_contact_multiplicity"),
                      QStringLiteral("one action landed %1 contacts by %2 s, but at "
                                     "most %3 is authored")
                          .arg(combat.action_hit_count)
                          .arg(frame->time_seconds, 0, 'f', 2)
                          .arg(allowed));
            break;
          }
        }
        break;
      }
      case ArenaExpectationKind::CommanderActionObserved: {
        auto const frames = commander_frames();
        if (frames.empty()) {
          add_issue(QStringLiteral("commander_action_not_traced"),
                    QStringLiteral("no commander presentation trace was recorded, so "
                                   "action %1 cannot be verified")
                        .arg(expectation.combat_action_id));
          break;
        }
        float const window_start = expectation.start_seconds;
        float const window_end = expectation.end_seconds > 0.0F
                                     ? expectation.end_seconds
                                     : std::numeric_limits<float>::max();
        bool sampled_window = false;
        bool observed = false;
        for (auto const* frame : frames) {
          if (frame->time_seconds < window_start || frame->time_seconds > window_end) {
            continue;
          }
          sampled_window = true;
          if (frame->commander.combat.action_running &&
              frame->commander.combat.action_id == expectation.combat_action_id) {
            observed = true;
            break;
          }
        }
        if (!sampled_window) {
          add_issue(QStringLiteral("commander_action_window_empty"),
                    QStringLiteral("no traced frame fell inside %1 s - %2 s for "
                                   "action %3")
                        .arg(window_start, 0, 'f', 2)
                        .arg(expectation.end_seconds, 0, 'f', 2)
                        .arg(expectation.combat_action_id));
        } else if (!observed) {
          add_issue(QStringLiteral("commander_action_not_observed"),
                    QStringLiteral("%1 never ran authored action %2 between %3 s and "
                                   "%4 s")
                        .arg(expectation.group)
                        .arg(expectation.combat_action_id)
                        .arg(window_start, 0, 'f', 2)
                        .arg(expectation.end_seconds, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::GroupIsRendered:
        if (rendered_by_group.value(expectation.group, 0U) == 0U) {
          add_issue(QStringLiteral("group_not_rendered"),
                    QStringLiteral("%1 produced no rendered soldier observations")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupExists:
        if (ids(expectation.group).empty() || group_destroyed(expectation.group)) {
          add_issue(QStringLiteral("group_missing"),
                    QStringLiteral("%1 did not retain a live scenario entity")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupDestroyed:
        if (!group_destroyed(expectation.group)) {
          add_issue(QStringLiteral("group_not_destroyed"),
                    QStringLiteral("%1 retained a living scenario entity")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupReachedDestination: {
        QVector3D centroid;
        int living = 0;
        for (auto entity_id : ids(expectation.group)) {
          auto const* transform =
              world.try_get<Engine::Core::TransformComponent>(entity_id);
          if (transform != nullptr && entity_alive(entity_id)) {
            centroid += vector_from_transform(*transform);
            ++living;
          }
        }
        float const tolerance =
            expectation.distance > 0.0F ? expectation.distance : 2.5F;
        QVector3D const destination = world_origin + expectation.position;
        if (living == 0 ||
            horizontal_distance(centroid / static_cast<float>(std::max(living, 1)),
                                destination) > tolerance) {
          add_issue(QStringLiteral("group_missed_destination"),
                    QStringLiteral("%1 did not reach its scenario destination")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::NoPermanentStall: {
        float const budget =
            expectation.threshold > 0.0F ? expectation.threshold : 12.0F;
        QStringList wedged;
        for (auto entity_id : ids(expectation.group)) {
          if (!entity_alive(entity_id)) {
            continue;
          }
          auto const* movement =
              world.try_get<Engine::Core::MovementComponent>(entity_id);
          auto const* movement_facts =
              world.try_get<Engine::Core::MovementFactsComponent>(entity_id);
          if (movement == nullptr || movement_facts == nullptr) {
            continue;
          }
          auto const& stall = movement_facts->progress.stall;
          float const going_nowhere =
              std::max(stall.stalled_seconds, stall.no_closer_seconds);
          if (movement->get_has_target() && going_nowhere > budget) {
            wedged.push_back(
                QStringLiteral("%1 (%2 s in %3)")
                    .arg(entity_id)
                    .arg(going_nowhere, 0, 'f', 1)
                    .arg(QString::fromLatin1(Engine::Core::movement_state_name(
                        movement_facts->progress.state))));
          }
        }
        auto const observation = stall_observations.value(expectation.group);
        if (!wedged.isEmpty()) {
          add_issue(QStringLiteral("permanent_stall"),
                    QStringLiteral("%1 ended the run holding an objective it had "
                                   "stopped advancing on: %2")
                        .arg(expectation.group, wedged.join(QStringLiteral(", "))));
        } else if (observation.worst_stalled_seconds > budget &&
                   observation.recovery_attempts == 0U && observation.abandons == 0U) {

          add_issue(QStringLiteral("unnoticed_stall"),
                    QStringLiteral("%1 went nowhere for %2 s at %3 s (%4) without "
                                   "the recovery ladder ever engaging")
                        .arg(expectation.group)
                        .arg(observation.worst_stalled_seconds, 0, 'f', 1)
                        .arg(observation.worst_stalled_at, 0, 'f', 1)
                        .arg(observation.worst_state.isEmpty()
                                 ? QStringLiteral("unknown state")
                                 : observation.worst_state));
        }
        break;
      }
      case ArenaExpectationKind::StallRecoveryObserved: {
        auto const observation = stall_observations.value(expectation.group);
        if (!observation.recovery_seen && observation.abandons == 0U) {
          add_issue(QStringLiteral("stall_recovery_not_exercised"),
                    QStringLiteral("%1 never reached the recovery ladder, so this "
                                   "scenario proved nothing about it")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::GroupHeldOutsideDestination: {
        QVector3D centroid;
        int living = 0;
        for (auto entity_id : ids(expectation.group)) {
          auto const* transform =
              world.try_get<Engine::Core::TransformComponent>(entity_id);
          if (transform != nullptr && entity_alive(entity_id)) {
            centroid += vector_from_transform(*transform);
            ++living;
          }
        }
        float const tolerance =
            expectation.distance > 0.0F ? expectation.distance : 2.5F;
        QVector3D const destination = world_origin + expectation.position;
        if (living > 0 && horizontal_distance(centroid / static_cast<float>(living),
                                              destination) <= tolerance) {
          add_issue(QStringLiteral("group_passed_barrier"),
                    QStringLiteral("%1 reached a destination it should have been "
                                   "held out of")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::GateOpenedObserved:
        if (!gate_opened_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("gate_never_opened"),
                    QStringLiteral("%1 never opened far enough to walk through")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GateRemainedClosed:
        if (!gate_seen.value(expectation.group, false)) {
          add_issue(
              QStringLiteral("gate_not_observed"),
              QStringLiteral("%1 was never sampled as a gate").arg(expectation.group));
        } else if (gate_opened_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("gate_opened_for_enemy"),
                    QStringLiteral("%1 opened while only hostile units were near it")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::BridgeTraversalObserved:
        if (!bridge_traversal_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("bridge_not_traversed"),
                    QStringLiteral("%1 never occupied the bridge deck")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::BridgeCenterlineAligned: {
        auto const observation = bridge_alignment.value(expectation.group);
        float const tolerance =
            expectation.distance > 0.0F ? expectation.distance : 0.50F;
        if (!observation.sampled || observation.lateral_offset > tolerance) {
          add_issue(
              QStringLiteral("bridge_centerline_missed"),
              observation.sampled
                  ? QStringLiteral("%1 crossed %2 m from the bridge centerline at "
                                   "midspan (allowed %3 m)")
                        .arg(expectation.group)
                        .arg(observation.lateral_offset, 0, 'f', 2)
                        .arg(tolerance, 0, 'f', 2)
                  : QStringLiteral("%1 produced no bridge-midspan alignment sample")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::DefensiveUnitLayoutLocked: {
        if (!defensive_layout_locked.value(expectation.group, false)) {
          add_issue(QStringLiteral("defensive_unit_layout_never_locked"),
                    QStringLiteral("%1 never locked its internal defensive unit "
                                   "layout")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::ElevationGainObserved: {
        float const required_gain =
            expectation.threshold > 0.0F ? expectation.threshold : 1.0F;
        float const gain = maximum_elevation.value(expectation.group) -
                           initial_elevation.value(expectation.group);
        if (gain < required_gain) {
          add_issue(QStringLiteral("elevation_gain_not_observed"),
                    QStringLiteral("%1 climbed only %2 m (required %3 m)")
                        .arg(expectation.group)
                        .arg(gain, 0, 'f', 2)
                        .arg(required_gain, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::ElevationClimbIsMonotonic:
      case ArenaExpectationKind::ElevationDescentIsMonotonic: {
        bool const climbing =
            expectation.kind == ArenaExpectationKind::ElevationClimbIsMonotonic;
        auto const& legs = climbing ? elevation_climb_legs : elevation_descent_legs;
        auto const leg = legs.value(expectation.group);
        float const tolerance =
            expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
        if (!leg.seeded) {
          add_issue(
              QStringLiteral("elevation_leg_not_sampled"),
              QStringLiteral("%1 produced no elevation sample for its %2 leg")
                  .arg(expectation.group)
                  .arg(climbing ? QStringLiteral("climb") : QStringLiteral("descent")));
          break;
        }
        if (leg.worst_reversal > tolerance) {
          add_issue(
              QStringLiteral("elevation_leg_reversed"),
              QStringLiteral("%1 reversed %2 m against its %3 at %4 s (allowed %5 m)")
                  .arg(expectation.group)
                  .arg(leg.worst_reversal, 0, 'f', 2)
                  .arg(climbing ? QStringLiteral("climb") : QStringLiteral("descent"))
                  .arg(leg.worst_at, 0, 'f', 2)
                  .arg(tolerance, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::ElevationHeldAbove: {
        auto const floor = elevation_floors.value(expectation.group);
        if (!floor.seeded) {
          add_issue(QStringLiteral("elevation_floor_not_sampled"),
                    QStringLiteral("%1 produced no elevation sample while it was "
                                   "meant to be holding the high ground")
                        .arg(expectation.group));
          break;
        }
        if (floor.lowest < expectation.threshold) {
          add_issue(QStringLiteral("elevation_floor_broken"),
                    QStringLiteral("%1 dropped to %2 m at %3 s near (%4, %5); it "
                                   "was meant to stay above %6 m")
                        .arg(expectation.group)
                        .arg(floor.lowest, 0, 'f', 2)
                        .arg(floor.lowest_at, 0, 'f', 2)
                        .arg(floor.lowest_where.x(), 0, 'f', 2)
                        .arg(floor.lowest_where.z(), 0, 'f', 2)
                        .arg(expectation.threshold, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::NarrowLayoutEngaged: {
        auto const state = narrow_layout.value(expectation.group);
        if (!state.engaged) {
          add_issue(QStringLiteral("narrow_layout_never_engaged"),
                    QStringLiteral("%1 crossed with %2 m of corridor against a "
                                   "%3 m frontage and never took narrow order")
                        .arg(expectation.group)
                        .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                        .arg(state.formation_half_width * 2.0F, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::NarrowLayoutStaysWide: {
        auto const state = narrow_layout.value(expectation.group);
        if (state.engaged) {
          add_issue(QStringLiteral("narrow_layout_false_positive"),
                    QStringLiteral("%1 took narrow order at %2 s with %3 m of "
                                   "corridor for a %4 m frontage; it went to %5 "
                                   "files and %6 m between files")
                        .arg(expectation.group)
                        .arg(state.engaged_from, 0, 'f', 2)
                        .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                        .arg(state.formation_half_width * 2.0F, 0, 'f', 2)
                        .arg(state.narrowest_files)
                        .arg(state.tightest_file_spacing, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::NarrowLayoutKeepsFiles: {
        auto const state = narrow_layout.value(expectation.group);
        auto const floor_files =
            static_cast<std::uint32_t>(std::max(1.0F, expectation.threshold));
        if (state.engaged && state.narrowest_files < floor_files) {
          add_issue(
              QStringLiteral("narrow_layout_over_collapsed"),
              QStringLiteral("%1 fell to %2 files (%3) in a %4 m corridor where "
                             "%5 files fit; frontage %6 m, files %7 m apart, "
                             "column %8 m deep")
                  .arg(expectation.group)
                  .arg(state.narrowest_files)
                  .arg(QString::fromLatin1(
                      Engine::Core::traversal_layout_mode_name(state.narrowest_mode)))
                  .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                  .arg(floor_files)
                  .arg(state.narrowest_frontage, 0, 'f', 2)
                  .arg(state.tightest_file_spacing, 0, 'f', 2)
                  .arg(state.deepest_column, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::NarrowLayoutModeSettles: {
        auto const state = narrow_layout.value(expectation.group);
        auto const allowance =
            static_cast<std::uint32_t>(std::max(1.0F, expectation.threshold));
        if (state.mode_changes > allowance) {
          add_issue(QStringLiteral("narrow_layout_oscillated"),
                    QStringLiteral("%1 changed layout mode %2 times (allowed %3) "
                                   "between %4 m of corridor and a %5 m frontage")
                        .arg(expectation.group)
                        .arg(state.mode_changes)
                        .arg(allowance)
                        .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2)
                        .arg(state.formation_half_width * 2.0F, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::NarrowLayoutRestores: {
        auto const state = narrow_layout.value(expectation.group);
        float const tolerance =
            expectation.threshold > 0.0F ? expectation.threshold : 0.6F;
        if (state.active_at_end) {
          add_issue(QStringLiteral("narrow_layout_never_released"),
                    QStringLiteral("%1 was still in narrow order at the end with "
                                   "%2 m of corridor")
                        .arg(expectation.group)
                        .arg(state.narrowest_corridor * 2.0F, 0, 'f', 2));
        } else if (state.files_at_end < state.normal_files) {
          add_issue(QStringLiteral("narrow_layout_kept_files"),
                    QStringLiteral("%1 finished on %2 of its %3 files")
                        .arg(expectation.group)
                        .arg(state.files_at_end)
                        .arg(state.normal_files));
        }
        if (state.worst_reform_error > tolerance) {
          add_issue(QStringLiteral("narrow_layout_left_disorder"),
                    QStringLiteral("%1 left a soldier %2 m from its slot after "
                                   "the passage (allowed %3 m)")
                        .arg(expectation.group)
                        .arg(state.worst_reform_error, 0, 'f', 2)
                        .arg(tolerance, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::SoldiersStayOnWalkableGround: {
        auto const state = off_walkable_soldiers.value(expectation.group);
        int const allowance = static_cast<int>(std::max(0.0F, expectation.threshold));
        if (state.samples > allowance) {
          add_issue(QStringLiteral("soldier_stood_on_blocked_ground"),
                    QStringLiteral("%1 drew a soldier on blocked ground %2 times "
                                   "(allowed %3), first at %4 s near (%5, %6)")
                        .arg(expectation.group)
                        .arg(state.samples)
                        .arg(allowance)
                        .arg(state.worst_at, 0, 'f', 2)
                        .arg(state.worst.x(), 0, 'f', 2)
                        .arg(state.worst.z(), 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::UnitsStayOnWalkableGround: {
        auto const state = off_walkable_ground.value(expectation.group);
        if (state.samples > 0) {
          add_issue(QStringLiteral("unit_stood_on_blocked_ground"),
                    QStringLiteral("%1 stood on blocked ground %2 times, first at "
                                   "%3 s near (%4, %5)")
                        .arg(expectation.group)
                        .arg(state.samples)
                        .arg(state.worst_at, 0, 'f', 2)
                        .arg(state.worst.x(), 0, 'f', 2)
                        .arg(state.worst.z(), 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::OwnerCompletesConstruction: {
        int owner_id = 0;
        if (!ids(expectation.group).empty()) {
          auto* anchor = world.get_entity(ids(expectation.group).front());
          auto const* unit = anchor != nullptr
                                 ? anchor->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
          owner_id = unit != nullptr ? unit->owner_id : 0;
        }
        const int required = std::max(1, static_cast<int>(expectation.threshold));
        if (owner_id <= 0 ||
            completed_construction_by_owner.value(owner_id) < required) {
          add_issue(QStringLiteral("economy_no_construction"),
                    QStringLiteral("%1 completed %2 of %3 required AI constructions")
                        .arg(expectation.group)
                        .arg(completed_construction_by_owner.value(owner_id))
                        .arg(required));
        }
        break;
      }
      case ArenaExpectationKind::OwnerHarvestsResource: {
        int owner_id = 0;
        if (!ids(expectation.group).empty()) {
          auto* anchor = world.get_entity(ids(expectation.group).front());
          auto const* unit = anchor != nullptr
                                 ? anchor->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
          owner_id = unit != nullptr ? unit->owner_id : 0;
        }
        const int required = std::max(1, static_cast<int>(expectation.threshold));
        if (owner_id <= 0 || completed_harvest_by_owner.value(owner_id) < required) {
          add_issue(QStringLiteral("economy_no_harvest"),
                    QStringLiteral("%1 completed %2 of %3 required resource harvests")
                        .arg(expectation.group)
                        .arg(completed_harvest_by_owner.value(owner_id))
                        .arg(required));
        }
        break;
      }
      case ArenaExpectationKind::FrameBudget: {
        std::vector<double> samples;
        samples.reserve(trace.size());
        std::uint64_t peak_visible_soldiers = 0U;
        std::uint64_t peak_draw_commands = 0U;
        std::uint64_t peak_rigged_commands = 0U;
        std::uint64_t peak_rigged_instanced_instances = 0U;
        std::uint64_t peak_rigged_single_draws = 0U;
        std::uint64_t peak_shadow_rigged_instanced_instances = 0U;
        std::uint64_t peak_shadow_rigged_single_draws = 0U;
        std::uint64_t prewarm_frames = 0U;
        double prewarm_max_ms = 0.0;
        std::uint64_t gpu_timed_frames = 0U;
        for (auto const& frame : trace) {
          bool const after_start =
              frame.time_seconds + 1.0e-5F >= expectation.start_seconds;
          bool const before_end =
              expectation.end_seconds <= 0.0F ||
              frame.time_seconds <= expectation.end_seconds + 1.0e-5F;
          if (frame.time_seconds < k_arena_prewarm_seconds) {

            ++prewarm_frames;
            prewarm_max_ms = std::max(prewarm_max_ms, frame.frame_time_ms);
            continue;
          }
          if (frame.timings.gpu_color_ms > 0.0 || frame.timings.gpu_shadow_ms > 0.0) {
            ++gpu_timed_frames;
          }
          if (after_start && before_end) {
            samples.push_back(frame.frame_time_ms);
            peak_visible_soldiers =
                std::max(peak_visible_soldiers, frame.timings.visible_soldiers);
            peak_draw_commands = std::max(peak_draw_commands, frame.timings.draw_calls);
            peak_rigged_commands =
                std::max(peak_rigged_commands, frame.timings.rigged_commands);
            peak_rigged_instanced_instances =
                std::max(peak_rigged_instanced_instances,
                         frame.timings.rigged_instanced_instances);
            peak_rigged_single_draws =
                std::max(peak_rigged_single_draws, frame.timings.rigged_single_draws);
            peak_shadow_rigged_instanced_instances =
                std::max(peak_shadow_rigged_instanced_instances,
                         frame.timings.shadow_rigged_instanced_instances);
            peak_shadow_rigged_single_draws =
                std::max(peak_shadow_rigged_single_draws,
                         frame.timings.shadow_rigged_single_draws);
          }
        }
        if (samples.empty()) {
          break;
        }
        std::sort(samples.begin(), samples.end());
        double const budget = expectation.threshold > 0.0F ? expectation.threshold
                                                           : k_default_frame_budget_ms;
        std::size_t const p95_index = std::min<std::size_t>(
            samples.size() - 1U, ((samples.size() * 95U) + 99U) / 100U - 1U);
        std::size_t const p50_index = std::min<std::size_t>(
            samples.size() - 1U, ((samples.size() * 50U) + 99U) / 100U - 1U);
        std::size_t const p99_index = std::min<std::size_t>(
            samples.size() - 1U, ((samples.size() * 99U) + 99U) / 100U - 1U);
        double const p50 = samples[p50_index];
        double const p95 = samples[p95_index];
        double const p99 = samples[p99_index];
        double const maximum = samples.back();

        report.frame_time_samples = samples.size();
        report.frame_budget_ms = budget;
        report.frame_time_p50_ms = p50;
        report.frame_time_p95_ms = p95;
        report.frame_time_p99_ms = p99;
        report.frame_time_max_ms = maximum;
        report.prewarm_frames = prewarm_frames;
        report.prewarm_max_ms = prewarm_max_ms;
        report.prewarm_seconds = k_arena_prewarm_seconds;
        report.gpu_timed_frames = gpu_timed_frames;

        auto percentile_of = [this](auto&& pick) -> double {
          std::vector<double> values;
          values.reserve(trace.size());
          for (auto const& frame : this->trace) {
            if (frame.time_seconds < k_arena_prewarm_seconds ||
                !frame.commander.valid) {
              continue;
            }
            values.push_back(static_cast<double>(pick(frame.commander.costs)));
          }
          if (values.empty()) {
            return 0.0;
          }
          std::sort(values.begin(), values.end());
          std::size_t const index = std::min<std::size_t>(
              values.size() - 1U, ((values.size() * 95U) + 99U) / 100U - 1U);
          return values[index];
        };
        report.rpg_cost_p95_motor_ms =
            percentile_of([](auto const& costs) { return costs.motor_ms; });
        report.rpg_cost_p95_targeting_ms =
            percentile_of([](auto const& costs) { return costs.targeting_ms; });
        report.rpg_cost_p95_weapon_trace_ms =
            percentile_of([](auto const& costs) { return costs.weapon_trace_ms; });
        report.rpg_cost_p95_engagement_ms =
            percentile_of([](auto const& costs) { return costs.engagement_ms; });
        report.rpg_cost_p95_camera_ms =
            percentile_of([](auto const& costs) { return costs.camera_ms; });
        report.rpg_cost_p95_total_ms =
            percentile_of([](auto const& costs) { return costs.total_ms(); });

        std::vector<double> simulation_samples;
        simulation_samples.reserve(trace.size());
        for (auto const& frame : trace) {
          if (frame.time_seconds < k_arena_prewarm_seconds) {
            continue;
          }
          simulation_samples.push_back(frame.timings.simulation_ms);
        }
        if (!simulation_samples.empty()) {
          std::sort(simulation_samples.begin(), simulation_samples.end());
          std::size_t const simulation_index = std::min<std::size_t>(
              simulation_samples.size() - 1U,
              ((simulation_samples.size() * 95U) + 99U) / 100U - 1U);
          report.simulation_p95_ms = simulation_samples[simulation_index];
        }
        report.peak_visible_soldiers = peak_visible_soldiers;
        report.peak_draw_commands = peak_draw_commands;
        report.peak_rigged_commands = peak_rigged_commands;
        report.peak_rigged_instanced_instances = peak_rigged_instanced_instances;
        report.peak_rigged_single_draws = peak_rigged_single_draws;
        report.peak_shadow_rigged_instanced_instances =
            peak_shadow_rigged_instanced_instances;
        report.peak_shadow_rigged_single_draws = peak_shadow_rigged_single_draws;

        bool const contains_troop_groups = std::any_of(
            scenario.groups.begin(), scenario.groups.end(), [](auto const& group) {
              return !group.spawn_type.has_value();
            });
        if (contains_troop_groups && peak_visible_soldiers == 0U &&
            peak_rigged_commands == 0U) {
          add_issue(
              QStringLiteral("performance_scene_rendered_no_creatures"),
              QStringLiteral("performance sampling observed no visible creatures"));
        }
        if (scenario.require_rigged_instancing &&
            peak_rigged_instanced_instances == 0U) {
          add_issue(QStringLiteral("performance_rigged_instancing_unused"),
                    QStringLiteral("performance sampling observed no instanced "
                                   "rigged creature playback"));
        }

        if (p95 > budget || maximum > budget * 10.0) {
          add_issue(
              QStringLiteral("frame_budget_exceeded"),
              QStringLiteral("render frame p95/p99/max was %1/%2/%3 ms (budget %4 ms)")
                  .arg(p95, 0, 'f', 2)
                  .arg(p99, 0, 'f', 2)
                  .arg(maximum, 0, 'f', 2)
                  .arg(budget, 0, 'f', 2));
        }
        if (gpu_timed_frames == 0U) {
          add_issue(QStringLiteral("performance_gpu_timing_missing"),
                    QStringLiteral("no post-prewarm frame reported a GPU timing, so "
                                   "the frame budget cannot be attributed"));
        }
        break;
      }
      case ArenaExpectationKind::CommanderAuraActivated:
        if (!commander_aura_active_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_not_activated"),
                    QStringLiteral("%1 never entered its timed command aura state")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::CommanderAuraBuffObserved:
        if (!commander_aura_buff_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_buff_not_observed"),
                    QStringLiteral("%1 never received the nearby commander bonus")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::CommanderAuraExpired:
        if (!commander_aura_expired_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_not_expired"),
                    QStringLiteral("%1 command aura did not expire into cooldown")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoCommanderAuraBuffObserved:
        if (commander_aura_buff_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_leaked_outside_radius"),
                    QStringLiteral("%1 received a commander bonus outside the aura")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::ExactRpgTargetObserved:
        if (!exact_rpg_target_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("exact_rpg_target_not_observed"),
                    QStringLiteral("%1 never selected an exact in-range soldier")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgDamageContactObserved:
        if (!rpg_damage_contact_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_damage_contact_not_observed"),
                    QStringLiteral("%1 never published a visible RPG damage contact")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgBlockContactObserved:
        if (!rpg_block_contact_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_block_contact_not_observed"),
                    QStringLiteral("%1 never published a visible block contact")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgDodgeContactObserved:
        if (!rpg_dodge_contact_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_dodge_contact_not_observed"),
                    QStringLiteral("%1 never published a visible dodge contact")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgDodgeWindowObserved:
        if (!rpg_dodge_window_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_dodge_window_not_observed"),
                    QStringLiteral("%1 never entered its RPG dodge protection window")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgHealthReduced:
        if (!initial_rpg_health_by_group.contains(expectation.group) ||
            minimum_rpg_health_by_group.value(expectation.group) >=
                initial_rpg_health_by_group.value(expectation.group)) {
          add_issue(
              QStringLiteral("rpg_health_not_reduced"),
              QStringLiteral("%1 took no RPG health damage").arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgHealthUnchanged:
        if (!initial_rpg_health_by_group.contains(expectation.group) ||
            minimum_rpg_health_by_group.value(expectation.group) !=
                initial_rpg_health_by_group.value(expectation.group)) {
          add_issue(QStringLiteral("rpg_health_changed"),
                    QStringLiteral("%1 lost RPG health during a protected scenario")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgWalkObserved:
        if (!rpg_walk_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_walk_not_observed"),
                    QStringLiteral("%1 never entered a walking locomotion state")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgRunObserved:
        if (!rpg_run_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("rpg_run_not_observed"),
                    QStringLiteral("%1 never entered a running locomotion state")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RpgLocomotionAnimationMatched:
        if (auto const mismatch = rpg_locomotion_mismatch.constFind(expectation.group);
            mismatch != rpg_locomotion_mismatch.cend()) {
          add_issue(QStringLiteral("rpg_locomotion_desynchronized"),
                    QStringLiteral("%1 %2").arg(expectation.group, mismatch.value()));
        }
        break;
      case ArenaExpectationKind::RpgStrikeAnimationMatched:
        if (auto const mismatch = rpg_strike_mismatch.constFind(expectation.group);
            mismatch != rpg_strike_mismatch.cend()) {
          add_issue(QStringLiteral("rpg_strike_desynchronized"),
                    QStringLiteral("%1 %2").arg(expectation.group, mismatch.value()));
        }
        break;
      case ArenaExpectationKind::RpgSwingCadenceWithin: {
        auto const& starts = rpg_swing_starts[expectation.group];
        int const required_swings =
            std::max(2, static_cast<int>(std::lround(expectation.distance)));
        float const allowed_gap =
            expectation.threshold > 0.0F ? expectation.threshold : 1.0F;
        if (static_cast<int>(starts.size()) < required_swings) {
          add_issue(QStringLiteral("rpg_swing_cadence_too_few"),
                    QStringLiteral("%1 started %2 swings while holding the attack "
                                   "input but needed %3")
                        .arg(expectation.group)
                        .arg(starts.size())
                        .arg(required_swings));
          break;
        }
        for (std::size_t i = 1; i < starts.size(); ++i) {
          float const gap = starts[i] - starts[i - 1U];
          if (gap > allowed_gap) {
            add_issue(QStringLiteral("rpg_swing_cadence_too_slow"),
                      QStringLiteral("%1 waited %2 s between swings at %3 s but the "
                                     "held attack has to chain within %4 s")
                          .arg(expectation.group)
                          .arg(gap, 0, 'f', 2)
                          .arg(starts[i], 0, 'f', 2)
                          .arg(allowed_gap, 0, 'f', 2));
            break;
          }
        }
        break;
      }
      case ArenaExpectationKind::RpgSwingCarriesBody: {
        auto const& carries = rpg_swing_carry[expectation.group];
        int const required_swings =
            std::max(1, static_cast<int>(std::lround(expectation.distance)));
        float const required_carry = std::max(expectation.threshold, 0.0F);
        if (static_cast<int>(carries.size()) < required_swings) {
          add_issue(QStringLiteral("rpg_swing_carry_too_few"),
                    QStringLiteral("%1 swung %2 times but needed %3 to judge the "
                                   "body carry")
                        .arg(expectation.group)
                        .arg(carries.size())
                        .arg(required_swings));
          break;
        }
        for (std::size_t i = 0; i < carries.size(); ++i) {
          if (carries[i] + 1.0e-3F < required_carry) {
            add_issue(QStringLiteral("rpg_swing_planted"),
                      QStringLiteral("%1 swing %2 carried the body %3 m but a strike "
                                     "has to drive it at least %4 m")
                          .arg(expectation.group)
                          .arg(i + 1U)
                          .arg(carries[i], 0, 'f', 2)
                          .arg(required_carry, 0, 'f', 2));
            break;
          }
        }
        break;
      }
      case ArenaExpectationKind::RpgTravelObserved: {
        auto const observation = rpg_travel_observations.value(travel_key(expectation));
        float const required = std::max(expectation.threshold, 0.0F);
        if (!observation.has_start || !observation.has_end) {
          add_issue(QStringLiteral("rpg_travel_not_sampled"),
                    QStringLiteral("%1 was never sampled across its travel window")
                        .arg(expectation.group));
          break;
        }
        float const travelled = horizontal_distance(observation.start, observation.end);
        if (travelled < required) {
          add_issue(QStringLiteral("rpg_travel_blocked"),
                    QStringLiteral("%1 travelled %2 m between %3 s and %4 s but had "
                                   "to cover %5 m")
                        .arg(expectation.group)
                        .arg(travelled, 0, 'f', 2)
                        .arg(expectation.start_seconds, 0, 'f', 2)
                        .arg(expectation.end_seconds, 0, 'f', 2)
                        .arg(required, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::RpgApproachWithin: {
        float const required =
            expectation.distance > 0.0F ? expectation.distance : 1.0F;
        const QString key =
            projectile_pair_key(expectation.group, expectation.target_group);
        auto const closest = minimum_group_pair_distance.constFind(key);
        if (closest == minimum_group_pair_distance.cend()) {
          add_issue(QStringLiteral("rpg_approach_not_sampled"),
                    QStringLiteral("%1 and %2 were never sampled together")
                        .arg(expectation.group, expectation.target_group));
        } else if (closest.value() > required) {
          add_issue(QStringLiteral("rpg_approach_blocked"),
                    QStringLiteral("%1 closed to %2 m of %3 but had to reach %4 m")
                        .arg(expectation.group)
                        .arg(closest.value(), 0, 'f', 2)
                        .arg(expectation.target_group)
                        .arg(required, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::UndeadZoneDormantBefore: {
        auto const state = undead_zone_state(expectation.zone_id);
        float const window = std::max(expectation.end_seconds, expectation.threshold);
        if (state.first_spawn_at >= 0.0F && state.first_spawn_at < window) {
          add_issue(QStringLiteral("undead_zone_woke_too_early"),
                    QStringLiteral("%1 spawned guardians at %2 s but had to stay "
                                   "dormant for %3 s")
                        .arg(expectation.zone_id)
                        .arg(state.first_spawn_at, 0, 'f', 2)
                        .arg(window, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::UndeadZoneAwakened: {
        auto const state = undead_zone_state(expectation.zone_id);
        int const required = std::max(1, static_cast<int>(expectation.threshold));
        if (state.spawned_total < required) {
          add_issue(QStringLiteral("undead_zone_never_awakened"),
                    QStringLiteral("%1 spawned %2 guardian(s); expected at least %3")
                        .arg(expectation.zone_id)
                        .arg(state.spawned_total)
                        .arg(required));
        }
        break;
      }
      case ArenaExpectationKind::UndeadZoneCleared: {
        auto const state = undead_zone_state(expectation.zone_id);
        if (state.spawned_total == 0) {
          add_issue(
              QStringLiteral("undead_zone_never_awakened"),
              QStringLiteral("%1 never spawned guardians, so it cannot be cleared")
                  .arg(expectation.zone_id));
        } else if (state.alive > 0) {
          add_issue(QStringLiteral("undead_zone_not_cleared"),
                    QStringLiteral("%1 still holds %2 living guardian(s)")
                        .arg(expectation.zone_id)
                        .arg(state.alive));
        }
        break;
      }
      case ArenaExpectationKind::UndeadZoneShrineStands: {
        auto const state = undead_zone_state(expectation.zone_id);
        if (!state.shrine_seen) {
          add_issue(QStringLiteral("undead_zone_shrine_missing"),
                    QStringLiteral("%1 never raised a magic shrine")
                        .arg(expectation.zone_id));
        } else if (!state.shrine_standing) {
          add_issue(QStringLiteral("undead_zone_shrine_lost"),
                    QStringLiteral("%1 lost the shrine it was meant to keep")
                        .arg(expectation.zone_id));
        }
        break;
      }
      case ArenaExpectationKind::UndeadZoneShrineDestroyed: {
        auto const state = undead_zone_state(expectation.zone_id);
        if (!state.shrine_seen) {
          add_issue(QStringLiteral("undead_zone_shrine_missing"),
                    QStringLiteral("%1 never raised a magic shrine to destroy")
                        .arg(expectation.zone_id));
        } else if (!state.shrine_destroyed) {
          add_issue(
              QStringLiteral("undead_zone_shrine_survived"),
              QStringLiteral("%1 still holds its shrine").arg(expectation.zone_id));
        }
        break;
      }
      case ArenaExpectationKind::WildlifeGrazingObserved:
        if (!wildlife_observation.grazing_seen) {
          add_issue(QStringLiteral("wildlife_never_grazed"),
                    QStringLiteral("no animal settled into grazing during the run"));
        }
        break;
      case ArenaExpectationKind::WildlifeFleeObserved:
        if (!wildlife_observation.flee_seen) {
          add_issue(QStringLiteral("wildlife_never_fled"),
                    QStringLiteral("no animal fled or scattered during the run"));
        }
        break;
      case ArenaExpectationKind::WildlifeHuntObserved:
        if (!wildlife_observation.hunt_seen) {
          add_issue(QStringLiteral("wildlife_never_hunted"),
                    QStringLiteral("no wolf started stalking prey during the run"));
        }
        break;
      case ArenaExpectationKind::WildlifeBirdsScattered:
        if (wildlife_observation.bird_scatter_events == 0U) {
          add_issue(QStringLiteral("bird_flock_never_scattered"),
                    QStringLiteral("the flock never scattered from a threat"));
        }
        break;
      case ArenaExpectationKind::WildlifeBirdFlyoverObserved:
        if (wildlife_observation.bird_flyovers == 0U) {
          add_issue(QStringLiteral("bird_flyover_never_launched"),
                    QStringLiteral("no flock crossed the sky during the run"));
        }
        break;
      case ArenaExpectationKind::WildlifePopulationHeld: {
        int const required = std::max(1, static_cast<int>(expectation.threshold));
        if (wildlife_observation.min_population < required) {
          add_issue(QStringLiteral("wildlife_population_dropped"),
                    QStringLiteral("wildlife population fell to %1, below the "
                                   "required %2 (peak %3)")
                        .arg(wildlife_observation.min_population)
                        .arg(required)
                        .arg(wildlife_observation.peak_population));
        }
        break;
      }
      case ArenaExpectationKind::WildlifeCasualtyObserved:
        if (wildlife_observation.min_population >=
            wildlife_observation.peak_population) {
          add_issue(QStringLiteral("wildlife_never_culled"),
                    QStringLiteral("no animal was killed during the run (population "
                                   "held at %1)")
                        .arg(wildlife_observation.peak_population));
        }
        break;
      case ArenaExpectationKind::UnitsClearOfBuildings:
        if (building_overlap_report.contains(expectation.group)) {
          add_issue(QStringLiteral("unit_inside_building"),
                    QStringLiteral("%1 walked through a building: %2")
                        .arg(expectation.group,
                             building_overlap_report.value(expectation.group)));
        }
        break;
      case ArenaExpectationKind::NoRenderVisibilityChurn:
      case ArenaExpectationKind::RpgFormationSurvivesLensGap:
      case ArenaExpectationKind::FullCreatureDetailOnly:
      case ArenaExpectationKind::NoFullscreenFlash:

        break;
      default:
        break;
      }
    }
  }
};

ArenaScenarioRunner::ArenaScenarioRunner(Engine::Core::World& world,
                                         ArenaScenarioHost host,
                                         const ArenaScenarioDefinition& definition,
                                         QVector3D world_origin)
    : m_impl(std::make_unique<Impl>(world, std::move(host), definition, world_origin)) {
}

ArenaScenarioRunner::~ArenaScenarioRunner() = default;

auto ArenaScenarioRunner::start() -> bool {
  if (m_impl->started) {
    return false;
  }
  auto const validation = validate_scenario(m_impl->scenario);
  if (!validation.empty()) {
    for (auto const& error : validation) {
      qWarning().noquote() << QStringLiteral("Arena scenario '%1' invalid: %2 -- %3")
                                  .arg(m_impl->scenario.id, error.field, error.message);
    }
    return false;
  }
  if (!m_impl->host.spawn_unit) {
    return false;
  }
  m_impl->started = true;
  for (auto const& group : m_impl->scenario.groups) {
    if (group.spawn_at_start) {
      m_impl->spawn_group(group);
    }
  }
  if (m_impl->scenario.rpg_mode) {
    auto const& commanders = m_impl->ids(m_impl->scenario.rpg_commander_group);
    if (commanders.size() != 1U || !m_impl->host.configure_rpg_commander) {
      m_impl->add_issue(
          QStringLiteral("rpg_commander_setup_failed"),
          QStringLiteral("RPG scenario requires one spawned commander and an "
                         "RPG-capable host"));
      return false;
    }
    Engine::Core::EntityID const commander_id = commanders.front();
    m_impl->host.configure_rpg_commander(commander_id);
    auto* commander = m_impl->world.get_entity(commander_id);
    auto const* commander_group =
        m_impl->group_definition(m_impl->scenario.rpg_commander_group);
    if (commander != nullptr && commander_group != nullptr &&
        (commander_group->stamina_override > 0.0F ||
         commander_group->max_stamina_override > 0.0F)) {
      auto* stamina =
          Engine::Core::get_or_add_component<Engine::Core::StaminaComponent>(commander);
      if (stamina != nullptr) {
        if (commander_group->max_stamina_override > 0.0F) {
          stamina->max_stamina = commander_group->max_stamina_override;
        }
        stamina->stamina =
            commander_group->stamina_override > 0.0F
                ? std::min(commander_group->stamina_override, stamina->max_stamina)
                : stamina->max_stamina;
        stamina->regen_delay_remaining = 0.0F;
      }
    }
    auto const* rpg = commander != nullptr
                          ? commander->get_component<Engine::Core::RpgHealthComponent>()
                          : nullptr;
    if (rpg == nullptr || !rpg->active) {
      m_impl->add_issue(QStringLiteral("rpg_commander_setup_failed"),
                        QStringLiteral("RPG host did not activate commander health"),
                        commander_id);
      return false;
    }
    auto const* commander_unit =
        commander->get_component<Engine::Core::UnitComponent>();
    int const commander_health = commander_unit != nullptr ? commander_unit->health : 0;
    m_impl->initial_rpg_health_by_group[m_impl->scenario.rpg_commander_group] =
        commander_health;
    m_impl->minimum_rpg_health_by_group[m_impl->scenario.rpg_commander_group] =
        commander_health;
  }
  for (auto* entity :
       m_impl->world.collect_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity != nullptr) {
      m_impl->initial_building_ids.insert(entity->get_id());
    }
  }
  m_impl->observe_undead_zones();
  m_impl->initialize_battle_sides();
  if (m_impl->host.set_camera) {
    m_impl->host.set_camera(m_impl->all_entities(), m_impl->scenario.camera);
  }
  for (std::size_t i = 0; i < m_impl->scenario.steps.size(); ++i) {
    auto const& step = m_impl->scenario.steps[i];
    if (!m_impl->steps[i].executed && m_impl->trigger_ready(i, step)) {
      m_impl->execute_step(i, step);
    }
  }
  return true;
}

void ArenaScenarioRunner::update(float simulation_dt) {
  if (!m_impl->started || m_impl->complete || simulation_dt <= 0.0F) {
    return;
  }
  m_impl->elapsed += simulation_dt;
  m_impl->report.elapsed_seconds = m_impl->elapsed;
  m_impl->observe_undead_zones();
  m_impl->observe_wildlife();
  m_impl->observe_battle();
  for (std::size_t i = 0; i < m_impl->scenario.steps.size(); ++i) {
    auto const& step = m_impl->scenario.steps[i];
    if (!m_impl->steps[i].executed && m_impl->trigger_ready(i, step)) {
      m_impl->execute_step(i, step);
    }
  }
  m_impl->track_rpg_aim();
  m_impl->observe_commander_aura_state();
  m_impl->observe_range_rings();
  m_impl->observe_projectiles();
  for (auto const& expectation : m_impl->scenario.expectations) {
    if (expectation.kind == ArenaExpectationKind::FormationOrderPreserved &&
        m_impl->expectation_active(expectation)) {
      m_impl->check_formation_order(expectation);
    }
  }
  if (m_impl->elapsed + 1.0e-5F >= m_impl->duration_limit ||
      m_impl->battle_decision_ends_scenario()) {
    m_impl->check_end_expectations();
    m_impl->complete = true;
  }
}

void ArenaScenarioRunner::observe_rendered_frame(double frame_time_ms) {
  ArenaRenderedFrameTimings timings;
  timings.total_ms = frame_time_ms;
  observe_rendered_frame(timings);
}

void ArenaScenarioRunner::set_animation_time(float seconds) {
  m_impl->animation_time = seconds;
}

void ArenaScenarioRunner::observe_commander_presentation(
    const App::Core::CommanderPresentationTrace& trace) {
  m_impl->commander_trace = trace;
}

void ArenaScenarioRunner::observe_rendered_frame(
    const ArenaRenderedFrameTimings& timings) {
  if (!m_impl->started) {
    return;
  }
  ++m_impl->report.rendered_frames;
  Impl::TraceFrame frame;
  frame.time_seconds = m_impl->elapsed;
  frame.frame_time_ms = timings.total_ms;
  frame.timings = timings;
  frame.commander = m_impl->commander_trace;
  frame.animation_time = m_impl->animation_time;
  m_impl->record_animals(frame);
  for (auto* entity :
       m_impl->world.collect_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity == nullptr || m_impl->initial_building_ids.contains(entity->get_id()) ||
        m_impl->observed_constructed_building_ids.contains(entity->get_id())) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->health > 0) {
      m_impl->observed_constructed_building_ids.insert(entity->get_id());
      m_impl->completed_construction_by_owner[unit->owner_id]++;
    }
  }
  for (auto const& group : m_impl->scenario.groups) {
    for (auto entity_id : m_impl->ids(group.name)) {
      m_impl->observe_entity(entity_id, group.name, frame);
      m_impl->observe_narrow_layout(group.name, entity_id);
      m_impl->observe_building_clearance(entity_id, group.name);
      m_impl->observe_soldiers(entity_id, group.name, frame);
    }
    m_impl->observe_bridge_centerline_alignment(group.name);
  }
  m_impl->observe_rpg_locomotion_presentation(frame);
  m_impl->observe_rpg_swing_cadence(frame);
  m_impl->observe_rpg_travel(frame);
  m_impl->observe_group_pair_proximity(frame);
  m_impl->trace.push_back(std::move(frame));
}

void ArenaScenarioRunner::report_external_issue(QString code, QString message) {
  if (!m_impl->started || m_impl->complete) {
    return;
  }
  m_impl->add_issue(std::move(code), std::move(message));
}

void ArenaScenarioRunner::set_duration_limit(float duration_seconds) {
  if (duration_seconds > 0.0F) {
    m_impl->duration_limit = duration_seconds;
  }
}

void ArenaScenarioRunner::set_environment_snapshot(
    const ArenaEnvironmentSnapshot& snapshot) {
  m_impl->environment_snapshot = snapshot;
}

auto ArenaScenarioRunner::definition() const noexcept
    -> const ArenaScenarioDefinition& {
  return m_impl->scenario;
}

auto ArenaScenarioRunner::elapsed_seconds() const noexcept -> float {
  return m_impl->elapsed;
}

auto ArenaScenarioRunner::finished() const noexcept -> bool {
  return m_impl->complete;
}

auto ArenaScenarioRunner::report() const noexcept -> const ArenaScenarioReport& {
  return m_impl->report;
}

auto ArenaScenarioRunner::live_battle_sides() const
    -> std::vector<ArenaBattleSideResult> {
  return m_impl->live_sides();
}

auto ArenaScenarioRunner::battle_decided() const noexcept -> bool {
  return m_impl->battle_decided;
}

auto ArenaScenarioRunner::group_entities(const QString& group) const
    -> const std::vector<Engine::Core::EntityID>& {
  return m_impl->ids(group);
}

auto ArenaScenarioRunner::all_entities() const -> std::vector<Engine::Core::EntityID> {
  return m_impl->all_entities();
}

auto ArenaScenarioRunner::issue_revision() const noexcept -> std::size_t {
  return m_impl->report.issues.size();
}

auto ArenaScenarioRunner::write_artifacts(const QString& directory,
                                          QString* error) const -> bool {
  QDir const dir;
  if (!dir.mkpath(directory)) {
    if (error != nullptr) {
      *error = QStringLiteral("failed to create artifact directory %1").arg(directory);
    }
    return false;
  }

  QJsonObject report_object;
  report_object.insert(QStringLiteral("scenario"), m_impl->report.scenario_id);
  report_object.insert(QStringLiteral("completed"), m_impl->complete);
  report_object.insert(QStringLiteral("passed"),
                       m_impl->complete && m_impl->report.passed());
  report_object.insert(QStringLiteral("elapsed_seconds"),
                       m_impl->report.elapsed_seconds);
  report_object.insert(QStringLiteral("rendered_frames"),
                       static_cast<qint64>(m_impl->report.rendered_frames));
  report_object.insert(QStringLiteral("rendered_soldier_samples"),
                       static_cast<qint64>(m_impl->report.rendered_soldier_samples));
  if (m_impl->report.battle.tracked) {
    QJsonArray sides;
    for (auto const& side : m_impl->report.battle.sides) {
      sides.append(QJsonObject{
          {QStringLiteral("owner_id"), side.owner_id},
          {QStringLiteral("label"), side.label},
          {QStringLiteral("living_units"), side.living_units},
          {QStringLiteral("living_buildings"), side.living_buildings},
          {QStringLiteral("peak_units"), side.peak_units},
          {QStringLiteral("units_produced"), side.units_produced},
          {QStringLiteral("peak_advance"), side.peak_advance},
          {QStringLiteral("final_advance"), side.final_advance},
          {QStringLiteral("eliminated_at"), side.eliminated_at},
          {QStringLiteral("strategy"), side.strategy},
          {QStringLiteral("posture"), side.posture},
          {QStringLiteral("seconds_attacking"), side.seconds_attacking},
          {QStringLiteral("seconds_observed"), side.seconds_observed},
          {QStringLiteral("buildings_constructed"), side.buildings_constructed},
          {QStringLiteral("peak_buildings"), side.peak_buildings},
          {QStringLiteral("building_census"), side.building_census},
          {QStringLiteral("peak_home_units"), side.peak_home_units},
          {QStringLiteral("peak_forward_units"), side.peak_forward_units},
          {QStringLiteral("mean_home_share"), side.mean_home_share}});
    }
    report_object.insert(
        QStringLiteral("battle"),
        QJsonObject{
            {QStringLiteral("decided"), m_impl->report.battle.decided},
            {QStringLiteral("victor_owner_id"), m_impl->report.battle.victor_owner_id},
            {QStringLiteral("victor"), m_impl->report.battle.victor_label},
            {QStringLiteral("decided_at_seconds"),
             m_impl->report.battle.decided_at_seconds},
            {QStringLiteral("sides"), sides}});
  }
  if (m_impl->report.frame_time_samples > 0U) {
    double const p95_fps = m_impl->report.frame_time_p95_ms > 0.0
                               ? 1000.0 / m_impl->report.frame_time_p95_ms
                               : 0.0;
    report_object.insert(
        QStringLiteral("performance"),
        QJsonObject{
            {QStringLiteral("sample_count"),
             static_cast<qint64>(m_impl->report.frame_time_samples)},
            {QStringLiteral("budget_ms"), m_impl->report.frame_budget_ms},
            {QStringLiteral("p50_ms"), m_impl->report.frame_time_p50_ms},
            {QStringLiteral("p95_ms"), m_impl->report.frame_time_p95_ms},
            {QStringLiteral("p99_ms"), m_impl->report.frame_time_p99_ms},
            {QStringLiteral("max_ms"), m_impl->report.frame_time_max_ms},
            {QStringLiteral("p95_fps"), p95_fps},
            {QStringLiteral("prewarm_seconds"), m_impl->report.prewarm_seconds},
            {QStringLiteral("prewarm_frames"),
             static_cast<qint64>(m_impl->report.prewarm_frames)},
            {QStringLiteral("prewarm_max_ms"), m_impl->report.prewarm_max_ms},
            {QStringLiteral("gpu_timed_frames"),
             static_cast<qint64>(m_impl->report.gpu_timed_frames)},
            {QStringLiteral("rpg_cost_p95_ms"),
             QJsonObject{
                 {QStringLiteral("motor"), m_impl->report.rpg_cost_p95_motor_ms},
                 {QStringLiteral("targeting"),
                  m_impl->report.rpg_cost_p95_targeting_ms},
                 {QStringLiteral("weapon_trace"),
                  m_impl->report.rpg_cost_p95_weapon_trace_ms},
                 {QStringLiteral("engagement"),
                  m_impl->report.rpg_cost_p95_engagement_ms},
                 {QStringLiteral("camera"), m_impl->report.rpg_cost_p95_camera_ms},
                 {QStringLiteral("total"), m_impl->report.rpg_cost_p95_total_ms}}},
            {QStringLiteral("simulation_p95_ms"), m_impl->report.simulation_p95_ms},
            {QStringLiteral("peak_visible_soldiers"),
             static_cast<qint64>(m_impl->report.peak_visible_soldiers)},
            {QStringLiteral("peak_draw_commands"),
             static_cast<qint64>(m_impl->report.peak_draw_commands)},
            {QStringLiteral("peak_rigged_commands"),
             static_cast<qint64>(m_impl->report.peak_rigged_commands)},
            {QStringLiteral("peak_rigged_instanced_instances"),
             static_cast<qint64>(m_impl->report.peak_rigged_instanced_instances)},
            {QStringLiteral("peak_rigged_single_draws"),
             static_cast<qint64>(m_impl->report.peak_rigged_single_draws)},
            {QStringLiteral("peak_shadow_rigged_instanced_instances"),
             static_cast<qint64>(
                 m_impl->report.peak_shadow_rigged_instanced_instances)},
            {QStringLiteral("peak_shadow_rigged_single_draws"),
             static_cast<qint64>(m_impl->report.peak_shadow_rigged_single_draws)}});
  }
  QJsonArray issues;
  for (auto const& issue : m_impl->report.issues) {
    issues.append(
        QJsonObject{{QStringLiteral("code"), issue.code},
                    {QStringLiteral("message"), issue.message},
                    {QStringLiteral("time_seconds"), issue.time_seconds},
                    {QStringLiteral("entity_id"), static_cast<qint64>(issue.entity_id)},
                    {QStringLiteral("soldier_index"), issue.soldier_index}});
  }
  report_object.insert(QStringLiteral("issues"), issues);

  report_object.insert(QStringLiteral("asset_counters"),
                       Render::Profiling::asset_counters_json());
  report_object.insert(QStringLiteral("navigation"),
                       Render::Profiling::navigation_counters_json());
  report_object.insert(
      QStringLiteral("simulation_systems"),
      Render::Profiling::system_profiler_json(m_impl->world.system_profiler()));
  if (m_impl->report.frame_time_samples > 0U) {
    Render::Profiling::PerformanceMeasurement measurement;
    measurement.frames = m_impl->report.frame_time_samples;
    measurement.frame_p50_ms = m_impl->report.frame_time_p50_ms;
    measurement.frame_p95_ms = m_impl->report.frame_time_p95_ms;
    measurement.frame_p99_ms = m_impl->report.frame_time_p99_ms;
    measurement.frame_max_ms = m_impl->report.frame_time_max_ms;
    measurement.update_p95_ms = m_impl->report.simulation_p95_ms;
    measurement.update_average_ms = m_impl->report.simulation_p95_ms;
    measurement.gpu_timed = m_impl->report.gpu_timed_frames > 0U;
    const auto& graphics = Render::GraphicsSettings::instance();
    measurement.ultra_preset = graphics.quality() == Render::GraphicsQuality::Ultra;
    measurement.full_creature_lod = !graphics.creature_lod_enabled();
    report_object.insert(QStringLiteral("budget"),
                         Render::Profiling::budget_verdict_json(
                             Render::Profiling::PerformanceBudget::scale_gate(
                                 m_impl->report.frame_budget_ms),
                             measurement));
  }

  if (const auto& env = m_impl->environment_snapshot; env.valid) {
    const auto vec3 = [](const QVector3D& value) {
      return QJsonArray{value.x(), value.y(), value.z()};
    };
    report_object.insert(
        QStringLiteral("environment"),
        QJsonObject{{QStringLiteral("hour"), env.hour},
                    {QStringLiteral("time_of_day"), env.time_of_day},
                    {QStringLiteral("time_mode"), env.time_mode},
                    {QStringLiteral("lighting_profile"), env.lighting_profile},
                    {QStringLiteral("primary_direction"), vec3(env.primary_direction)},
                    {QStringLiteral("primary_color"), vec3(env.primary_color)},
                    {QStringLiteral("sky_color"), vec3(env.sky_color)},
                    {QStringLiteral("primary_intensity"), env.primary_intensity},
                    {QStringLiteral("ambient_intensity"), env.ambient_intensity},
                    {QStringLiteral("exposure"), env.exposure},
                    {QStringLiteral("fog_density"), env.fog_density},
                    {QStringLiteral("cloud_cover"), env.cloud_cover},
                    {QStringLiteral("wetness"), env.wetness}});
    report_object.insert(
        QStringLiteral("shadows"),
        QJsonObject{
            {QStringLiteral("quality"), env.shadow_quality},
            {QStringLiteral("directional_enabled"), env.directional_shadows_enabled},
            {QStringLiteral("resolution"), env.shadow_resolution},
            {QStringLiteral("cascades"), env.shadow_cascades},
            {QStringLiteral("distance"), env.shadow_distance},
            {QStringLiteral("contact_shadow_casters"), env.contact_shadow_casters}});
  }

  if (!m_impl->scenario.undead_zones.empty()) {
    QJsonArray zones;
    for (auto const& zone : m_impl->scenario.undead_zones) {
      auto const state = m_impl->undead_zone_state(zone.id);
      zones.append(
          QJsonObject{{QStringLiteral("id"), zone.id},
                      {QStringLiteral("owner_id"), zone.owner_id},
                      {QStringLiteral("spawned_total"), state.spawned_total},
                      {QStringLiteral("peak_alive"), state.peak_alive},
                      {QStringLiteral("alive"), state.alive},
                      {QStringLiteral("first_spawn_seconds"), state.first_spawn_at},
                      {QStringLiteral("shrine_seen"), state.shrine_seen},
                      {QStringLiteral("shrine_standing"), state.shrine_standing},
                      {QStringLiteral("shrine_destroyed"), state.shrine_destroyed}});
    }
    report_object.insert(QStringLiteral("undead_zones"), zones);
  }

  QFile report_file(QDir(directory).filePath(QStringLiteral("report.json")));
  if (!report_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = report_file.errorString();
    }
    return false;
  }
  report_file.write(QJsonDocument(report_object).toJson(QJsonDocument::Indented));
  report_file.close();

  QFile trace_file(QDir(directory).filePath(QStringLiteral("trace.jsonl")));
  if (!trace_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error != nullptr) {
      *error = trace_file.errorString();
    }
    return false;
  }
  for (auto const& frame : m_impl->trace) {
    QJsonArray units;
    for (auto const& unit : frame.units) {
      QJsonArray engaged_soldiers;
      for (auto index : unit.engaged_soldiers) {
        engaged_soldiers.append(static_cast<int>(index));
      }
      QJsonArray engagement_pairs;
      for (auto const& pair : unit.engagement_pairs) {
        engagement_pairs.append(
            QJsonObject{{QStringLiteral("attacker_slot"), pair.attacker_slot},
                        {QStringLiteral("target_slot"), pair.target_slot},
                        {QStringLiteral("root_distance"), pair.root_distance},
                        {QStringLiteral("surface_gap"), pair.surface_gap}});
      }
      units.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(unit.entity_id)},
          {QStringLiteral("group"), unit.group},
          {QStringLiteral("position"), json_vector(unit.position)},
          {QStringLiteral("health"), unit.health},
          {QStringLiteral("target_id"), static_cast<qint64>(unit.target_id)},
          {QStringLiteral("motion"), unit.motion},
          {QStringLiteral("combat_mode"), unit.combat_mode},
          {QStringLiteral("mounted_charge_state"), unit.mounted_charge_state},
          {QStringLiteral("mounted_charge_cancel_reason"),
           unit.mounted_charge_cancel_reason},
          {QStringLiteral("combat_action_id"), unit.combat_action_id},
          {QStringLiteral("melee_lock"), unit.melee_lock},
          {QStringLiteral("melee_lock_target_id"),
           static_cast<qint64>(unit.melee_lock_target_id)},
          {QStringLiteral("combat_indicator_submitted"),
           unit.combat_indicator_submitted},
          {QStringLiteral("yaw"), unit.yaw},
          {QStringLiteral("movement_target"), unit.movement_target},
          {QStringLiteral("movement_velocity"),
           QJsonArray{unit.movement_vx, unit.movement_vz}},
          {QStringLiteral("movement_goal"),
           QJsonArray{unit.movement_goal_x, unit.movement_goal_z}},
          {QStringLiteral("formation_contact"), unit.formation_contact},
          {QStringLiteral("formation_surface_gap"), unit.formation_surface_gap},
          {QStringLiteral("engaged_soldiers"), engaged_soldiers},
          {QStringLiteral("engagement_pairs"), engagement_pairs},
          {QStringLiteral("construction_type"), unit.construction_type},
          {QStringLiteral("construction_site"), unit.construction_site},
          {QStringLiteral("construction_in_progress"), unit.construction_in_progress},
          {QStringLiteral("construction_time_remaining"),
           unit.construction_time_remaining},
          {QStringLiteral("commander_aura_active"), unit.commander_aura_active},
          {QStringLiteral("commander_aura_buffed"), unit.commander_aura_buffed},
          {QStringLiteral("rpg_health"), unit.rpg_health},
          {QStringLiteral("rpg_guard_active"), unit.rpg_guard_active},
          {QStringLiteral("rpg_dodge_grace"), unit.rpg_dodge_grace},
          {QStringLiteral("rpg_aim_target_id"),
           static_cast<qint64>(unit.rpg_aim_target_id)},
          {QStringLiteral("rpg_aim_soldier_slot"), unit.rpg_aim_soldier_slot},
          {QStringLiteral("rpg_action_phase"), unit.rpg_action_phase},
          {QStringLiteral("rpg_action_time"), unit.rpg_action_normalized_time}});
    }
    QJsonArray animals;
    for (auto const& animal : frame.animals) {
      animals.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(animal.entity_id)},
          {QStringLiteral("species"), animal.species},
          {QStringLiteral("position"), json_vector(animal.position)},
          {QStringLiteral("health"), animal.health},
          {QStringLiteral("behavior"), animal.behavior},
          {QStringLiteral("focus_id"), static_cast<qint64>(animal.focus_id)},
          {QStringLiteral("yaw"), animal.yaw},
          {QStringLiteral("desired_yaw"), animal.desired_yaw},
          {QStringLiteral("has_desired_yaw"), animal.has_desired_yaw},
          {QStringLiteral("velocity"), QJsonArray{animal.vx, animal.vz}},
          {QStringLiteral("biting"), animal.biting},
          {QStringLiteral("bite_phase"), animal.bite_phase},
          {QStringLiteral("flinch_phase"), animal.flinch_phase},
          {QStringLiteral("bite_target_id"),
           static_cast<qint64>(animal.bite_target_id)},
          {QStringLiteral("impact_pending"), animal.impact_pending},
          {QStringLiteral("dying"), animal.dying}});
    }

    QJsonArray soldiers;
    for (auto const& soldier : frame.soldiers) {
      soldiers.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(soldier.entity_id)},
          {QStringLiteral("soldier_index"), soldier.soldier_index},
          {QStringLiteral("root_position"), json_vector(soldier.root_position)},
          {QStringLiteral("root_yaw_degrees"), soldier.root_yaw_degrees},
          {QStringLiteral("root_up_y"), soldier.root_up_y},
          {QStringLiteral("submitted_body_up_y"), soldier.submitted_body_up_y},
          {QStringLiteral("submitted_max_arm_reach"), soldier.submitted_max_arm_reach},
          {QStringLiteral("submitted_body_pose_valid"),
           soldier.submitted_body_pose_valid},
          {QStringLiteral("foot_l_world"), json_vector(soldier.foot_l_world)},
          {QStringLiteral("foot_r_world"), json_vector(soldier.foot_r_world)},
          {QStringLiteral("locomotion_blend"), soldier.locomotion_blend},
          {QStringLiteral("locomotion_presence"), soldier.locomotion_presence},
          {QStringLiteral("cycle_phase"), soldier.cycle_phase},
          {QStringLiteral("persistent_valid"), soldier.persistent_valid},
          {QStringLiteral("sample_time"), soldier.sample_time},
          {QStringLiteral("previous_locomotion_presence"),
           soldier.persistent_last_sample_time},
          {QStringLiteral("declared_action"), soldier.declared_action},
          {QStringLiteral("declared_target_slot"), soldier.declared_target_slot},
          {QStringLiteral("declared_surface_gap"), soldier.declared_surface_gap},
          {QStringLiteral("animation"), soldier.animation},
          {QStringLiteral("visual"), soldier.visual},
          {QStringLiteral("swing_recoil"), soldier.swing_recoil},
          {QStringLiteral("hit_reaction_tilt_degrees"),
           soldier.hit_reaction_tilt_degrees},
          {QStringLiteral("attack_phase"), soldier.attack_phase},
          {QStringLiteral("transitions_last_second"),
           static_cast<qint64>(soldier.transitions)},
          {QStringLiteral("culled"), soldier.culled},
          {QStringLiteral("cull_reason"), soldier.cull_reason}});
    }
    QJsonObject line{
        {QStringLiteral("time_seconds"), frame.time_seconds},
        {QStringLiteral("frame_time_ms"), frame.frame_time_ms},
        {QStringLiteral("frame_breakdown_ms"),
         QJsonObject{
             {QStringLiteral("simulation"), frame.timings.simulation_ms},
             {QStringLiteral("terrain_submit"), frame.timings.terrain_submit_ms},
             {QStringLiteral("world_submit"), frame.timings.world_submit_ms},
             {QStringLiteral("effects_submit"), frame.timings.effects_submit_ms},
             {QStringLiteral("render_execute"), frame.timings.render_execute_ms},
             {QStringLiteral("overlays"), frame.timings.overlays_ms},
             {QStringLiteral("humanoid_preparation"),
              frame.timings.humanoid_preparation_ms},
             {QStringLiteral("animation_sampling"),
              frame.timings.animation_sampling_ms},
             {QStringLiteral("bpat_playback"), frame.timings.bpat_playback_ms},
             {QStringLiteral("layout_generation"),
              frame.timings.layout_generation_ms}}},
        {QStringLiteral("gpu_ms"),
         QJsonObject{{QStringLiteral("shadow"), frame.timings.gpu_shadow_ms},
                     {QStringLiteral("color"), frame.timings.gpu_color_ms},
                     {QStringLiteral("wait"), frame.timings.gpu_wait_ms}}},
        {QStringLiteral("visible_soldiers"),
         static_cast<qint64>(frame.timings.visible_soldiers)},
        {QStringLiteral("draw_calls"), static_cast<qint64>(frame.timings.draw_calls)},
        {QStringLiteral("prepared_batches"),
         static_cast<qint64>(frame.timings.prepared_batches)},
        {QStringLiteral("rigged_playback"),
         QJsonObject{
             {QStringLiteral("commands"),
              static_cast<qint64>(frame.timings.rigged_commands)},
             {QStringLiteral("instanced_draws"),
              static_cast<qint64>(frame.timings.rigged_instanced_draws)},
             {QStringLiteral("instanced_instances"),
              static_cast<qint64>(frame.timings.rigged_instanced_instances)},
             {QStringLiteral("single_draws"),
              static_cast<qint64>(frame.timings.rigged_single_draws)},
             {QStringLiteral("shadow_instanced_instances"),
              static_cast<qint64>(frame.timings.shadow_rigged_instanced_instances)},
             {QStringLiteral("shadow_instanced_draws"),
              static_cast<qint64>(frame.timings.shadow_rigged_instanced_draws)},
             {QStringLiteral("shadow_single_draws"),
              static_cast<qint64>(frame.timings.shadow_rigged_single_draws)}}},
        {QStringLiteral("animation_time"), frame.animation_time},
        {QStringLiteral("units"), units},
        {QStringLiteral("animals"), animals},
        {QStringLiteral("soldiers"), soldiers}};
    if (frame.commander.valid) {
      line.insert(QStringLiteral("commander"), commander_trace_json(frame.commander));
    }
    trace_file.write(QJsonDocument(line).toJson(QJsonDocument::Compact));
    trace_file.write("\n");
  }
  return true;
}

} // namespace Arena

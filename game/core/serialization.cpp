#include "serialization.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qstringliteral.h>
#include <qstringview.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../systems/nation_id.h"
#include "../systems/owner_registry.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "component.h"
#include "entity.h"
#include "world.h"

namespace Engine::Core {

namespace {

auto combat_mode_to_string(AttackComponent::CombatMode mode) -> QString {
  switch (mode) {
  case AttackComponent::CombatMode::Melee:
    return "melee";
  case AttackComponent::CombatMode::Ranged:
    return "ranged";
  case AttackComponent::CombatMode::Auto:
  default:
    return "auto";
  }
}

auto combat_mode_from_string(const QString& value) -> AttackComponent::CombatMode {
  if (value == "melee") {
    return AttackComponent::CombatMode::Melee;
  }
  if (value == "ranged") {
    return AttackComponent::CombatMode::Ranged;
  }
  return AttackComponent::CombatMode::Auto;
}

auto healer_affinity_to_string(HealerComponent::TargetAffinity affinity) -> QString {
  switch (affinity) {
  case HealerComponent::TargetAffinity::UndeadAllies:
    return "undead_allies";
  case HealerComponent::TargetAffinity::LivingAllies:
  default:
    return "living_allies";
  }
}

auto healer_affinity_from_string(const QString& value)
    -> HealerComponent::TargetAffinity {
  if (value == "undead_allies") {
    return HealerComponent::TargetAffinity::UndeadAllies;
  }
  return HealerComponent::TargetAffinity::LivingAllies;
}

auto projectile_kind_to_string(Game::Systems::ProjectileKind kind) -> QString {
  switch (kind) {
  case Game::Systems::ProjectileKind::Fireball:
    return "fireball";
  case Game::Systems::ProjectileKind::CursedArrow:
    return "cursed_arrow";
  case Game::Systems::ProjectileKind::Stone:
    return "stone";
  case Game::Systems::ProjectileKind::Arrow:
  default:
    return "arrow";
  }
}

auto projectile_kind_from_string(const QString& value)
    -> Game::Systems::ProjectileKind {
  if (value == "fireball") {
    return Game::Systems::ProjectileKind::Fireball;
  }
  if (value == "cursed_arrow") {
    return Game::Systems::ProjectileKind::CursedArrow;
  }
  if (value == "stone") {
    return Game::Systems::ProjectileKind::Stone;
  }
  return Game::Systems::ProjectileKind::Arrow;
}

auto serialize_color(const std::array<float, 3>& color) -> QJsonArray {
  QJsonArray array;
  array.append(color[0]);
  array.append(color[1]);
  array.append(color[2]);
  return array;
}

void deserialize_color(const QJsonArray& array, std::array<float, 3>& color) {
  if (array.size() >= 3) {
    color[0] = static_cast<float>(array.at(0).toDouble());
    color[1] = static_cast<float>(array.at(1).toDouble());
    color[2] = static_cast<float>(array.at(2).toDouble());
  }
}

} // namespace

auto Serialization::serialize_entity(const Entity* entity) -> QJsonObject {
  QJsonObject entity_obj;
  entity_obj["id"] = static_cast<qint64>(entity->get_id());

  if (const auto* transform = entity->get_component<TransformComponent>()) {
    QJsonObject transform_obj;
    transform_obj["pos_x"] = transform->position.x;
    transform_obj["pos_y"] = transform->position.y;
    transform_obj["pos_z"] = transform->position.z;
    transform_obj["rot_x"] = transform->rotation.x;
    transform_obj["rot_y"] = transform->rotation.y;
    transform_obj["rot_z"] = transform->rotation.z;
    transform_obj["scale_x"] = transform->scale.x;
    transform_obj["scale_y"] = transform->scale.y;
    transform_obj["scale_z"] = transform->scale.z;
    transform_obj["has_desired_yaw"] = transform->has_desired_yaw;
    transform_obj["desired_yaw"] = transform->desired_yaw;
    entity_obj["transform"] = transform_obj;
  }

  if (const auto* renderable = entity->get_component<RenderableComponent>()) {
    QJsonObject renderable_obj;
    renderable_obj["mesh_path"] = QString::fromStdString(renderable->mesh_path);
    renderable_obj["texture_path"] = QString::fromStdString(renderable->texture_path);
    if (!renderable->renderer_id.empty()) {
      renderable_obj["renderer_id"] = QString::fromStdString(renderable->renderer_id);
    }
    renderable_obj["visible"] = renderable->visible;
    renderable_obj["mesh"] = static_cast<int>(renderable->mesh);
    renderable_obj["color"] = serialize_color(renderable->color);
    entity_obj["renderable"] = renderable_obj;
  }

  if (const auto* unit = entity->get_component<UnitComponent>()) {
    QJsonObject unit_obj;
    unit_obj["health"] = unit->health;
    unit_obj["max_health"] = unit->max_health;
    unit_obj["speed"] = unit->speed;
    unit_obj["vision_range"] = unit->vision_range;
    unit_obj["unit_type"] =
        QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));
    unit_obj["owner_id"] = unit->owner_id;
    unit_obj["nation_id"] = Game::Systems::nation_id_to_qstring(unit->nation_id);
    unit_obj["render_individuals_per_unit_override"] =
        unit->render_individuals_per_unit_override;
    unit_obj["render_rider"] = unit->render_rider;
    unit_obj["death_sequence_override"] =
        static_cast<int>(unit->death_sequence_override);
    entity_obj["unit"] = unit_obj;
  }

  if (const auto* movement = entity->get_component<MovementComponent>()) {
    QJsonObject movement_obj;
    movement_obj["has_target"] = movement->has_target;
    movement_obj["target_x"] = movement->target_x;
    movement_obj["target_y"] = movement->target_y;
    movement_obj["goal_x"] = movement->goal_x;
    movement_obj["goal_y"] = movement->goal_y;
    movement_obj["vx"] = movement->vx;
    movement_obj["vz"] = movement->vz;
    movement_obj["path_pending"] = movement->path_pending;
    movement_obj["pending_request_id"] =
        static_cast<qint64>(movement->pending_request_id);
    movement_obj["repath_cooldown"] = movement->repath_cooldown;
    movement_obj["last_goal_x"] = movement->last_goal_x;
    movement_obj["last_goal_y"] = movement->last_goal_y;
    movement_obj["time_since_last_path_request"] =
        movement->time_since_last_path_request;
    movement_obj["path_index"] = static_cast<int>(movement->path_index);

    QJsonArray path_array;
    for (const auto& waypoint : movement->path) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      path_array.append(waypoint_obj);
    }
    movement_obj["path"] = path_array;
    movement_obj["last_position_x"] = static_cast<double>(movement->last_position_x);
    movement_obj["last_position_z"] = static_cast<double>(movement->last_position_z);
    movement_obj["time_stuck"] = static_cast<double>(movement->time_stuck);
    movement_obj["unstuck_cooldown"] = static_cast<double>(movement->unstuck_cooldown);
    entity_obj["movement"] = movement_obj;
  }

  if (const auto* intent = entity->get_component<PlayerOrderIntentComponent>()) {
    QJsonObject intent_obj;
    intent_obj["kind"] = static_cast<int>(intent->kind);
    intent_obj["suppress_opportunistic_combat"] = intent->suppress_opportunistic_combat;
    entity_obj["player_order_intent"] = intent_obj;
  }

  if (const auto* attack = entity->get_component<AttackComponent>()) {
    QJsonObject attack_obj;
    attack_obj["range"] = attack->range;
    attack_obj["damage"] = attack->damage;
    attack_obj["cooldown"] = attack->cooldown;
    attack_obj["time_since_last"] = attack->time_since_last;
    attack_obj["melee_range"] = attack->melee_range;
    attack_obj["melee_damage"] = attack->melee_damage;
    attack_obj["melee_cooldown"] = attack->melee_cooldown;
    attack_obj["preferred_mode"] = combat_mode_to_string(attack->preferred_mode);
    attack_obj["current_mode"] = combat_mode_to_string(attack->current_mode);
    attack_obj["can_melee"] = attack->can_melee;
    attack_obj["can_ranged"] = attack->can_ranged;
    attack_obj["max_height_difference"] = attack->max_height_difference;
    attack_obj["in_melee_lock"] = attack->in_melee_lock;
    attack_obj["melee_lock_target_id"] =
        static_cast<qint64>(attack->melee_lock_target_id);
    attack_obj["has_pending_melee_strike"] = attack->has_pending_melee_strike;
    attack_obj["pending_melee_target_id"] =
        static_cast<qint64>(attack->pending_melee_target_id);
    attack_obj["pending_melee_damage"] = attack->pending_melee_damage;
    attack_obj["pending_melee_elapsed"] = attack->pending_melee_elapsed;
    attack_obj["pending_melee_contact_time"] = attack->pending_melee_contact_time;
    entity_obj["attack"] = attack_obj;
  }

  if (const auto* attack_target = entity->get_component<AttackTargetComponent>()) {
    QJsonObject attack_target_obj;
    attack_target_obj["target_id"] = static_cast<qint64>(attack_target->target_id);
    attack_target_obj["should_chase"] = attack_target->should_chase;
    entity_obj["attack_target"] = attack_target_obj;
  }

  if (const auto* commander = entity->get_component<CommanderComponent>()) {
    QJsonObject commander_obj;
    commander_obj["commander_id"] = QString::fromStdString(commander->commander_id);
    commander_obj["display_name"] = QString::fromStdString(commander->display_name);
    commander_obj["strategic_identity"] =
        QString::fromStdString(commander->strategic_identity);
    commander_obj["passive_aura"] = QString::fromStdString(commander->passive_aura);
    commander_obj["bonus_type"] = QString::fromStdString(commander->bonus_type);
    commander_obj["bonus_summary"] = QString::fromStdString(commander->bonus_summary);
    commander_obj["rally_ability"] = QString::fromStdString(commander->rally_ability);
    commander_obj["death_consequence"] =
        QString::fromStdString(commander->death_consequence);
    commander_obj["bodyguard_count"] = commander->bodyguard_count;
    commander_obj["aura_radius"] = commander->aura_radius;
    commander_obj["aura_morale_bonus"] = commander->aura_morale_bonus;
    commander_obj["aura_bonus_value"] = commander->aura_bonus_value;
    commander_obj["rally_range"] = commander->rally_range;
    commander_obj["rally_cooldown"] = commander->rally_cooldown;
    commander_obj["rally_morale_restore"] = commander->rally_morale_restore;
    commander_obj["rally_cooldown_remaining"] = commander->rally_cooldown_remaining;
    commander_obj["rally_feedback_time"] = commander->rally_feedback_time;
    commander_obj["death_shock_radius"] = commander->death_shock_radius;
    commander_obj["death_morale_shock"] = commander->death_morale_shock;
    commander_obj["aura_active"] = commander->aura_active;
    commander_obj["wounded"] = commander->wounded;
    commander_obj["rally_requested"] = commander->rally_requested;
    commander_obj["rally_requires_manual_trigger"] =
        commander->rally_requires_manual_trigger;
    commander_obj["fpv_controlled"] = commander->fpv_controlled;
    commander_obj["flag_rally_cost"] = commander->flag_rally_cost;
    commander_obj["flag_rally_pending_x"] = commander->flag_rally_pending_x;
    commander_obj["flag_rally_pending_z"] = commander->flag_rally_pending_z;
    commander_obj["flag_rally_animation_timer"] = commander->flag_rally_animation_timer;
    commander_obj["flag_rally_in_progress"] = commander->flag_rally_in_progress;
    commander_obj["flag_rally_at_position"] = commander->flag_rally_at_position;
    commander_obj["flag_rally_flag_x"] = commander->flag_rally_flag_x;
    commander_obj["flag_rally_flag_z"] = commander->flag_rally_flag_z;
    commander_obj["flag_rally_flag_active"] = commander->flag_rally_flag_active;
    commander_obj["flag_rally_issue_commands"] = commander->flag_rally_issue_commands;
    entity_obj["commander"] = commander_obj;
  }

  if (const auto* rpg = entity->get_component<RpgHealthComponent>()) {
    QJsonObject rpg_obj;
    rpg_obj["rpg_hp"] = rpg->rpg_hp;
    rpg_obj["rpg_max_hp"] = rpg->rpg_max_hp;
    rpg_obj["armor"] = static_cast<double>(rpg->armor);
    rpg_obj["crit_chance"] = static_cast<double>(rpg->crit_chance);
    rpg_obj["crit_multiplier"] = static_cast<double>(rpg->crit_multiplier);

    entity_obj["rpg_health"] = rpg_obj;
  }

  if (const auto* morale = entity->get_component<MoraleComponent>()) {
    QJsonObject morale_obj;
    morale_obj["morale"] = morale->morale;
    morale_obj["commander_aura_bonus"] = morale->commander_aura_bonus;
    morale_obj["shock_timer"] = morale->shock_timer;
    morale_obj["wavering"] = morale->wavering;
    morale_obj["routing"] = morale->routing;
    entity_obj["morale"] = morale_obj;
  }

  if (const auto* undead = entity->get_component<UndeadComponent>()) {
    QJsonObject undead_obj;
    undead_obj["morale_immune"] = undead->morale_immune;
    undead_obj["fire_damage_multiplier"] =
        static_cast<double>(undead->fire_damage_multiplier);
    undead_obj["priest_damage_multiplier"] =
        static_cast<double>(undead->priest_damage_multiplier);
    undead_obj["cavalry_charge_damage_multiplier"] =
        static_cast<double>(undead->cavalry_charge_damage_multiplier);
    undead_obj["counts_for_economy"] = undead->counts_for_economy;
    entity_obj["undead"] = undead_obj;
  }

  if (const auto* cursed = entity->get_component<CursedStatusComponent>()) {
    QJsonObject cursed_obj;
    cursed_obj["morale_penalty_per_hit"] =
        static_cast<double>(cursed->morale_penalty_per_hit);
    cursed_obj["duration"] = static_cast<double>(cursed->duration);
    cursed_obj["remaining_duration"] = static_cast<double>(cursed->remaining_duration);
    cursed_obj["stacks"] = cursed->stacks;
    entity_obj["cursed_status"] = cursed_obj;
  }

  if (const auto* burning = entity->get_component<BurningStatusComponent>()) {
    QJsonObject burning_obj;
    burning_obj["duration"] = static_cast<double>(burning->duration);
    burning_obj["remaining_duration"] =
        static_cast<double>(burning->remaining_duration);
    burning_obj["ignition_elapsed"] = static_cast<double>(burning->ignition_elapsed);
    burning_obj["tick_interval"] = static_cast<double>(burning->tick_interval);
    burning_obj["tick_accumulator"] = static_cast<double>(burning->tick_accumulator);
    burning_obj["damage_per_tick"] = burning->damage_per_tick;
    burning_obj["attacker_id"] = static_cast<qint64>(burning->attacker_id);
    burning_obj["fire_bonus_multiplier"] =
        static_cast<double>(burning->fire_bonus_multiplier);
    entity_obj["burning_status"] = burning_obj;
  }

  if (const auto* patrol = entity->get_component<PatrolComponent>()) {
    QJsonObject patrol_obj;
    patrol_obj["current_waypoint"] = static_cast<int>(patrol->current_waypoint);
    patrol_obj["patrolling"] = patrol->patrolling;

    QJsonArray waypoints_array;
    for (const auto& waypoint : patrol->waypoints) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      waypoints_array.append(waypoint_obj);
    }
    patrol_obj["waypoints"] = waypoints_array;
    entity_obj["patrol"] = patrol_obj;
  }

  if (entity->get_component<BuildingComponent>() != nullptr) {
    const auto* building = entity->get_component<BuildingComponent>();
    QJsonObject building_obj;
    building_obj["original_nation_id"] =
        Game::Systems::nation_id_to_qstring(building->original_nation_id);
    entity_obj["building"] = building_obj;
  }

  if (const auto* production = entity->get_component<ProductionComponent>()) {
    QJsonObject production_obj;
    production_obj["in_progress"] = production->in_progress;
    production_obj["build_time"] = production->build_time;
    production_obj["time_remaining"] = production->time_remaining;
    production_obj["produced_count"] = production->produced_count;
    production_obj["max_units"] = production->max_units;
    production_obj["product_type"] = QString::fromStdString(
        Game::Units::troop_typeToString(production->product_type));
    production_obj["rally_x"] = production->rally_x;
    production_obj["rally_z"] = production->rally_z;
    production_obj["rally_set"] = production->rally_set;
    production_obj["villager_cost"] = production->villager_cost;
    production_obj["manpower_available"] = production->manpower_available;

    QJsonArray queue_array;
    for (const auto& queued : production->production_queue) {
      queue_array.append(
          QString::fromStdString(Game::Units::troop_typeToString(queued)));
    }
    production_obj["queue"] = queue_array;
    entity_obj["production"] = production_obj;
  }

  if (entity->get_component<AIControlledComponent>() != nullptr) {
    entity_obj["aiControlled"] = true;
  }

  if (const auto* capture = entity->get_component<CaptureComponent>()) {
    QJsonObject capture_obj;
    capture_obj["capturing_player_id"] = capture->capturing_player_id;
    capture_obj["capture_progress"] = static_cast<double>(capture->capture_progress);
    capture_obj["required_time"] = static_cast<double>(capture->required_time);
    capture_obj["is_being_captured"] = capture->is_being_captured;
    entity_obj["capture"] = capture_obj;
  }

  if (const auto* hold_mode = entity->get_component<HoldModeComponent>()) {
    QJsonObject hold_mode_obj;
    hold_mode_obj["active"] = hold_mode->active;
    hold_mode_obj["exit_cooldown"] = static_cast<double>(hold_mode->exit_cooldown);
    hold_mode_obj["stand_up_duration"] =
        static_cast<double>(hold_mode->stand_up_duration);
    hold_mode_obj["kneel_entry_progress"] =
        static_cast<double>(hold_mode->kneel_entry_progress);
    hold_mode_obj["kneel_duration"] = static_cast<double>(hold_mode->kneel_duration);
    entity_obj["hold_mode"] = hold_mode_obj;
  }

  if (const auto* guard_mode = entity->get_component<GuardModeComponent>()) {
    QJsonObject guard_mode_obj;
    guard_mode_obj["active"] = guard_mode->active;
    guard_mode_obj["guarded_entity_id"] =
        static_cast<qint64>(guard_mode->guarded_entity_id);
    guard_mode_obj["guard_position_x"] =
        static_cast<double>(guard_mode->guard_position_x);
    guard_mode_obj["guard_position_z"] =
        static_cast<double>(guard_mode->guard_position_z);
    guard_mode_obj["guard_radius"] = static_cast<double>(guard_mode->guard_radius);
    guard_mode_obj["returning_to_guard_position"] =
        guard_mode->returning_to_guard_position;
    guard_mode_obj["has_guard_target"] = guard_mode->has_guard_target;
    entity_obj["guard_mode"] = guard_mode_obj;
  }

  if (const auto* healer = entity->get_component<HealerComponent>()) {
    QJsonObject healer_obj;
    healer_obj["healing_range"] = static_cast<double>(healer->healing_range);
    healer_obj["healing_amount"] = healer->healing_amount;
    healer_obj["healing_cooldown"] = static_cast<double>(healer->healing_cooldown);
    healer_obj["time_since_last_heal"] =
        static_cast<double>(healer->time_since_last_heal);
    healer_obj["is_healing_active"] = healer->is_healing_active;
    healer_obj["healing_target_x"] = static_cast<double>(healer->healing_target_x);
    healer_obj["healing_target_z"] = static_cast<double>(healer->healing_target_z);
    healer_obj["target_affinity"] = healer_affinity_to_string(healer->target_affinity);
    healer_obj["suppress_attack_while_healing"] = healer->suppress_attack_while_healing;
    entity_obj["healer"] = healer_obj;
  }

  if (const auto* special_attack = entity->get_component<SpecialAttackComponent>()) {
    QJsonObject special_attack_obj;
    special_attack_obj["projectile_kind"] =
        projectile_kind_to_string(special_attack->projectile_kind);
    special_attack_obj["use_projectile_system"] = special_attack->use_projectile_system;
    special_attack_obj["friendly_fire"] = special_attack->friendly_fire;
    special_attack_obj["splash_radius"] =
        static_cast<double>(special_attack->splash_radius);
    special_attack_obj["splash_damage_multiplier"] =
        static_cast<double>(special_attack->splash_damage_multiplier);
    special_attack_obj["bonus_damage_multiplier_vs_fire_vulnerable"] =
        static_cast<double>(special_attack->bonus_damage_multiplier_vs_fire_vulnerable);
    special_attack_obj["cursed_duration"] =
        static_cast<double>(special_attack->cursed_duration);
    special_attack_obj["cursed_morale_penalty_per_hit"] =
        static_cast<double>(special_attack->cursed_morale_penalty_per_hit);
    special_attack_obj["cursed_stacks_per_hit"] = special_attack->cursed_stacks_per_hit;
    special_attack_obj["fire_patch_duration"] =
        static_cast<double>(special_attack->fire_patch_duration);
    special_attack_obj["fire_patch_radius"] =
        static_cast<double>(special_attack->fire_patch_radius);
    special_attack_obj["burn_duration"] =
        static_cast<double>(special_attack->burn_duration);
    special_attack_obj["burn_tick_interval"] =
        static_cast<double>(special_attack->burn_tick_interval);
    special_attack_obj["burn_damage_per_tick"] = special_attack->burn_damage_per_tick;
    entity_obj["special_attack"] = special_attack_obj;
  }

  if (const auto* fire_patch = entity->get_component<FirePatchComponent>()) {
    QJsonObject fire_patch_obj;
    fire_patch_obj["radius"] = static_cast<double>(fire_patch->radius);
    fire_patch_obj["duration"] = static_cast<double>(fire_patch->duration);
    fire_patch_obj["remaining_duration"] =
        static_cast<double>(fire_patch->remaining_duration);
    fire_patch_obj["burn_duration"] = static_cast<double>(fire_patch->burn_duration);
    fire_patch_obj["burn_tick_interval"] =
        static_cast<double>(fire_patch->burn_tick_interval);
    fire_patch_obj["burn_damage_per_tick"] = fire_patch->burn_damage_per_tick;
    fire_patch_obj["attacker_owner_id"] = fire_patch->attacker_owner_id;
    fire_patch_obj["attacker_id"] = static_cast<qint64>(fire_patch->attacker_id);
    fire_patch_obj["friendly_fire"] = fire_patch->friendly_fire;
    fire_patch_obj["fire_bonus_multiplier"] =
        static_cast<double>(fire_patch->fire_bonus_multiplier);
    entity_obj["fire_patch"] = fire_patch_obj;
  }

  if (const auto* commander_guard = entity->get_component<CommanderGuardComponent>()) {
    QJsonObject guard_obj;
    guard_obj["active"] = commander_guard->active;
    guard_obj["frontal_arc_dot"] =
        static_cast<double>(commander_guard->frontal_arc_dot);
    guard_obj["damage_multiplier"] =
        static_cast<double>(commander_guard->damage_multiplier);
    entity_obj["commander_guard"] = guard_obj;
  }

  if (const auto* catapult = entity->get_component<CatapultLoadingComponent>()) {
    QJsonObject catapult_obj;
    catapult_obj["state"] = static_cast<int>(catapult->state);
    catapult_obj["loading_time"] = static_cast<double>(catapult->loading_time);
    catapult_obj["loading_duration"] = static_cast<double>(catapult->loading_duration);
    catapult_obj["firing_time"] = static_cast<double>(catapult->firing_time);
    catapult_obj["firing_duration"] = static_cast<double>(catapult->firing_duration);
    catapult_obj["target_id"] = static_cast<qint64>(catapult->target_id);
    catapult_obj["target_locked_x"] = static_cast<double>(catapult->target_locked_x);
    catapult_obj["target_locked_y"] = static_cast<double>(catapult->target_locked_y);
    catapult_obj["target_locked_z"] = static_cast<double>(catapult->target_locked_z);
    catapult_obj["target_position_locked"] = catapult->target_position_locked;
    entity_obj["catapult_loading"] = catapult_obj;
  }

  if (const auto* elephant = entity->get_component<ElephantComponent>()) {
    QJsonObject elephant_obj;
    elephant_obj["charge_state"] = static_cast<int>(elephant->charge_state);
    elephant_obj["charge_speed_multiplier"] =
        static_cast<double>(elephant->charge_speed_multiplier);
    elephant_obj["charge_duration"] = static_cast<double>(elephant->charge_duration);
    elephant_obj["charge_cooldown"] = static_cast<double>(elephant->charge_cooldown);
    elephant_obj["trample_radius"] = static_cast<double>(elephant->trample_radius);
    elephant_obj["trample_damage"] = elephant->trample_damage;
    elephant_obj["trample_damage_accumulator"] =
        static_cast<double>(elephant->trample_damage_accumulator);
    entity_obj["elephant"] = elephant_obj;
  }

  if (const auto* panic = entity->get_component<ElephantPanicComponent>()) {
    QJsonObject panic_obj;
    panic_obj["duration"] = static_cast<double>(panic->duration);
    entity_obj["elephant_panic"] = panic_obj;
  }

  if (const auto* stomp_impact =
          entity->get_component<ElephantStompImpactComponent>()) {
    QJsonArray impacts_array;
    for (const auto& impact : stomp_impact->impacts) {
      QJsonObject impact_obj;
      impact_obj["x"] = static_cast<double>(impact.x);
      impact_obj["z"] = static_cast<double>(impact.z);
      impact_obj["time"] = static_cast<double>(impact.time);
      impacts_array.append(impact_obj);
    }
    entity_obj["elephant_stomp_impacts"] = impacts_array;
  }

  if (const auto* combat_state = entity->get_component<CombatStateComponent>()) {
    QJsonObject combat_state_obj;
    combat_state_obj["animation_state"] =
        static_cast<int>(combat_state->animation_state);
    combat_state_obj["attack_family"] = static_cast<int>(combat_state->attack_family);
    combat_state_obj["state_time"] = static_cast<double>(combat_state->state_time);
    combat_state_obj["state_duration"] =
        static_cast<double>(combat_state->state_duration);
    combat_state_obj["attack_offset"] =
        static_cast<double>(combat_state->attack_offset);
    combat_state_obj["attack_variant"] = static_cast<int>(combat_state->attack_variant);
    combat_state_obj["finisher_attack"] = combat_state->finisher_attack;
    combat_state_obj["is_hit_paused"] = combat_state->is_hit_paused;
    combat_state_obj["hit_pause_remaining"] =
        static_cast<double>(combat_state->hit_pause_remaining);
    entity_obj["combat_state"] = combat_state_obj;
  }

  if (const auto* hit_feedback = entity->get_component<HitFeedbackComponent>()) {
    QJsonObject hit_feedback_obj;
    hit_feedback_obj["is_reacting"] = hit_feedback->is_reacting;
    hit_feedback_obj["reaction_time"] =
        static_cast<double>(hit_feedback->reaction_time);
    hit_feedback_obj["reaction_intensity"] =
        static_cast<double>(hit_feedback->reaction_intensity);
    hit_feedback_obj["knockback_x"] = static_cast<double>(hit_feedback->knockback_x);
    hit_feedback_obj["knockback_z"] = static_cast<double>(hit_feedback->knockback_z);
    entity_obj["hit_feedback"] = hit_feedback_obj;
  }

  if (const auto* builder = entity->get_component<BuilderProductionComponent>()) {
    QJsonObject builder_obj;
    builder_obj["in_progress"] = builder->in_progress;
    builder_obj["build_time"] = static_cast<double>(builder->build_time);
    builder_obj["time_remaining"] = static_cast<double>(builder->time_remaining);
    builder_obj["product_type"] = QString::fromStdString(builder->product_type);
    builder_obj["construction_complete"] = builder->construction_complete;
    builder_obj["has_construction_site"] = builder->has_construction_site;
    builder_obj["construction_site_x"] =
        static_cast<double>(builder->construction_site_x);
    builder_obj["construction_site_z"] =
        static_cast<double>(builder->construction_site_z);
    builder_obj["construction_site_rotation_y"] =
        static_cast<double>(builder->construction_site_rotation_y);
    builder_obj["has_task_target"] = builder->has_task_target;
    builder_obj["task_target_id"] = static_cast<qint64>(builder->task_target_id);
    builder_obj["task_target_x"] = static_cast<double>(builder->task_target_x);
    builder_obj["task_target_z"] = static_cast<double>(builder->task_target_z);
    builder_obj["task_target_reserved"] = builder->task_target_reserved;
    builder_obj["at_construction_site"] = builder->at_construction_site;
    builder_obj["is_placement_preview"] = builder->is_placement_preview;
    builder_obj["construction_site_entity_id"] =
        static_cast<qint64>(builder->construction_site_entity_id);
    QJsonArray queued_sites;
    for (const auto site_id : builder->queued_construction_site_ids) {
      queued_sites.append(static_cast<qint64>(site_id));
    }
    builder_obj["queued_construction_site_ids"] = queued_sites;
    builder_obj["bypass_movement_active"] = builder->bypass_movement_active;
    builder_obj["bypass_target_x"] = static_cast<double>(builder->bypass_target_x);
    builder_obj["bypass_target_z"] = static_cast<double>(builder->bypass_target_z);
    entity_obj["builder_production"] = builder_obj;
  }

  if (const auto* wall = entity->get_component<WallSegmentComponent>()) {
    QJsonObject wall_obj;
    wall_obj["grid_x"] = wall->grid_x;
    wall_obj["grid_z"] = wall->grid_z;
    entity_obj["wall_segment"] = wall_obj;
  }

  if (const auto* site = entity->get_component<WallConstructionSiteComponent>()) {
    QJsonObject site_obj;
    site_obj["owner_id"] = site->owner_id;
    site_obj["nation_id"] = static_cast<int>(site->nation_id);
    site_obj["build_time"] = static_cast<double>(site->build_time);
    site_obj["progress"] = static_cast<double>(site->progress);
    entity_obj["wall_construction_site"] = site_obj;
  }

  if (const auto* formation = entity->get_component<FormationModeComponent>()) {
    QJsonObject formation_obj;
    formation_obj["active"] = formation->active;
    formation_obj["formation_center_x"] =
        static_cast<double>(formation->formation_center_x);
    formation_obj["formation_center_z"] =
        static_cast<double>(formation->formation_center_z);
    entity_obj["formation_mode"] = formation_obj;
  }

  if (const auto* stamina = entity->get_component<StaminaComponent>()) {
    QJsonObject stamina_obj;
    stamina_obj["stamina"] = static_cast<double>(stamina->stamina);
    stamina_obj["max_stamina"] = static_cast<double>(stamina->max_stamina);
    stamina_obj["regen_rate"] = static_cast<double>(stamina->regen_rate);
    stamina_obj["depletion_rate"] = static_cast<double>(stamina->depletion_rate);
    stamina_obj["is_running"] = stamina->is_running;
    stamina_obj["run_requested"] = stamina->run_requested;
    entity_obj["stamina"] = stamina_obj;
  }

  if (const auto* terrain_context = entity->get_component<TerrainContextComponent>()) {
    QJsonObject terrain_context_obj;
    terrain_context_obj["is_on_bridge"] = terrain_context->is_on_bridge;
    terrain_context_obj["is_at_hill_entrance"] = terrain_context->is_at_hill_entrance;
    terrain_context_obj["audio_cooldown"] =
        static_cast<double>(terrain_context->audio_cooldown);
    entity_obj["terrain_context"] = terrain_context_obj;
  }

  if (const auto* home = entity->get_component<HomeComponent>()) {
    QJsonObject home_obj;
    home_obj["population_contribution"] = home->population_contribution;
    home_obj["nearest_barracks_id"] = static_cast<qint64>(home->nearest_barracks_id);
    home_obj["update_cooldown"] = static_cast<double>(home->update_cooldown);
    home_obj["family_generation_cooldown"] =
        static_cast<double>(home->family_generation_cooldown);
    home_obj["family_generation_interval"] =
        static_cast<double>(home->family_generation_interval);
    home_obj["family_manpower_value"] = home->family_manpower_value;
    entity_obj["home"] = home_obj;
  }

  if (const auto* delivery = entity->get_component<CivilianDeliveryComponent>()) {
    QJsonObject delivery_obj;
    delivery_obj["target_barracks_id"] =
        static_cast<qint64>(delivery->target_barracks_id);
    entity_obj["civilian_delivery"] = delivery_obj;
  }

  return entity_obj;
}

void Serialization::deserialize_entity(Entity* entity, const QJsonObject& json) {
  if (json.contains("transform")) {
    const auto transform_obj = json["transform"].toObject();
    auto* transform = entity->add_component<TransformComponent>();
    transform->position.x = static_cast<float>(transform_obj["pos_x"].toDouble());
    transform->position.y = static_cast<float>(transform_obj["pos_y"].toDouble());
    transform->position.z = static_cast<float>(transform_obj["pos_z"].toDouble());
    transform->rotation.x = static_cast<float>(transform_obj["rot_x"].toDouble());
    transform->rotation.y = static_cast<float>(transform_obj["rot_y"].toDouble());
    transform->rotation.z = static_cast<float>(transform_obj["rot_z"].toDouble());
    transform->scale.x = static_cast<float>(transform_obj["scale_x"].toDouble());
    transform->scale.y = static_cast<float>(transform_obj["scale_y"].toDouble());
    transform->scale.z = static_cast<float>(transform_obj["scale_z"].toDouble());
    transform->has_desired_yaw = transform_obj["has_desired_yaw"].toBool(false);
    transform->desired_yaw =
        static_cast<float>(transform_obj["desired_yaw"].toDouble());
  }

  if (json.contains("renderable")) {
    const auto renderable_obj = json["renderable"].toObject();
    auto* renderable = entity->add_component<RenderableComponent>("", "");
    renderable->mesh_path = renderable_obj["mesh_path"].toString().toStdString();
    renderable->texture_path = renderable_obj["texture_path"].toString().toStdString();
    renderable->renderer_id = renderable_obj["renderer_id"].toString().toStdString();
    renderable->visible = renderable_obj["visible"].toBool(true);
    renderable->mesh =
        static_cast<RenderableComponent::MeshKind>(renderable_obj["mesh"].toInt(
            static_cast<int>(RenderableComponent::MeshKind::Cube)));
    if (renderable_obj.contains("color")) {
      deserialize_color(renderable_obj["color"].toArray(), renderable->color);
    }
  }

  if (json.contains("unit")) {
    const auto unit_obj = json["unit"].toObject();
    auto* unit = entity->add_component<UnitComponent>();
    unit->health = unit_obj["health"].toInt(Defaults::k_unit_default_health);
    unit->max_health = unit_obj["max_health"].toInt(Defaults::k_unit_default_health);
    unit->speed = static_cast<float>(unit_obj["speed"].toDouble());
    unit->vision_range = static_cast<float>(unit_obj["vision_range"].toDouble(
        static_cast<double>(Defaults::k_unit_default_vision_range)));

    QString const unit_type_str = unit_obj["unit_type"].toString();
    Game::Units::SpawnType spawn_type;
    if (Game::Units::try_parse_spawn_type(unit_type_str, spawn_type)) {
      unit->spawn_type = spawn_type;
    } else {
      qWarning() << "Unknown spawn type in save file:" << unit_type_str
                 << "- defaulting to Archer";
      unit->spawn_type = Game::Units::SpawnType::Archer;
    }

    unit->owner_id = unit_obj["owner_id"].toInt(0);
    if (unit_obj.contains("nation_id")) {
      const QString nation_str = unit_obj["nation_id"].toString();
      Game::Systems::NationID nation_id;
      if (Game::Systems::try_parse_nation_id(nation_str, nation_id)) {
        unit->nation_id = nation_id;
      } else {
        qWarning() << "Unknown nation ID in save file:" << nation_str
                   << "- using default";
        unit->nation_id = Game::Systems::NationID::RomanRepublic;
      }
    }
    unit->render_individuals_per_unit_override =
        unit_obj["render_individuals_per_unit_override"].toInt(0);
    unit->render_rider = unit_obj["render_rider"].toBool(true);
    unit->death_sequence_override =
        static_cast<std::uint8_t>(unit_obj["death_sequence_override"].toInt(0xFF));
  }

  if (json.contains("movement")) {
    const auto movement_obj = json["movement"].toObject();
    auto* movement = entity->add_component<MovementComponent>();
    movement->has_target = movement_obj["has_target"].toBool(false);
    movement->target_x = static_cast<float>(movement_obj["target_x"].toDouble());
    movement->target_y = static_cast<float>(movement_obj["target_y"].toDouble());
    movement->goal_x = static_cast<float>(movement_obj["goal_x"].toDouble());
    movement->goal_y = static_cast<float>(movement_obj["goal_y"].toDouble());
    movement->vx = static_cast<float>(movement_obj["vx"].toDouble());
    movement->vz = static_cast<float>(movement_obj["vz"].toDouble());
    movement->path_pending = movement_obj["path_pending"].toBool(false);
    movement->pending_request_id = static_cast<std::uint64_t>(
        movement_obj["pending_request_id"].toVariant().toULongLong());
    movement->repath_cooldown =
        static_cast<float>(movement_obj["repath_cooldown"].toDouble());
    movement->last_goal_x = static_cast<float>(movement_obj["last_goal_x"].toDouble());
    movement->last_goal_y = static_cast<float>(movement_obj["last_goal_y"].toDouble());
    movement->time_since_last_path_request =
        static_cast<float>(movement_obj["time_since_last_path_request"].toDouble());

    movement->clear_path();
    const auto path_array = movement_obj["path"].toArray();
    movement->path.reserve(path_array.size());
    for (const auto& value : path_array) {
      const auto waypoint_obj = value.toObject();
      movement->path.emplace_back(static_cast<float>(waypoint_obj["x"].toDouble()),
                                  static_cast<float>(waypoint_obj["y"].toDouble()));
    }

    if (movement_obj.contains("path_index")) {
      movement->path_index =
          static_cast<std::size_t>(movement_obj["path_index"].toInt());
    }

    movement->validate_path_index();
    movement->last_position_x =
        static_cast<float>(movement_obj["last_position_x"].toDouble(0.0));
    movement->last_position_z =
        static_cast<float>(movement_obj["last_position_z"].toDouble(0.0));
    movement->time_stuck = static_cast<float>(movement_obj["time_stuck"].toDouble(0.0));
    movement->unstuck_cooldown =
        static_cast<float>(movement_obj["unstuck_cooldown"].toDouble(0.0));
  }

  if (json.contains("player_order_intent")) {
    const auto intent_obj = json["player_order_intent"].toObject();
    auto* intent = entity->add_component<PlayerOrderIntentComponent>();
    intent->kind = static_cast<PlayerOrderIntentKind>(
        intent_obj["kind"].toInt(static_cast<int>(PlayerOrderIntentKind::None)));
    intent->suppress_opportunistic_combat =
        intent_obj["suppress_opportunistic_combat"].toBool(false);
  }

  if (json.contains("attack")) {
    const auto attack_obj = json["attack"].toObject();
    auto* attack = entity->add_component<AttackComponent>();
    attack->range = static_cast<float>(attack_obj["range"].toDouble());
    attack->damage = attack_obj["damage"].toInt(0);
    attack->cooldown = static_cast<float>(attack_obj["cooldown"].toDouble());
    attack->time_since_last =
        static_cast<float>(attack_obj["time_since_last"].toDouble());
    attack->melee_range = static_cast<float>(attack_obj["melee_range"].toDouble(
        static_cast<double>(Defaults::k_attack_melee_range)));
    attack->melee_damage = attack_obj["melee_damage"].toInt(0);
    attack->melee_cooldown =
        static_cast<float>(attack_obj["melee_cooldown"].toDouble());
    attack->preferred_mode =
        combat_mode_from_string(attack_obj["preferred_mode"].toString());
    attack->current_mode =
        combat_mode_from_string(attack_obj["current_mode"].toString());
    attack->can_melee = attack_obj["can_melee"].toBool(true);
    attack->can_ranged = attack_obj["can_ranged"].toBool(false);
    attack->max_height_difference =
        static_cast<float>(attack_obj["max_height_difference"].toDouble(
            static_cast<double>(Defaults::k_attack_height_tolerance)));
    attack->in_melee_lock = attack_obj["in_melee_lock"].toBool(false);
    attack->melee_lock_target_id = static_cast<EntityID>(
        attack_obj["melee_lock_target_id"].toVariant().toULongLong());
    attack->has_pending_melee_strike =
        attack_obj["has_pending_melee_strike"].toBool(false);
    attack->pending_melee_target_id = static_cast<EntityID>(
        attack_obj["pending_melee_target_id"].toVariant().toULongLong());
    attack->pending_melee_damage = attack_obj["pending_melee_damage"].toInt(0);
    attack->pending_melee_elapsed =
        static_cast<float>(attack_obj["pending_melee_elapsed"].toDouble());
    attack->pending_melee_contact_time =
        static_cast<float>(attack_obj["pending_melee_contact_time"].toDouble());
  }

  if (json.contains("attack_target")) {
    const auto attack_target_obj = json["attack_target"].toObject();
    auto* attack_target = entity->add_component<AttackTargetComponent>();
    attack_target->target_id =
        static_cast<EntityID>(attack_target_obj["target_id"].toVariant().toULongLong());
    attack_target->should_chase = attack_target_obj["should_chase"].toBool(false);
  }

  if (json.contains("commander")) {
    const auto commander_obj = json["commander"].toObject();
    auto* commander = entity->add_component<CommanderComponent>();
    commander->commander_id = commander_obj["commander_id"].toString().toStdString();
    commander->display_name = commander_obj["display_name"].toString().toStdString();
    commander->strategic_identity =
        commander_obj["strategic_identity"].toString().toStdString();
    commander->passive_aura = commander_obj["passive_aura"].toString().toStdString();
    commander->bonus_type = commander_obj["bonus_type"].toString().toStdString();
    commander->bonus_summary = commander_obj["bonus_summary"].toString().toStdString();
    commander->rally_ability = commander_obj["rally_ability"].toString().toStdString();
    commander->death_consequence =
        commander_obj["death_consequence"].toString().toStdString();
    commander->bodyguard_count =
        commander_obj["bodyguard_count"].toInt(commander->bodyguard_count);
    commander->aura_radius = static_cast<float>(
        commander_obj["aura_radius"].toDouble(commander->aura_radius));
    commander->aura_morale_bonus = static_cast<float>(
        commander_obj["aura_morale_bonus"].toDouble(commander->aura_morale_bonus));
    commander->aura_bonus_value = static_cast<float>(
        commander_obj["aura_bonus_value"].toDouble(commander->aura_bonus_value));
    commander->rally_range = static_cast<float>(
        commander_obj["rally_range"].toDouble(commander->rally_range));
    commander->rally_cooldown = static_cast<float>(
        commander_obj["rally_cooldown"].toDouble(commander->rally_cooldown));
    commander->rally_morale_restore =
        static_cast<float>(commander_obj["rally_morale_restore"].toDouble(
            commander->rally_morale_restore));
    commander->rally_cooldown_remaining =
        static_cast<float>(commander_obj["rally_cooldown_remaining"].toDouble(
            commander->rally_cooldown_remaining));
    commander->rally_feedback_time = static_cast<float>(
        commander_obj["rally_feedback_time"].toDouble(commander->rally_feedback_time));
    commander->death_shock_radius = static_cast<float>(
        commander_obj["death_shock_radius"].toDouble(commander->death_shock_radius));
    commander->death_morale_shock = static_cast<float>(
        commander_obj["death_morale_shock"].toDouble(commander->death_morale_shock));
    commander->aura_active =
        commander_obj["aura_active"].toBool(commander->aura_active);
    commander->wounded = commander_obj["wounded"].toBool(commander->wounded);
    commander->rally_requested = commander_obj["rally_requested"].toBool(false);
    commander->rally_requires_manual_trigger =
        commander_obj["rally_requires_manual_trigger"].toBool(false);
    commander->fpv_controlled = commander_obj["fpv_controlled"].toBool(false);
    commander->flag_rally_cost = static_cast<float>(
        commander_obj["flag_rally_cost"].toDouble(commander->flag_rally_cost));
    commander->flag_rally_pending_x =
        static_cast<float>(commander_obj["flag_rally_pending_x"].toDouble(
            commander->flag_rally_pending_x));
    commander->flag_rally_pending_z =
        static_cast<float>(commander_obj["flag_rally_pending_z"].toDouble(
            commander->flag_rally_pending_z));
    commander->flag_rally_animation_timer =
        static_cast<float>(commander_obj["flag_rally_animation_timer"].toDouble(
            commander->flag_rally_animation_timer));
    commander->flag_rally_in_progress =
        commander_obj["flag_rally_in_progress"].toBool(false);
    commander->flag_rally_at_position =
        commander_obj["flag_rally_at_position"].toBool(false);
    commander->flag_rally_flag_x = static_cast<float>(
        commander_obj["flag_rally_flag_x"].toDouble(commander->flag_rally_flag_x));
    commander->flag_rally_flag_z = static_cast<float>(
        commander_obj["flag_rally_flag_z"].toDouble(commander->flag_rally_flag_z));
    commander->flag_rally_flag_active =
        commander_obj["flag_rally_flag_active"].toBool(false);
    commander->flag_rally_issue_commands =
        commander_obj["flag_rally_issue_commands"].toBool(false);
  }

  if (json.contains("rpg_health")) {
    const auto rpg_obj = json["rpg_health"].toObject();
    auto* rpg = entity->add_component<RpgHealthComponent>();
    rpg->rpg_hp = rpg_obj["rpg_hp"].toInt(rpg->rpg_hp);
    rpg->rpg_max_hp = rpg_obj["rpg_max_hp"].toInt(rpg->rpg_max_hp);
    rpg->armor = static_cast<float>(rpg_obj["armor"].toDouble(rpg->armor));
    rpg->crit_chance =
        static_cast<float>(rpg_obj["crit_chance"].toDouble(rpg->crit_chance));
    rpg->crit_multiplier =
        static_cast<float>(rpg_obj["crit_multiplier"].toDouble(rpg->crit_multiplier));
    rpg->active = false;
  }

  if (json.contains("morale")) {
    const auto morale_obj = json["morale"].toObject();
    auto* morale = entity->add_component<MoraleComponent>();
    morale->morale = static_cast<float>(morale_obj["morale"].toDouble(morale->morale));
    morale->commander_aura_bonus = static_cast<float>(
        morale_obj["commander_aura_bonus"].toDouble(morale->commander_aura_bonus));
    morale->shock_timer =
        static_cast<float>(morale_obj["shock_timer"].toDouble(morale->shock_timer));
    morale->wavering = morale_obj["wavering"].toBool(morale->wavering);
    morale->routing = morale_obj["routing"].toBool(morale->routing);
  }

  if (json.contains("undead")) {
    const auto undead_obj = json["undead"].toObject();
    auto* undead = entity->add_component<UndeadComponent>();
    undead->morale_immune = undead_obj["morale_immune"].toBool(undead->morale_immune);
    undead->fire_damage_multiplier = static_cast<float>(
        undead_obj["fire_damage_multiplier"].toDouble(undead->fire_damage_multiplier));
    undead->priest_damage_multiplier =
        static_cast<float>(undead_obj["priest_damage_multiplier"].toDouble(
            undead->priest_damage_multiplier));
    undead->cavalry_charge_damage_multiplier =
        static_cast<float>(undead_obj["cavalry_charge_damage_multiplier"].toDouble(
            undead->cavalry_charge_damage_multiplier));
    undead->counts_for_economy =
        undead_obj["counts_for_economy"].toBool(undead->counts_for_economy);
  }

  if (json.contains("cursed_status")) {
    const auto cursed_obj = json["cursed_status"].toObject();
    auto* cursed = entity->add_component<CursedStatusComponent>();
    cursed->morale_penalty_per_hit = static_cast<float>(
        cursed_obj["morale_penalty_per_hit"].toDouble(cursed->morale_penalty_per_hit));
    cursed->duration =
        static_cast<float>(cursed_obj["duration"].toDouble(cursed->duration));
    cursed->remaining_duration = static_cast<float>(
        cursed_obj["remaining_duration"].toDouble(cursed->remaining_duration));
    cursed->stacks = cursed_obj["stacks"].toInt(cursed->stacks);
  }

  if (json.contains("burning_status")) {
    const auto burning_obj = json["burning_status"].toObject();
    auto* burning = entity->add_component<BurningStatusComponent>();
    burning->duration =
        static_cast<float>(burning_obj["duration"].toDouble(burning->duration));
    burning->remaining_duration = static_cast<float>(
        burning_obj["remaining_duration"].toDouble(burning->remaining_duration));
    burning->ignition_elapsed = static_cast<float>(
        burning_obj["ignition_elapsed"].toDouble(burning->ignition_elapsed));
    burning->tick_interval = static_cast<float>(
        burning_obj["tick_interval"].toDouble(burning->tick_interval));
    burning->tick_accumulator = static_cast<float>(
        burning_obj["tick_accumulator"].toDouble(burning->tick_accumulator));
    burning->damage_per_tick =
        burning_obj["damage_per_tick"].toInt(burning->damage_per_tick);
    burning->attacker_id =
        static_cast<EntityID>(burning_obj["attacker_id"].toVariant().toULongLong());
    burning->fire_bonus_multiplier = static_cast<float>(
        burning_obj["fire_bonus_multiplier"].toDouble(burning->fire_bonus_multiplier));
  }

  if (json.contains("patrol")) {
    const auto patrol_obj = json["patrol"].toObject();
    auto* patrol = entity->add_component<PatrolComponent>();
    patrol->current_waypoint =
        static_cast<size_t>(std::max(0, patrol_obj["current_waypoint"].toInt()));
    patrol->patrolling = patrol_obj["patrolling"].toBool(false);

    patrol->waypoints.clear();
    const auto waypoints_array = patrol_obj["waypoints"].toArray();
    patrol->waypoints.reserve(waypoints_array.size());
    for (const auto& value : waypoints_array) {
      const auto waypoint_obj = value.toObject();
      patrol->waypoints.emplace_back(static_cast<float>(waypoint_obj["x"].toDouble()),
                                     static_cast<float>(waypoint_obj["y"].toDouble()));
    }
  }

  if (json.contains("building")) {
    auto* building = entity->add_component<BuildingComponent>();
    if (json["building"].isObject()) {
      const auto building_obj = json["building"].toObject();
      if (building_obj.contains("original_nation_id")) {
        const QString nation_str = building_obj["original_nation_id"].toString();
        Game::Systems::NationID nation_id;
        if (Game::Systems::try_parse_nation_id(nation_str, nation_id)) {
          building->original_nation_id = nation_id;
        }
      }
    }
  }

  if (json.contains("production")) {
    const auto production_obj = json["production"].toObject();
    auto* production = entity->add_component<ProductionComponent>();
    production->in_progress = production_obj["in_progress"].toBool(false);
    production->build_time =
        static_cast<float>(production_obj["build_time"].toDouble());
    production->time_remaining =
        static_cast<float>(production_obj["time_remaining"].toDouble());
    production->produced_count = production_obj["produced_count"].toInt(0);
    production->max_units = production_obj["max_units"].toInt(0);
    production->product_type = Game::Units::troop_typeFromString(
        production_obj["product_type"].toString().toStdString());
    production->rally_x = static_cast<float>(production_obj["rally_x"].toDouble());
    production->rally_z = static_cast<float>(production_obj["rally_z"].toDouble());
    production->rally_set = production_obj["rally_set"].toBool(false);
    production->villager_cost = production_obj["villager_cost"].toInt(1);
    production->manpower_available = production_obj["manpower_available"].toInt(0);

    production->production_queue.clear();
    const auto queue_array = production_obj["queue"].toArray();
    production->production_queue.reserve(queue_array.size());
    for (const auto& value : queue_array) {
      production->production_queue.push_back(
          Game::Units::troop_typeFromString(value.toString().toStdString()));
    }
  }

  if (json.contains("aiControlled") && json["aiControlled"].toBool()) {
    entity->add_component<AIControlledComponent>();
  }

  if (json.contains("capture")) {
    const auto capture_obj = json["capture"].toObject();
    auto* capture = entity->add_component<CaptureComponent>();
    capture->capturing_player_id = capture_obj["capturing_player_id"].toInt(-1);
    capture->capture_progress =
        static_cast<float>(capture_obj["capture_progress"].toDouble(0.0));
    capture->required_time = static_cast<float>(capture_obj["required_time"].toDouble(
        static_cast<double>(Defaults::k_capture_required_time)));
    capture->is_being_captured = capture_obj["is_being_captured"].toBool(false);
  }

  if (json.contains("hold_mode")) {
    const auto hold_mode_obj = json["hold_mode"].toObject();
    auto* hold_mode = entity->add_component<HoldModeComponent>();
    hold_mode->active = hold_mode_obj["active"].toBool(true);
    hold_mode->exit_cooldown =
        static_cast<float>(hold_mode_obj["exit_cooldown"].toDouble(0.0));
    hold_mode->stand_up_duration =
        static_cast<float>(hold_mode_obj["stand_up_duration"].toDouble(
            static_cast<double>(Defaults::k_hold_stand_up_duration)));
    hold_mode->kneel_entry_progress =
        static_cast<float>(hold_mode_obj["kneel_entry_progress"].toDouble(0.0));
    hold_mode->kneel_duration =
        static_cast<float>(hold_mode_obj["kneel_duration"].toDouble(
            static_cast<double>(Defaults::k_hold_kneel_duration)));
  }

  if (json.contains("guard_mode")) {
    const auto guard_mode_obj = json["guard_mode"].toObject();
    auto* guard_mode = entity->add_component<GuardModeComponent>();
    guard_mode->active = guard_mode_obj["active"].toBool(true);
    guard_mode->guarded_entity_id = static_cast<EntityID>(
        guard_mode_obj["guarded_entity_id"].toVariant().toULongLong());
    guard_mode->guard_position_x =
        static_cast<float>(guard_mode_obj["guard_position_x"].toDouble(0.0));
    guard_mode->guard_position_z =
        static_cast<float>(guard_mode_obj["guard_position_z"].toDouble(0.0));
    guard_mode->guard_radius =
        static_cast<float>(guard_mode_obj["guard_radius"].toDouble(
            static_cast<double>(Defaults::k_guard_default_radius)));
    guard_mode->returning_to_guard_position =
        guard_mode_obj["returning_to_guard_position"].toBool(false);
    guard_mode->has_guard_target = guard_mode_obj["has_guard_target"].toBool(false);
  }

  if (json.contains("healer")) {
    const auto healer_obj = json["healer"].toObject();
    auto* healer = entity->add_component<HealerComponent>();
    healer->healing_range =
        static_cast<float>(healer_obj["healing_range"].toDouble(8.0));
    healer->healing_amount = healer_obj["healing_amount"].toInt(5);
    healer->healing_cooldown =
        static_cast<float>(healer_obj["healing_cooldown"].toDouble(2.0));
    healer->time_since_last_heal =
        static_cast<float>(healer_obj["time_since_last_heal"].toDouble(0.0));
    healer->is_healing_active = healer_obj["is_healing_active"].toBool(false);
    healer->healing_target_x =
        static_cast<float>(healer_obj["healing_target_x"].toDouble(0.0));
    healer->healing_target_z =
        static_cast<float>(healer_obj["healing_target_z"].toDouble(0.0));
    healer->target_affinity =
        healer_affinity_from_string(healer_obj["target_affinity"].toString());
    healer->suppress_attack_while_healing =
        healer_obj["suppress_attack_while_healing"].toBool(
            healer->suppress_attack_while_healing);
  }

  if (json.contains("special_attack")) {
    const auto special_attack_obj = json["special_attack"].toObject();
    auto* special_attack = entity->add_component<SpecialAttackComponent>();
    special_attack->projectile_kind =
        projectile_kind_from_string(special_attack_obj["projectile_kind"].toString());
    special_attack->use_projectile_system =
        special_attack_obj["use_projectile_system"].toBool(false);
    special_attack->friendly_fire = special_attack_obj["friendly_fire"].toBool(false);
    special_attack->splash_radius = static_cast<float>(
        special_attack_obj["splash_radius"].toDouble(special_attack->splash_radius));
    special_attack->splash_damage_multiplier =
        static_cast<float>(special_attack_obj["splash_damage_multiplier"].toDouble(
            special_attack->splash_damage_multiplier));
    special_attack->bonus_damage_multiplier_vs_fire_vulnerable = static_cast<float>(
        special_attack_obj["bonus_damage_multiplier_vs_fire_vulnerable"].toDouble(
            special_attack->bonus_damage_multiplier_vs_fire_vulnerable));
    special_attack->cursed_duration =
        static_cast<float>(special_attack_obj["cursed_duration"].toDouble(
            special_attack->cursed_duration));
    special_attack->cursed_morale_penalty_per_hit =
        static_cast<float>(special_attack_obj["cursed_morale_penalty_per_hit"].toDouble(
            special_attack->cursed_morale_penalty_per_hit));
    special_attack->cursed_stacks_per_hit =
        special_attack_obj["cursed_stacks_per_hit"].toInt(
            special_attack->cursed_stacks_per_hit);
    special_attack->fire_patch_duration =
        static_cast<float>(special_attack_obj["fire_patch_duration"].toDouble(
            special_attack->fire_patch_duration));
    special_attack->fire_patch_radius =
        static_cast<float>(special_attack_obj["fire_patch_radius"].toDouble(
            special_attack->fire_patch_radius));
    special_attack->burn_duration = static_cast<float>(
        special_attack_obj["burn_duration"].toDouble(special_attack->burn_duration));
    special_attack->burn_tick_interval =
        static_cast<float>(special_attack_obj["burn_tick_interval"].toDouble(
            special_attack->burn_tick_interval));
    special_attack->burn_damage_per_tick =
        special_attack_obj["burn_damage_per_tick"].toInt(
            special_attack->burn_damage_per_tick);
  }

  if (json.contains("fire_patch")) {
    const auto fire_patch_obj = json["fire_patch"].toObject();
    auto* fire_patch = entity->add_component<FirePatchComponent>();
    fire_patch->radius =
        static_cast<float>(fire_patch_obj["radius"].toDouble(fire_patch->radius));
    fire_patch->duration =
        static_cast<float>(fire_patch_obj["duration"].toDouble(fire_patch->duration));
    fire_patch->remaining_duration = static_cast<float>(
        fire_patch_obj["remaining_duration"].toDouble(fire_patch->remaining_duration));
    fire_patch->burn_duration = static_cast<float>(
        fire_patch_obj["burn_duration"].toDouble(fire_patch->burn_duration));
    fire_patch->burn_tick_interval = static_cast<float>(
        fire_patch_obj["burn_tick_interval"].toDouble(fire_patch->burn_tick_interval));
    fire_patch->burn_damage_per_tick =
        fire_patch_obj["burn_damage_per_tick"].toInt(fire_patch->burn_damage_per_tick);
    fire_patch->attacker_owner_id =
        fire_patch_obj["attacker_owner_id"].toInt(fire_patch->attacker_owner_id);
    fire_patch->attacker_id =
        static_cast<EntityID>(fire_patch_obj["attacker_id"].toVariant().toULongLong());
    fire_patch->friendly_fire =
        fire_patch_obj["friendly_fire"].toBool(fire_patch->friendly_fire);
    fire_patch->fire_bonus_multiplier =
        static_cast<float>(fire_patch_obj["fire_bonus_multiplier"].toDouble(
            fire_patch->fire_bonus_multiplier));
  }

  if (json.contains("commander_guard")) {
    const auto guard_obj = json["commander_guard"].toObject();
    auto* commander_guard = entity->add_component<CommanderGuardComponent>();
    commander_guard->active = guard_obj["active"].toBool(false);
    commander_guard->frontal_arc_dot =
        static_cast<float>(guard_obj["frontal_arc_dot"].toDouble(0.15));
    commander_guard->damage_multiplier =
        static_cast<float>(guard_obj["damage_multiplier"].toDouble(0.45));
  }

  if (json.contains("catapult_loading")) {
    const auto catapult_obj = json["catapult_loading"].toObject();
    auto* catapult = entity->add_component<CatapultLoadingComponent>();
    catapult->state =
        static_cast<CatapultLoadingComponent::LoadingState>(catapult_obj["state"].toInt(
            static_cast<int>(CatapultLoadingComponent::LoadingState::Idle)));
    catapult->loading_time =
        static_cast<float>(catapult_obj["loading_time"].toDouble(0.0));
    catapult->loading_duration =
        static_cast<float>(catapult_obj["loading_duration"].toDouble(2.0));
    catapult->firing_time =
        static_cast<float>(catapult_obj["firing_time"].toDouble(0.0));
    catapult->firing_duration =
        static_cast<float>(catapult_obj["firing_duration"].toDouble(0.5));
    catapult->target_id =
        static_cast<EntityID>(catapult_obj["target_id"].toVariant().toULongLong());
    catapult->target_locked_x =
        static_cast<float>(catapult_obj["target_locked_x"].toDouble(0.0));
    catapult->target_locked_y =
        static_cast<float>(catapult_obj["target_locked_y"].toDouble(0.0));
    catapult->target_locked_z =
        static_cast<float>(catapult_obj["target_locked_z"].toDouble(0.0));
    catapult->target_position_locked =
        catapult_obj["target_position_locked"].toBool(false);
  }

  if (json.contains("elephant")) {
    const auto elephant_obj = json["elephant"].toObject();
    auto* elephant = entity->add_component<ElephantComponent>();
    elephant->charge_state =
        static_cast<ElephantComponent::ChargeState>(elephant_obj["charge_state"].toInt(
            static_cast<int>(ElephantComponent::ChargeState::Idle)));
    elephant->charge_speed_multiplier =
        static_cast<float>(elephant_obj["charge_speed_multiplier"].toDouble(1.8));
    elephant->charge_duration =
        static_cast<float>(elephant_obj["charge_duration"].toDouble(0.0));
    elephant->charge_cooldown =
        static_cast<float>(elephant_obj["charge_cooldown"].toDouble(0.0));
    elephant->trample_radius =
        static_cast<float>(elephant_obj["trample_radius"].toDouble(2.5));
    elephant->trample_damage = elephant_obj["trample_damage"].toInt(40);
    elephant->trample_damage_accumulator =
        static_cast<float>(elephant_obj["trample_damage_accumulator"].toDouble(0.0));
    if (elephant_obj["is_panicked"].toBool(false)) {
      auto* panic = entity->add_component<ElephantPanicComponent>();
      panic->duration =
          static_cast<float>(elephant_obj["panic_duration"].toDouble(0.0));
    }
  }

  if (json.contains("elephant_panic")) {
    const auto panic_obj = json["elephant_panic"].toObject();
    auto* panic = entity->get_component<ElephantPanicComponent>();
    if (panic == nullptr) {
      panic = entity->add_component<ElephantPanicComponent>();
    }
    panic->duration = static_cast<float>(panic_obj["duration"].toDouble(0.0));
  }

  if (json.contains("elephant_stomp_impacts")) {
    const auto impacts_array = json["elephant_stomp_impacts"].toArray();
    auto* stomp_impact = entity->add_component<ElephantStompImpactComponent>();
    stomp_impact->impacts.clear();
    stomp_impact->impacts.reserve(impacts_array.size());
    for (const auto& value : impacts_array) {
      const auto impact_obj = value.toObject();
      ElephantStompImpactComponent::ImpactRecord impact{};
      impact.x = static_cast<float>(impact_obj["x"].toDouble(0.0));
      impact.z = static_cast<float>(impact_obj["z"].toDouble(0.0));
      impact.time = static_cast<float>(impact_obj["time"].toDouble(0.0));
      stomp_impact->impacts.push_back(impact);
    }
  }

  if (json.contains("combat_state")) {
    const auto combat_state_obj = json["combat_state"].toObject();
    auto* combat_state = entity->add_component<CombatStateComponent>();
    combat_state->animation_state =
        static_cast<CombatAnimationState>(combat_state_obj["animation_state"].toInt(
            static_cast<int>(CombatAnimationState::Idle)));
    combat_state->attack_family =
        static_cast<CombatAttackFamily>(combat_state_obj["attack_family"].toInt(
            static_cast<int>(CombatAttackFamily::None)));
    combat_state->state_time =
        static_cast<float>(combat_state_obj["state_time"].toDouble(0.0));
    combat_state->state_duration =
        static_cast<float>(combat_state_obj["state_duration"].toDouble(0.0));
    combat_state->attack_offset =
        static_cast<float>(combat_state_obj["attack_offset"].toDouble(0.0));
    combat_state->attack_variant =
        static_cast<std::uint8_t>(combat_state_obj["attack_variant"].toInt(0));
    combat_state->finisher_attack = combat_state_obj["finisher_attack"].toBool(false);
    combat_state->is_hit_paused = combat_state_obj["is_hit_paused"].toBool(false);
    combat_state->hit_pause_remaining =
        static_cast<float>(combat_state_obj["hit_pause_remaining"].toDouble(0.0));
  }

  if (json.contains("hit_feedback")) {
    const auto hit_feedback_obj = json["hit_feedback"].toObject();
    auto* hit_feedback = entity->add_component<HitFeedbackComponent>();
    hit_feedback->is_reacting = hit_feedback_obj["is_reacting"].toBool(false);
    hit_feedback->reaction_time =
        static_cast<float>(hit_feedback_obj["reaction_time"].toDouble(0.0));
    hit_feedback->reaction_intensity =
        static_cast<float>(hit_feedback_obj["reaction_intensity"].toDouble(0.0));
    hit_feedback->knockback_x =
        static_cast<float>(hit_feedback_obj["knockback_x"].toDouble(0.0));
    hit_feedback->knockback_z =
        static_cast<float>(hit_feedback_obj["knockback_z"].toDouble(0.0));
  }

  if (json.contains("builder_production")) {
    const auto builder_obj = json["builder_production"].toObject();
    auto* builder = entity->add_component<BuilderProductionComponent>();
    builder->in_progress = builder_obj["in_progress"].toBool(false);
    builder->build_time = static_cast<float>(builder_obj["build_time"].toDouble(10.0));
    builder->time_remaining =
        static_cast<float>(builder_obj["time_remaining"].toDouble(0.0));
    builder->product_type = builder_obj["product_type"].toString().toStdString();
    builder->construction_complete = builder_obj["construction_complete"].toBool(false);
    builder->has_construction_site = builder_obj["has_construction_site"].toBool(false);
    builder->construction_site_x =
        static_cast<float>(builder_obj["construction_site_x"].toDouble(0.0));
    builder->construction_site_z =
        static_cast<float>(builder_obj["construction_site_z"].toDouble(0.0));
    builder->construction_site_rotation_y =
        static_cast<float>(builder_obj["construction_site_rotation_y"].toDouble(0.0));
    builder->has_task_target = builder_obj["has_task_target"].toBool(false);
    builder->task_target_id = static_cast<std::uint64_t>(
        builder_obj["task_target_id"].toVariant().toULongLong());
    builder->task_target_x =
        static_cast<float>(builder_obj["task_target_x"].toDouble(0.0));
    builder->task_target_z =
        static_cast<float>(builder_obj["task_target_z"].toDouble(0.0));
    builder->task_target_reserved = builder_obj["task_target_reserved"].toBool(false);
    builder->at_construction_site = builder_obj["at_construction_site"].toBool(false);
    builder->is_placement_preview = builder_obj["is_placement_preview"].toBool(false);
    builder->construction_site_entity_id = static_cast<EntityID>(
        builder_obj["construction_site_entity_id"].toVariant().toULongLong());
    if (builder_obj.contains("queued_construction_site_ids")) {
      const auto queued_sites = builder_obj["queued_construction_site_ids"].toArray();
      builder->queued_construction_site_ids.reserve(queued_sites.size());
      for (const auto& value : queued_sites) {
        builder->queued_construction_site_ids.push_back(
            static_cast<EntityID>(value.toVariant().toULongLong()));
      }
    }
    builder->bypass_movement_active =
        builder_obj["bypass_movement_active"].toBool(false);
    builder->bypass_target_x =
        static_cast<float>(builder_obj["bypass_target_x"].toDouble(0.0));
    builder->bypass_target_z =
        static_cast<float>(builder_obj["bypass_target_z"].toDouble(0.0));
  }

  if (json.contains("wall_segment")) {
    const auto wall_obj = json["wall_segment"].toObject();
    auto* wall = entity->add_component<WallSegmentComponent>();
    wall->grid_x = wall_obj["grid_x"].toInt(0);
    wall->grid_z = wall_obj["grid_z"].toInt(0);
  }

  if (json.contains("wall_construction_site")) {
    const auto site_obj = json["wall_construction_site"].toObject();
    auto* site = entity->add_component<WallConstructionSiteComponent>();
    site->owner_id = site_obj["owner_id"].toInt(0);
    site->nation_id = static_cast<Game::Systems::NationID>(site_obj["nation_id"].toInt(
        static_cast<int>(Game::Systems::NationID::RomanRepublic)));
    site->build_time = static_cast<float>(site_obj["build_time"].toDouble(0.0));
    site->progress = static_cast<float>(site_obj["progress"].toDouble(0.0));
  }

  if (json.contains("formation_mode")) {
    const auto formation_obj = json["formation_mode"].toObject();
    auto* formation = entity->add_component<FormationModeComponent>();
    formation->active = formation_obj["active"].toBool(false);
    formation->formation_center_x =
        static_cast<float>(formation_obj["formation_center_x"].toDouble(0.0));
    formation->formation_center_z =
        static_cast<float>(formation_obj["formation_center_z"].toDouble(0.0));
  }

  if (json.contains("stamina")) {
    const auto stamina_obj = json["stamina"].toObject();
    auto* stamina = entity->add_component<StaminaComponent>();
    stamina->stamina = static_cast<float>(stamina_obj["stamina"].toDouble(
        static_cast<double>(StaminaComponent::k_default_max_stamina)));
    stamina->max_stamina = static_cast<float>(stamina_obj["max_stamina"].toDouble(
        static_cast<double>(StaminaComponent::k_default_max_stamina)));
    stamina->regen_rate = static_cast<float>(stamina_obj["regen_rate"].toDouble(
        static_cast<double>(StaminaComponent::k_default_regen_rate)));
    stamina->depletion_rate = static_cast<float>(stamina_obj["depletion_rate"].toDouble(
        static_cast<double>(StaminaComponent::k_default_depletion_rate)));
    stamina->is_running = stamina_obj["is_running"].toBool(false);
    stamina->run_requested = stamina_obj["run_requested"].toBool(false);
  }

  if (json.contains("terrain_context")) {
    const auto terrain_context_obj = json["terrain_context"].toObject();
    auto* terrain_context = entity->add_component<TerrainContextComponent>();
    terrain_context->is_on_bridge = terrain_context_obj["is_on_bridge"].toBool(false);
    terrain_context->is_at_hill_entrance =
        terrain_context_obj["is_at_hill_entrance"].toBool(false);
    terrain_context->audio_cooldown =
        static_cast<float>(terrain_context_obj["audio_cooldown"].toDouble(0.0));
  }

  if (json.contains("home")) {
    const auto home_obj = json["home"].toObject();
    auto* home = entity->add_component<HomeComponent>();
    home->population_contribution = home_obj["population_contribution"].toInt(50);
    home->nearest_barracks_id = static_cast<EntityID>(
        home_obj["nearest_barracks_id"].toVariant().toULongLong());
    home->update_cooldown =
        static_cast<float>(home_obj["update_cooldown"].toDouble(0.0));
    home->family_generation_cooldown =
        static_cast<float>(home_obj["family_generation_cooldown"].toDouble(0.0));
    home->family_generation_interval =
        static_cast<float>(home_obj["family_generation_interval"].toDouble(12.0));
    home->family_manpower_value = home_obj["family_manpower_value"].toInt(8);
  }

  if (json.contains("civilian_delivery")) {
    const auto delivery_obj = json["civilian_delivery"].toObject();
    auto* delivery = entity->add_component<CivilianDeliveryComponent>();
    delivery->target_barracks_id = static_cast<EntityID>(
        delivery_obj["target_barracks_id"].toVariant().toULongLong());
  }
}

auto Serialization::serialize_terrain(
    const Game::Map::TerrainHeightMap* height_map,
    const Game::Map::BiomeSettings& biome,
    const std::vector<Game::Map::RoadSegment>& roads,
    const std::vector<Game::Map::WorldProp>& world_props,
    const std::vector<Game::Map::WorldProp>& authored_world_props) -> QJsonObject {
  QJsonObject terrain_obj;

  if (height_map == nullptr) {
    return terrain_obj;
  }

  terrain_obj["width"] = height_map->get_width();
  terrain_obj["height"] = height_map->get_height();
  terrain_obj["tile_size"] = height_map->get_tile_size();

  QJsonArray heights_array;
  const auto& heights = height_map->get_height_data();
  for (float const h : heights) {
    heights_array.append(h);
  }
  terrain_obj["heights"] = heights_array;

  QJsonArray terrain_types_array;
  const auto& terrain_types = height_map->getTerrainTypes();
  for (auto type : terrain_types) {
    terrain_types_array.append(static_cast<int>(type));
  }
  terrain_obj["terrain_types"] = terrain_types_array;

  QJsonArray rivers_array;
  const auto& rivers = height_map->get_river_segments();
  for (const auto& river : rivers) {
    QJsonObject river_obj;
    river_obj["startX"] = river.start.x();
    river_obj["startY"] = river.start.y();
    river_obj["startZ"] = river.start.z();
    river_obj["endX"] = river.end.x();
    river_obj["endY"] = river.end.y();
    river_obj["endZ"] = river.end.z();
    river_obj["width"] = river.width;
    rivers_array.append(river_obj);
  }
  terrain_obj["rivers"] = rivers_array;

  QJsonArray bridges_array;
  const auto& bridges = height_map->get_bridges();
  for (const auto& bridge : bridges) {
    QJsonObject bridge_obj;
    bridge_obj["startX"] = bridge.start.x();
    bridge_obj["startY"] = bridge.start.y();
    bridge_obj["startZ"] = bridge.start.z();
    bridge_obj["endX"] = bridge.end.x();
    bridge_obj["endY"] = bridge.end.y();
    bridge_obj["endZ"] = bridge.end.z();
    bridge_obj["width"] = bridge.width;
    bridge_obj["height"] = bridge.height;
    bridges_array.append(bridge_obj);
  }
  terrain_obj["bridges"] = bridges_array;

  QJsonArray roads_array;
  for (const auto& road : roads) {
    QJsonObject road_obj;
    road_obj["startX"] = road.start.x();
    road_obj["startY"] = road.start.y();
    road_obj["startZ"] = road.start.z();
    road_obj["endX"] = road.end.x();
    road_obj["endY"] = road.end.y();
    road_obj["endZ"] = road.end.z();
    road_obj["width"] = road.width;
    road_obj["style"] = road.style;
    roads_array.append(road_obj);
  }
  terrain_obj["roads"] = roads_array;

  const auto serialize_world_props_array =
      [](const std::vector<Game::Map::WorldProp>& props) -> QJsonArray {
    QJsonArray world_props_array;
    for (const auto& world_prop : props) {
      QJsonObject world_prop_obj;
      world_prop_obj["id"] = static_cast<qint64>(world_prop.id);
      world_prop_obj["type"] =
          QString(Game::Map::world_prop_type_to_string(world_prop.type));
      world_prop_obj["x"] = world_prop.x;
      world_prop_obj["z"] = world_prop.z;
      world_prop_obj["scale"] = world_prop.scale;
      world_prop_obj["rotation"] = world_prop.rotation;
      if (world_prop.type == Game::Map::WorldProp::Type::FireCamp) {
        world_prop_obj["intensity"] = world_prop.intensity;
        world_prop_obj["radius"] = world_prop.radius;
        world_prop_obj["persistent"] = world_prop.persistent;
      }
      world_props_array.append(world_prop_obj);
    }
    return world_props_array;
  };

  QJsonArray const world_props_array = serialize_world_props_array(world_props);
  terrain_obj["world_props"] = world_props_array;
  terrain_obj["authored_world_props"] =
      serialize_world_props_array(authored_world_props);

  const auto profiles = Game::Map::make_biome_profiles(biome);
  const auto& surface = profiles.surface;
  const auto& scatter = profiles.scatter;
  const auto& climate = profiles.climate;
  const auto& wind = profiles.wind;

  QJsonObject biome_obj;
  biome_obj["grassPrimaryR"] = surface.grass_primary.x();
  biome_obj["grassPrimaryG"] = surface.grass_primary.y();
  biome_obj["grassPrimaryB"] = surface.grass_primary.z();
  biome_obj["grassSecondaryR"] = surface.grass_secondary.x();
  biome_obj["grassSecondaryG"] = surface.grass_secondary.y();
  biome_obj["grassSecondaryB"] = surface.grass_secondary.z();
  biome_obj["grassDryR"] = surface.grass_dry.x();
  biome_obj["grassDryG"] = surface.grass_dry.y();
  biome_obj["grassDryB"] = surface.grass_dry.z();
  biome_obj["soilColorR"] = surface.soil_color.x();
  biome_obj["soilColorG"] = surface.soil_color.y();
  biome_obj["soilColorB"] = surface.soil_color.z();
  biome_obj["rockLowR"] = surface.rock_low.x();
  biome_obj["rockLowG"] = surface.rock_low.y();
  biome_obj["rockLowB"] = surface.rock_low.z();
  biome_obj["rockHighR"] = surface.rock_high.x();
  biome_obj["rockHighG"] = surface.rock_high.y();
  biome_obj["rockHighB"] = surface.rock_high.z();
  biome_obj["patchDensity"] = scatter.patch_density;
  biome_obj["patchJitter"] = scatter.patch_jitter;
  biome_obj["backgroundBladeDensity"] = scatter.background_blade_density;
  biome_obj["bladeHeightMin"] = scatter.blade_height_min;
  biome_obj["bladeHeightMax"] = scatter.blade_height_max;
  biome_obj["bladeWidthMin"] = scatter.blade_width_min;
  biome_obj["bladeWidthMax"] = scatter.blade_width_max;
  biome_obj["sway_strength"] = wind.sway_strength;
  biome_obj["sway_speed"] = wind.sway_speed;
  biome_obj["heightNoiseAmplitude"] = surface.height_noise_amplitude;
  biome_obj["heightNoiseFrequency"] = surface.height_noise_frequency;
  biome_obj["terrainMacroNoiseScale"] = surface.terrain_macro_noise_scale;
  biome_obj["terrainDetailNoiseScale"] = surface.terrain_detail_noise_scale;
  biome_obj["terrainSoilHeight"] = surface.terrain_soil_height;
  biome_obj["terrainSoilSharpness"] = surface.terrain_soil_sharpness;
  biome_obj["terrainRockThreshold"] = surface.terrain_rock_threshold;
  biome_obj["terrainRockSharpness"] = surface.terrain_rock_sharpness;
  biome_obj["terrainAmbientBoost"] = surface.terrain_ambient_boost;
  biome_obj["terrainRockDetailStrength"] = surface.terrain_rock_detail_strength;
  biome_obj["backgroundSwayVariance"] = wind.background_sway_variance;
  biome_obj["backgroundScatterRadius"] = scatter.background_scatter_radius;
  biome_obj["plant_density"] = scatter.plant_density;
  biome_obj["spawnEdgePadding"] = scatter.spawn_edge_padding;
  biome_obj["snowCoverage"] = climate.snow_coverage;
  biome_obj["moistureLevel"] = climate.moisture_level;
  biome_obj["crackIntensity"] = climate.crack_intensity;
  biome_obj["rockExposure"] = climate.rock_exposure;
  biome_obj["grassSaturation"] = climate.grass_saturation;
  biome_obj["soilRoughness"] = climate.soil_roughness;
  biome_obj["snowColorR"] = climate.snow_color.x();
  biome_obj["snowColorG"] = climate.snow_color.y();
  biome_obj["snowColorB"] = climate.snow_color.z();
  biome_obj["seed"] = static_cast<qint64>(surface.seed);
  terrain_obj["biome"] = biome_obj;

  return terrain_obj;
}

void Serialization::deserialize_terrain(
    Game::Map::TerrainHeightMap* height_map,
    Game::Map::BiomeSettings& biome,
    std::vector<Game::Map::RoadSegment>& roads,
    std::vector<Game::Map::WorldProp>& world_props,
    std::vector<Game::Map::WorldProp>& authored_world_props,
    const QJsonObject& json) {
  if ((height_map == nullptr) || json.isEmpty()) {
    return;
  }

  if (json.contains("biome")) {
    const auto biome_obj = json["biome"].toObject();
    const Game::Map::BiomeSettings default_biome{};
    const auto read_color = [&](const QString& base,
                                const QVector3D& fallback) -> QVector3D {
      const auto r_key = base + QStringLiteral("R");
      const auto g_key = base + QStringLiteral("G");
      const auto b_key = base + QStringLiteral("B");
      const float r = static_cast<float>(
          biome_obj[r_key].toDouble(static_cast<double>(fallback.x())));
      const float g = static_cast<float>(
          biome_obj[g_key].toDouble(static_cast<double>(fallback.y())));
      const float b = static_cast<float>(
          biome_obj[b_key].toDouble(static_cast<double>(fallback.z())));
      return {r, g, b};
    };

    biome.grass_primary =
        read_color(QStringLiteral("grassPrimary"), default_biome.grass_primary);
    biome.grass_secondary =
        read_color(QStringLiteral("grassSecondary"), default_biome.grass_secondary);
    biome.grass_dry = read_color(QStringLiteral("grassDry"), default_biome.grass_dry);
    biome.soil_color =
        read_color(QStringLiteral("soilColor"), default_biome.soil_color);
    biome.rock_low = read_color(QStringLiteral("rockLow"), default_biome.rock_low);
    biome.rock_high = read_color(QStringLiteral("rockHigh"), default_biome.rock_high);

    biome.patch_density = static_cast<float>(
        biome_obj["patchDensity"].toDouble(default_biome.patch_density));
    biome.patch_jitter = static_cast<float>(
        biome_obj["patchJitter"].toDouble(default_biome.patch_jitter));
    biome.background_blade_density =
        static_cast<float>(biome_obj["backgroundBladeDensity"].toDouble(
            default_biome.background_blade_density));
    biome.blade_height_min = static_cast<float>(
        biome_obj["bladeHeightMin"].toDouble(default_biome.blade_height_min));
    biome.blade_height_max = static_cast<float>(
        biome_obj["bladeHeightMax"].toDouble(default_biome.blade_height_max));
    biome.blade_width_min = static_cast<float>(
        biome_obj["bladeWidthMin"].toDouble(default_biome.blade_width_min));
    biome.blade_width_max = static_cast<float>(
        biome_obj["bladeWidthMax"].toDouble(default_biome.blade_width_max));
    biome.sway_strength = static_cast<float>(
        biome_obj["sway_strength"].toDouble(default_biome.sway_strength));
    biome.sway_speed =
        static_cast<float>(biome_obj["sway_speed"].toDouble(default_biome.sway_speed));
    biome.height_noise_amplitude =
        static_cast<float>(biome_obj["heightNoiseAmplitude"].toDouble(
            default_biome.height_noise_amplitude));
    biome.height_noise_frequency =
        static_cast<float>(biome_obj["heightNoiseFrequency"].toDouble(
            default_biome.height_noise_frequency));
    biome.terrain_macro_noise_scale =
        static_cast<float>(biome_obj["terrainMacroNoiseScale"].toDouble(
            default_biome.terrain_macro_noise_scale));
    biome.terrain_detail_noise_scale =
        static_cast<float>(biome_obj["terrainDetailNoiseScale"].toDouble(
            default_biome.terrain_detail_noise_scale));
    biome.terrain_soil_height = static_cast<float>(
        biome_obj["terrainSoilHeight"].toDouble(default_biome.terrain_soil_height));
    biome.terrain_soil_sharpness =
        static_cast<float>(biome_obj["terrainSoilSharpness"].toDouble(
            default_biome.terrain_soil_sharpness));
    biome.terrain_rock_threshold =
        static_cast<float>(biome_obj["terrainRockThreshold"].toDouble(
            default_biome.terrain_rock_threshold));
    biome.terrain_rock_sharpness =
        static_cast<float>(biome_obj["terrainRockSharpness"].toDouble(
            default_biome.terrain_rock_sharpness));
    biome.terrain_ambient_boost = static_cast<float>(
        biome_obj["terrainAmbientBoost"].toDouble(default_biome.terrain_ambient_boost));
    biome.terrain_rock_detail_strength =
        static_cast<float>(biome_obj["terrainRockDetailStrength"].toDouble(
            default_biome.terrain_rock_detail_strength));
    biome.background_sway_variance =
        static_cast<float>(biome_obj["backgroundSwayVariance"].toDouble(
            default_biome.background_sway_variance));
    biome.background_scatter_radius =
        static_cast<float>(biome_obj["backgroundScatterRadius"].toDouble(
            default_biome.background_scatter_radius));
    biome.plant_density = static_cast<float>(
        biome_obj["plant_density"].toDouble(default_biome.plant_density));
    biome.spawn_edge_padding = static_cast<float>(
        biome_obj["spawnEdgePadding"].toDouble(default_biome.spawn_edge_padding));
    if (biome_obj.contains("seed")) {
      biome.seed =
          static_cast<std::uint32_t>(biome_obj["seed"].toVariant().toULongLong());
    } else {
      biome.seed = default_biome.seed;
    }
    biome.snow_coverage = static_cast<float>(
        biome_obj["snowCoverage"].toDouble(default_biome.snow_coverage));
    biome.moisture_level = static_cast<float>(
        biome_obj["moistureLevel"].toDouble(default_biome.moisture_level));
    biome.crack_intensity = static_cast<float>(
        biome_obj["crackIntensity"].toDouble(default_biome.crack_intensity));
    biome.rock_exposure = static_cast<float>(
        biome_obj["rockExposure"].toDouble(default_biome.rock_exposure));
    biome.grass_saturation = static_cast<float>(
        biome_obj["grassSaturation"].toDouble(default_biome.grass_saturation));
    biome.soil_roughness = static_cast<float>(
        biome_obj["soilRoughness"].toDouble(default_biome.soil_roughness));
    biome.snow_color =
        read_color(QStringLiteral("snowColor"), default_biome.snow_color);
  }

  std::vector<float> heights;
  if (json.contains("heights")) {
    const auto heights_array = json["heights"].toArray();
    heights.reserve(heights_array.size());
    for (const auto& val : heights_array) {
      heights.push_back(static_cast<float>(val.toDouble(0.0)));
    }
  }

  std::vector<Game::Map::TerrainType> terrain_types;
  if (json.contains("terrain_types")) {
    const auto types_array = json["terrain_types"].toArray();
    terrain_types.reserve(types_array.size());
    for (const auto& val : types_array) {
      terrain_types.push_back(static_cast<Game::Map::TerrainType>(val.toInt(0)));
    }
  }

  std::vector<Game::Map::RiverSegment> rivers;
  if (json.contains("rivers")) {
    const auto rivers_array = json["rivers"].toArray();
    rivers.reserve(rivers_array.size());
    const Game::Map::RiverSegment default_river{};
    for (const auto& val : rivers_array) {
      const auto river_obj = val.toObject();
      Game::Map::RiverSegment river;
      river.start = QVector3D(static_cast<float>(river_obj["startX"].toDouble(0.0)),
                              static_cast<float>(river_obj["startY"].toDouble(0.0)),
                              static_cast<float>(river_obj["startZ"].toDouble(0.0)));
      river.end = QVector3D(static_cast<float>(river_obj["endX"].toDouble(0.0)),
                            static_cast<float>(river_obj["endY"].toDouble(0.0)),
                            static_cast<float>(river_obj["endZ"].toDouble(0.0)));
      river.width = static_cast<float>(
          river_obj["width"].toDouble(static_cast<double>(default_river.width)));
      rivers.push_back(river);
    }
  }

  std::vector<Game::Map::Bridge> bridges;
  if (json.contains("bridges")) {
    const auto bridges_array = json["bridges"].toArray();
    bridges.reserve(bridges_array.size());
    const Game::Map::Bridge default_bridge{};
    for (const auto& val : bridges_array) {
      const auto bridge_obj = val.toObject();
      Game::Map::Bridge bridge;
      bridge.start = QVector3D(static_cast<float>(bridge_obj["startX"].toDouble(0.0)),
                               static_cast<float>(bridge_obj["startY"].toDouble(0.0)),
                               static_cast<float>(bridge_obj["startZ"].toDouble(0.0)));
      bridge.end = QVector3D(static_cast<float>(bridge_obj["endX"].toDouble(0.0)),
                             static_cast<float>(bridge_obj["endY"].toDouble(0.0)),
                             static_cast<float>(bridge_obj["endZ"].toDouble(0.0)));
      bridge.width = static_cast<float>(
          bridge_obj["width"].toDouble(static_cast<double>(default_bridge.width)));
      bridge.height = static_cast<float>(
          bridge_obj["height"].toDouble(static_cast<double>(default_bridge.height)));
      bridges.push_back(bridge);
    }
  }

  roads.clear();
  if (json.contains("roads")) {
    const auto roads_array = json["roads"].toArray();
    roads.reserve(roads_array.size());
    const Game::Map::RoadSegment default_road{};
    for (const auto& val : roads_array) {
      const auto road_obj = val.toObject();
      Game::Map::RoadSegment road;
      road.start = QVector3D(static_cast<float>(road_obj["startX"].toDouble(0.0)),
                             static_cast<float>(road_obj["startY"].toDouble(0.0)),
                             static_cast<float>(road_obj["startZ"].toDouble(0.0)));
      road.end = QVector3D(static_cast<float>(road_obj["endX"].toDouble(0.0)),
                           static_cast<float>(road_obj["endY"].toDouble(0.0)),
                           static_cast<float>(road_obj["endZ"].toDouble(0.0)));
      road.width = static_cast<float>(
          road_obj["width"].toDouble(static_cast<double>(default_road.width)));
      road.style = road_obj["style"].toString(default_road.style);
      roads.push_back(road);
    }
  }

  const auto deserialize_world_props_array =
      [](const QJsonValue& json_value,
         std::vector<Game::Map::WorldProp>& out_world_props) {
        if (!json_value.isArray()) {
          return;
        }
        const auto world_props_array = json_value.toArray();
        out_world_props.reserve(out_world_props.size() + world_props_array.size());
        for (const auto& val : world_props_array) {
          const auto world_prop_obj = val.toObject();
          Game::Map::WorldProp world_prop;
          if (!Game::Map::world_prop_type_from_string(world_prop_obj["type"].toString(),
                                                      world_prop.type)) {
            qWarning() << "Unknown world prop type in save file:"
                       << world_prop_obj["type"].toString() << "- skipping";
            continue;
          }
          world_prop.id = static_cast<std::uint64_t>(
              world_prop_obj["id"].toVariant().toULongLong());
          world_prop.x = static_cast<float>(world_prop_obj["x"].toDouble(0.0));
          world_prop.z = static_cast<float>(world_prop_obj["z"].toDouble(0.0));
          world_prop.scale = static_cast<float>(world_prop_obj["scale"].toDouble(1.0));
          world_prop.rotation =
              static_cast<float>(world_prop_obj["rotation"].toDouble(0.0));
          world_prop.intensity =
              static_cast<float>(world_prop_obj["intensity"].toDouble(1.0));
          world_prop.radius =
              static_cast<float>(world_prop_obj["radius"].toDouble(3.0));
          world_prop.persistent = world_prop_obj["persistent"].toBool(true);
          out_world_props.push_back(world_prop);
        }
      };

  world_props.clear();
  if (json.contains("firecamps")) {
    const auto fire_camps_array = json["firecamps"].toArray();
    world_props.reserve(world_props.size() + fire_camps_array.size());
    for (const auto& val : fire_camps_array) {
      const auto fire_camp_obj = val.toObject();
      Game::Map::WorldProp fire_camp;
      fire_camp.type = Game::Map::WorldProp::Type::FireCamp;
      fire_camp.x = static_cast<float>(fire_camp_obj["x"].toDouble(0.0));
      fire_camp.z = static_cast<float>(fire_camp_obj["z"].toDouble(0.0));
      fire_camp.intensity =
          static_cast<float>(fire_camp_obj["intensity"].toDouble(1.0));
      fire_camp.radius = static_cast<float>(fire_camp_obj["radius"].toDouble(3.0));
      fire_camp.persistent = fire_camp_obj["persistent"].toBool(true);
      world_props.push_back(fire_camp);
    }
  }

  if (json.contains("world_props")) {
    deserialize_world_props_array(json["world_props"], world_props);
  }

  authored_world_props.clear();
  if (json.contains("authored_world_props")) {
    deserialize_world_props_array(json["authored_world_props"], authored_world_props);
  } else {
    authored_world_props = world_props;
  }

  height_map->restore_from_data(heights, terrain_types, rivers, bridges);
}

auto Serialization::serialize_world(const World* world) -> QJsonDocument {
  QJsonObject world_obj;
  QJsonArray entities_array;

  const auto& entities = world->get_entities();
  for (const auto& [id, entity] : entities) {
    if (entity != nullptr &&
        entity->get_component<ConstructionPreviewComponent>() != nullptr) {
      continue;
    }
    QJsonObject const entity_obj = serialize_entity(entity.get());
    entities_array.append(entity_obj);
  }

  world_obj["entities"] = entities_array;
  world_obj["nextEntityId"] = static_cast<qint64>(world->get_next_entity_id());
  world_obj["schemaVersion"] = 2;
  world_obj["owner_registry"] = Game::Systems::OwnerRegistry::instance().to_json();

  const auto& terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.is_initialized() &&
      (terrain_service.get_height_map() != nullptr)) {
    world_obj["terrain"] = serialize_terrain(terrain_service.get_height_map(),
                                             terrain_service.biome_settings(),
                                             terrain_service.road_segments(),
                                             terrain_service.world_props(),
                                             terrain_service.authored_world_props());
  }

  return QJsonDocument(world_obj);
}

void Serialization::deserialize_world(World* world, const QJsonDocument& doc) {
  auto world_obj = doc.object();
  auto entities_array = world_obj["entities"].toArray();
  for (const auto& value : entities_array) {
    auto entity_obj = value.toObject();
    const auto entity_id =
        static_cast<EntityID>(entity_obj["id"].toVariant().toULongLong());
    auto* entity = entity_id == NULL_ENTITY ? world->create_entity()
                                            : world->create_entity_with_id(entity_id);
    if (entity != nullptr) {
      deserialize_entity(entity, entity_obj);
    }
  }

  if (world_obj.contains("nextEntityId")) {
    const auto next_id =
        static_cast<EntityID>(world_obj["nextEntityId"].toVariant().toULongLong());
    world->set_next_entity_id(next_id);
  }

  if (world_obj.contains("owner_registry")) {
    Game::Systems::OwnerRegistry::instance().from_json(
        world_obj["owner_registry"].toObject());
  }

  if (world_obj.contains("terrain")) {
    const auto terrain_obj = world_obj["terrain"].toObject();
    const int width = terrain_obj["width"].toInt(50);
    const int height = terrain_obj["height"].toInt(50);
    const float tile_size = static_cast<float>(terrain_obj["tile_size"].toDouble(1.0));

    Game::Map::BiomeSettings biome;
    std::vector<Game::Map::RoadSegment> roads;
    std::vector<Game::Map::WorldProp> world_props;
    std::vector<Game::Map::WorldProp> authored_world_props;
    std::vector<float> const heights;
    std::vector<Game::Map::TerrainType> const terrain_types;
    std::vector<Game::Map::RiverSegment> const rivers;
    std::vector<Game::Map::Bridge> const bridges;

    auto temp_height_map =
        std::make_unique<Game::Map::TerrainHeightMap>(width, height, tile_size);
    deserialize_terrain(temp_height_map.get(),
                        biome,
                        roads,
                        world_props,
                        authored_world_props,
                        terrain_obj);

    auto& terrain_service = Game::Map::TerrainService::instance();
    terrain_service.restore_from_serialized(width,
                                            height,
                                            tile_size,
                                            temp_height_map->get_height_data(),
                                            temp_height_map->getTerrainTypes(),
                                            temp_height_map->get_river_segments(),
                                            roads,
                                            temp_height_map->get_bridges(),
                                            biome,
                                            world_props,
                                            authored_world_props);
  }
}

auto Serialization::save_to_file(const QString& filename,
                                 const QJsonDocument& doc) -> bool {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open file for writing:" << filename;
    return false;
  }
  file.write(doc.toJson());
  return true;
}

auto Serialization::load_from_file(const QString& filename) -> QJsonDocument {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Could not open file for reading:" << filename;
    return {};
  }
  const QByteArray data = file.readAll();
  return QJsonDocument::fromJson(data);
}

} // namespace Engine::Core

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "entity_codec.h"
#include "serialization.h"

namespace Engine::Core::EntityCodec {

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

void write_attack(const Entity* entity, QJsonObject& entity_obj) {
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
    entity_obj["attack"] = attack_obj;
  }
}

void read_attack(Entity* entity, const QJsonObject& json) {
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
  }
}

void write_attack_target(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* attack_target = entity->get_component<AttackTargetComponent>()) {
    QJsonObject attack_target_obj;
    attack_target_obj["target_id"] = static_cast<qint64>(attack_target->target_id);
    attack_target_obj["should_chase"] = attack_target->should_chase;
    attack_target_obj["is_player_command"] = attack_target->is_player_command;
    entity_obj["attack_target"] = attack_target_obj;
  }
}

void read_attack_target(Entity* entity, const QJsonObject& json) {
  if (json.contains("attack_target")) {
    const auto attack_target_obj = json["attack_target"].toObject();
    auto* attack_target = entity->add_component<AttackTargetComponent>();
    attack_target->target_id =
        static_cast<EntityID>(attack_target_obj["target_id"].toVariant().toULongLong());
    attack_target->should_chase = attack_target_obj["should_chase"].toBool(false);
    attack_target->is_player_command =
        attack_target_obj["is_player_command"].toBool(false);
  }
}

void write_hold_mode(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_hold_mode(Entity* entity, const QJsonObject& json) {
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
}

void write_guard_mode(const Entity* entity, QJsonObject& entity_obj) {
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
    if (guard_mode->has_reach_center) {
      guard_mode_obj["reach_center_x"] =
          static_cast<double>(guard_mode->reach_center_x);
      guard_mode_obj["reach_center_z"] =
          static_cast<double>(guard_mode->reach_center_z);
    }
    entity_obj["guard_mode"] = guard_mode_obj;
  }
}

void read_guard_mode(Entity* entity, const QJsonObject& json) {
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
    guard_mode->has_reach_center = guard_mode_obj.contains("reach_center_x");
    guard_mode->reach_center_x =
        static_cast<float>(guard_mode_obj["reach_center_x"].toDouble(0.0));
    guard_mode->reach_center_z =
        static_cast<float>(guard_mode_obj["reach_center_z"].toDouble(0.0));
  }
}

void write_combat_state(const Entity* entity, QJsonObject& entity_obj) {
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

    combat_state_obj["swing_strike_x"] =
        static_cast<double>(combat_state->intent.strike_dir_x);
    combat_state_obj["swing_strike_y"] =
        static_cast<double>(combat_state->intent.strike_dir_y);
    combat_state_obj["swing_thrust"] =
        static_cast<double>(combat_state->intent.thrust_amount);
    combat_state_obj["swing_elevation"] =
        static_cast<double>(combat_state->intent.elevation);
    combat_state_obj["swing_charge"] = static_cast<double>(combat_state->intent.charge);
    combat_state_obj["swing_speed"] =
        static_cast<double>(combat_state->intent.swing_speed);
    combat_state_obj["swing_follow_through"] =
        static_cast<double>(combat_state->intent.follow_through);
    combat_state_obj["finisher_attack"] = combat_state->finisher_attack;
    combat_state_obj["is_hit_paused"] = combat_state->is_hit_paused;
    combat_state_obj["hit_pause_remaining"] =
        static_cast<double>(combat_state->hit_pause_remaining);
    entity_obj["combat_state"] = combat_state_obj;
  }
}

void read_combat_state(Entity* entity, const QJsonObject& json) {
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

    auto& swing = combat_state->intent;
    swing.strike_dir_x = static_cast<float>(
        combat_state_obj["swing_strike_x"].toDouble(swing.strike_dir_x));
    swing.strike_dir_y = static_cast<float>(
        combat_state_obj["swing_strike_y"].toDouble(swing.strike_dir_y));
    swing.windup_dir_x = -swing.strike_dir_x;
    swing.windup_dir_y = -swing.strike_dir_y;
    swing.thrust_amount =
        static_cast<float>(combat_state_obj["swing_thrust"].toDouble(0.0));
    swing.elevation =
        static_cast<float>(combat_state_obj["swing_elevation"].toDouble(0.0));
    swing.charge = static_cast<float>(combat_state_obj["swing_charge"].toDouble(0.0));
    swing.swing_speed =
        static_cast<float>(combat_state_obj["swing_speed"].toDouble(1.0));
    swing.follow_through =
        static_cast<float>(combat_state_obj["swing_follow_through"].toDouble(0.5));
    Engine::Core::complete_melee_intent(swing);
    combat_state->finisher_attack = combat_state_obj["finisher_attack"].toBool(false);
    combat_state->is_hit_paused = combat_state_obj["is_hit_paused"].toBool(false);
    combat_state->hit_pause_remaining =
        static_cast<float>(combat_state_obj["hit_pause_remaining"].toDouble(0.0));
  }
}

void write_hit_feedback(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* hit_feedback = entity->get_component<HitFeedbackComponent>()) {
    QJsonObject hit_feedback_obj;
    hit_feedback_obj["source_attacker_id"] =
        static_cast<qint64>(hit_feedback->source_attacker_id);
    hit_feedback_obj["is_reacting"] = hit_feedback->is_reacting;
    hit_feedback_obj["reaction_time"] =
        static_cast<double>(hit_feedback->reaction_time);
    hit_feedback_obj["reaction_intensity"] =
        static_cast<double>(hit_feedback->reaction_intensity);
    hit_feedback_obj["knockback_x"] = static_cast<double>(hit_feedback->knockback_x);
    hit_feedback_obj["knockback_z"] = static_cast<double>(hit_feedback->knockback_z);
    hit_feedback_obj["reaction_duration"] =
        static_cast<double>(hit_feedback->reaction_duration);
    hit_feedback_obj["reaction_kind"] = static_cast<int>(hit_feedback->reaction_kind);
    entity_obj["hit_feedback"] = hit_feedback_obj;
  }
}

void read_hit_feedback(Entity* entity, const QJsonObject& json) {
  if (json.contains("hit_feedback")) {
    const auto hit_feedback_obj = json["hit_feedback"].toObject();
    auto* hit_feedback = entity->add_component<HitFeedbackComponent>();
    hit_feedback->source_attacker_id =
        static_cast<EntityID>(hit_feedback_obj["source_attacker_id"].toInteger(0));
    hit_feedback->is_reacting = hit_feedback_obj["is_reacting"].toBool(false);
    hit_feedback->reaction_time =
        static_cast<float>(hit_feedback_obj["reaction_time"].toDouble(0.0));
    hit_feedback->reaction_intensity =
        static_cast<float>(hit_feedback_obj["reaction_intensity"].toDouble(0.0));
    hit_feedback->knockback_x =
        static_cast<float>(hit_feedback_obj["knockback_x"].toDouble(0.0));
    hit_feedback->knockback_z =
        static_cast<float>(hit_feedback_obj["knockback_z"].toDouble(0.0));
    hit_feedback->reaction_duration =
        static_cast<float>(hit_feedback_obj["reaction_duration"].toDouble(
            static_cast<double>(HitFeedbackComponent::k_reaction_duration)));
    hit_feedback->reaction_kind = static_cast<HitReactionKind>(
        std::clamp(hit_feedback_obj["reaction_kind"].toInt(0), 0, 4));
  }
}

} // namespace

void write_combat(const Entity* entity, QJsonObject& entity_obj) {
  write_attack(entity, entity_obj);
  write_attack_target(entity, entity_obj);
  write_hold_mode(entity, entity_obj);
  write_guard_mode(entity, entity_obj);
  write_combat_state(entity, entity_obj);
  write_hit_feedback(entity, entity_obj);
}

void read_combat(Entity* entity, const QJsonObject& json) {
  read_attack(entity, json);
  read_attack_target(entity, json);
  read_hold_mode(entity, json);
  read_guard_mode(entity, json);
  read_combat_state(entity, json);
  read_hit_feedback(entity, json);
}

} // namespace Engine::Core::EntityCodec

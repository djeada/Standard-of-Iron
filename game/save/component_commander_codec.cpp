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

void write_commander_profile(const Entity* entity, QJsonObject& entity_obj) {
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
    commander_obj["aura_ability_active"] = commander->aura_ability_active;
    commander_obj["aura_ability_requested"] = commander->aura_ability_requested;
    commander_obj["aura_ability_duration"] = commander->aura_ability_duration;
    commander_obj["aura_ability_remaining"] = commander->aura_ability_remaining;
    commander_obj["aura_ability_cooldown"] = commander->aura_ability_cooldown;
    commander_obj["aura_ability_cooldown_remaining"] =
        commander->aura_ability_cooldown_remaining;
    commander_obj["aura_affinity_spawn_type"] =
        Game::Units::spawn_typeToQString(commander->aura_affinity_spawn_type);
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
}

void read_commander_identity(CommanderComponent* commander,
                             const QJsonObject& commander_obj) {
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
  commander->wounded = commander_obj["wounded"].toBool(commander->wounded);
  commander->fpv_controlled = commander_obj["fpv_controlled"].toBool(false);
}

void read_commander_aura(CommanderComponent* commander,
                         const QJsonObject& commander_obj) {
  commander->aura_radius =
      static_cast<float>(commander_obj["aura_radius"].toDouble(commander->aura_radius));
  commander->aura_morale_bonus = static_cast<float>(
      commander_obj["aura_morale_bonus"].toDouble(commander->aura_morale_bonus));
  commander->aura_bonus_value = static_cast<float>(
      commander_obj["aura_bonus_value"].toDouble(commander->aura_bonus_value));
  commander->aura_active = commander_obj["aura_active"].toBool(commander->aura_active);
  commander->aura_ability_active = commander_obj["aura_ability_active"].toBool(false);
  commander->aura_ability_requested =
      commander_obj["aura_ability_requested"].toBool(false);
  commander->aura_ability_duration =
      static_cast<float>(commander_obj["aura_ability_duration"].toDouble(
          commander->aura_ability_duration));
  commander->aura_ability_remaining =
      static_cast<float>(commander_obj["aura_ability_remaining"].toDouble(0.0));
  commander->aura_ability_cooldown =
      static_cast<float>(commander_obj["aura_ability_cooldown"].toDouble(
          commander->aura_ability_cooldown));
  commander->aura_ability_cooldown_remaining = static_cast<float>(
      commander_obj["aura_ability_cooldown_remaining"].toDouble(0.0));
  const QJsonValue affinity = commander_obj["aura_affinity_spawn_type"];
  if (affinity.isString()) {
    Game::Units::try_parse_spawn_type(affinity.toString(),
                                      commander->aura_affinity_spawn_type);
  } else if (affinity.isDouble()) {
    const int ordinal =
        affinity.toInt(static_cast<int>(commander->aura_affinity_spawn_type));
    if (ordinal >= 0 && ordinal <= static_cast<int>(Game::Units::SpawnType::Farm)) {
      commander->aura_affinity_spawn_type =
          static_cast<Game::Units::SpawnType>(ordinal);
    }
  }
}

void read_commander_rally(CommanderComponent* commander,
                          const QJsonObject& commander_obj) {
  commander->rally_range =
      static_cast<float>(commander_obj["rally_range"].toDouble(commander->rally_range));
  commander->rally_cooldown = static_cast<float>(
      commander_obj["rally_cooldown"].toDouble(commander->rally_cooldown));
  commander->rally_morale_restore = static_cast<float>(
      commander_obj["rally_morale_restore"].toDouble(commander->rally_morale_restore));
  commander->rally_cooldown_remaining =
      static_cast<float>(commander_obj["rally_cooldown_remaining"].toDouble(
          commander->rally_cooldown_remaining));
  commander->rally_feedback_time = static_cast<float>(
      commander_obj["rally_feedback_time"].toDouble(commander->rally_feedback_time));
  commander->death_shock_radius = static_cast<float>(
      commander_obj["death_shock_radius"].toDouble(commander->death_shock_radius));
  commander->death_morale_shock = static_cast<float>(
      commander_obj["death_morale_shock"].toDouble(commander->death_morale_shock));
  commander->rally_requested = commander_obj["rally_requested"].toBool(false);
  commander->rally_requires_manual_trigger =
      commander_obj["rally_requires_manual_trigger"].toBool(true);
  commander->flag_rally_cost = static_cast<float>(
      commander_obj["flag_rally_cost"].toDouble(commander->flag_rally_cost));
  commander->flag_rally_pending_x = static_cast<float>(
      commander_obj["flag_rally_pending_x"].toDouble(commander->flag_rally_pending_x));
  commander->flag_rally_pending_z = static_cast<float>(
      commander_obj["flag_rally_pending_z"].toDouble(commander->flag_rally_pending_z));
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

void read_commander_profile(Entity* entity, const QJsonObject& json) {
  if (json.contains("commander")) {
    const auto commander_obj = json["commander"].toObject();
    auto* commander = entity->add_component<CommanderComponent>();
    read_commander_identity(commander, commander_obj);
    read_commander_aura(commander, commander_obj);
    read_commander_rally(commander, commander_obj);
  }
}

void write_rpg_health(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* rpg = entity->get_component<RpgHealthComponent>()) {
    QJsonObject rpg_obj;
    rpg_obj["incoming_damage_scale"] = static_cast<double>(rpg->incoming_damage_scale);
    rpg_obj["armor"] = static_cast<double>(rpg->armor);
    rpg_obj["crit_chance"] = static_cast<double>(rpg->crit_chance);
    rpg_obj["crit_multiplier"] = static_cast<double>(rpg->crit_multiplier);

    entity_obj["rpg_health"] = rpg_obj;
  }
}

void read_rpg_health(Entity* entity, const QJsonObject& json) {
  if (json.contains("rpg_health")) {
    const auto rpg_obj = json["rpg_health"].toObject();
    auto* rpg = entity->add_component<RpgHealthComponent>();
    rpg->incoming_damage_scale = static_cast<float>(
        rpg_obj["incoming_damage_scale"].toDouble(rpg->incoming_damage_scale));
    rpg->armor = static_cast<float>(rpg_obj["armor"].toDouble(rpg->armor));
    rpg->crit_chance =
        static_cast<float>(rpg_obj["crit_chance"].toDouble(rpg->crit_chance));
    rpg->crit_multiplier =
        static_cast<float>(rpg_obj["crit_multiplier"].toDouble(rpg->crit_multiplier));
    rpg->active = false;
  }
}

void write_morale(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* morale = entity->get_component<MoraleComponent>()) {
    QJsonObject morale_obj;
    morale_obj["morale"] = morale->morale;
    morale_obj["commander_aura_bonus"] = morale->commander_aura_bonus;
    morale_obj["shock_timer"] = morale->shock_timer;
    morale_obj["wavering"] = morale->wavering;
    morale_obj["routing"] = morale->routing;
    entity_obj["morale"] = morale_obj;
  }
}

void read_morale(Entity* entity, const QJsonObject& json) {
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
}

void write_commander_guard(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* commander_guard = entity->get_component<CommanderGuardComponent>()) {
    QJsonObject guard_obj;
    guard_obj["active"] = commander_guard->active;
    guard_obj["frontal_arc_dot"] =
        static_cast<double>(commander_guard->frontal_arc_dot);
    guard_obj["damage_multiplier"] =
        static_cast<double>(commander_guard->damage_multiplier);
    entity_obj["commander_guard"] = guard_obj;
  }
}

void read_commander_guard(Entity* entity, const QJsonObject& json) {
  if (json.contains("commander_guard")) {
    const auto guard_obj = json["commander_guard"].toObject();
    auto* commander_guard = entity->add_component<CommanderGuardComponent>();
    commander_guard->active = guard_obj["active"].toBool(false);
    commander_guard->frontal_arc_dot =
        static_cast<float>(guard_obj["frontal_arc_dot"].toDouble(0.15));
    commander_guard->damage_multiplier =
        static_cast<float>(guard_obj["damage_multiplier"].toDouble(0.45));
  }
}

} // namespace

void write_commander(const Entity* entity, QJsonObject& entity_obj) {
  write_commander_profile(entity, entity_obj);
  write_rpg_health(entity, entity_obj);
  write_morale(entity, entity_obj);
  write_commander_guard(entity, entity_obj);
}

void read_commander(Entity* entity, const QJsonObject& json) {
  read_commander_profile(entity, json);
  read_rpg_health(entity, json);
  read_morale(entity, json);
  read_commander_guard(entity, json);
}

} // namespace Engine::Core::EntityCodec

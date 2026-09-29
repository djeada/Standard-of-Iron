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

void write_healer(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_healer(Entity* entity, const QJsonObject& json) {
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
}

void write_special_attack(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_special_attack(Entity* entity, const QJsonObject& json) {
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
}

void write_fire_patch(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_fire_patch(Entity* entity, const QJsonObject& json) {
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
}

void write_catapult_loading(const Entity* entity, QJsonObject& entity_obj) {
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
    catapult_obj["loaded_projectile_kind"] =
        static_cast<int>(catapult->loaded_projectile_kind);
    entity_obj["catapult_loading"] = catapult_obj;
  }
}

void read_catapult_loading(Entity* entity, const QJsonObject& json) {
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
    catapult->loaded_projectile_kind = static_cast<Game::Systems::ProjectileKind>(
        catapult_obj["loaded_projectile_kind"].toInt(
            static_cast<int>(Game::Systems::ProjectileKind::Stone)));
  }
}

} // namespace

void write_ability(const Entity* entity, QJsonObject& entity_obj) {
  write_healer(entity, entity_obj);
  write_special_attack(entity, entity_obj);
  write_fire_patch(entity, entity_obj);
  write_catapult_loading(entity, entity_obj);
}

void read_ability(Entity* entity, const QJsonObject& json) {
  read_healer(entity, json);
  read_special_attack(entity, json);
  read_fire_patch(entity, json);
  read_catapult_loading(entity, json);
}

} // namespace Engine::Core::EntityCodec

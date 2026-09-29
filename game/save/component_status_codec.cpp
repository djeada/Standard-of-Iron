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

void write_undead(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_undead(Entity* entity, const QJsonObject& json) {
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
}

void write_cursed_status(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* cursed = entity->get_component<CursedStatusComponent>()) {
    QJsonObject cursed_obj;
    cursed_obj["morale_penalty_per_hit"] =
        static_cast<double>(cursed->morale_penalty_per_hit);
    cursed_obj["duration"] = static_cast<double>(cursed->duration);
    cursed_obj["remaining_duration"] = static_cast<double>(cursed->remaining_duration);
    cursed_obj["stacks"] = cursed->stacks;
    entity_obj["cursed_status"] = cursed_obj;
  }
}

void read_cursed_status(Entity* entity, const QJsonObject& json) {
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
}

void write_burning_status(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_burning_status(Entity* entity, const QJsonObject& json) {
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
}

void write_elephant(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_elephant(Entity* entity, const QJsonObject& json) {
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
}

void write_elephant_panic(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* panic = entity->get_component<ElephantPanicComponent>()) {
    QJsonObject panic_obj;
    panic_obj["duration"] = static_cast<double>(panic->duration);
    entity_obj["elephant_panic"] = panic_obj;
  }
}

void read_elephant_panic(Entity* entity, const QJsonObject& json) {
  if (json.contains("elephant_panic")) {
    const auto panic_obj = json["elephant_panic"].toObject();
    auto* panic = entity->get_component<ElephantPanicComponent>();
    if (panic == nullptr) {
      panic = entity->add_component<ElephantPanicComponent>();
    }
    panic->duration = static_cast<float>(panic_obj["duration"].toDouble(0.0));
  }
}

void write_elephant_stomp_impacts(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_elephant_stomp_impacts(Entity* entity, const QJsonObject& json) {
  if (json.contains("elephant_stomp_impacts")) {
    const auto impacts_array = json["elephant_stomp_impacts"].toArray();
    auto* stomp_impact = entity->add_component<ElephantStompImpactComponent>();
    stomp_impact->impacts.clear();
    stomp_impact->impacts.reserve(impacts_array.size());
    for (const auto value : impacts_array) {
      const auto impact_obj = value.toObject();
      ElephantStompImpactComponent::ImpactRecord impact{};
      impact.x = static_cast<float>(impact_obj["x"].toDouble(0.0));
      impact.z = static_cast<float>(impact_obj["z"].toDouble(0.0));
      impact.time = static_cast<float>(impact_obj["time"].toDouble(0.0));
      stomp_impact->impacts.push_back(impact);
    }
  }
}

void write_wildlife(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* wildlife = entity->get_component<WildlifeComponent>()) {
    QJsonObject wildlife_obj;
    wildlife_obj["species"] =
        QString::fromUtf8(Game::Wildlife::species_name(wildlife->species).data());
    wildlife_obj["behavior"] =
        QString::fromUtf8(Game::Wildlife::behavior_name(wildlife->behavior).data());
    wildlife_obj["group_id"] = static_cast<int>(wildlife->group_id);
    wildlife_obj["home_x"] = static_cast<double>(wildlife->home_x);
    wildlife_obj["home_z"] = static_cast<double>(wildlife->home_z);
    wildlife_obj["roam_radius"] = static_cast<double>(wildlife->roam_radius);
    wildlife_obj["anchor_assigned"] = wildlife->anchor_assigned;
    wildlife_obj["target_x"] = static_cast<double>(wildlife->target_x);
    wildlife_obj["target_z"] = static_cast<double>(wildlife->target_z);
    wildlife_obj["think_cooldown"] = static_cast<double>(wildlife->think_cooldown);
    wildlife_obj["state_timer"] = static_cast<double>(wildlife->state_timer);
    wildlife_obj["alarm_timer"] = static_cast<double>(wildlife->alarm_timer);
    wildlife_obj["hostile_timer"] = static_cast<double>(wildlife->hostile_timer);
    wildlife_obj["bite_timer"] = static_cast<double>(wildlife->bite_timer);
    wildlife_obj["bite_target_id"] = static_cast<qint64>(wildlife->bite_target_id);
    wildlife_obj["bite_impact_pending"] = wildlife->bite_impact_pending;
    wildlife_obj["focus_id"] = static_cast<qint64>(wildlife->focus_id);
    wildlife_obj["aggressor_id"] = static_cast<qint64>(wildlife->aggressor_id);
    wildlife_obj["rng_state"] = static_cast<qint64>(wildlife->rng_state);
    entity_obj["wildlife"] = wildlife_obj;
  }
}

void read_wildlife(Entity* entity, const QJsonObject& json) {
  if (json.contains("wildlife")) {
    const auto wildlife_obj = json["wildlife"].toObject();
    auto* wildlife = entity->add_component<WildlifeComponent>();
    Game::Wildlife::Species species = Game::Wildlife::Species::Sheep;
    if (Game::Wildlife::try_parse_species(
            wildlife_obj["species"].toString().toStdString(), species)) {
      wildlife->species = species;
    }
    Game::Wildlife::Behavior behavior = Game::Wildlife::Behavior::Graze;
    if (Game::Wildlife::try_parse_behavior(
            wildlife_obj["behavior"].toString().toStdString(), behavior)) {
      wildlife->behavior = behavior;
    }
    wildlife->group_id = static_cast<std::uint16_t>(wildlife_obj["group_id"].toInt(0));
    wildlife->home_x = static_cast<float>(wildlife_obj["home_x"].toDouble(0.0));
    wildlife->home_z = static_cast<float>(wildlife_obj["home_z"].toDouble(0.0));
    wildlife->roam_radius =
        static_cast<float>(wildlife_obj["roam_radius"].toDouble(14.0));
    wildlife->anchor_assigned = wildlife_obj["anchor_assigned"].toBool(false);
    wildlife->target_x = static_cast<float>(wildlife_obj["target_x"].toDouble(0.0));
    wildlife->target_z = static_cast<float>(wildlife_obj["target_z"].toDouble(0.0));
    wildlife->think_cooldown =
        static_cast<float>(wildlife_obj["think_cooldown"].toDouble(0.0));
    wildlife->state_timer =
        static_cast<float>(wildlife_obj["state_timer"].toDouble(0.0));
    wildlife->alarm_timer =
        static_cast<float>(wildlife_obj["alarm_timer"].toDouble(0.0));
    wildlife->hostile_timer =
        static_cast<float>(wildlife_obj["hostile_timer"].toDouble(0.0));
    wildlife->bite_timer = static_cast<float>(wildlife_obj["bite_timer"].toDouble(0.0));
    wildlife->bite_target_id =
        static_cast<EntityID>(wildlife_obj["bite_target_id"].toVariant().toULongLong());
    wildlife->bite_impact_pending = wildlife_obj["bite_impact_pending"].toBool(false);
    wildlife->focus_id =
        static_cast<EntityID>(wildlife_obj["focus_id"].toVariant().toULongLong());
    wildlife->aggressor_id =
        static_cast<EntityID>(wildlife_obj["aggressor_id"].toVariant().toULongLong());
    wildlife->rng_state =
        static_cast<std::uint32_t>(wildlife_obj["rng_state"].toVariant().toULongLong());
  }
}

} // namespace

void write_status(const Entity* entity, QJsonObject& entity_obj) {
  write_undead(entity, entity_obj);
  write_cursed_status(entity, entity_obj);
  write_burning_status(entity, entity_obj);
  write_elephant(entity, entity_obj);
  write_elephant_panic(entity, entity_obj);
  write_elephant_stomp_impacts(entity, entity_obj);
  write_wildlife(entity, entity_obj);
}

void read_status(Entity* entity, const QJsonObject& json) {
  read_undead(entity, json);
  read_cursed_status(entity, json);
  read_burning_status(entity, json);
  read_elephant(entity, json);
  read_elephant_panic(entity, json);
  read_elephant_stomp_impacts(entity, json);
  read_wildlife(entity, json);
}

} // namespace Engine::Core::EntityCodec

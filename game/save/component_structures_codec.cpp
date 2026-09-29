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

void write_building(const Entity* entity, QJsonObject& entity_obj) {
  if (entity->get_component<BuildingComponent>() != nullptr) {
    const auto* building = entity->get_component<BuildingComponent>();
    QJsonObject building_obj;
    building_obj["original_nation_id"] =
        Game::Systems::nation_id_to_qstring(building->original_nation_id);
    entity_obj["building"] = building_obj;
  }
}

void read_building(Entity* entity, const QJsonObject& json) {
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
}

void write_capture(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* capture = entity->get_component<CaptureComponent>()) {
    QJsonObject capture_obj;
    capture_obj["capturing_player_id"] = capture->capturing_player_id;
    capture_obj["capture_progress"] = static_cast<double>(capture->capture_progress);
    capture_obj["required_time"] = static_cast<double>(capture->required_time);
    capture_obj["is_being_captured"] = capture->is_being_captured;
    entity_obj["capture"] = capture_obj;
  }
}

void read_capture(Entity* entity, const QJsonObject& json) {
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
}

void write_assault_wave(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* assault_wave = entity->get_component<AssaultWaveComponent>()) {
    QJsonObject assault_obj;
    assault_obj["active"] = assault_wave->active;
    assault_obj["wave_phase"] = assault_wave->wave_phase;
    assault_obj["has_march_target"] = assault_wave->has_march_target;
    assault_obj["march_target_x"] = static_cast<double>(assault_wave->march_target_x);
    assault_obj["march_target_z"] = static_cast<double>(assault_wave->march_target_z);
    entity_obj["assault_wave"] = assault_obj;
  }
}

void read_assault_wave(Entity* entity, const QJsonObject& json) {
  if (json.contains("assault_wave")) {
    const auto assault_obj = json["assault_wave"].toObject();
    auto* assault_wave = entity->add_component<AssaultWaveComponent>();
    assault_wave->active = assault_obj["active"].toBool(true);
    assault_wave->wave_phase = assault_obj["wave_phase"].toInt(0);
    assault_wave->has_march_target = assault_obj["has_march_target"].toBool(false);
    assault_wave->march_target_x =
        static_cast<float>(assault_obj["march_target_x"].toDouble(0.0));
    assault_wave->march_target_z =
        static_cast<float>(assault_obj["march_target_z"].toDouble(0.0));
  }
}

void write_structure_fire(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* structure_fire = entity->get_component<StructureFireComponent>()) {
    QJsonObject structure_fire_obj;
    structure_fire_obj["ignition_progress"] =
        static_cast<double>(structure_fire->ignition_progress);
    structure_fire_obj["ignition_threshold"] =
        static_cast<double>(structure_fire->ignition_threshold);
    structure_fire_obj["duration"] = static_cast<double>(structure_fire->duration);
    structure_fire_obj["remaining_duration"] =
        static_cast<double>(structure_fire->remaining_duration);
    structure_fire_obj["ignition_elapsed"] =
        static_cast<double>(structure_fire->ignition_elapsed);
    structure_fire_obj["tick_interval"] =
        static_cast<double>(structure_fire->tick_interval);
    structure_fire_obj["tick_accumulator"] =
        static_cast<double>(structure_fire->tick_accumulator);
    structure_fire_obj["damage_per_tick"] = structure_fire->damage_per_tick;
    structure_fire_obj["attacker_id"] =
        static_cast<qint64>(structure_fire->attacker_id);
    entity_obj["structure_fire"] = structure_fire_obj;
  }
}

void read_structure_fire(Entity* entity, const QJsonObject& json) {
  if (json.contains("structure_fire")) {
    const auto structure_fire_obj = json["structure_fire"].toObject();
    auto* structure_fire = entity->add_component<StructureFireComponent>();
    structure_fire->ignition_progress =
        static_cast<float>(structure_fire_obj["ignition_progress"].toDouble(
            structure_fire->ignition_progress));
    structure_fire->ignition_threshold =
        static_cast<float>(structure_fire_obj["ignition_threshold"].toDouble(
            structure_fire->ignition_threshold));
    structure_fire->duration = static_cast<float>(
        structure_fire_obj["duration"].toDouble(structure_fire->duration));
    structure_fire->remaining_duration =
        static_cast<float>(structure_fire_obj["remaining_duration"].toDouble(
            structure_fire->remaining_duration));
    structure_fire->ignition_elapsed =
        static_cast<float>(structure_fire_obj["ignition_elapsed"].toDouble(
            structure_fire->ignition_elapsed));
    structure_fire->tick_interval = static_cast<float>(
        structure_fire_obj["tick_interval"].toDouble(structure_fire->tick_interval));
    structure_fire->tick_accumulator =
        static_cast<float>(structure_fire_obj["tick_accumulator"].toDouble(
            structure_fire->tick_accumulator));
    structure_fire->damage_per_tick =
        structure_fire_obj["damage_per_tick"].toInt(structure_fire->damage_per_tick);
    structure_fire->attacker_id = static_cast<EntityID>(
        structure_fire_obj["attacker_id"].toVariant().toULongLong());
  }
}

void write_wall_segment(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* wall = entity->get_component<WallSegmentComponent>()) {
    QJsonObject wall_obj;
    wall_obj["grid_x"] = wall->grid_x;
    wall_obj["grid_z"] = wall->grid_z;
    wall_obj["freeform"] = wall->freeform;
    entity_obj["wall_segment"] = wall_obj;
  }
}

void read_wall_segment(Entity* entity, const QJsonObject& json) {
  if (json.contains("wall_segment")) {
    const auto wall_obj = json["wall_segment"].toObject();
    auto* wall = entity->add_component<WallSegmentComponent>();
    wall->grid_x = wall_obj["grid_x"].toInt(0);
    wall->grid_z = wall_obj["grid_z"].toInt(0);
    wall->freeform = wall_obj["freeform"].toBool(false);
  }
}

void write_wall_construction_site(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* site = entity->get_component<WallConstructionSiteComponent>()) {
    QJsonObject site_obj;
    site_obj["owner_id"] = site->owner_id;
    site_obj["nation_id"] = static_cast<int>(site->nation_id);
    site_obj["build_time"] = static_cast<double>(site->build_time);
    site_obj["progress"] = static_cast<double>(site->progress);
    site_obj["product_type"] =
        QString::fromStdString(Game::Units::spawn_typeToString(site->product_type));
    entity_obj["wall_construction_site"] = site_obj;
  }
}

void read_wall_construction_site(Entity* entity, const QJsonObject& json) {
  if (json.contains("wall_construction_site")) {
    const auto site_obj = json["wall_construction_site"].toObject();
    auto* site = entity->add_component<WallConstructionSiteComponent>();
    site->owner_id = site_obj["owner_id"].toInt(0);
    site->nation_id = static_cast<Game::Systems::NationID>(site_obj["nation_id"].toInt(
        static_cast<int>(Game::Systems::NationID::RomanRepublic)));
    site->build_time = static_cast<float>(site_obj["build_time"].toDouble(0.0));
    site->progress = static_cast<float>(site_obj["progress"].toDouble(0.0));
    if (auto product = Game::Units::spawn_typeFromString(
            site_obj["product_type"].toString("wall_segment").toStdString())) {
      site->product_type = *product;
    }
  }
}

void write_dismantle_site(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* site = entity->get_component<DismantleSiteComponent>()) {
    QJsonObject site_obj;
    site_obj["duration"] = static_cast<double>(site->duration);
    site_obj["progress"] = static_cast<double>(site->progress);
    entity_obj["dismantle_site"] = site_obj;
  }
}

void read_dismantle_site(Entity* entity, const QJsonObject& json) {
  if (json.contains("dismantle_site")) {
    const auto site_obj = json["dismantle_site"].toObject();
    auto* site = entity->add_component<DismantleSiteComponent>();
    site->duration = static_cast<float>(site_obj["duration"].toDouble(1.0));
    site->progress = static_cast<float>(site_obj["progress"].toDouble(0.0));
  }
}

void write_gate(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* gate = entity->get_component<GateComponent>()) {
    QJsonObject gate_obj;
    gate_obj["state"] = static_cast<int>(gate->state);
    gate_obj["manual_mode"] = static_cast<int>(gate->manual_mode);
    gate_obj["open_amount"] = static_cast<double>(gate->open_amount);
    gate_obj["open_speed"] = static_cast<double>(gate->open_speed);
    gate_obj["trigger_radius"] = static_cast<double>(gate->trigger_radius);
    gate_obj["hold_open_seconds"] = static_cast<double>(gate->hold_open_seconds);
    gate_obj["hold_timer"] = static_cast<double>(gate->hold_timer);
    entity_obj["gate"] = gate_obj;
  }
}

void read_gate(Entity* entity, const QJsonObject& json) {
  if (json.contains("gate")) {
    const auto gate_obj = json["gate"].toObject();
    auto* gate = entity->add_component<GateComponent>();
    gate->state = static_cast<GateComponent::State>(
        gate_obj["state"].toInt(static_cast<int>(GateComponent::State::Closed)));
    gate->manual_mode =
        static_cast<GateComponent::ManualMode>(gate_obj["manual_mode"].toInt(
            static_cast<int>(GateComponent::ManualMode::Automatic)));
    gate->open_amount = static_cast<float>(gate_obj["open_amount"].toDouble(0.0));
    gate->open_speed = static_cast<float>(gate_obj["open_speed"].toDouble(1.6));
    gate->trigger_radius = static_cast<float>(gate_obj["trigger_radius"].toDouble(4.0));
    gate->hold_open_seconds =
        static_cast<float>(gate_obj["hold_open_seconds"].toDouble(1.25));
    gate->hold_timer = static_cast<float>(gate_obj["hold_timer"].toDouble(0.0));
  }
}

} // namespace

void write_structures(const Entity* entity, QJsonObject& entity_obj) {
  write_building(entity, entity_obj);
  write_capture(entity, entity_obj);
  write_assault_wave(entity, entity_obj);
  write_structure_fire(entity, entity_obj);
  write_wall_segment(entity, entity_obj);
  write_wall_construction_site(entity, entity_obj);
  write_dismantle_site(entity, entity_obj);
  write_gate(entity, entity_obj);
}

void read_structures(Entity* entity, const QJsonObject& json) {
  read_building(entity, json);
  read_capture(entity, json);
  read_assault_wave(entity, json);
  read_structure_fire(entity, json);
  read_wall_segment(entity, json);
  read_wall_construction_site(entity, json);
  read_dismantle_site(entity, json);
  read_gate(entity, json);
}

} // namespace Engine::Core::EntityCodec

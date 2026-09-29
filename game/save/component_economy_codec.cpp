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
#include "../core/component_economy.h"
#include "../core/entity.h"
#include "entity_codec.h"
#include "serialization.h"

namespace Engine::Core::EntityCodec {

namespace {

void write_production(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* production = entity->get_component<ProductionComponent>()) {
    QJsonObject production_obj;
    production_obj["in_progress"] = production->in_progress;
    production_obj["build_time"] = production->build_time;
    production_obj["time_remaining"] = production->time_remaining;
    production_obj["produced_count"] = production->produced_count;
    production_obj["max_units"] = production->max_units;
    production_obj["manpower_ceiling"] = production->manpower_ceiling;
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
}

void read_production(Entity* entity, const QJsonObject& json) {
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
    production->manpower_ceiling = production_obj["manpower_ceiling"].toInt(0);
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
    for (const auto value : queue_array) {
      production->production_queue.push_back(
          Game::Units::troop_typeFromString(value.toString().toStdString()));
    }
  }
}

void write_builder_production(const Entity* entity, QJsonObject& entity_obj) {
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
    builder_obj["construction_site_entity_id"] =
        static_cast<qint64>(builder->construction_site_entity_id);
    builder_obj["structure_task_entity_id"] =
        static_cast<qint64>(builder->structure_task_entity_id);
    QJsonArray queued_sites;
    for (const auto site_id : builder->queued_construction_site_ids) {
      queued_sites.append(static_cast<qint64>(site_id));
    }
    builder_obj["queued_construction_site_ids"] = queued_sites;
    builder_obj["bypass_movement_active"] = builder->bypass_movement_active;
    builder_obj["bypass_target_x"] = static_cast<double>(builder->bypass_target_x);
    builder_obj["bypass_target_z"] = static_cast<double>(builder->bypass_target_z);
    builder_obj["has_gather_order"] = builder->has_gather_order;
    builder_obj["gather_product_type"] =
        QString::fromStdString(builder->gather_product_type);
    builder_obj["gather_anchor_x"] = static_cast<double>(builder->gather_anchor_x);
    builder_obj["gather_anchor_z"] = static_cast<double>(builder->gather_anchor_z);
    builder_obj["auto_gather"] = builder->auto_gather;
    builder_obj["auto_gather_priority"] =
        QString::fromStdString(builder->auto_gather_priority);
    entity_obj["builder_production"] = builder_obj;
  }
}

void read_builder_production(Entity* entity, const QJsonObject& json) {
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
    builder->construction_site_entity_id = static_cast<EntityID>(
        builder_obj["construction_site_entity_id"].toVariant().toULongLong());
    builder->structure_task_entity_id = static_cast<EntityID>(
        builder_obj["structure_task_entity_id"].toVariant().toULongLong());
    if (builder_obj.contains("queued_construction_site_ids")) {
      const auto queued_sites = builder_obj["queued_construction_site_ids"].toArray();
      builder->queued_construction_site_ids.reserve(queued_sites.size());
      for (const auto value : queued_sites) {
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
    builder->has_gather_order = builder_obj["has_gather_order"].toBool(false);
    builder->gather_product_type =
        builder_obj["gather_product_type"].toString().toStdString();
    builder->gather_anchor_x =
        static_cast<float>(builder_obj["gather_anchor_x"].toDouble(0.0));
    builder->gather_anchor_z =
        static_cast<float>(builder_obj["gather_anchor_z"].toDouble(0.0));
    builder->auto_gather = builder_obj["auto_gather"].toBool(false);
    builder->auto_gather_priority =
        builder_obj["auto_gather_priority"].toString().toStdString();
  }
}

void write_home(const Entity* entity, QJsonObject& entity_obj) {
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
}

void read_home(Entity* entity, const QJsonObject& json) {
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
}

void write_farm(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* farm = entity->get_component<FarmComponent>()) {
    QJsonObject farm_obj;
    farm_obj["growth"] = static_cast<double>(farm->growth);
    farm_obj["cycle_seconds"] = static_cast<double>(farm->cycle_seconds);
    farm_obj["harvests"] = farm->harvests;
    entity_obj["farm"] = farm_obj;
  }
}

void read_farm(Entity* entity, const QJsonObject& json) {
  if (json.contains("farm")) {
    const auto farm_obj = json["farm"].toObject();
    auto* farm = entity->add_component<FarmComponent>();
    farm->growth = static_cast<float>(farm_obj["growth"].toDouble(0.0));
    farm->cycle_seconds = static_cast<float>(farm_obj["cycle_seconds"].toDouble(60.0));
    farm->harvests = farm_obj["harvests"].toInt(0);
  }
}

void write_civilian_delivery(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* delivery = entity->get_component<CivilianDeliveryComponent>()) {
    QJsonObject delivery_obj;
    delivery_obj["target_barracks_id"] =
        static_cast<qint64>(delivery->target_barracks_id);
    entity_obj["civilian_delivery"] = delivery_obj;
  }
}

void read_civilian_delivery(Entity* entity, const QJsonObject& json) {
  if (json.contains("civilian_delivery")) {
    const auto delivery_obj = json["civilian_delivery"].toObject();
    auto* delivery = entity->add_component<CivilianDeliveryComponent>();
    delivery->target_barracks_id = static_cast<EntityID>(
        delivery_obj["target_barracks_id"].toVariant().toULongLong());
  }
}

void write_resource_carry(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* carry = entity->get_component<ResourceCarryComponent>()) {
    QJsonObject carry_obj;
    QJsonObject amounts_obj;
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      int const amount = carry->amounts.get(type);
      if (amount > 0) {
        amounts_obj[QLatin1String(Game::Systems::resource_type_key(type))] = amount;
      }
    }
    carry_obj["amounts"] = amounts_obj;
    carry_obj["depot_entity_id"] = static_cast<qint64>(carry->depot_entity_id);
    carry_obj["has_depot"] = carry->has_depot;
    carry_obj["food_form"] = static_cast<int>(carry->food_form);
    entity_obj["resource_carry"] = carry_obj;
  }
}

void read_resource_carry(Entity* entity, const QJsonObject& json) {
  if (json.contains("resource_carry")) {
    const auto carry_obj = json["resource_carry"].toObject();
    auto* carry = entity->add_component<ResourceCarryComponent>();
    const auto amounts_obj = carry_obj["amounts"].toObject();
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      carry->amounts.set(
          type,
          amounts_obj[QLatin1String(Game::Systems::resource_type_key(type))].toInt(0));
    }
    carry->depot_entity_id =
        static_cast<EntityID>(carry_obj["depot_entity_id"].toVariant().toULongLong());
    carry->has_depot = carry_obj["has_depot"].toBool(false);
    carry->food_form = carry_obj["food_form"].toInt(0) == 1
                           ? Engine::Core::CarriedFoodForm::Meat
                           : Engine::Core::CarriedFoodForm::Grain;
  }
}

void write_settlement_resident(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* resident = entity->get_component<SettlementResidentComponent>()) {
    QJsonObject resident_obj;
    resident_obj["hearth_x"] = static_cast<double>(resident->hearth_x);
    resident_obj["hearth_z"] = static_cast<double>(resident->hearth_z);
    resident_obj["roam_radius"] = static_cast<double>(resident->roam_radius);
    resident_obj["hearth_assigned"] = resident->hearth_assigned;
    resident_obj["released"] = resident->released;
    resident_obj["errand"] = static_cast<int>(resident->errand);
    resident_obj["role"] = static_cast<int>(resident->role);
    resident_obj["focus_id"] = static_cast<qint64>(resident->focus_id);
    resident_obj["errand_x"] = static_cast<double>(resident->errand_x);
    resident_obj["errand_z"] = static_cast<double>(resident->errand_z);
    resident_obj["focus_x"] = static_cast<double>(resident->focus_x);
    resident_obj["focus_z"] = static_cast<double>(resident->focus_z);
    resident_obj["work_elapsed"] = static_cast<double>(resident->work_elapsed);
    resident_obj["errand_remaining"] = static_cast<double>(resident->errand_remaining);
    resident_obj["planned_dwell"] = static_cast<double>(resident->planned_dwell);
    resident_obj["think_cooldown"] = static_cast<double>(resident->think_cooldown);
    resident_obj["rng_state"] = static_cast<qint64>(resident->rng_state);
    entity_obj["settlement_resident"] = resident_obj;
  }
}

void read_settlement_resident(Entity* entity, const QJsonObject& json) {
  if (json.contains("settlement_resident")) {
    const auto resident_obj = json["settlement_resident"].toObject();
    auto* resident = entity->add_component<SettlementResidentComponent>();
    resident->hearth_x = static_cast<float>(resident_obj["hearth_x"].toDouble(0.0));
    resident->hearth_z = static_cast<float>(resident_obj["hearth_z"].toDouble(0.0));
    resident->roam_radius =
        static_cast<float>(resident_obj["roam_radius"].toDouble(16.0));
    resident->hearth_assigned = resident_obj["hearth_assigned"].toBool(false);
    resident->released = resident_obj["released"].toBool(false);
    resident->errand = static_cast<SettlementErrand>(resident_obj["errand"].toInt(0));
    resident->role = static_cast<SettlementErrandRole>(resident_obj["role"].toInt(0));
    resident->focus_id =
        static_cast<EntityID>(resident_obj["focus_id"].toVariant().toULongLong());
    resident->errand_x = static_cast<float>(resident_obj["errand_x"].toDouble(0.0));
    resident->errand_z = static_cast<float>(resident_obj["errand_z"].toDouble(0.0));
    resident->focus_x = static_cast<float>(resident_obj["focus_x"].toDouble(0.0));
    resident->focus_z = static_cast<float>(resident_obj["focus_z"].toDouble(0.0));
    resident->work_elapsed =
        static_cast<float>(resident_obj["work_elapsed"].toDouble(0.0));
    resident->errand_remaining =
        static_cast<float>(resident_obj["errand_remaining"].toDouble(0.0));
    resident->planned_dwell =
        static_cast<float>(resident_obj["planned_dwell"].toDouble(3.0));
    resident->think_cooldown =
        static_cast<float>(resident_obj["think_cooldown"].toDouble(0.0));
    resident->rng_state =
        static_cast<std::uint32_t>(resident_obj["rng_state"].toVariant().toULongLong());
  }
}

} // namespace

void write_economy(const Entity* entity, QJsonObject& entity_obj) {
  write_production(entity, entity_obj);
  write_builder_production(entity, entity_obj);
  write_home(entity, entity_obj);
  write_farm(entity, entity_obj);
  write_civilian_delivery(entity, entity_obj);
  write_resource_carry(entity, entity_obj);
  write_settlement_resident(entity, entity_obj);
}

void read_economy(Entity* entity, const QJsonObject& json) {
  read_production(entity, json);
  read_builder_production(entity, json);
  read_home(entity, json);
  read_farm(entity, json);
  read_civilian_delivery(entity, json);
  read_resource_carry(entity, json);
  read_settlement_resident(entity, json);
}

} // namespace Engine::Core::EntityCodec

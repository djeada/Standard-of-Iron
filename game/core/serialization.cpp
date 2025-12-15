#include "serialization.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../systems/nation_id.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "component.h"
#include "entity.h"
#include "world.h"
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <vector>

#include "../systems/owner_registry.h"

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

auto combat_mode_from_string(const QString &value)
    -> AttackComponent::CombatMode {
  if (value == "melee") {
    return AttackComponent::CombatMode::Melee;
  }
  if (value == "ranged") {
    return AttackComponent::CombatMode::Ranged;
  }
  return AttackComponent::CombatMode::Auto;
}

auto serialize_color(const std::array<float, 3> &color) -> QJsonArray {
  QJsonArray array;
  array.append(color[0]);
  array.append(color[1]);
  array.append(color[2]);
  return array;
}

void deserialize_color(const QJsonArray &array, std::array<float, 3> &color) {
  if (array.size() >= 3) {
    color[0] = static_cast<float>(array.at(0).toDouble());
    color[1] = static_cast<float>(array.at(1).toDouble());
    color[2] = static_cast<float>(array.at(2).toDouble());
  }
}

} // namespace

auto Serialization::serialize_entity(const Entity *entity) -> QJsonObject {
  QJsonObject entity_obj;
  entity_obj["id"] = static_cast<qint64>(entity->get_id());

  if (const auto *transform = entity->get_component<TransformComponent>()) {
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

  if (const auto *renderable = entity->get_component<RenderableComponent>()) {
    QJsonObject renderable_obj;
    renderable_obj["mesh_path"] = QString::fromStdString(renderable->mesh_path);
    renderable_obj["texture_path"] =
        QString::fromStdString(renderable->texture_path);
    if (!renderable->renderer_id.empty()) {
      renderable_obj["renderer_id"] =
          QString::fromStdString(renderable->renderer_id);
    }
    renderable_obj["visible"] = renderable->visible;
    renderable_obj["mesh"] = static_cast<int>(renderable->mesh);
    renderable_obj["color"] = serialize_color(renderable->color);
    entity_obj["renderable"] = renderable_obj;
  }

  if (const auto *unit = entity->get_component<UnitComponent>()) {
    QJsonObject unit_obj;
    unit_obj["health"] = unit->health;
    unit_obj["max_health"] = unit->max_health;
    unit_obj["speed"] = unit->speed;
    unit_obj["vision_range"] = unit->vision_range;
    unit_obj["unit_type"] = QString::fromStdString(
        Game::Units::spawn_typeToString(unit->spawn_type));
    unit_obj["owner_id"] = unit->owner_id;
    unit_obj["nation_id"] = Game::Systems::nationIDToQString(unit->nation_id);
    entity_obj["unit"] = unit_obj;
  }

  if (const auto *movement = entity->get_component<MovementComponent>()) {
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

    QJsonArray path_array;
    for (const auto &waypoint : movement->path) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      path_array.append(waypoint_obj);
    }
    movement_obj["path"] = path_array;
    entity_obj["movement"] = movement_obj;
  }

  if (const auto *attack = entity->get_component<AttackComponent>()) {
    QJsonObject attack_obj;
    attack_obj["range"] = attack->range;
    attack_obj["damage"] = attack->damage;
    attack_obj["cooldown"] = attack->cooldown;
    attack_obj["time_since_last"] = attack->time_since_last;
    attack_obj["melee_range"] = attack->melee_range;
    attack_obj["melee_damage"] = attack->melee_damage;
    attack_obj["melee_cooldown"] = attack->melee_cooldown;
    attack_obj["preferred_mode"] =
        combat_mode_to_string(attack->preferred_mode);
    attack_obj["current_mode"] = combat_mode_to_string(attack->current_mode);
    attack_obj["can_melee"] = attack->can_melee;
    attack_obj["can_ranged"] = attack->can_ranged;
    attack_obj["max_height_difference"] = attack->max_height_difference;
    attack_obj["in_melee_lock"] = attack->in_melee_lock;
    attack_obj["melee_lock_target_id"] =
        static_cast<qint64>(attack->melee_lock_target_id);
    entity_obj["attack"] = attack_obj;
  }

  if (const auto *attack_target =
          entity->get_component<AttackTargetComponent>()) {
    QJsonObject attack_target_obj;
    attack_target_obj["target_id"] =
        static_cast<qint64>(attack_target->target_id);
    attack_target_obj["should_chase"] = attack_target->should_chase;
    entity_obj["attack_target"] = attack_target_obj;
  }

  if (const auto *patrol = entity->get_component<PatrolComponent>()) {
    QJsonObject patrol_obj;
    patrol_obj["current_waypoint"] = static_cast<int>(patrol->current_waypoint);
    patrol_obj["patrolling"] = patrol->patrolling;

    QJsonArray waypoints_array;
    for (const auto &waypoint : patrol->waypoints) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      waypoints_array.append(waypoint_obj);
    }
    patrol_obj["waypoints"] = waypoints_array;
    entity_obj["patrol"] = patrol_obj;
  }

  if (entity->get_component<BuildingComponent>() != nullptr) {
    entity_obj["building"] = true;
  }

  if (const auto *production = entity->get_component<ProductionComponent>()) {
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

    QJsonArray queue_array;
    for (const auto &queued : production->production_queue) {
      queue_array.append(
          QString::fromStdString(Game::Units::troop_typeToString(queued)));
    }
    production_obj["queue"] = queue_array;
    entity_obj["production"] = production_obj;
  }

  if (entity->get_component<AIControlledComponent>() != nullptr) {
    entity_obj["aiControlled"] = true;
  }

  if (const auto *capture = entity->get_component<CaptureComponent>()) {
    QJsonObject capture_obj;
    capture_obj["capturing_player_id"] = capture->capturing_player_id;
    capture_obj["capture_progress"] =
        static_cast<double>(capture->capture_progress);
    capture_obj["required_time"] = static_cast<double>(capture->required_time);
    capture_obj["is_being_captured"] = capture->is_being_captured;
    entity_obj["capture"] = capture_obj;
  }

  if (const auto *hold_mode = entity->get_component<HoldModeComponent>()) {
    QJsonObject hold_mode_obj;
    hold_mode_obj["active"] = hold_mode->active;
    hold_mode_obj["exit_cooldown"] =
        static_cast<double>(hold_mode->exit_cooldown);
    hold_mode_obj["stand_up_duration"] =
        static_cast<double>(hold_mode->stand_up_duration);
    entity_obj["hold_mode"] = hold_mode_obj;
  }

  if (const auto *guard_mode = entity->get_component<GuardModeComponent>()) {
    QJsonObject guard_mode_obj;
    guard_mode_obj["active"] = guard_mode->active;
    guard_mode_obj["guarded_entity_id"] =
        static_cast<qint64>(guard_mode->guarded_entity_id);
    guard_mode_obj["guard_position_x"] =
        static_cast<double>(guard_mode->guard_position_x);
    guard_mode_obj["guard_position_z"] =
        static_cast<double>(guard_mode->guard_position_z);
    guard_mode_obj["guard_radius"] =
        static_cast<double>(guard_mode->guard_radius);
    guard_mode_obj["returning_to_guard_position"] =
        guard_mode->returning_to_guard_position;
    entity_obj["guard_mode"] = guard_mode_obj;
  }

  if (const auto *healer = entity->get_component<HealerComponent>()) {
    QJsonObject healer_obj;
    healer_obj["healing_range"] = static_cast<double>(healer->healing_range);
    healer_obj["healing_amount"] = healer->healing_amount;
    healer_obj["healing_cooldown"] =
        static_cast<double>(healer->healing_cooldown);
    healer_obj["time_since_last_heal"] =
        static_cast<double>(healer->time_since_last_heal);
    entity_obj["healer"] = healer_obj;
  }

  if (const auto *catapult =
          entity->get_component<CatapultLoadingComponent>()) {
    QJsonObject catapult_obj;
    catapult_obj["state"] = static_cast<int>(catapult->state);
    catapult_obj["loading_time"] = static_cast<double>(catapult->loading_time);
    catapult_obj["loading_duration"] =
        static_cast<double>(catapult->loading_duration);
    catapult_obj["firing_time"] = static_cast<double>(catapult->firing_time);
    catapult_obj["firing_duration"] =
        static_cast<double>(catapult->firing_duration);
    catapult_obj["target_id"] = static_cast<qint64>(catapult->target_id);
    catapult_obj["target_locked_x"] =
        static_cast<double>(catapult->target_locked_x);
    catapult_obj["target_locked_y"] =
        static_cast<double>(catapult->target_locked_y);
    catapult_obj["target_locked_z"] =
        static_cast<double>(catapult->target_locked_z);
    catapult_obj["target_position_locked"] = catapult->target_position_locked;
    entity_obj["catapult_loading"] = catapult_obj;
  }

  return entity_obj;
}

void Serialization::deserialize_entity(Entity *entity,
                                       const QJsonObject &json) {
  if (json.contains("transform")) {
    const auto transform_obj = json["transform"].toObject();
    auto *transform = entity->add_component<TransformComponent>();
    transform->position.x =
        static_cast<float>(transform_obj["pos_x"].toDouble());
    transform->position.y =
        static_cast<float>(transform_obj["pos_y"].toDouble());
    transform->position.z =
        static_cast<float>(transform_obj["pos_z"].toDouble());
    transform->rotation.x =
        static_cast<float>(transform_obj["rot_x"].toDouble());
    transform->rotation.y =
        static_cast<float>(transform_obj["rot_y"].toDouble());
    transform->rotation.z =
        static_cast<float>(transform_obj["rot_z"].toDouble());
    transform->scale.x =
        static_cast<float>(transform_obj["scale_x"].toDouble());
    transform->scale.y =
        static_cast<float>(transform_obj["scale_y"].toDouble());
    transform->scale.z =
        static_cast<float>(transform_obj["scale_z"].toDouble());
    transform->has_desired_yaw = transform_obj["has_desired_yaw"].toBool(false);
    transform->desired_yaw =
        static_cast<float>(transform_obj["desired_yaw"].toDouble());
  }

  if (json.contains("renderable")) {
    const auto renderable_obj = json["renderable"].toObject();
    auto *renderable = entity->add_component<RenderableComponent>("", "");
    renderable->mesh_path =
        renderable_obj["mesh_path"].toString().toStdString();
    renderable->texture_path =
        renderable_obj["texture_path"].toString().toStdString();
    renderable->renderer_id =
        renderable_obj["renderer_id"].toString().toStdString();
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
    auto *unit = entity->add_component<UnitComponent>();
    unit->health = unit_obj["health"].toInt(Defaults::kUnitDefaultHealth);
    unit->max_health =
        unit_obj["max_health"].toInt(Defaults::kUnitDefaultHealth);
    unit->speed = static_cast<float>(unit_obj["speed"].toDouble());
    unit->vision_range = static_cast<float>(unit_obj["vision_range"].toDouble(
        static_cast<double>(Defaults::kUnitDefaultVisionRange)));

    QString const unit_type_str = unit_obj["unit_type"].toString();
    Game::Units::SpawnType spawn_type;
    if (Game::Units::tryParseSpawnType(unit_type_str, spawn_type)) {
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
      if (Game::Systems::tryParseNationID(nation_str, nation_id)) {
        unit->nation_id = nation_id;
      } else {
        qWarning() << "Unknown nation ID in save file:" << nation_str
                   << "- using default";
        unit->nation_id = Game::Systems::NationID::RomanRepublic;
      }
    }
  }

  if (json.contains("movement")) {
    const auto movement_obj = json["movement"].toObject();
    auto *movement = entity->add_component<MovementComponent>();
    movement->has_target = movement_obj["has_target"].toBool(false);
    movement->target_x =
        static_cast<float>(movement_obj["target_x"].toDouble());
    movement->target_y =
        static_cast<float>(movement_obj["target_y"].toDouble());
    movement->goal_x = static_cast<float>(movement_obj["goal_x"].toDouble());
    movement->goal_y = static_cast<float>(movement_obj["goal_y"].toDouble());
    movement->vx = static_cast<float>(movement_obj["vx"].toDouble());
    movement->vz = static_cast<float>(movement_obj["vz"].toDouble());
    movement->path_pending = movement_obj["path_pending"].toBool(false);
    movement->pending_request_id = static_cast<std::uint64_t>(
        movement_obj["pending_request_id"].toVariant().toULongLong());
    movement->repath_cooldown =
        static_cast<float>(movement_obj["repath_cooldown"].toDouble());
    movement->last_goal_x =
        static_cast<float>(movement_obj["last_goal_x"].toDouble());
    movement->last_goal_y =
        static_cast<float>(movement_obj["last_goal_y"].toDouble());
    movement->time_since_last_path_request = static_cast<float>(
        movement_obj["time_since_last_path_request"].toDouble());

    movement->path.clear();
    const auto path_array = movement_obj["path"].toArray();
    movement->path.reserve(path_array.size());
    for (const auto &value : path_array) {
      const auto waypoint_obj = value.toObject();
      movement->path.emplace_back(
          static_cast<float>(waypoint_obj["x"].toDouble()),
          static_cast<float>(waypoint_obj["y"].toDouble()));
    }
  }

  if (json.contains("attack")) {
    const auto attack_obj = json["attack"].toObject();
    auto *attack = entity->add_component<AttackComponent>();
    attack->range = static_cast<float>(attack_obj["range"].toDouble());
    attack->damage = attack_obj["damage"].toInt(0);
    attack->cooldown = static_cast<float>(attack_obj["cooldown"].toDouble());
    attack->time_since_last =
        static_cast<float>(attack_obj["time_since_last"].toDouble());
    attack->melee_range = static_cast<float>(attack_obj["melee_range"].toDouble(
        static_cast<double>(Defaults::kAttackMeleeRange)));
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
            static_cast<double>(Defaults::kAttackHeightTolerance)));
    attack->in_melee_lock = attack_obj["in_melee_lock"].toBool(false);
    attack->melee_lock_target_id = static_cast<EntityID>(
        attack_obj["melee_lock_target_id"].toVariant().toULongLong());
  }

  if (json.contains("attack_target")) {
    const auto attack_target_obj = json["attack_target"].toObject();
    auto *attack_target = entity->add_component<AttackTargetComponent>();
    attack_target->target_id = static_cast<EntityID>(
        attack_target_obj["target_id"].toVariant().toULongLong());
    attack_target->should_chase =
        attack_target_obj["should_chase"].toBool(false);
  }

  if (json.contains("patrol")) {
    const auto patrol_obj = json["patrol"].toObject();
    auto *patrol = entity->add_component<PatrolComponent>();
    patrol->current_waypoint = static_cast<size_t>(
        std::max(0, patrol_obj["current_waypoint"].toInt()));
    patrol->patrolling = patrol_obj["patrolling"].toBool(false);

    patrol->waypoints.clear();
    const auto waypoints_array = patrol_obj["waypoints"].toArray();
    patrol->waypoints.reserve(waypoints_array.size());
    for (const auto &value : waypoints_array) {
      const auto waypoint_obj = value.toObject();
      patrol->waypoints.emplace_back(
          static_cast<float>(waypoint_obj["x"].toDouble()),
          static_cast<float>(waypoint_obj["y"].toDouble()));
    }
  }

  if (json.contains("building") && json["building"].toBool()) {
    entity->add_component<BuildingComponent>();
  }

  if (json.contains("production")) {
    const auto production_obj = json["production"].toObject();
    auto *production = entity->add_component<ProductionComponent>();
    production->in_progress = production_obj["in_progress"].toBool(false);
    production->build_time =
        static_cast<float>(production_obj["build_time"].toDouble());
    production->time_remaining =
        static_cast<float>(production_obj["time_remaining"].toDouble());
    production->produced_count = production_obj["produced_count"].toInt(0);
    production->max_units = production_obj["max_units"].toInt(0);
    production->product_type = Game::Units::troop_typeFromString(
        production_obj["product_type"].toString().toStdString());
    production->rally_x =
        static_cast<float>(production_obj["rally_x"].toDouble());
    production->rally_z =
        static_cast<float>(production_obj["rally_z"].toDouble());
    production->rally_set = production_obj["rally_set"].toBool(false);
    production->villager_cost = production_obj["villager_cost"].toInt(1);

    production->production_queue.clear();
    const auto queue_array = production_obj["queue"].toArray();
    production->production_queue.reserve(queue_array.size());
    for (const auto &value : queue_array) {
      production->production_queue.push_back(
          Game::Units::troop_typeFromString(value.toString().toStdString()));
    }
  }

  if (json.contains("aiControlled") && json["aiControlled"].toBool()) {
    entity->add_component<AIControlledComponent>();
  }

  if (json.contains("capture")) {
    const auto capture_obj = json["capture"].toObject();
    auto *capture = entity->add_component<CaptureComponent>();
    capture->capturing_player_id = capture_obj["capturing_player_id"].toInt(-1);
    capture->capture_progress =
        static_cast<float>(capture_obj["capture_progress"].toDouble(0.0));
    capture->required_time =
        static_cast<float>(capture_obj["required_time"].toDouble(
            static_cast<double>(Defaults::kCaptureRequiredTime)));
    capture->is_being_captured = capture_obj["is_being_captured"].toBool(false);
  }

  if (json.contains("hold_mode")) {
    const auto hold_mode_obj = json["hold_mode"].toObject();
    auto *hold_mode = entity->add_component<HoldModeComponent>();
    hold_mode->active = hold_mode_obj["active"].toBool(true);
    hold_mode->exit_cooldown =
        static_cast<float>(hold_mode_obj["exit_cooldown"].toDouble(0.0));
    hold_mode->stand_up_duration =
        static_cast<float>(hold_mode_obj["stand_up_duration"].toDouble(
            static_cast<double>(Defaults::kHoldStandUpDuration)));
  }

  if (json.contains("guard_mode")) {
    const auto guard_mode_obj = json["guard_mode"].toObject();
    auto *guard_mode = entity->add_component<GuardModeComponent>();
    guard_mode->active = guard_mode_obj["active"].toBool(true);
    guard_mode->guarded_entity_id = static_cast<EntityID>(
        guard_mode_obj["guarded_entity_id"].toVariant().toULongLong());
    guard_mode->guard_position_x =
        static_cast<float>(guard_mode_obj["guard_position_x"].toDouble(0.0));
    guard_mode->guard_position_z =
        static_cast<float>(guard_mode_obj["guard_position_z"].toDouble(0.0));
    guard_mode->guard_radius =
        static_cast<float>(guard_mode_obj["guard_radius"].toDouble(
            static_cast<double>(Defaults::kGuardDefaultRadius)));
    guard_mode->returning_to_guard_position =
        guard_mode_obj["returning_to_guard_position"].toBool(false);
  }

  if (json.contains("healer")) {
    const auto healer_obj = json["healer"].toObject();
    auto *healer = entity->add_component<HealerComponent>();
    healer->healing_range =
        static_cast<float>(healer_obj["healing_range"].toDouble(8.0));
    healer->healing_amount = healer_obj["healing_amount"].toInt(5);
    healer->healing_cooldown =
        static_cast<float>(healer_obj["healing_cooldown"].toDouble(2.0));
    healer->time_since_last_heal =
        static_cast<float>(healer_obj["time_since_last_heal"].toDouble(0.0));
  }

  if (json.contains("catapult_loading")) {
    const auto catapult_obj = json["catapult_loading"].toObject();
    auto *catapult = entity->add_component<CatapultLoadingComponent>();
    catapult->state = static_cast<CatapultLoadingComponent::LoadingState>(
        catapult_obj["state"].toInt(
            static_cast<int>(CatapultLoadingComponent::LoadingState::Idle)));
    catapult->loading_time =
        static_cast<float>(catapult_obj["loading_time"].toDouble(0.0));
    catapult->loading_duration =
        static_cast<float>(catapult_obj["loading_duration"].toDouble(2.0));
    catapult->firing_time =
        static_cast<float>(catapult_obj["firing_time"].toDouble(0.0));
    catapult->firing_duration =
        static_cast<float>(catapult_obj["firing_duration"].toDouble(0.5));
    catapult->target_id = static_cast<EntityID>(
        catapult_obj["target_id"].toVariant().toULongLong());
    catapult->target_locked_x =
        static_cast<float>(catapult_obj["target_locked_x"].toDouble(0.0));
    catapult->target_locked_y =
        static_cast<float>(catapult_obj["target_locked_y"].toDouble(0.0));
    catapult->target_locked_z =
        static_cast<float>(catapult_obj["target_locked_z"].toDouble(0.0));
    catapult->target_position_locked =
        catapult_obj["target_position_locked"].toBool(false);
  }
}

auto Serialization::serialize_terrain(
    const Game::Map::TerrainHeightMap *height_map,
    const Game::Map::BiomeSettings &biome,
    const std::vector<Game::Map::RoadSegment> &roads) -> QJsonObject {
  QJsonObject terrain_obj;

  if (height_map == nullptr) {
    return terrain_obj;
  }

  terrain_obj["width"] = height_map->getWidth();
  terrain_obj["height"] = height_map->getHeight();
  terrain_obj["tile_size"] = height_map->getTileSize();

  QJsonArray heights_array;
  const auto &heights = height_map->getHeightData();
  for (float const h : heights) {
    heights_array.append(h);
  }
  terrain_obj["heights"] = heights_array;

  QJsonArray terrain_types_array;
  const auto &terrain_types = height_map->getTerrainTypes();
  for (auto type : terrain_types) {
    terrain_types_array.append(static_cast<int>(type));
  }
  terrain_obj["terrain_types"] = terrain_types_array;

  QJsonArray rivers_array;
  const auto &rivers = height_map->getRiverSegments();
  for (const auto &river : rivers) {
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
  const auto &bridges = height_map->getBridges();
  for (const auto &bridge : bridges) {
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
  for (const auto &road : roads) {
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

  QJsonObject biome_obj;
  biome_obj["grassPrimaryR"] = biome.grass_primary.x();
  biome_obj["grassPrimaryG"] = biome.grass_primary.y();
  biome_obj["grassPrimaryB"] = biome.grass_primary.z();
  biome_obj["grassSecondaryR"] = biome.grass_secondary.x();
  biome_obj["grassSecondaryG"] = biome.grass_secondary.y();
  biome_obj["grassSecondaryB"] = biome.grass_secondary.z();
  biome_obj["grassDryR"] = biome.grass_dry.x();
  biome_obj["grassDryG"] = biome.grass_dry.y();
  biome_obj["grassDryB"] = biome.grass_dry.z();
  biome_obj["soilColorR"] = biome.soil_color.x();
  biome_obj["soilColorG"] = biome.soil_color.y();
  biome_obj["soilColorB"] = biome.soil_color.z();
  biome_obj["rockLowR"] = biome.rock_low.x();
  biome_obj["rockLowG"] = biome.rock_low.y();
  biome_obj["rockLowB"] = biome.rock_low.z();
  biome_obj["rockHighR"] = biome.rock_high.x();
  biome_obj["rockHighG"] = biome.rock_high.y();
  biome_obj["rockHighB"] = biome.rock_high.z();
  biome_obj["patchDensity"] = biome.patch_density;
  biome_obj["patchJitter"] = biome.patch_jitter;
  biome_obj["backgroundBladeDensity"] = biome.background_blade_density;
  biome_obj["bladeHeightMin"] = biome.blade_height_min;
  biome_obj["bladeHeightMax"] = biome.blade_height_max;
  biome_obj["bladeWidthMin"] = biome.blade_width_min;
  biome_obj["bladeWidthMax"] = biome.blade_width_max;
  biome_obj["sway_strength"] = biome.sway_strength;
  biome_obj["sway_speed"] = biome.sway_speed;
  biome_obj["heightNoiseAmplitude"] = biome.height_noise_amplitude;
  biome_obj["heightNoiseFrequency"] = biome.height_noise_frequency;
  biome_obj["terrainMacroNoiseScale"] = biome.terrain_macro_noise_scale;
  biome_obj["terrainDetailNoiseScale"] = biome.terrain_detail_noise_scale;
  biome_obj["terrainSoilHeight"] = biome.terrain_soil_height;
  biome_obj["terrainSoilSharpness"] = biome.terrain_soil_sharpness;
  biome_obj["terrainRockThreshold"] = biome.terrain_rock_threshold;
  biome_obj["terrainRockSharpness"] = biome.terrain_rock_sharpness;
  biome_obj["terrainAmbientBoost"] = biome.terrain_ambient_boost;
  biome_obj["terrainRockDetailStrength"] = biome.terrain_rock_detail_strength;
  biome_obj["backgroundSwayVariance"] = biome.background_sway_variance;
  biome_obj["backgroundScatterRadius"] = biome.background_scatter_radius;
  biome_obj["plant_density"] = biome.plant_density;
  biome_obj["spawnEdgePadding"] = biome.spawn_edge_padding;
  biome_obj["seed"] = static_cast<qint64>(biome.seed);
  terrain_obj["biome"] = biome_obj;

  return terrain_obj;
}

void Serialization::deserialize_terrain(
    Game::Map::TerrainHeightMap *height_map, Game::Map::BiomeSettings &biome,
    std::vector<Game::Map::RoadSegment> &roads, const QJsonObject &json) {
  if ((height_map == nullptr) || json.isEmpty()) {
    return;
  }

  if (json.contains("biome")) {
    const auto biome_obj = json["biome"].toObject();
    const Game::Map::BiomeSettings default_biome{};
    const auto read_color = [&](const QString &base,
                                const QVector3D &fallback) -> QVector3D {
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
    biome.grass_secondary = read_color(QStringLiteral("grassSecondary"),
                                       default_biome.grass_secondary);
    biome.grass_dry =
        read_color(QStringLiteral("grassDry"), default_biome.grass_dry);
    biome.soil_color =
        read_color(QStringLiteral("soilColor"), default_biome.soil_color);
    biome.rock_low =
        read_color(QStringLiteral("rockLow"), default_biome.rock_low);
    biome.rock_high =
        read_color(QStringLiteral("rockHigh"), default_biome.rock_high);

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
    biome.sway_speed = static_cast<float>(
        biome_obj["sway_speed"].toDouble(default_biome.sway_speed));
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
    biome.terrain_soil_height =
        static_cast<float>(biome_obj["terrainSoilHeight"].toDouble(
            default_biome.terrain_soil_height));
    biome.terrain_soil_sharpness =
        static_cast<float>(biome_obj["terrainSoilSharpness"].toDouble(
            default_biome.terrain_soil_sharpness));
    biome.terrain_rock_threshold =
        static_cast<float>(biome_obj["terrainRockThreshold"].toDouble(
            default_biome.terrain_rock_threshold));
    biome.terrain_rock_sharpness =
        static_cast<float>(biome_obj["terrainRockSharpness"].toDouble(
            default_biome.terrain_rock_sharpness));
    biome.terrain_ambient_boost =
        static_cast<float>(biome_obj["terrainAmbientBoost"].toDouble(
            default_biome.terrain_ambient_boost));
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
    biome.spawn_edge_padding =
        static_cast<float>(biome_obj["spawnEdgePadding"].toDouble(
            default_biome.spawn_edge_padding));
    if (biome_obj.contains("seed")) {
      biome.seed = static_cast<std::uint32_t>(
          biome_obj["seed"].toVariant().toULongLong());
    } else {
      biome.seed = default_biome.seed;
    }
  }

  std::vector<float> heights;
  if (json.contains("heights")) {
    const auto heights_array = json["heights"].toArray();
    heights.reserve(heights_array.size());
    for (const auto &val : heights_array) {
      heights.push_back(static_cast<float>(val.toDouble(0.0)));
    }
  }

  std::vector<Game::Map::TerrainType> terrain_types;
  if (json.contains("terrain_types")) {
    const auto types_array = json["terrain_types"].toArray();
    terrain_types.reserve(types_array.size());
    for (const auto &val : types_array) {
      terrain_types.push_back(
          static_cast<Game::Map::TerrainType>(val.toInt(0)));
    }
  }

  std::vector<Game::Map::RiverSegment> rivers;
  if (json.contains("rivers")) {
    const auto rivers_array = json["rivers"].toArray();
    rivers.reserve(rivers_array.size());
    const Game::Map::RiverSegment default_river{};
    for (const auto &val : rivers_array) {
      const auto river_obj = val.toObject();
      Game::Map::RiverSegment river;
      river.start =
          QVector3D(static_cast<float>(river_obj["startX"].toDouble(0.0)),
                    static_cast<float>(river_obj["startY"].toDouble(0.0)),
                    static_cast<float>(river_obj["startZ"].toDouble(0.0)));
      river.end =
          QVector3D(static_cast<float>(river_obj["endX"].toDouble(0.0)),
                    static_cast<float>(river_obj["endY"].toDouble(0.0)),
                    static_cast<float>(river_obj["endZ"].toDouble(0.0)));
      river.width = static_cast<float>(river_obj["width"].toDouble(
          static_cast<double>(default_river.width)));
      rivers.push_back(river);
    }
  }

  std::vector<Game::Map::Bridge> bridges;
  if (json.contains("bridges")) {
    const auto bridges_array = json["bridges"].toArray();
    bridges.reserve(bridges_array.size());
    const Game::Map::Bridge default_bridge{};
    for (const auto &val : bridges_array) {
      const auto bridge_obj = val.toObject();
      Game::Map::Bridge bridge;
      bridge.start =
          QVector3D(static_cast<float>(bridge_obj["startX"].toDouble(0.0)),
                    static_cast<float>(bridge_obj["startY"].toDouble(0.0)),
                    static_cast<float>(bridge_obj["startZ"].toDouble(0.0)));
      bridge.end =
          QVector3D(static_cast<float>(bridge_obj["endX"].toDouble(0.0)),
                    static_cast<float>(bridge_obj["endY"].toDouble(0.0)),
                    static_cast<float>(bridge_obj["endZ"].toDouble(0.0)));
      bridge.width = static_cast<float>(bridge_obj["width"].toDouble(
          static_cast<double>(default_bridge.width)));
      bridge.height = static_cast<float>(bridge_obj["height"].toDouble(
          static_cast<double>(default_bridge.height)));
      bridges.push_back(bridge);
    }
  }

  roads.clear();
  if (json.contains("roads")) {
    const auto roads_array = json["roads"].toArray();
    roads.reserve(roads_array.size());
    const Game::Map::RoadSegment default_road{};
    for (const auto &val : roads_array) {
      const auto road_obj = val.toObject();
      Game::Map::RoadSegment road;
      road.start =
          QVector3D(static_cast<float>(road_obj["startX"].toDouble(0.0)),
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

  height_map->restoreFromData(heights, terrain_types, rivers, bridges);
}

auto Serialization::serialize_world(const World *world) -> QJsonDocument {
  QJsonObject world_obj;
  QJsonArray entities_array;

  const auto &entities = world->get_entities();
  for (const auto &[id, entity] : entities) {
    QJsonObject const entity_obj = serialize_entity(entity.get());
    entities_array.append(entity_obj);
  }

  world_obj["entities"] = entities_array;
  world_obj["nextEntityId"] = static_cast<qint64>(world->get_next_entity_id());
  world_obj["schemaVersion"] = 1;
  world_obj["owner_registry"] =
      Game::Systems::OwnerRegistry::instance().to_json();

  const auto &terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.is_initialized() &&
      (terrain_service.get_height_map() != nullptr)) {
    world_obj["terrain"] = serialize_terrain(terrain_service.get_height_map(),
                                             terrain_service.biome_settings(),
                                             terrain_service.road_segments());
  }

  return QJsonDocument(world_obj);
}

void Serialization::deserialize_world(World *world, const QJsonDocument &doc) {
  auto world_obj = doc.object();
  auto entities_array = world_obj["entities"].toArray();
  for (const auto &value : entities_array) {
    auto entity_obj = value.toObject();
    const auto entity_id =
        static_cast<EntityID>(entity_obj["id"].toVariant().toULongLong());
    auto *entity = entity_id == NULL_ENTITY
                       ? world->create_entity()
                       : world->create_entity_with_id(entity_id);
    if (entity != nullptr) {
      deserialize_entity(entity, entity_obj);
    }
  }

  if (world_obj.contains("nextEntityId")) {
    const auto next_id = static_cast<EntityID>(
        world_obj["nextEntityId"].toVariant().toULongLong());
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
    const float tile_size =
        static_cast<float>(terrain_obj["tile_size"].toDouble(1.0));

    Game::Map::BiomeSettings biome;
    std::vector<Game::Map::RoadSegment> roads;
    std::vector<float> const heights;
    std::vector<Game::Map::TerrainType> const terrain_types;
    std::vector<Game::Map::RiverSegment> const rivers;
    std::vector<Game::Map::Bridge> const bridges;

    auto temp_height_map =
        std::make_unique<Game::Map::TerrainHeightMap>(width, height, tile_size);
    deserialize_terrain(temp_height_map.get(), biome, roads, terrain_obj);

    auto &terrain_service = Game::Map::TerrainService::instance();
    terrain_service.restore_from_serialized(
        width, height, tile_size, temp_height_map->getHeightData(),
        temp_height_map->getTerrainTypes(), temp_height_map->getRiverSegments(),
        roads, temp_height_map->getBridges(), biome);
  }
}

auto Serialization::save_to_file(const QString &filename,
                                 const QJsonDocument &doc) -> bool {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open file for writing:" << filename;
    return false;
  }
  file.write(doc.toJson());
  return true;
}

auto Serialization::load_from_file(const QString &filename) -> QJsonDocument {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Could not open file for reading:" << filename;
    return {};
  }
  const QByteArray data = file.readAll();
  return QJsonDocument::fromJson(data);
}

} // namespace Engine::Core

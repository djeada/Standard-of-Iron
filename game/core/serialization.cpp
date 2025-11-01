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

auto combatModeToString(AttackComponent::CombatMode mode) -> QString {
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

auto combatModeFromString(const QString &value) -> AttackComponent::CombatMode {
  if (value == "melee") {
    return AttackComponent::CombatMode::Melee;
  }
  if (value == "ranged") {
    return AttackComponent::CombatMode::Ranged;
  }
  return AttackComponent::CombatMode::Auto;
}

auto serializeColor(const std::array<float, 3> &color) -> QJsonArray {
  QJsonArray array;
  array.append(color[0]);
  array.append(color[1]);
  array.append(color[2]);
  return array;
}

void deserializeColor(const QJsonArray &array, std::array<float, 3> &color) {
  if (array.size() >= 3) {
    color[0] = static_cast<float>(array.at(0).toDouble());
    color[1] = static_cast<float>(array.at(1).toDouble());
    color[2] = static_cast<float>(array.at(2).toDouble());
  }
}

} // namespace

auto Serialization::serializeEntity(const Entity *entity) -> QJsonObject {
  QJsonObject entity_obj;
  entity_obj["id"] = static_cast<qint64>(entity->getId());

  if (const auto *transform = entity->getComponent<TransformComponent>()) {
    QJsonObject transform_obj;
    transform_obj["posX"] = transform->position.x;
    transform_obj["posY"] = transform->position.y;
    transform_obj["posZ"] = transform->position.z;
    transform_obj["rotX"] = transform->rotation.x;
    transform_obj["rotY"] = transform->rotation.y;
    transform_obj["rotZ"] = transform->rotation.z;
    transform_obj["scale_x"] = transform->scale.x;
    transform_obj["scaleY"] = transform->scale.y;
    transform_obj["scale_z"] = transform->scale.z;
    transform_obj["hasDesiredYaw"] = transform->hasDesiredYaw;
    transform_obj["desiredYaw"] = transform->desiredYaw;
    entity_obj["transform"] = transform_obj;
  }

  if (const auto *renderable = entity->getComponent<RenderableComponent>()) {
    QJsonObject renderable_obj;
    renderable_obj["meshPath"] = QString::fromStdString(renderable->meshPath);
    renderable_obj["texturePath"] =
        QString::fromStdString(renderable->texturePath);
    if (!renderable->rendererId.empty()) {
      renderable_obj["rendererId"] =
          QString::fromStdString(renderable->rendererId);
    }
    renderable_obj["visible"] = renderable->visible;
    renderable_obj["mesh"] = static_cast<int>(renderable->mesh);
    renderable_obj["color"] = serializeColor(renderable->color);
    entity_obj["renderable"] = renderable_obj;
  }

  if (const auto *unit = entity->getComponent<UnitComponent>()) {
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

  if (const auto *movement = entity->getComponent<MovementComponent>()) {
    QJsonObject movement_obj;
    movement_obj["hasTarget"] = movement->hasTarget;
    movement_obj["target_x"] = movement->target_x;
    movement_obj["target_y"] = movement->target_y;
    movement_obj["goalX"] = movement->goalX;
    movement_obj["goalY"] = movement->goalY;
    movement_obj["vx"] = movement->vx;
    movement_obj["vz"] = movement->vz;
    movement_obj["pathPending"] = movement->pathPending;
    movement_obj["pendingRequestId"] =
        static_cast<qint64>(movement->pendingRequestId);
    movement_obj["repathCooldown"] = movement->repathCooldown;
    movement_obj["lastGoalX"] = movement->lastGoalX;
    movement_obj["lastGoalY"] = movement->lastGoalY;
    movement_obj["timeSinceLastPathRequest"] =
        movement->timeSinceLastPathRequest;

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

  if (const auto *attack = entity->getComponent<AttackComponent>()) {
    QJsonObject attack_obj;
    attack_obj["range"] = attack->range;
    attack_obj["damage"] = attack->damage;
    attack_obj["cooldown"] = attack->cooldown;
    attack_obj["timeSinceLast"] = attack->timeSinceLast;
    attack_obj["meleeRange"] = attack->meleeRange;
    attack_obj["meleeDamage"] = attack->meleeDamage;
    attack_obj["meleeCooldown"] = attack->meleeCooldown;
    attack_obj["preferredMode"] = combatModeToString(attack->preferredMode);
    attack_obj["currentMode"] = combatModeToString(attack->currentMode);
    attack_obj["canMelee"] = attack->canMelee;
    attack_obj["canRanged"] = attack->canRanged;
    attack_obj["max_heightDifference"] = attack->max_heightDifference;
    attack_obj["inMeleeLock"] = attack->inMeleeLock;
    attack_obj["meleeLockTargetId"] =
        static_cast<qint64>(attack->meleeLockTargetId);
    entity_obj["attack"] = attack_obj;
  }

  if (const auto *attack_target =
          entity->getComponent<AttackTargetComponent>()) {
    QJsonObject attack_target_obj;
    attack_target_obj["target_id"] =
        static_cast<qint64>(attack_target->target_id);
    attack_target_obj["shouldChase"] = attack_target->shouldChase;
    entity_obj["attack_target"] = attack_target_obj;
  }

  if (const auto *patrol = entity->getComponent<PatrolComponent>()) {
    QJsonObject patrol_obj;
    patrol_obj["currentWaypoint"] = static_cast<int>(patrol->currentWaypoint);
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

  if (entity->getComponent<BuildingComponent>() != nullptr) {
    entity_obj["building"] = true;
  }

  if (const auto *production = entity->getComponent<ProductionComponent>()) {
    QJsonObject production_obj;
    production_obj["inProgress"] = production->inProgress;
    production_obj["buildTime"] = production->buildTime;
    production_obj["timeRemaining"] = production->timeRemaining;
    production_obj["producedCount"] = production->producedCount;
    production_obj["maxUnits"] = production->maxUnits;
    production_obj["product_type"] = QString::fromStdString(
        Game::Units::troop_typeToString(production->product_type));
    production_obj["rallyX"] = production->rallyX;
    production_obj["rallyZ"] = production->rallyZ;
    production_obj["rallySet"] = production->rallySet;
    production_obj["villagerCost"] = production->villagerCost;

    QJsonArray queue_array;
    for (const auto &queued : production->productionQueue) {
      queue_array.append(
          QString::fromStdString(Game::Units::troop_typeToString(queued)));
    }
    production_obj["queue"] = queue_array;
    entity_obj["production"] = production_obj;
  }

  if (entity->getComponent<AIControlledComponent>() != nullptr) {
    entity_obj["aiControlled"] = true;
  }

  if (const auto *capture = entity->getComponent<CaptureComponent>()) {
    QJsonObject capture_obj;
    capture_obj["capturing_player_id"] = capture->capturing_player_id;
    capture_obj["captureProgress"] =
        static_cast<double>(capture->captureProgress);
    capture_obj["requiredTime"] = static_cast<double>(capture->requiredTime);
    capture_obj["isBeingCaptured"] = capture->isBeingCaptured;
    entity_obj["capture"] = capture_obj;
  }

  return entity_obj;
}

void Serialization::deserializeEntity(Entity *entity, const QJsonObject &json) {
  if (json.contains("transform")) {
    const auto transform_obj = json["transform"].toObject();
    auto *transform = entity->addComponent<TransformComponent>();
    transform->position.x =
        static_cast<float>(transform_obj["posX"].toDouble());
    transform->position.y =
        static_cast<float>(transform_obj["posY"].toDouble());
    transform->position.z =
        static_cast<float>(transform_obj["posZ"].toDouble());
    transform->rotation.x =
        static_cast<float>(transform_obj["rotX"].toDouble());
    transform->rotation.y =
        static_cast<float>(transform_obj["rotY"].toDouble());
    transform->rotation.z =
        static_cast<float>(transform_obj["rotZ"].toDouble());
    transform->scale.x =
        static_cast<float>(transform_obj["scale_x"].toDouble());
    transform->scale.y = static_cast<float>(transform_obj["scaleY"].toDouble());
    transform->scale.z =
        static_cast<float>(transform_obj["scale_z"].toDouble());
    transform->hasDesiredYaw = transform_obj["hasDesiredYaw"].toBool(false);
    transform->desiredYaw =
        static_cast<float>(transform_obj["desiredYaw"].toDouble());
  }

  if (json.contains("renderable")) {
    const auto renderable_obj = json["renderable"].toObject();
    auto *renderable = entity->addComponent<RenderableComponent>("", "");
    renderable->meshPath = renderable_obj["meshPath"].toString().toStdString();
    renderable->texturePath =
        renderable_obj["texturePath"].toString().toStdString();
    renderable->rendererId =
        renderable_obj["rendererId"].toString().toStdString();
    renderable->visible = renderable_obj["visible"].toBool(true);
    renderable->mesh =
        static_cast<RenderableComponent::MeshKind>(renderable_obj["mesh"].toInt(
            static_cast<int>(RenderableComponent::MeshKind::Cube)));
    if (renderable_obj.contains("color")) {
      deserializeColor(renderable_obj["color"].toArray(), renderable->color);
    }
  }

  if (json.contains("unit")) {
    const auto unit_obj = json["unit"].toObject();
    auto *unit = entity->addComponent<UnitComponent>();
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
        unit->nation_id = Game::Systems::NationID::KingdomOfIron;
      }
    }
  }

  if (json.contains("movement")) {
    const auto movement_obj = json["movement"].toObject();
    auto *movement = entity->addComponent<MovementComponent>();
    movement->hasTarget = movement_obj["hasTarget"].toBool(false);
    movement->target_x =
        static_cast<float>(movement_obj["target_x"].toDouble());
    movement->target_y =
        static_cast<float>(movement_obj["target_y"].toDouble());
    movement->goalX = static_cast<float>(movement_obj["goalX"].toDouble());
    movement->goalY = static_cast<float>(movement_obj["goalY"].toDouble());
    movement->vx = static_cast<float>(movement_obj["vx"].toDouble());
    movement->vz = static_cast<float>(movement_obj["vz"].toDouble());
    movement->pathPending = movement_obj["pathPending"].toBool(false);
    movement->pendingRequestId = static_cast<std::uint64_t>(
        movement_obj["pendingRequestId"].toVariant().toULongLong());
    movement->repathCooldown =
        static_cast<float>(movement_obj["repathCooldown"].toDouble());
    movement->lastGoalX =
        static_cast<float>(movement_obj["lastGoalX"].toDouble());
    movement->lastGoalY =
        static_cast<float>(movement_obj["lastGoalY"].toDouble());
    movement->timeSinceLastPathRequest =
        static_cast<float>(movement_obj["timeSinceLastPathRequest"].toDouble());

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
    auto *attack = entity->addComponent<AttackComponent>();
    attack->range = static_cast<float>(attack_obj["range"].toDouble());
    attack->damage = attack_obj["damage"].toInt(0);
    attack->cooldown = static_cast<float>(attack_obj["cooldown"].toDouble());
    attack->timeSinceLast =
        static_cast<float>(attack_obj["timeSinceLast"].toDouble());
    attack->meleeRange = static_cast<float>(attack_obj["meleeRange"].toDouble(
        static_cast<double>(Defaults::kAttackMeleeRange)));
    attack->meleeDamage = attack_obj["meleeDamage"].toInt(0);
    attack->meleeCooldown =
        static_cast<float>(attack_obj["meleeCooldown"].toDouble());
    attack->preferredMode =
        combatModeFromString(attack_obj["preferredMode"].toString());
    attack->currentMode =
        combatModeFromString(attack_obj["currentMode"].toString());
    attack->canMelee = attack_obj["canMelee"].toBool(true);
    attack->canRanged = attack_obj["canRanged"].toBool(false);
    attack->max_heightDifference =
        static_cast<float>(attack_obj["max_heightDifference"].toDouble(
            static_cast<double>(Defaults::kAttackHeightTolerance)));
    attack->inMeleeLock = attack_obj["inMeleeLock"].toBool(false);
    attack->meleeLockTargetId = static_cast<EntityID>(
        attack_obj["meleeLockTargetId"].toVariant().toULongLong());
  }

  if (json.contains("attack_target")) {
    const auto attack_target_obj = json["attack_target"].toObject();
    auto *attack_target = entity->addComponent<AttackTargetComponent>();
    attack_target->target_id = static_cast<EntityID>(
        attack_target_obj["target_id"].toVariant().toULongLong());
    attack_target->shouldChase = attack_target_obj["shouldChase"].toBool(false);
  }

  if (json.contains("patrol")) {
    const auto patrol_obj = json["patrol"].toObject();
    auto *patrol = entity->addComponent<PatrolComponent>();
    patrol->currentWaypoint =
        static_cast<size_t>(std::max(0, patrol_obj["currentWaypoint"].toInt()));
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
    entity->addComponent<BuildingComponent>();
  }

  if (json.contains("production")) {
    const auto production_obj = json["production"].toObject();
    auto *production = entity->addComponent<ProductionComponent>();
    production->inProgress = production_obj["inProgress"].toBool(false);
    production->buildTime =
        static_cast<float>(production_obj["buildTime"].toDouble());
    production->timeRemaining =
        static_cast<float>(production_obj["timeRemaining"].toDouble());
    production->producedCount = production_obj["producedCount"].toInt(0);
    production->maxUnits = production_obj["maxUnits"].toInt(0);
    production->product_type = Game::Units::troop_typeFromString(
        production_obj["product_type"].toString().toStdString());
    production->rallyX =
        static_cast<float>(production_obj["rallyX"].toDouble());
    production->rallyZ =
        static_cast<float>(production_obj["rallyZ"].toDouble());
    production->rallySet = production_obj["rallySet"].toBool(false);
    production->villagerCost = production_obj["villagerCost"].toInt(1);

    production->productionQueue.clear();
    const auto queue_array = production_obj["queue"].toArray();
    production->productionQueue.reserve(queue_array.size());
    for (const auto &value : queue_array) {
      production->productionQueue.push_back(
          Game::Units::troop_typeFromString(value.toString().toStdString()));
    }
  }

  if (json.contains("aiControlled") && json["aiControlled"].toBool()) {
    entity->addComponent<AIControlledComponent>();
  }

  if (json.contains("capture")) {
    const auto capture_obj = json["capture"].toObject();
    auto *capture = entity->addComponent<CaptureComponent>();
    capture->capturing_player_id = capture_obj["capturing_player_id"].toInt(-1);
    capture->captureProgress =
        static_cast<float>(capture_obj["captureProgress"].toDouble(0.0));
    capture->requiredTime =
        static_cast<float>(capture_obj["requiredTime"].toDouble(
            static_cast<double>(Defaults::kCaptureRequiredTime)));
    capture->isBeingCaptured = capture_obj["isBeingCaptured"].toBool(false);
  }
}

auto Serialization::serializeTerrain(
    const Game::Map::TerrainHeightMap *height_map,
    const Game::Map::BiomeSettings &biome) -> QJsonObject {
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

  QJsonObject biome_obj;
  biome_obj["grassPrimaryR"] = biome.grassPrimary.x();
  biome_obj["grassPrimaryG"] = biome.grassPrimary.y();
  biome_obj["grassPrimaryB"] = biome.grassPrimary.z();
  biome_obj["grassSecondaryR"] = biome.grassSecondary.x();
  biome_obj["grassSecondaryG"] = biome.grassSecondary.y();
  biome_obj["grassSecondaryB"] = biome.grassSecondary.z();
  biome_obj["grassDryR"] = biome.grassDry.x();
  biome_obj["grassDryG"] = biome.grassDry.y();
  biome_obj["grassDryB"] = biome.grassDry.z();
  biome_obj["soilColorR"] = biome.soilColor.x();
  biome_obj["soilColorG"] = biome.soilColor.y();
  biome_obj["soilColorB"] = biome.soilColor.z();
  biome_obj["rockLowR"] = biome.rockLow.x();
  biome_obj["rockLowG"] = biome.rockLow.y();
  biome_obj["rockLowB"] = biome.rockLow.z();
  biome_obj["rockHighR"] = biome.rockHigh.x();
  biome_obj["rockHighG"] = biome.rockHigh.y();
  biome_obj["rockHighB"] = biome.rockHigh.z();
  biome_obj["patchDensity"] = biome.patchDensity;
  biome_obj["patchJitter"] = biome.patchJitter;
  biome_obj["backgroundBladeDensity"] = biome.backgroundBladeDensity;
  biome_obj["bladeHeightMin"] = biome.bladeHeightMin;
  biome_obj["bladeHeightMax"] = biome.bladeHeightMax;
  biome_obj["bladeWidthMin"] = biome.bladeWidthMin;
  biome_obj["bladeWidthMax"] = biome.bladeWidthMax;
  biome_obj["sway_strength"] = biome.sway_strength;
  biome_obj["sway_speed"] = biome.sway_speed;
  biome_obj["heightNoiseAmplitude"] = biome.heightNoiseAmplitude;
  biome_obj["heightNoiseFrequency"] = biome.heightNoiseFrequency;
  biome_obj["terrainMacroNoiseScale"] = biome.terrainMacroNoiseScale;
  biome_obj["terrainDetailNoiseScale"] = biome.terrainDetailNoiseScale;
  biome_obj["terrainSoilHeight"] = biome.terrainSoilHeight;
  biome_obj["terrainSoilSharpness"] = biome.terrainSoilSharpness;
  biome_obj["terrainRockThreshold"] = biome.terrainRockThreshold;
  biome_obj["terrainRockSharpness"] = biome.terrainRockSharpness;
  biome_obj["terrainAmbientBoost"] = biome.terrainAmbientBoost;
  biome_obj["terrainRockDetailStrength"] = biome.terrainRockDetailStrength;
  biome_obj["backgroundSwayVariance"] = biome.backgroundSwayVariance;
  biome_obj["backgroundScatterRadius"] = biome.backgroundScatterRadius;
  biome_obj["plant_density"] = biome.plant_density;
  biome_obj["spawnEdgePadding"] = biome.spawnEdgePadding;
  biome_obj["seed"] = static_cast<qint64>(biome.seed);
  terrain_obj["biome"] = biome_obj;

  return terrain_obj;
}

void Serialization::deserializeTerrain(Game::Map::TerrainHeightMap *height_map,
                                       Game::Map::BiomeSettings &biome,
                                       const QJsonObject &json) {
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

    biome.grassPrimary =
        read_color(QStringLiteral("grassPrimary"), default_biome.grassPrimary);
    biome.grassSecondary = read_color(QStringLiteral("grassSecondary"),
                                      default_biome.grassSecondary);
    biome.grassDry =
        read_color(QStringLiteral("grassDry"), default_biome.grassDry);
    biome.soilColor =
        read_color(QStringLiteral("soilColor"), default_biome.soilColor);
    biome.rockLow =
        read_color(QStringLiteral("rockLow"), default_biome.rockLow);
    biome.rockHigh =
        read_color(QStringLiteral("rockHigh"), default_biome.rockHigh);

    biome.patchDensity = static_cast<float>(
        biome_obj["patchDensity"].toDouble(default_biome.patchDensity));
    biome.patchJitter = static_cast<float>(
        biome_obj["patchJitter"].toDouble(default_biome.patchJitter));
    biome.backgroundBladeDensity =
        static_cast<float>(biome_obj["backgroundBladeDensity"].toDouble(
            default_biome.backgroundBladeDensity));
    biome.bladeHeightMin = static_cast<float>(
        biome_obj["bladeHeightMin"].toDouble(default_biome.bladeHeightMin));
    biome.bladeHeightMax = static_cast<float>(
        biome_obj["bladeHeightMax"].toDouble(default_biome.bladeHeightMax));
    biome.bladeWidthMin = static_cast<float>(
        biome_obj["bladeWidthMin"].toDouble(default_biome.bladeWidthMin));
    biome.bladeWidthMax = static_cast<float>(
        biome_obj["bladeWidthMax"].toDouble(default_biome.bladeWidthMax));
    biome.sway_strength = static_cast<float>(
        biome_obj["sway_strength"].toDouble(default_biome.sway_strength));
    biome.sway_speed = static_cast<float>(
        biome_obj["sway_speed"].toDouble(default_biome.sway_speed));
    biome.heightNoiseAmplitude =
        static_cast<float>(biome_obj["heightNoiseAmplitude"].toDouble(
            default_biome.heightNoiseAmplitude));
    biome.heightNoiseFrequency =
        static_cast<float>(biome_obj["heightNoiseFrequency"].toDouble(
            default_biome.heightNoiseFrequency));
    biome.terrainMacroNoiseScale =
        static_cast<float>(biome_obj["terrainMacroNoiseScale"].toDouble(
            default_biome.terrainMacroNoiseScale));
    biome.terrainDetailNoiseScale =
        static_cast<float>(biome_obj["terrainDetailNoiseScale"].toDouble(
            default_biome.terrainDetailNoiseScale));
    biome.terrainSoilHeight =
        static_cast<float>(biome_obj["terrainSoilHeight"].toDouble(
            default_biome.terrainSoilHeight));
    biome.terrainSoilSharpness =
        static_cast<float>(biome_obj["terrainSoilSharpness"].toDouble(
            default_biome.terrainSoilSharpness));
    biome.terrainRockThreshold =
        static_cast<float>(biome_obj["terrainRockThreshold"].toDouble(
            default_biome.terrainRockThreshold));
    biome.terrainRockSharpness =
        static_cast<float>(biome_obj["terrainRockSharpness"].toDouble(
            default_biome.terrainRockSharpness));
    biome.terrainAmbientBoost =
        static_cast<float>(biome_obj["terrainAmbientBoost"].toDouble(
            default_biome.terrainAmbientBoost));
    biome.terrainRockDetailStrength =
        static_cast<float>(biome_obj["terrainRockDetailStrength"].toDouble(
            default_biome.terrainRockDetailStrength));
    biome.backgroundSwayVariance =
        static_cast<float>(biome_obj["backgroundSwayVariance"].toDouble(
            default_biome.backgroundSwayVariance));
    biome.backgroundScatterRadius =
        static_cast<float>(biome_obj["backgroundScatterRadius"].toDouble(
            default_biome.backgroundScatterRadius));
    biome.plant_density = static_cast<float>(
        biome_obj["plant_density"].toDouble(default_biome.plant_density));
    biome.spawnEdgePadding = static_cast<float>(
        biome_obj["spawnEdgePadding"].toDouble(default_biome.spawnEdgePadding));
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

  height_map->restoreFromData(heights, terrain_types, rivers, bridges);
}

auto Serialization::serializeWorld(const World *world) -> QJsonDocument {
  QJsonObject world_obj;
  QJsonArray entities_array;

  const auto &entities = world->getEntities();
  for (const auto &[id, entity] : entities) {
    QJsonObject const entity_obj = serializeEntity(entity.get());
    entities_array.append(entity_obj);
  }

  world_obj["entities"] = entities_array;
  world_obj["nextEntityId"] = static_cast<qint64>(world->getNextEntityId());
  world_obj["schemaVersion"] = 1;
  world_obj["owner_registry"] =
      Game::Systems::OwnerRegistry::instance().toJson();

  const auto &terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.isInitialized() &&
      (terrain_service.getHeightMap() != nullptr)) {
    world_obj["terrain"] = serializeTerrain(terrain_service.getHeightMap(),
                                            terrain_service.biomeSettings());
  }

  return QJsonDocument(world_obj);
}

void Serialization::deserializeWorld(World *world, const QJsonDocument &doc) {
  auto world_obj = doc.object();
  auto entities_array = world_obj["entities"].toArray();
  for (const auto &value : entities_array) {
    auto entity_obj = value.toObject();
    const auto entity_id =
        static_cast<EntityID>(entity_obj["id"].toVariant().toULongLong());
    auto *entity = entity_id == NULL_ENTITY
                       ? world->createEntity()
                       : world->createEntityWithId(entity_id);
    if (entity != nullptr) {
      deserializeEntity(entity, entity_obj);
    }
  }

  if (world_obj.contains("nextEntityId")) {
    const auto next_id = static_cast<EntityID>(
        world_obj["nextEntityId"].toVariant().toULongLong());
    world->setNextEntityId(next_id);
  }

  if (world_obj.contains("owner_registry")) {
    Game::Systems::OwnerRegistry::instance().fromJson(
        world_obj["owner_registry"].toObject());
  }

  if (world_obj.contains("terrain")) {
    const auto terrain_obj = world_obj["terrain"].toObject();
    const int width = terrain_obj["width"].toInt(50);
    const int height = terrain_obj["height"].toInt(50);
    const float tile_size =
        static_cast<float>(terrain_obj["tile_size"].toDouble(1.0));

    Game::Map::BiomeSettings biome;
    std::vector<float> const heights;
    std::vector<Game::Map::TerrainType> const terrain_types;
    std::vector<Game::Map::RiverSegment> const rivers;
    std::vector<Game::Map::Bridge> const bridges;

    auto temp_height_map =
        std::make_unique<Game::Map::TerrainHeightMap>(width, height, tile_size);
    deserializeTerrain(temp_height_map.get(), biome, terrain_obj);

    auto &terrain_service = Game::Map::TerrainService::instance();
    terrain_service.restoreFromSerialized(
        width, height, tile_size, temp_height_map->getHeightData(),
        temp_height_map->getTerrainTypes(), temp_height_map->getRiverSegments(),
        temp_height_map->getBridges(), biome);
  }
}

auto Serialization::saveToFile(const QString &filename,
                               const QJsonDocument &doc) -> bool {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open file for writing:" << filename;
    return false;
  }
  file.write(doc.toJson());
  return true;
}

auto Serialization::loadFromFile(const QString &filename) -> QJsonDocument {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Could not open file for reading:" << filename;
    return {};
  }
  const QByteArray data = file.readAll();
  return QJsonDocument::fromJson(data);
}

} // namespace Engine::Core

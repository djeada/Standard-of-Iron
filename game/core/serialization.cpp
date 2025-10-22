#include "serialization.h"
#include "component.h"
#include "entity.h"
#include "world.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector3D>
#include <algorithm>
#include <memory>

#include "../systems/owner_registry.h"

namespace Engine::Core {

namespace {

QString combatModeToString(AttackComponent::CombatMode mode) {
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

AttackComponent::CombatMode combatModeFromString(const QString &value) {
  if (value == "melee") {
    return AttackComponent::CombatMode::Melee;
  }
  if (value == "ranged") {
    return AttackComponent::CombatMode::Ranged;
  }
  return AttackComponent::CombatMode::Auto;
}

QJsonArray serializeColor(const float color[3]) {
  QJsonArray array;
  array.append(color[0]);
  array.append(color[1]);
  array.append(color[2]);
  return array;
}

void deserializeColor(const QJsonArray &array, float color[3]) {
  if (array.size() >= 3) {
    color[0] = static_cast<float>(array.at(0).toDouble());
    color[1] = static_cast<float>(array.at(1).toDouble());
    color[2] = static_cast<float>(array.at(2).toDouble());
  }
}

} // namespace

QJsonObject Serialization::serializeEntity(const Entity *entity) {
  QJsonObject entityObj;
  entityObj["id"] = static_cast<qint64>(entity->getId());

  if (const auto *transform = entity->getComponent<TransformComponent>()) {
    QJsonObject transformObj;
    transformObj["posX"] = transform->position.x;
    transformObj["posY"] = transform->position.y;
    transformObj["posZ"] = transform->position.z;
    transformObj["rotX"] = transform->rotation.x;
    transformObj["rotY"] = transform->rotation.y;
    transformObj["rotZ"] = transform->rotation.z;
    transformObj["scaleX"] = transform->scale.x;
    transformObj["scaleY"] = transform->scale.y;
    transformObj["scaleZ"] = transform->scale.z;
    transformObj["hasDesiredYaw"] = transform->hasDesiredYaw;
    transformObj["desiredYaw"] = transform->desiredYaw;
    entityObj["transform"] = transformObj;
  }

  if (const auto *renderable = entity->getComponent<RenderableComponent>()) {
    QJsonObject renderableObj;
    renderableObj["meshPath"] = QString::fromStdString(renderable->meshPath);
    renderableObj["texturePath"] =
        QString::fromStdString(renderable->texturePath);
    renderableObj["visible"] = renderable->visible;
    renderableObj["mesh"] = static_cast<int>(renderable->mesh);
    renderableObj["color"] = serializeColor(renderable->color);
    entityObj["renderable"] = renderableObj;
  }

  if (const auto *unit = entity->getComponent<UnitComponent>()) {
    QJsonObject unitObj;
    unitObj["health"] = unit->health;
    unitObj["maxHealth"] = unit->maxHealth;
    unitObj["speed"] = unit->speed;
    unitObj["visionRange"] = unit->visionRange;
    unitObj["unitType"] = QString::fromStdString(unit->unitType);
    unitObj["ownerId"] = unit->ownerId;
    entityObj["unit"] = unitObj;
  }

  if (const auto *movement = entity->getComponent<MovementComponent>()) {
    QJsonObject movementObj;
    movementObj["hasTarget"] = movement->hasTarget;
    movementObj["targetX"] = movement->targetX;
    movementObj["targetY"] = movement->targetY;
    movementObj["goalX"] = movement->goalX;
    movementObj["goalY"] = movement->goalY;
    movementObj["vx"] = movement->vx;
    movementObj["vz"] = movement->vz;
    movementObj["pathPending"] = movement->pathPending;
    movementObj["pendingRequestId"] =
        static_cast<qint64>(movement->pendingRequestId);
    movementObj["repathCooldown"] = movement->repathCooldown;
    movementObj["lastGoalX"] = movement->lastGoalX;
    movementObj["lastGoalY"] = movement->lastGoalY;
    movementObj["timeSinceLastPathRequest"] =
        movement->timeSinceLastPathRequest;

    QJsonArray pathArray;
    for (const auto &waypoint : movement->path) {
      QJsonObject waypointObj;
      waypointObj["x"] = waypoint.first;
      waypointObj["y"] = waypoint.second;
      pathArray.append(waypointObj);
    }
    movementObj["path"] = pathArray;
    entityObj["movement"] = movementObj;
  }

  if (const auto *attack = entity->getComponent<AttackComponent>()) {
    QJsonObject attackObj;
    attackObj["range"] = attack->range;
    attackObj["damage"] = attack->damage;
    attackObj["cooldown"] = attack->cooldown;
    attackObj["timeSinceLast"] = attack->timeSinceLast;
    attackObj["meleeRange"] = attack->meleeRange;
    attackObj["meleeDamage"] = attack->meleeDamage;
    attackObj["meleeCooldown"] = attack->meleeCooldown;
    attackObj["preferredMode"] = combatModeToString(attack->preferredMode);
    attackObj["currentMode"] = combatModeToString(attack->currentMode);
    attackObj["canMelee"] = attack->canMelee;
    attackObj["canRanged"] = attack->canRanged;
    attackObj["maxHeightDifference"] = attack->maxHeightDifference;
    attackObj["inMeleeLock"] = attack->inMeleeLock;
    attackObj["meleeLockTargetId"] =
        static_cast<qint64>(attack->meleeLockTargetId);
    entityObj["attack"] = attackObj;
  }

  if (const auto *attackTarget =
          entity->getComponent<AttackTargetComponent>()) {
    QJsonObject attackTargetObj;
    attackTargetObj["targetId"] = static_cast<qint64>(attackTarget->targetId);
    attackTargetObj["shouldChase"] = attackTarget->shouldChase;
    entityObj["attackTarget"] = attackTargetObj;
  }

  if (const auto *patrol = entity->getComponent<PatrolComponent>()) {
    QJsonObject patrolObj;
    patrolObj["currentWaypoint"] = static_cast<int>(patrol->currentWaypoint);
    patrolObj["patrolling"] = patrol->patrolling;

    QJsonArray waypointsArray;
    for (const auto &waypoint : patrol->waypoints) {
      QJsonObject waypointObj;
      waypointObj["x"] = waypoint.first;
      waypointObj["y"] = waypoint.second;
      waypointsArray.append(waypointObj);
    }
    patrolObj["waypoints"] = waypointsArray;
    entityObj["patrol"] = patrolObj;
  }

  if (entity->getComponent<BuildingComponent>()) {
    entityObj["building"] = true;
  }

  if (const auto *production = entity->getComponent<ProductionComponent>()) {
    QJsonObject productionObj;
    productionObj["inProgress"] = production->inProgress;
    productionObj["buildTime"] = production->buildTime;
    productionObj["timeRemaining"] = production->timeRemaining;
    productionObj["producedCount"] = production->producedCount;
    productionObj["maxUnits"] = production->maxUnits;
    productionObj["productType"] =
        QString::fromStdString(production->productType);
    productionObj["rallyX"] = production->rallyX;
    productionObj["rallyZ"] = production->rallyZ;
    productionObj["rallySet"] = production->rallySet;
    productionObj["villagerCost"] = production->villagerCost;

    QJsonArray queueArray;
    for (const auto &queued : production->productionQueue) {
      queueArray.append(QString::fromStdString(queued));
    }
    productionObj["queue"] = queueArray;
    entityObj["production"] = productionObj;
  }

  if (entity->getComponent<AIControlledComponent>()) {
    entityObj["aiControlled"] = true;
  }

  if (const auto *capture = entity->getComponent<CaptureComponent>()) {
    QJsonObject captureObj;
    captureObj["capturingPlayerId"] = capture->capturingPlayerId;
    captureObj["captureProgress"] =
        static_cast<double>(capture->captureProgress);
    captureObj["requiredTime"] = static_cast<double>(capture->requiredTime);
    captureObj["isBeingCaptured"] = capture->isBeingCaptured;
    entityObj["capture"] = captureObj;
  }

  return entityObj;
}

void Serialization::deserializeEntity(Entity *entity, const QJsonObject &json) {
  if (json.contains("transform")) {
    const auto transformObj = json["transform"].toObject();
    auto transform = entity->addComponent<TransformComponent>();
    transform->position.x = static_cast<float>(transformObj["posX"].toDouble());
    transform->position.y = static_cast<float>(transformObj["posY"].toDouble());
    transform->position.z = static_cast<float>(transformObj["posZ"].toDouble());
    transform->rotation.x = static_cast<float>(transformObj["rotX"].toDouble());
    transform->rotation.y = static_cast<float>(transformObj["rotY"].toDouble());
    transform->rotation.z = static_cast<float>(transformObj["rotZ"].toDouble());
    transform->scale.x = static_cast<float>(transformObj["scaleX"].toDouble());
    transform->scale.y = static_cast<float>(transformObj["scaleY"].toDouble());
    transform->scale.z = static_cast<float>(transformObj["scaleZ"].toDouble());
    transform->hasDesiredYaw = transformObj["hasDesiredYaw"].toBool(false);
    transform->desiredYaw =
        static_cast<float>(transformObj["desiredYaw"].toDouble());
  }

  if (json.contains("renderable")) {
    const auto renderableObj = json["renderable"].toObject();
    auto renderable = entity->addComponent<RenderableComponent>("", "");
    renderable->meshPath = renderableObj["meshPath"].toString().toStdString();
    renderable->texturePath =
        renderableObj["texturePath"].toString().toStdString();
    renderable->visible = renderableObj["visible"].toBool(true);
    renderable->mesh =
        static_cast<RenderableComponent::MeshKind>(renderableObj["mesh"].toInt(
            static_cast<int>(RenderableComponent::MeshKind::Cube)));
    if (renderableObj.contains("color")) {
      deserializeColor(renderableObj["color"].toArray(), renderable->color);
    }
  }

  if (json.contains("unit")) {
    const auto unitObj = json["unit"].toObject();
    auto unit = entity->addComponent<UnitComponent>();
    unit->health = unitObj["health"].toInt(100);
    unit->maxHealth = unitObj["maxHealth"].toInt(100);
    unit->speed = static_cast<float>(unitObj["speed"].toDouble());
    unit->visionRange =
        static_cast<float>(unitObj["visionRange"].toDouble(12.0));
    unit->unitType = unitObj["unitType"].toString().toStdString();
    unit->ownerId = unitObj["ownerId"].toInt(0);
  }

  if (json.contains("movement")) {
    const auto movementObj = json["movement"].toObject();
    auto movement = entity->addComponent<MovementComponent>();
    movement->hasTarget = movementObj["hasTarget"].toBool(false);
    movement->targetX = static_cast<float>(movementObj["targetX"].toDouble());
    movement->targetY = static_cast<float>(movementObj["targetY"].toDouble());
    movement->goalX = static_cast<float>(movementObj["goalX"].toDouble());
    movement->goalY = static_cast<float>(movementObj["goalY"].toDouble());
    movement->vx = static_cast<float>(movementObj["vx"].toDouble());
    movement->vz = static_cast<float>(movementObj["vz"].toDouble());
    movement->pathPending = movementObj["pathPending"].toBool(false);
    movement->pendingRequestId = static_cast<std::uint64_t>(
        movementObj["pendingRequestId"].toVariant().toULongLong());
    movement->repathCooldown =
        static_cast<float>(movementObj["repathCooldown"].toDouble());
    movement->lastGoalX =
        static_cast<float>(movementObj["lastGoalX"].toDouble());
    movement->lastGoalY =
        static_cast<float>(movementObj["lastGoalY"].toDouble());
    movement->timeSinceLastPathRequest =
        static_cast<float>(movementObj["timeSinceLastPathRequest"].toDouble());

    movement->path.clear();
    const auto pathArray = movementObj["path"].toArray();
    movement->path.reserve(pathArray.size());
    for (const auto &value : pathArray) {
      const auto waypointObj = value.toObject();
      movement->path.emplace_back(
          static_cast<float>(waypointObj["x"].toDouble()),
          static_cast<float>(waypointObj["y"].toDouble()));
    }
  }

  if (json.contains("attack")) {
    const auto attackObj = json["attack"].toObject();
    auto attack = entity->addComponent<AttackComponent>();
    attack->range = static_cast<float>(attackObj["range"].toDouble());
    attack->damage = attackObj["damage"].toInt(0);
    attack->cooldown = static_cast<float>(attackObj["cooldown"].toDouble());
    attack->timeSinceLast =
        static_cast<float>(attackObj["timeSinceLast"].toDouble());
    attack->meleeRange =
        static_cast<float>(attackObj["meleeRange"].toDouble(1.5));
    attack->meleeDamage = attackObj["meleeDamage"].toInt(0);
    attack->meleeCooldown =
        static_cast<float>(attackObj["meleeCooldown"].toDouble());
    attack->preferredMode =
        combatModeFromString(attackObj["preferredMode"].toString());
    attack->currentMode =
        combatModeFromString(attackObj["currentMode"].toString());
    attack->canMelee = attackObj["canMelee"].toBool(true);
    attack->canRanged = attackObj["canRanged"].toBool(false);
    attack->maxHeightDifference =
        static_cast<float>(attackObj["maxHeightDifference"].toDouble(2.0));
    attack->inMeleeLock = attackObj["inMeleeLock"].toBool(false);
    attack->meleeLockTargetId = static_cast<EntityID>(
        attackObj["meleeLockTargetId"].toVariant().toULongLong());
  }

  if (json.contains("attackTarget")) {
    const auto attackTargetObj = json["attackTarget"].toObject();
    auto attackTarget = entity->addComponent<AttackTargetComponent>();
    attackTarget->targetId = static_cast<EntityID>(
        attackTargetObj["targetId"].toVariant().toULongLong());
    attackTarget->shouldChase = attackTargetObj["shouldChase"].toBool(false);
  }

  if (json.contains("patrol")) {
    const auto patrolObj = json["patrol"].toObject();
    auto patrol = entity->addComponent<PatrolComponent>();
    patrol->currentWaypoint =
        static_cast<size_t>(std::max(0, patrolObj["currentWaypoint"].toInt()));
    patrol->patrolling = patrolObj["patrolling"].toBool(false);

    patrol->waypoints.clear();
    const auto waypointsArray = patrolObj["waypoints"].toArray();
    patrol->waypoints.reserve(waypointsArray.size());
    for (const auto &value : waypointsArray) {
      const auto waypointObj = value.toObject();
      patrol->waypoints.emplace_back(
          static_cast<float>(waypointObj["x"].toDouble()),
          static_cast<float>(waypointObj["y"].toDouble()));
    }
  }

  if (json.contains("building") && json["building"].toBool()) {
    entity->addComponent<BuildingComponent>();
  }

  if (json.contains("production")) {
    const auto productionObj = json["production"].toObject();
    auto production = entity->addComponent<ProductionComponent>();
    production->inProgress = productionObj["inProgress"].toBool(false);
    production->buildTime =
        static_cast<float>(productionObj["buildTime"].toDouble());
    production->timeRemaining =
        static_cast<float>(productionObj["timeRemaining"].toDouble());
    production->producedCount = productionObj["producedCount"].toInt(0);
    production->maxUnits = productionObj["maxUnits"].toInt(0);
    production->productType =
        productionObj["productType"].toString().toStdString();
    production->rallyX = static_cast<float>(productionObj["rallyX"].toDouble());
    production->rallyZ = static_cast<float>(productionObj["rallyZ"].toDouble());
    production->rallySet = productionObj["rallySet"].toBool(false);
    production->villagerCost = productionObj["villagerCost"].toInt(1);

    production->productionQueue.clear();
    const auto queueArray = productionObj["queue"].toArray();
    production->productionQueue.reserve(queueArray.size());
    for (const auto &value : queueArray) {
      production->productionQueue.push_back(value.toString().toStdString());
    }
  }

  if (json.contains("aiControlled") && json["aiControlled"].toBool()) {
    entity->addComponent<AIControlledComponent>();
  }

  if (json.contains("capture")) {
    const auto captureObj = json["capture"].toObject();
    auto capture = entity->addComponent<CaptureComponent>();
    capture->capturingPlayerId = captureObj["capturingPlayerId"].toInt(-1);
    capture->captureProgress =
        static_cast<float>(captureObj["captureProgress"].toDouble(0.0));
    capture->requiredTime =
        static_cast<float>(captureObj["requiredTime"].toDouble(5.0));
    capture->isBeingCaptured = captureObj["isBeingCaptured"].toBool(false);
  }
}

QJsonObject Serialization::serializeTerrain(
    const Game::Map::TerrainHeightMap *heightMap,
    const Game::Map::BiomeSettings &biome) {
  QJsonObject terrainObj;

  if (!heightMap) {
    return terrainObj;
  }

  terrainObj["width"] = heightMap->getWidth();
  terrainObj["height"] = heightMap->getHeight();
  terrainObj["tileSize"] = heightMap->getTileSize();

  QJsonArray heightsArray;
  const auto &heights = heightMap->getHeightData();
  for (float h : heights) {
    heightsArray.append(h);
  }
  terrainObj["heights"] = heightsArray;

  QJsonArray terrainTypesArray;
  const auto &terrainTypes = heightMap->getTerrainTypes();
  for (auto type : terrainTypes) {
    terrainTypesArray.append(static_cast<int>(type));
  }
  terrainObj["terrainTypes"] = terrainTypesArray;

  QJsonArray riversArray;
  const auto &rivers = heightMap->getRiverSegments();
  for (const auto &river : rivers) {
    QJsonObject riverObj;
    riverObj["startX"] = river.start.x();
    riverObj["startY"] = river.start.y();
    riverObj["startZ"] = river.start.z();
    riverObj["endX"] = river.end.x();
    riverObj["endY"] = river.end.y();
    riverObj["endZ"] = river.end.z();
    riverObj["width"] = river.width;
    riversArray.append(riverObj);
  }
  terrainObj["rivers"] = riversArray;

  QJsonArray bridgesArray;
  const auto &bridges = heightMap->getBridges();
  for (const auto &bridge : bridges) {
    QJsonObject bridgeObj;
    bridgeObj["startX"] = bridge.start.x();
    bridgeObj["startY"] = bridge.start.y();
    bridgeObj["startZ"] = bridge.start.z();
    bridgeObj["endX"] = bridge.end.x();
    bridgeObj["endY"] = bridge.end.y();
    bridgeObj["endZ"] = bridge.end.z();
    bridgeObj["width"] = bridge.width;
    bridgeObj["height"] = bridge.height;
    bridgesArray.append(bridgeObj);
  }
  terrainObj["bridges"] = bridgesArray;

  QJsonObject biomeObj;
  biomeObj["grassPrimaryR"] = biome.grassPrimary.x();
  biomeObj["grassPrimaryG"] = biome.grassPrimary.y();
  biomeObj["grassPrimaryB"] = biome.grassPrimary.z();
  biomeObj["grassSecondaryR"] = biome.grassSecondary.x();
  biomeObj["grassSecondaryG"] = biome.grassSecondary.y();
  biomeObj["grassSecondaryB"] = biome.grassSecondary.z();
  biomeObj["grassDryR"] = biome.grassDry.x();
  biomeObj["grassDryG"] = biome.grassDry.y();
  biomeObj["grassDryB"] = biome.grassDry.z();
  biomeObj["soilColorR"] = biome.soilColor.x();
  biomeObj["soilColorG"] = biome.soilColor.y();
  biomeObj["soilColorB"] = biome.soilColor.z();
  biomeObj["rockLowR"] = biome.rockLow.x();
  biomeObj["rockLowG"] = biome.rockLow.y();
  biomeObj["rockLowB"] = biome.rockLow.z();
  biomeObj["rockHighR"] = biome.rockHigh.x();
  biomeObj["rockHighG"] = biome.rockHigh.y();
  biomeObj["rockHighB"] = biome.rockHigh.z();
  biomeObj["patchDensity"] = biome.patchDensity;
  biomeObj["patchJitter"] = biome.patchJitter;
  biomeObj["backgroundBladeDensity"] = biome.backgroundBladeDensity;
  biomeObj["bladeHeightMin"] = biome.bladeHeightMin;
  biomeObj["bladeHeightMax"] = biome.bladeHeightMax;
  biomeObj["bladeWidthMin"] = biome.bladeWidthMin;
  biomeObj["bladeWidthMax"] = biome.bladeWidthMax;
  biomeObj["swayStrength"] = biome.swayStrength;
  biomeObj["swaySpeed"] = biome.swaySpeed;
  biomeObj["heightNoiseAmplitude"] = biome.heightNoiseAmplitude;
  biomeObj["heightNoiseFrequency"] = biome.heightNoiseFrequency;
  biomeObj["terrainMacroNoiseScale"] = biome.terrainMacroNoiseScale;
  biomeObj["terrainDetailNoiseScale"] = biome.terrainDetailNoiseScale;
  biomeObj["terrainSoilHeight"] = biome.terrainSoilHeight;
  biomeObj["terrainSoilSharpness"] = biome.terrainSoilSharpness;
  biomeObj["terrainRockThreshold"] = biome.terrainRockThreshold;
  biomeObj["terrainRockSharpness"] = biome.terrainRockSharpness;
  biomeObj["terrainAmbientBoost"] = biome.terrainAmbientBoost;
  biomeObj["terrainRockDetailStrength"] = biome.terrainRockDetailStrength;
  biomeObj["backgroundSwayVariance"] = biome.backgroundSwayVariance;
  biomeObj["backgroundScatterRadius"] = biome.backgroundScatterRadius;
  biomeObj["plantDensity"] = biome.plantDensity;
  biomeObj["spawnEdgePadding"] = biome.spawnEdgePadding;
  biomeObj["seed"] = static_cast<qint64>(biome.seed);
  terrainObj["biome"] = biomeObj;

  return terrainObj;
}

void Serialization::deserializeTerrain(Game::Map::TerrainHeightMap *heightMap,
                                       Game::Map::BiomeSettings &biome,
                                       const QJsonObject &json) {
  if (!heightMap || json.isEmpty()) {
    return;
  }

  if (json.contains("biome")) {
    const auto biomeObj = json["biome"].toObject();
    biome.grassPrimary = QVector3D(
        static_cast<float>(biomeObj["grassPrimaryR"].toDouble(0.3)),
        static_cast<float>(biomeObj["grassPrimaryG"].toDouble(0.6)),
        static_cast<float>(biomeObj["grassPrimaryB"].toDouble(0.28)));
    biome.grassSecondary = QVector3D(
        static_cast<float>(biomeObj["grassSecondaryR"].toDouble(0.44)),
        static_cast<float>(biomeObj["grassSecondaryG"].toDouble(0.7)),
        static_cast<float>(biomeObj["grassSecondaryB"].toDouble(0.32)));
    biome.grassDry = QVector3D(
        static_cast<float>(biomeObj["grassDryR"].toDouble(0.72)),
        static_cast<float>(biomeObj["grassDryG"].toDouble(0.66)),
        static_cast<float>(biomeObj["grassDryB"].toDouble(0.48)));
    biome.soilColor = QVector3D(
        static_cast<float>(biomeObj["soilColorR"].toDouble(0.28)),
        static_cast<float>(biomeObj["soilColorG"].toDouble(0.24)),
        static_cast<float>(biomeObj["soilColorB"].toDouble(0.18)));
    biome.rockLow = QVector3D(
        static_cast<float>(biomeObj["rockLowR"].toDouble(0.48)),
        static_cast<float>(biomeObj["rockLowG"].toDouble(0.46)),
        static_cast<float>(biomeObj["rockLowB"].toDouble(0.44)));
    biome.rockHigh = QVector3D(
        static_cast<float>(biomeObj["rockHighR"].toDouble(0.68)),
        static_cast<float>(biomeObj["rockHighG"].toDouble(0.69)),
        static_cast<float>(biomeObj["rockHighB"].toDouble(0.73)));
    biome.patchDensity =
        static_cast<float>(biomeObj["patchDensity"].toDouble(4.5));
    biome.patchJitter =
        static_cast<float>(biomeObj["patchJitter"].toDouble(0.95));
    biome.backgroundBladeDensity =
        static_cast<float>(biomeObj["backgroundBladeDensity"].toDouble(0.65));
    biome.bladeHeightMin =
        static_cast<float>(biomeObj["bladeHeightMin"].toDouble(0.55));
    biome.bladeHeightMax =
        static_cast<float>(biomeObj["bladeHeightMax"].toDouble(1.35));
    biome.bladeWidthMin =
        static_cast<float>(biomeObj["bladeWidthMin"].toDouble(0.025));
    biome.bladeWidthMax =
        static_cast<float>(biomeObj["bladeWidthMax"].toDouble(0.055));
    biome.swayStrength =
        static_cast<float>(biomeObj["swayStrength"].toDouble(0.25));
    biome.swaySpeed = static_cast<float>(biomeObj["swaySpeed"].toDouble(1.4));
    biome.heightNoiseAmplitude =
        static_cast<float>(biomeObj["heightNoiseAmplitude"].toDouble(0.16));
    biome.heightNoiseFrequency =
        static_cast<float>(biomeObj["heightNoiseFrequency"].toDouble(0.05));
    biome.terrainMacroNoiseScale =
        static_cast<float>(biomeObj["terrainMacroNoiseScale"].toDouble(0.035));
    biome.terrainDetailNoiseScale =
        static_cast<float>(biomeObj["terrainDetailNoiseScale"].toDouble(0.14));
    biome.terrainSoilHeight =
        static_cast<float>(biomeObj["terrainSoilHeight"].toDouble(0.6));
    biome.terrainSoilSharpness =
        static_cast<float>(biomeObj["terrainSoilSharpness"].toDouble(3.8));
    biome.terrainRockThreshold =
        static_cast<float>(biomeObj["terrainRockThreshold"].toDouble(0.42));
    biome.terrainRockSharpness =
        static_cast<float>(biomeObj["terrainRockSharpness"].toDouble(3.1));
    biome.terrainAmbientBoost =
        static_cast<float>(biomeObj["terrainAmbientBoost"].toDouble(1.08));
    biome.terrainRockDetailStrength = static_cast<float>(
        biomeObj["terrainRockDetailStrength"].toDouble(0.35));
    biome.backgroundSwayVariance =
        static_cast<float>(biomeObj["backgroundSwayVariance"].toDouble(0.2));
    biome.backgroundScatterRadius =
        static_cast<float>(biomeObj["backgroundScatterRadius"].toDouble(0.35));
    biome.plantDensity =
        static_cast<float>(biomeObj["plantDensity"].toDouble(0.5));
    biome.spawnEdgePadding =
        static_cast<float>(biomeObj["spawnEdgePadding"].toDouble(0.08));
    if (biomeObj.contains("seed")) {
      biome.seed = static_cast<std::uint32_t>(
          biomeObj["seed"].toVariant().toULongLong());
    } else {
      biome.seed = 1337u;
    }
  }

  std::vector<float> heights;
  if (json.contains("heights")) {
    const auto heightsArray = json["heights"].toArray();
    heights.reserve(heightsArray.size());
    for (const auto &val : heightsArray) {
      heights.push_back(static_cast<float>(val.toDouble(0.0)));
    }
  }

  std::vector<Game::Map::TerrainType> terrainTypes;
  if (json.contains("terrainTypes")) {
    const auto typesArray = json["terrainTypes"].toArray();
    terrainTypes.reserve(typesArray.size());
    for (const auto &val : typesArray) {
      terrainTypes.push_back(
          static_cast<Game::Map::TerrainType>(val.toInt(0)));
    }
  }

  std::vector<Game::Map::RiverSegment> rivers;
  if (json.contains("rivers")) {
    const auto riversArray = json["rivers"].toArray();
    rivers.reserve(riversArray.size());
    for (const auto &val : riversArray) {
      const auto riverObj = val.toObject();
      Game::Map::RiverSegment river;
      river.start = QVector3D(
          static_cast<float>(riverObj["startX"].toDouble(0.0)),
          static_cast<float>(riverObj["startY"].toDouble(0.0)),
          static_cast<float>(riverObj["startZ"].toDouble(0.0)));
      river.end = QVector3D(
          static_cast<float>(riverObj["endX"].toDouble(0.0)),
          static_cast<float>(riverObj["endY"].toDouble(0.0)),
          static_cast<float>(riverObj["endZ"].toDouble(0.0)));
      river.width = static_cast<float>(riverObj["width"].toDouble(2.0));
      rivers.push_back(river);
    }
  }

  std::vector<Game::Map::Bridge> bridges;
  if (json.contains("bridges")) {
    const auto bridgesArray = json["bridges"].toArray();
    bridges.reserve(bridgesArray.size());
    for (const auto &val : bridgesArray) {
      const auto bridgeObj = val.toObject();
      Game::Map::Bridge bridge;
      bridge.start = QVector3D(
          static_cast<float>(bridgeObj["startX"].toDouble(0.0)),
          static_cast<float>(bridgeObj["startY"].toDouble(0.0)),
          static_cast<float>(bridgeObj["startZ"].toDouble(0.0)));
      bridge.end = QVector3D(
          static_cast<float>(bridgeObj["endX"].toDouble(0.0)),
          static_cast<float>(bridgeObj["endY"].toDouble(0.0)),
          static_cast<float>(bridgeObj["endZ"].toDouble(0.0)));
      bridge.width = static_cast<float>(bridgeObj["width"].toDouble(3.0));
      bridge.height = static_cast<float>(bridgeObj["height"].toDouble(0.5));
      bridges.push_back(bridge);
    }
  }

  heightMap->restoreFromData(heights, terrainTypes, rivers, bridges);
}

QJsonDocument Serialization::serializeWorld(const World *world) {
  QJsonObject worldObj;
  QJsonArray entitiesArray;

  const auto &entities = world->getEntities();
  for (const auto &[id, entity] : entities) {
    QJsonObject entityObj = serializeEntity(entity.get());
    entitiesArray.append(entityObj);
  }

  worldObj["entities"] = entitiesArray;
  worldObj["nextEntityId"] = static_cast<qint64>(world->getNextEntityId());
  worldObj["schemaVersion"] = 1;
  worldObj["ownerRegistry"] = Game::Systems::OwnerRegistry::instance().toJson();

  const auto &terrainService = Game::Map::TerrainService::instance();
  if (terrainService.isInitialized() && terrainService.getHeightMap()) {
    worldObj["terrain"] = serializeTerrain(terrainService.getHeightMap(),
                                           terrainService.biomeSettings());
  }

  return QJsonDocument(worldObj);
}

void Serialization::deserializeWorld(World *world, const QJsonDocument &doc) {
  auto worldObj = doc.object();
  auto entitiesArray = worldObj["entities"].toArray();
  for (const auto &value : entitiesArray) {
    auto entityObj = value.toObject();
    const auto entityId =
        static_cast<EntityID>(entityObj["id"].toVariant().toULongLong());
    auto entity = entityId == NULL_ENTITY ? world->createEntity()
                                          : world->createEntityWithId(entityId);
    if (entity) {
      deserializeEntity(entity, entityObj);
    }
  }

  if (worldObj.contains("nextEntityId")) {
    const auto nextId = static_cast<EntityID>(
        worldObj["nextEntityId"].toVariant().toULongLong());
    world->setNextEntityId(nextId);
  }

  if (worldObj.contains("ownerRegistry")) {
    Game::Systems::OwnerRegistry::instance().fromJson(
        worldObj["ownerRegistry"].toObject());
  }

  if (worldObj.contains("terrain")) {
    const auto terrainObj = worldObj["terrain"].toObject();
    const int width = terrainObj["width"].toInt(50);
    const int height = terrainObj["height"].toInt(50);
    const float tileSize =
        static_cast<float>(terrainObj["tileSize"].toDouble(1.0));

    Game::Map::BiomeSettings biome;
    std::vector<float> heights;
    std::vector<Game::Map::TerrainType> terrainTypes;
    std::vector<Game::Map::RiverSegment> rivers;
    std::vector<Game::Map::Bridge> bridges;

    auto tempHeightMap =
        std::make_unique<Game::Map::TerrainHeightMap>(width, height, tileSize);
    deserializeTerrain(tempHeightMap.get(), biome, terrainObj);

    auto &terrainService = Game::Map::TerrainService::instance();
    terrainService.restoreFromSerialized(
        width, height, tileSize, tempHeightMap->getHeightData(),
        tempHeightMap->getTerrainTypes(), tempHeightMap->getRiverSegments(),
        tempHeightMap->getBridges(), biome);
  }
}

bool Serialization::saveToFile(const QString &filename,
                               const QJsonDocument &doc) {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open file for writing:" << filename;
    return false;
  }
  file.write(doc.toJson());
  return true;
}

QJsonDocument Serialization::loadFromFile(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Could not open file for reading:" << filename;
    return QJsonDocument();
  }
  QByteArray data = file.readAll();
  return QJsonDocument::fromJson(data);
}

} // namespace Engine::Core

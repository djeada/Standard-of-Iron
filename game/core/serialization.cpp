#include "serialization.h"
#include "component.h"
#include "entity.h"
#include "world.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

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

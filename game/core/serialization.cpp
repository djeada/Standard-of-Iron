#include "serialization.h"
#include "component.h"
#include "entity.h"
#include "world.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

namespace Engine::Core {

QJsonObject Serialization::serializeEntity(const Entity *entity) {
  QJsonObject entityObj;
  entityObj["id"] = static_cast<qint64>(entity->getId());

  if (auto transform =
          const_cast<Entity *>(entity)->getComponent<TransformComponent>()) {
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
    entityObj["transform"] = transformObj;
  }

  if (auto unit = const_cast<Entity *>(entity)->getComponent<UnitComponent>()) {
    QJsonObject unitObj;
    unitObj["health"] = unit->health;
    unitObj["maxHealth"] = unit->maxHealth;
    unitObj["speed"] = unit->speed;
    unitObj["unitType"] = QString::fromStdString(unit->unitType);
    unitObj["ownerId"] = unit->ownerId;
    entityObj["unit"] = unitObj;
  }

  return entityObj;
}

void Serialization::deserializeEntity(Entity *entity, const QJsonObject &json) {
  if (json.contains("transform")) {
    auto transformObj = json["transform"].toObject();
    auto transform = entity->addComponent<TransformComponent>();
    transform->position.x = transformObj["posX"].toDouble();
    transform->position.y = transformObj["posY"].toDouble();
    transform->position.z = transformObj["posZ"].toDouble();
    transform->rotation.x = transformObj["rotX"].toDouble();
    transform->rotation.y = transformObj["rotY"].toDouble();
    transform->rotation.z = transformObj["rotZ"].toDouble();
    transform->scale.x = transformObj["scaleX"].toDouble();
    transform->scale.y = transformObj["scaleY"].toDouble();
    transform->scale.z = transformObj["scaleZ"].toDouble();
  }

  if (json.contains("unit")) {
    auto unitObj = json["unit"].toObject();
    auto unit = entity->addComponent<UnitComponent>();
    unit->health = unitObj["health"].toInt();
    unit->maxHealth = unitObj["maxHealth"].toInt();
    unit->speed = unitObj["speed"].toDouble();
    unit->unitType = unitObj["unitType"].toString().toStdString();
    unit->ownerId = unitObj["ownerId"].toInt(0);
  }
}

QJsonDocument Serialization::serializeWorld(const World *world) {
  QJsonObject worldObj;
  QJsonArray entitiesArray;
  
  // Iterate over all entities and serialize them
  const auto &entities = world->getEntities();
  for (const auto &[id, entity] : entities) {
    QJsonObject entityObj = serializeEntity(entity.get());
    entitiesArray.append(entityObj);
  }
  
  worldObj["entities"] = entitiesArray;
  return QJsonDocument(worldObj);
}

void Serialization::deserializeWorld(World *world, const QJsonDocument &doc) {
  auto worldObj = doc.object();
  auto entitiesArray = worldObj["entities"].toArray();
  for (const auto &value : entitiesArray) {
    auto entityObj = value.toObject();
    auto entity = world->createEntity();
    deserializeEntity(entity, entityObj);
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

#pragma once

#include <QJsonObject>

namespace Engine::Core {
class Entity;
}

namespace Engine::Core::EntityCodec {

void write_core(const Entity* entity, QJsonObject& entity_obj);
void read_core(Entity* entity, const QJsonObject& json);

void write_combat(const Entity* entity, QJsonObject& entity_obj);
void read_combat(Entity* entity, const QJsonObject& json);

void write_ability(const Entity* entity, QJsonObject& entity_obj);
void read_ability(Entity* entity, const QJsonObject& json);

void write_status(const Entity* entity, QJsonObject& entity_obj);
void read_status(Entity* entity, const QJsonObject& json);

void write_commander(const Entity* entity, QJsonObject& entity_obj);
void read_commander(Entity* entity, const QJsonObject& json);

void write_economy(const Entity* entity, QJsonObject& entity_obj);
void read_economy(Entity* entity, const QJsonObject& json);

void write_structures(const Entity* entity, QJsonObject& entity_obj);
void read_structures(Entity* entity, const QJsonObject& json);

} // namespace Engine::Core::EntityCodec

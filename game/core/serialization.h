#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

namespace Engine::Core {

class Serialization {
public:
    static QJsonObject serializeEntity(const class Entity* entity);
    static void deserializeEntity(class Entity* entity, const QJsonObject& json);
    
    static QJsonDocument serializeWorld(const class World* world);
    static void deserializeWorld(class World* world, const QJsonDocument& doc);
    
    static bool saveToFile(const QString& filename, const QJsonDocument& doc);
    static QJsonDocument loadFromFile(const QString& filename);
};

} // namespace Engine::Core

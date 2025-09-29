#pragma once

#include <memory>
#include <string>
#include <utility>
#include <QVector3D>

namespace Engine { namespace Core {
class World; class Entity; using EntityID = unsigned int;
struct TransformComponent; struct RenderableComponent; struct UnitComponent; struct MovementComponent; struct AttackComponent;
} }

namespace Game { namespace Units {

struct SpawnParams {
    // world coordinates (XZ plane)
    QVector3D position{0,0,0};
    int playerId = 0;
    std::string unitType; // optional label; renderer may use it
};

// Thin OOP facade over ECS components; no duplicate state.
class Unit {
public:
    virtual ~Unit() = default;

    Engine::Core::EntityID id() const { return m_id; }
    const std::string& type() const { return m_type; }

    // Convenience controls that mutate ECS components
    void moveTo(float x, float z);
    void setSelected(bool sel);
    bool isSelected() const;
    bool isAlive() const;
    QVector3D position() const;

protected:
    Unit(Engine::Core::World& world, const std::string& type);
    Engine::Core::Entity* entity() const; // cached but validated

    // Helpers for derived classes to configure components
    void ensureCoreComponents();

    Engine::Core::World* m_world = nullptr;
    Engine::Core::EntityID m_id = 0;
    std::string m_type;
    // Cached pointers (owned by ECS entity)
    Engine::Core::TransformComponent* m_t = nullptr;
    Engine::Core::RenderableComponent* m_r = nullptr;
    Engine::Core::UnitComponent*       m_u = nullptr;
    Engine::Core::MovementComponent*   m_mv = nullptr;
    Engine::Core::AttackComponent*     m_atk = nullptr;
};

} } // namespace Game::Units

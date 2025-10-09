#pragma once

#include <QVector3D>
#include <memory>
#include <string>
#include <utility>

namespace Engine {
namespace Core {
class World;
class Entity;
using EntityID = unsigned int;
struct TransformComponent;
struct RenderableComponent;
struct UnitComponent;
struct MovementComponent;
struct AttackComponent;
} // namespace Core
} // namespace Engine

namespace Game {
namespace Units {

struct SpawnParams {

  QVector3D position{0, 0, 0};
  int playerId = 0;
  std::string unitType;
  bool aiControlled = false;
};

class Unit {
public:
  virtual ~Unit() = default;

  Engine::Core::EntityID id() const { return m_id; }
  const std::string &type() const { return m_type; }

  void moveTo(float x, float z);
  bool isAlive() const;
  QVector3D position() const;

protected:
  Unit(Engine::Core::World &world, const std::string &type);
  Engine::Core::Entity *entity() const;

  void ensureCoreComponents();

  Engine::Core::World *m_world = nullptr;
  Engine::Core::EntityID m_id = 0;
  std::string m_type;

  Engine::Core::TransformComponent *m_t = nullptr;
  Engine::Core::RenderableComponent *m_r = nullptr;
  Engine::Core::UnitComponent *m_u = nullptr;
  Engine::Core::MovementComponent *m_mv = nullptr;
  Engine::Core::AttackComponent *m_atk = nullptr;
};

} // namespace Units
} // namespace Game

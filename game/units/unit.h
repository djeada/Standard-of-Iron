#pragma once

#include "../systems/nation_id.h"
#include "spawn_type.h"
#include "troop_type.h"
#include <QVector3D>
#include <memory>
#include <string>
#include <utility>

namespace Engine::Core {
class World;
class Entity;
using EntityID = unsigned int;
struct TransformComponent;
struct RenderableComponent;
struct UnitComponent;
struct MovementComponent;
struct AttackComponent;
} // namespace Engine::Core

namespace Game::Units {

struct SpawnParams {

  QVector3D position{0, 0, 0};
  int player_id = 0;
  SpawnType spawn_type = SpawnType::Archer;
  bool aiControlled = false;
  int maxPopulation = 100;
  Game::Systems::NationID nation_id = Game::Systems::NationID::KingdomOfIron;
};

class Unit {
public:
  virtual ~Unit() = default;

  [[nodiscard]] auto id() const -> Engine::Core::EntityID { return m_id; }
  [[nodiscard]] auto type_string() const -> std::string {
    return m_type_string;
  }

  void moveTo(float x, float z);
  [[nodiscard]] auto isAlive() const -> bool;
  [[nodiscard]] auto position() const -> QVector3D;

  void setHoldMode(bool enabled);
  [[nodiscard]] auto isInHoldMode() const -> bool;

protected:
  Unit(Engine::Core::World &world, TroopType type);
  Unit(Engine::Core::World &world, std::string type);
  [[nodiscard]] auto entity() const -> Engine::Core::Entity *;

  void ensureCoreComponents();

  static auto resolve_nation_id(const SpawnParams &params) -> Game::Systems::NationID;

  Engine::Core::World *m_world = nullptr;
  Engine::Core::EntityID m_id = 0;
  std::string m_type_string;

  Engine::Core::TransformComponent *m_t = nullptr;
  Engine::Core::RenderableComponent *m_r = nullptr;
  Engine::Core::UnitComponent *m_u = nullptr;
  Engine::Core::MovementComponent *m_mv = nullptr;
  Engine::Core::AttackComponent *m_atk = nullptr;
};

} // namespace Game::Units

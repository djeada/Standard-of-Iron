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
  bool ai_controlled = false;
  int max_population = 100;
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
};

class Unit {
public:
  virtual ~Unit() = default;

  [[nodiscard]] auto id() const -> Engine::Core::EntityID { return m_id; }
  [[nodiscard]] auto type_string() const -> std::string {
    return m_type_string;
  }

  void move_to(float x, float z);
  [[nodiscard]] auto is_alive() const -> bool;
  [[nodiscard]] auto position() const -> QVector3D;

  void set_hold_mode(bool enabled);
  [[nodiscard]] auto is_in_hold_mode() const -> bool;

  void set_guard_mode(bool enabled);
  void set_guard_target(Engine::Core::EntityID target_id);
  void set_guard_position(float x, float z);
  [[nodiscard]] auto is_in_guard_mode() const -> bool;
  void clear_guard_mode();

  void set_run_mode(bool enabled);
  [[nodiscard]] auto is_running() const -> bool;
  [[nodiscard]] auto can_run() const -> bool;

protected:
  Unit(Engine::Core::World &world, TroopType type);
  Unit(Engine::Core::World &world, std::string type);
  [[nodiscard]] auto entity() const -> Engine::Core::Entity *;

  void ensureCoreComponents();

  static auto
  resolve_nation_id(const SpawnParams &params) -> Game::Systems::NationID;

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

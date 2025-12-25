#pragma once
#include "../core/entity.h"
#include <QVector3D>
#include <memory>

namespace Game::Systems {

class Projectile {
public:
  virtual ~Projectile() = default;

  virtual auto get_start() const -> QVector3D = 0;
  virtual auto get_end() const -> QVector3D = 0;
  virtual auto get_color() const -> QVector3D = 0;
  virtual auto get_speed() const -> float = 0;
  virtual auto get_arc_height() const -> float = 0;
  virtual auto get_progress() const -> float = 0;
  virtual auto get_scale() const -> float = 0;

  virtual auto is_active() const -> bool = 0;
  virtual void update(float delta_time) = 0;
  virtual void deactivate() = 0;

  virtual auto should_apply_damage() const -> bool = 0;
  virtual auto get_damage() const -> int = 0;
  virtual auto get_target_id() const -> Engine::Core::EntityID = 0;
  virtual auto get_attacker_id() const -> Engine::Core::EntityID = 0;
  virtual auto get_target_locked_position() const -> QVector3D = 0;
};

using ProjectilePtr = std::unique_ptr<Projectile>;

} 

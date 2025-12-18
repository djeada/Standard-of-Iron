#pragma once
#include "../core/entity.h"
#include "../core/system.h"
#include "../core/world.h"
#include "../game_config.h"
#include "projectile.h"
#include <QVector3D>
#include <memory>
#include <vector>

namespace Game::Systems {

class ProjectileSystem : public Engine::Core::System {
public:
  ProjectileSystem();
  void update(Engine::Core::World *world, float delta_time) override;

  void spawn_arrow(const QVector3D &start, const QVector3D &end,
                   const QVector3D &color, float speed = 8.0F,
                   bool is_ballista_bolt = false);

  void spawn_stone(const QVector3D &start, const QVector3D &end,
                   const QVector3D &color, float speed = 5.0F,
                   float scale = 1.0F, bool should_apply_damage = false,
                   int damage = 0, Engine::Core::EntityID attacker_id = 0,
                   Engine::Core::EntityID target_id = 0);

  [[nodiscard]] auto projectiles() const -> const std::vector<ProjectilePtr> & {
    return m_projectiles;
  }

private:
  void apply_impact_damage(Engine::Core::World *world, Projectile *projectile);

  std::vector<ProjectilePtr> m_projectiles;
  ArrowConfig m_arrow_config;
};

} 

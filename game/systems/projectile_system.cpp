#include "projectile_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "arrow_projectile.h"
#include "stone_projectile.h"
#include <algorithm>
#include <cmath>
#include <qvectornd.h>

namespace Game::Systems {

ProjectileSystem::ProjectileSystem()
    : m_arrow_config(GameConfig::instance().arrow()) {}

void ProjectileSystem::spawn_arrow(const QVector3D &start, const QVector3D &end,
                                   const QVector3D &color, float speed,
                                   bool is_ballista_bolt) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  float arc_height;
  if (is_ballista_bolt) {

    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist * 0.4F,
                            m_arrow_config.arc_height_min * 0.5F,
                            m_arrow_config.arc_height_max * 0.6F);
  } else {

    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist,
                            m_arrow_config.arc_height_min,
                            m_arrow_config.arc_height_max);
  }
  float inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<ArrowProjectile>(
      start, end, color, speed, arc_height, inv_dist, is_ballista_bolt));
}

void ProjectileSystem::spawn_stone(const QVector3D &start, const QVector3D &end,
                                   const QVector3D &color, float speed,
                                   float scale, bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  constexpr float k_stone_arc_multiplier = 0.35F;
  constexpr float k_stone_arc_min = 1.0F;
  constexpr float k_stone_arc_max = 4.0F;
  float arc_height = std::clamp(k_stone_arc_multiplier * dist, k_stone_arc_min,
                                k_stone_arc_max);
  float inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<StoneProjectile>(
      start, end, color, speed, arc_height, inv_dist, scale,
      should_apply_damage, damage, attacker_id, target_id));
}

void ProjectileSystem::update(Engine::Core::World *world, float delta_time) {
  for (auto &projectile : m_projectiles) {
    projectile->update(delta_time);

    if (projectile->should_apply_damage() && world != nullptr) {
      apply_impact_damage(world, projectile.get());
    }

    if (!projectile->is_active()) {
      continue;
    }
  }

  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                     [](const ProjectilePtr &p) { return !p->is_active(); }),
      m_projectiles.end());
}

void ProjectileSystem::apply_impact_damage(Engine::Core::World *world,
                                           Projectile *projectile) {
  if (projectile == nullptr) {
    return;
  }

  constexpr float k_min_progress_for_impact = 0.98F;
  if (projectile->get_progress() < k_min_progress_for_impact) {
    return;
  }

  if (projectile->get_target_id() == 0) {
    return;
  }

  auto *target = world->get_entity(projectile->get_target_id());
  if (target == nullptr) {
    return;
  }

  if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }

  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr) {
    QVector3D const current_pos(target_transform->position.x,
                                target_transform->position.y,
                                target_transform->position.z);
    float const distance =
        (current_pos - projectile->get_target_locked_position()).length();

    constexpr float k_escape_radius = 1.5F;
    if (distance > k_escape_radius) {
      return;
    }
  }

  target_unit->health -= projectile->get_damage();
  projectile->deactivate();
  if (target_unit->health <= 0) {
    target_unit->health = 0;

    int killer_owner_id = 0;
    if (projectile->get_attacker_id() != 0 && world != nullptr) {
      auto *attacker = world->get_entity(projectile->get_attacker_id());
      if (attacker != nullptr) {
        auto *attacker_unit =
            attacker->get_component<Engine::Core::UnitComponent>();
        if (attacker_unit != nullptr) {
          killer_owner_id = attacker_unit->owner_id;
        }
      }
    }

    Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
        projectile->get_target_id(), target_unit->owner_id,
        target_unit->spawn_type, projectile->get_attacker_id(),
        killer_owner_id));
  }
}

} // namespace Game::Systems

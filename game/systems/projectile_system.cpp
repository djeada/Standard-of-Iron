#include "projectile_system.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>

#include "../core/component.h"
#include "../core/world.h"
#include "arrow_projectile.h"
#include "combat_system/combat_hit_resolver.h"
#include "stone_projectile.h"

namespace Game::Systems {

namespace {

constexpr float k_min_progress_for_impact = 0.98F;
constexpr float k_projectile_escape_radius = 1.5F;

auto target_escaped_impact(const Engine::Core::Entity* target,
                           Projectile* projectile) -> bool {
  if (target == nullptr || projectile == nullptr) {
    return true;
  }

  const auto* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  QVector3D const current_pos(target_transform->position.x,
                              target_transform->position.y,
                              target_transform->position.z);
  return (current_pos - projectile->get_target_locked_position()).length() >
         k_projectile_escape_radius;
}

void apply_projectile_damage(Engine::Core::World* world,
                             Engine::Core::Entity* target,
                             int damage,
                             Engine::Core::EntityID attacker_id,
                             const QVector3D& contact_point,
                             ProjectileKind projectile_kind) {
  if (world == nullptr || target == nullptr || damage <= 0) {
    return;
  }

  (void)Game::Systems::Combat::resolve_projectile_impact_hit(
      world,
      {.contact = {.attacker_id = attacker_id,
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = Engine::Core::CombatAttackFamily::Bow,
                   .attack_direction = Engine::Core::AttackDirection::Thrust,
                   .contact_point = contact_point,
                   .from_projectile = true,
                   .projectile_kind = projectile_kind},
       .explicit_raw_damage = damage});
}

void apply_arrow_like_impact(Engine::Core::World* world,
                             Projectile* projectile,
                             ArrowProjectile* arrow_projectile) {
  if (world == nullptr || projectile == nullptr || arrow_projectile == nullptr) {
    return;
  }

  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    projectile->deactivate();
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    projectile->deactivate();
    return;
  }

  switch (arrow_projectile->get_kind()) {
  case ProjectileKind::Fireball: {
    QVector3D const impact_pos = projectile->get_target_locked_position();
    (void)Game::Systems::Combat::resolve_projectile_area_impact_hit(
        world,
        {.attacker_id = projectile->get_attacker_id(),
         .primary_target_id = projectile->get_target_id(),
         .impact_point = impact_pos,
         .projectile_kind = arrow_projectile->get_kind(),
         .explicit_raw_damage = projectile->get_damage(),
         .radius = arrow_projectile->impact_radius(),
         .splash_damage_multiplier = arrow_projectile->splash_damage_multiplier(),
         .friendly_fire = arrow_projectile->friendly_fire()});
    projectile->deactivate();
    return;
  }
  case ProjectileKind::CursedArrow:
    apply_projectile_damage(world,
                            target,
                            projectile->get_damage(),
                            projectile->get_attacker_id(),
                            projectile->get_target_locked_position(),
                            arrow_projectile->get_kind());
    projectile->deactivate();
    return;
  case ProjectileKind::Arrow:
  case ProjectileKind::Stone:
  default:
    apply_projectile_damage(world,
                            target,
                            projectile->get_damage(),
                            projectile->get_attacker_id(),
                            projectile->get_target_locked_position(),
                            arrow_projectile->get_kind());
    projectile->deactivate();
    return;
  }
}

} // namespace

ProjectileSystem::ProjectileSystem()
    : m_arrow_config(GameConfig::instance().arrow()) {
}

void ProjectileSystem::spawn_arrow(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   bool is_ballista_bolt,
                                   ProjectileKind kind,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id,
                                   float impact_radius,
                                   float splash_damage_multiplier,
                                   bool friendly_fire) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  float arc_height = NAN;
  if (is_ballista_bolt) {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist * 0.4F,
                            m_arrow_config.arc_height_min * 0.5F,
                            m_arrow_config.arc_height_max * 0.6F);
  } else {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist,
                            m_arrow_config.arc_height_min,
                            m_arrow_config.arc_height_max);
  }
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<ArrowProjectile>(start,
                                                            end,
                                                            color,
                                                            speed,
                                                            arc_height,
                                                            inv_dist,
                                                            is_ballista_bolt,
                                                            kind,
                                                            should_apply_damage,
                                                            damage,
                                                            attacker_id,
                                                            target_id,
                                                            impact_radius,
                                                            splash_damage_multiplier,
                                                            friendly_fire));
}

void ProjectileSystem::spawn_stone(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   float scale,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  constexpr float k_stone_arc_multiplier = 0.35F;
  constexpr float k_stone_arc_min = 1.0F;
  constexpr float k_stone_arc_max = 4.0F;
  float const arc_height =
      std::clamp(k_stone_arc_multiplier * dist, k_stone_arc_min, k_stone_arc_max);
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<StoneProjectile>(start,
                                                            end,
                                                            color,
                                                            speed,
                                                            arc_height,
                                                            inv_dist,
                                                            scale,
                                                            should_apply_damage,
                                                            damage,
                                                            attacker_id,
                                                            target_id));
}

void ProjectileSystem::update(Engine::Core::World* world, float delta_time) {
  for (auto& projectile : m_projectiles) {
    projectile->update(delta_time);

    if (projectile->should_apply_damage() && world != nullptr) {
      apply_impact_damage(world, projectile.get());
    }
  }

  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(),
                     m_projectiles.end(),
                     [](const ProjectilePtr& p) { return !p->is_active(); }),
      m_projectiles.end());
}

void ProjectileSystem::apply_impact_damage(Engine::Core::World* world,
                                           Projectile* projectile) {
  if (projectile == nullptr) {
    return;
  }

  if (projectile->get_progress() < k_min_progress_for_impact) {
    return;
  }

  if (projectile->get_target_id() == 0) {
    return;
  }

  if (auto* arrow_projectile = dynamic_cast<ArrowProjectile*>(projectile);
      arrow_projectile != nullptr) {
    apply_arrow_like_impact(world, projectile, arrow_projectile);
    return;
  }

  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    projectile->deactivate();
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    projectile->deactivate();
    return;
  }

  apply_projectile_damage(world,
                          target,
                          projectile->get_damage(),
                          projectile->get_attacker_id(),
                          projectile->get_target_locked_position(),
                          projectile->get_kind());
  projectile->deactivate();
}

} // namespace Game::Systems

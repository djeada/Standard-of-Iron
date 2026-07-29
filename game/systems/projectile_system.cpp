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

constexpr float k_min_progress_for_impact = 1.0F - 1.0e-5F;
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

  QVector3D const current_pos(
      target_transform->position.x, 0.0F, target_transform->position.z);
  QVector3D locked_pos = projectile->get_target_locked_position();
  locked_pos.setY(0.0F);
  return (current_pos - locked_pos).length() > k_projectile_escape_radius;
}

[[nodiscard]] auto apply_projectile_damage(Engine::Core::World* world,
                                           Engine::Core::Entity* target,
                                           int damage,
                                           Engine::Core::EntityID attacker_id,
                                           const QVector3D& contact_point,
                                           ProjectileKind projectile_kind) -> bool {
  if (world == nullptr || target == nullptr || damage <= 0) {
    return false;
  }

  auto const result = Game::Systems::Combat::resolve_projectile_impact_hit(
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
  return result.applied;
}

[[nodiscard]] auto impact_lifetime(ProjectileKind kind, bool ballista_bolt) -> float {
  if (ballista_bolt) {
    return 0.90F;
  }
  switch (kind) {
  case ProjectileKind::Fireball:
    return 1.10F;
  case ProjectileKind::Stone:
    return 1.25F;
  case ProjectileKind::CursedArrow:
    return 0.80F;
  case ProjectileKind::Arrow:
  default:
    return 0.65F;
  }
}

[[nodiscard]] auto
valid_direct_target(Engine::Core::World* world,
                    Projectile* projectile) -> Engine::Core::Entity* {
  if (world == nullptr || projectile == nullptr || projectile->get_target_id() == 0) {
    return nullptr;
  }
  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    return nullptr;
  }
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return nullptr;
  }
  return target;
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
                                   bool friendly_fire,
                                   ArrowVisualStyle visual_style,
                                   std::optional<QVector3D> target_origin_at_launch) {
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
  auto const visual_profile =
      make_arrow_visual_profile(visual_style, ++m_arrow_spawn_sequence);

  m_projectiles.push_back(
      std::make_unique<ArrowProjectile>(start,
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
                                        friendly_fire,
                                        visual_style,
                                        visual_profile,
                                        target_origin_at_launch.value_or(end)));
}

void ProjectileSystem::spawn_stone(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   float scale,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id,
                                   std::optional<QVector3D> target_origin_at_launch) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  constexpr float k_stone_arc_multiplier = 0.35F;
  constexpr float k_stone_arc_min = 1.0F;
  constexpr float k_stone_arc_max = 4.0F;
  float const arc_height =
      std::clamp(k_stone_arc_multiplier * dist, k_stone_arc_min, k_stone_arc_max);
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(
      std::make_unique<StoneProjectile>(start,
                                        end,
                                        color,
                                        speed,
                                        arc_height,
                                        inv_dist,
                                        scale,
                                        should_apply_damage,
                                        damage,
                                        attacker_id,
                                        target_id,
                                        target_origin_at_launch.value_or(end)));
}

void ProjectileSystem::update(Engine::Core::World* world, float delta_time) {
  for (auto& impact : m_impacts) {
    impact.age += std::max(0.0F, delta_time);
  }
  std::erase_if(m_impacts,
                [](auto const& impact) { return impact.age >= impact.lifetime; });

  for (auto& projectile : m_projectiles) {
    if (projectile == nullptr || !projectile->is_active()) {
      continue;
    }
    projectile->update(delta_time);
    if (projectile->get_progress() >= k_min_progress_for_impact) {
      auto const resolution = resolve_impact(world, projectile.get());
      publish_impact(*projectile, resolution);
      projectile->deactivate();
    }
  }

  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(),
                     m_projectiles.end(),
                     [](const ProjectilePtr& p) { return !p->is_active(); }),
      m_projectiles.end());
}

auto ProjectileSystem::resolve_impact(Engine::Core::World* world,
                                      Projectile* projectile) -> ImpactResolution {
  ImpactResolution resolution;
  if (projectile == nullptr || world == nullptr) {
    return resolution;
  }

  if (auto* arrow_projectile = dynamic_cast<ArrowProjectile*>(projectile);
      arrow_projectile != nullptr) {
    if (arrow_projectile->get_kind() == ProjectileKind::Fireball) {
      resolution.hit_target = valid_direct_target(world, projectile) != nullptr;
      if (projectile->should_apply_damage()) {
        auto const area_result =
            Game::Systems::Combat::resolve_projectile_area_impact_hit(
                world,
                {.attacker_id = projectile->get_attacker_id(),
                 .primary_target_id = projectile->get_target_id(),
                 .impact_point = projectile->get_end(),
                 .projectile_kind = arrow_projectile->get_kind(),
                 .explicit_raw_damage = projectile->get_damage(),
                 .radius = arrow_projectile->impact_radius(),
                 .splash_damage_multiplier =
                     arrow_projectile->splash_damage_multiplier(),
                 .friendly_fire = arrow_projectile->friendly_fire()});
        resolution.damage_applied =
            area_result.applied_hits > 0 || area_result.fire_patch_spawned;
      }
      return resolution;
    }
  }

  auto* target = valid_direct_target(world, projectile);
  resolution.hit_target = target != nullptr;
  if (target != nullptr && projectile->should_apply_damage()) {
    resolution.damage_applied = apply_projectile_damage(world,
                                                        target,
                                                        projectile->get_damage(),
                                                        projectile->get_attacker_id(),
                                                        projectile->get_end(),
                                                        projectile->get_kind());
  }
  return resolution;
}

void ProjectileSystem::publish_impact(const Projectile& projectile,
                                      const ImpactResolution& resolution) {
  bool ballista_bolt = false;
  if (auto const* arrow = dynamic_cast<const ArrowProjectile*>(&projectile)) {
    ballista_bolt = arrow->is_ballista_bolt();
  }
  QVector3D incoming_direction = projectile.get_end() - projectile.get_start();
  if (incoming_direction.lengthSquared() > 1.0e-6F) {
    incoming_direction.normalize();
  } else {
    incoming_direction = QVector3D(0.0F, -1.0F, 0.0F);
  }
  m_impacts.push_back({
      .sequence = ++m_impact_sequence,
      .position = projectile.get_end(),
      .incoming_direction = incoming_direction,
      .color = projectile.get_color(),
      .kind = projectile.get_kind(),
      .age = 0.0F,
      .lifetime = impact_lifetime(projectile.get_kind(), ballista_bolt),
      .scale = projectile.get_scale(),
      .ballista_bolt = ballista_bolt,
      .hit_target = resolution.hit_target,
      .damage_applied = resolution.damage_applied,
      .attacker_id = projectile.get_attacker_id(),
      .target_id = projectile.get_target_id(),
  });
}

} // namespace Game::Systems

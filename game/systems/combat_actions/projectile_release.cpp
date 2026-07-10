#include "projectile_release.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../visuals/team_colors.h"
#include "../combat_system/combat_types.h"
#include "../combat_system/combat_utils.h"
#include "../projectile_system.h"

namespace Game::Systems::CombatActions {

namespace {

[[nodiscard]] auto projectile_color_for_kind(Game::Systems::ProjectileKind kind,
                                             const QVector3D& team_color) -> QVector3D {
  switch (kind) {
  case Game::Systems::ProjectileKind::Fireball:
    return {1.0F, 0.45F, 0.10F};
  case Game::Systems::ProjectileKind::CursedArrow:
    return {0.58F, 0.20F, 0.82F};
  case Game::Systems::ProjectileKind::Arrow:
  case Game::Systems::ProjectileKind::Stone:
  default:
    return team_color;
  }
}

[[nodiscard]] auto
projectile_speed_for_kind(Game::Systems::ProjectileKind kind) -> float {
  switch (kind) {
  case Game::Systems::ProjectileKind::Fireball:
    return Game::Systems::Combat::Constants::k_arrow_speed * 0.72F;
  case Game::Systems::ProjectileKind::CursedArrow:
    return Game::Systems::Combat::Constants::k_arrow_speed * 0.92F;
  case Game::Systems::ProjectileKind::Arrow:
  case Game::Systems::ProjectileKind::Stone:
  default:
    return Game::Systems::Combat::Constants::k_arrow_speed;
  }
}

[[nodiscard]] auto
resolve_target(Engine::Core::World* world,
               Engine::Core::Entity& attacker,
               Engine::Core::EntityID target_hint_id) -> Engine::Core::Entity* {
  if (world == nullptr || target_hint_id == 0) {
    return nullptr;
  }

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* target = world->get_entity(target_hint_id);
  if (attacker_unit == nullptr ||
      !Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, target, true)) {
    return nullptr;
  }
  return target;
}

} // namespace

auto release_projectile_for_action(Engine::Core::World* world,
                                   Engine::Core::Entity& attacker,
                                   const CombatActionDefinition& definition,
                                   Engine::Core::EntityID target_hint_id)
    -> ProjectileReleaseResult {
  ProjectileReleaseResult result;
  if (world == nullptr || !definition.requires_projectile_release) {
    return result;
  }
  result.attempted = true;

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  auto* target = resolve_target(world, attacker, target_hint_id);
  if (projectile_system == nullptr || target == nullptr) {
    return result;
  }

  auto* attacker_transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  auto* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* attack = attacker.get_component<Engine::Core::AttackComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr ||
      attacker_unit == nullptr || attack == nullptr) {
    return result;
  }

  QVector3D const attacker_pos(attacker_transform->position.x,
                               attacker_transform->position.y,
                               attacker_transform->position.z);
  QVector3D const target_pos(target_transform->position.x,
                             target_transform->position.y,
                             target_transform->position.z);
  QVector3D direction = target_pos - attacker_pos;
  if (direction.lengthSquared() <= 0.000001F) {
    direction = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    direction.normalize();
  }

  auto const* special_attack =
      attacker.get_component<Engine::Core::SpecialAttackComponent>();
  Game::Systems::ProjectileKind projectile_kind = Game::Systems::ProjectileKind::Arrow;
  float splash_radius = 0.0F;
  float splash_damage_multiplier = 0.6F;
  bool friendly_fire = false;
  if (special_attack != nullptr && special_attack->use_projectile_system) {
    projectile_kind = special_attack->projectile_kind;
    splash_radius = special_attack->splash_radius;
    splash_damage_multiplier = special_attack->splash_damage_multiplier;
    friendly_fire = special_attack->friendly_fire;
  }

  int const damage = std::max(
      1,
      static_cast<int>(std::round(static_cast<float>(attack->get_current_damage()) *
                                  definition.damage.base_multiplier)));
  QVector3D const team_color =
      Game::Visuals::team_colorForOwner(attacker_unit->owner_id);
  QVector3D const start =
      attacker_pos + QVector3D(0.0F, 1.15F, 0.0F) +
      direction * Game::Systems::Combat::Constants::k_arrow_start_offset;
  QVector3D const end =
      target_pos + QVector3D(0.0F, 0.85F, 0.0F) +
      direction * Game::Systems::Combat::Constants::k_arrow_target_offset;

  projectile_system->spawn_arrow(start,
                                 end,
                                 projectile_color_for_kind(projectile_kind, team_color),
                                 projectile_speed_for_kind(projectile_kind),
                                 false,
                                 projectile_kind,
                                 true,
                                 damage,
                                 attacker.get_id(),
                                 target->get_id(),
                                 splash_radius,
                                 splash_damage_multiplier,
                                 friendly_fire);

  result.released = true;
  result.target_id = target->get_id();
  result.damage = damage;
  return result;
}

} // namespace Game::Systems::CombatActions

#pragma once

#include <QVector3D>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../combat_actions/combat_action_definition.h"
#include "../projectile_kind.h"

namespace Game::Systems::Combat {

struct CombatHitContact {
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID target_id{0};
  Game::Systems::CombatActions::CombatActionId action_id{
      Game::Systems::CombatActions::CombatActionId::None};
  Game::Systems::CombatActions::WeaponFamily weapon_family{
      Game::Systems::CombatActions::WeaponFamily::None};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  Engine::Core::AttackDirection attack_direction{
      Engine::Core::AttackDirection::LeftSlash};
  QVector3D contact_point{0.0F, 0.0F, 0.0F};
  float distance{0.0F};
  float local_forward{0.0F};
  float local_right{0.0F};
  float relative_speed{0.0F};
  bool from_projectile{false};
  Game::Systems::ProjectileKind projectile_kind{Game::Systems::ProjectileKind::Arrow};
};

struct CombatHitRequest {
  CombatHitContact contact;
  Game::Systems::CombatActions::DamageProfile damage_profile;
  int explicit_raw_damage{0};
};

struct ProjectileAreaImpactRequest {
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID primary_target_id{0};
  QVector3D impact_point{0.0F, 0.0F, 0.0F};
  Game::Systems::ProjectileKind projectile_kind{Game::Systems::ProjectileKind::Arrow};
  int explicit_raw_damage{0};
  float radius{0.0F};
  float splash_damage_multiplier{1.0F};
  bool friendly_fire{false};
  Game::Systems::CombatActions::DamageProfile damage_profile;
};

} // namespace Game::Systems::Combat

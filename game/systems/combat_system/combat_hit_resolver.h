#pragma once

#include "../rpg_combat_system/rpg_commander_damage.h"
#include "combat_hit_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

void launch_new_casualties(Engine::Core::Entity& casualty_unit,
                           const Engine::Core::Entity& impact_source,
                           int casualty_count,
                           float impact_speed);

struct CombatHitResult {
  CombatHitContact contact;
  Game::Systems::RpgCombat::CommanderDamageResult damage;
  int raw_damage{0};
  bool attempted{false};
  bool applied{false};
  bool interrupted_charge{false};
};

struct ProjectileAreaImpactResult {
  int attempted_hits{0};
  int applied_hits{0};
  bool fire_patch_spawned{false};
};

[[nodiscard]] auto
resolve_commander_action_hit(Engine::Core::World* world,
                             const CombatHitRequest& request) -> CombatHitResult;

[[nodiscard]] auto
resolve_projectile_impact_hit(Engine::Core::World* world,
                              const CombatHitRequest& request) -> CombatHitResult;

[[nodiscard]] auto resolve_projectile_area_impact_hit(
    Engine::Core::World* world,
    const ProjectileAreaImpactRequest& request) -> ProjectileAreaImpactResult;

[[nodiscard]] auto
apply_fire_patch_contact_effect(Engine::Core::World* world,
                                Engine::Core::Entity& fire_patch_entity,
                                Engine::Core::Entity& target) -> bool;

[[nodiscard]] auto
resolve_mounted_charge_impact_hit(Engine::Core::World* world,
                                  const CombatHitRequest& request) -> CombatHitResult;

} // namespace Game::Systems::Combat

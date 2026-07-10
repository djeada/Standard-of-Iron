#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

struct CombatStatusEffectUpdateResult {
  int expired_curses{0};
  int burning_ticks{0};
  int expired_burning_statuses{0};
  int expired_fire_patches{0};
  int fire_patch_contacts{0};
};

[[nodiscard]] auto
process_combat_status_effects(Engine::Core::World* world,
                              float delta_time) -> CombatStatusEffectUpdateResult;

} // namespace Game::Systems::Combat

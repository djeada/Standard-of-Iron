#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

inline constexpr float k_structure_ignition_damage_fraction = 0.05F;

inline constexpr float k_structure_ignition_decay_seconds = 8.0F;

inline constexpr float k_structure_fire_duration = 7.0F;
inline constexpr float k_structure_fire_tick_interval = 0.75F;
inline constexpr float k_structure_fire_damage_fraction_per_tick = 0.0025F;

[[nodiscard]] auto is_structure(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto can_ignite_structure(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto structure_is_burning(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto
structure_fire_intensity(const Engine::Core::Entity& entity) -> float;

auto apply_structure_incendiary_damage(Engine::Core::Entity& structure,
                                       int applied_damage,
                                       Engine::Core::EntityID attacker_id) -> bool;

void extinguish_structure_fire(Engine::Core::Entity& structure);

struct StructureFireUpdateResult {
  int burning_structures{0};
  int fire_ticks{0};
  int extinguished_fires{0};
};

auto process_structure_fires(Engine::Core::World* world,
                             float delta_time) -> StructureFireUpdateResult;

} // namespace Game::Systems::Combat

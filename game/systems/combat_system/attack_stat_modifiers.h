#pragma once

namespace Engine::Core {
class Entity;
struct UnitComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

void apply_hold_mode_bonuses(Engine::Core::Entity* attacker,
                             Engine::Core::UnitComponent* unit_comp,
                             float& range,
                             int& damage);

void apply_high_ground_defense_bonuses(Engine::Core::Entity* attacker,
                                       Engine::Core::Entity* target,
                                       Engine::Core::UnitComponent* target_unit,
                                       int& damage);

[[nodiscard]] auto
calculate_tactical_damage_multiplier(Engine::Core::Entity* attacker,
                                     Engine::Core::Entity* target,
                                     Engine::Core::UnitComponent* attacker_unit,
                                     Engine::Core::UnitComponent* target_unit) -> float;

} // namespace Game::Systems::Combat

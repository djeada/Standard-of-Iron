#pragma once

#include <cstdint>
#include <optional>

#include "../../core/component_combat.h"
#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::FormationCombat {
struct FormationLayout;
}

namespace Game::Systems::Combat {

auto preferred_formation_hit_slot(Engine::Core::Entity* target,
                                  Engine::Core::Entity* attacker)
    -> std::optional<std::uint16_t>;

void publish_formation_hit(
    Engine::Core::Entity& target,
    Engine::Core::EntityID attacker_id,
    std::optional<std::uint16_t> slot,
    Engine::Core::HitReactionKind kind = Engine::Core::HitReactionKind::Flinch,
    Engine::Core::World* world = nullptr);

auto begin_soldier_casualties(Engine::Core::Entity* target,
                              Engine::Core::Entity* attacker,
                              int prev_health,
                              int new_health,
                              std::optional<std::uint16_t> preferred_slot,
                              const FormationCombat::FormationLayout& previous_layout)
    -> int;

void fill_formation_front_vacancy(Engine::Core::World* world,
                                  Engine::Core::Entity* casualty);

} // namespace Game::Systems::Combat

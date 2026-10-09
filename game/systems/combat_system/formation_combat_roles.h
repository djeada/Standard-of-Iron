#pragma once

#include <cstdint>

#include "../../core/component.h"
#include "../formation_combat_geometry.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

constexpr float k_target_hold_seconds = 1.2F;

struct SoldierAssignment {
  const Engine::Core::FormationContactFront* front{nullptr};
  const Engine::Core::FormationEngagementPair* pair{nullptr};
};

struct RetainedTarget {
  std::uint16_t slot{0};
  float root_distance{0.0F};
};

[[nodiscard]] auto opponent_alive(Engine::Core::World& world,
                                  Engine::Core::EntityID opponent_id) -> bool;

[[nodiscard]] auto
combat_role_for(std::uint32_t formation_seed,
                std::uint16_t stable_slot,
                bool engaged,
                float combat_seconds) -> Engine::Core::FormationSoldierCombatRole;

[[nodiscard]] auto brawls_as_a_crowd(const Engine::Core::World& world,
                                     Engine::Core::EntityID entity_id) -> bool;

[[nodiscard]] auto
crowd_brawl_role(std::uint16_t stable_slot) -> Engine::Core::FormationSoldierCombatRole;

[[nodiscard]] auto action_for_role(Engine::Core::FormationSoldierCombatRole role)
    -> Engine::Core::FormationSoldierAction;

[[nodiscard]] auto
assignment_for_slot(const Engine::Core::FormationContactComponent* contact,
                    const Engine::Core::FormationSoldierPresentation* previous,
                    std::uint16_t stable_slot) -> SoldierAssignment;

[[nodiscard]] auto
retained_target_slot(const FormationCombat::FormationLayout& opponent_layout,
                     const FormationCombat::SoldierSlot* attacker_slot,
                     const Engine::Core::FormationSoldierPresentation* previous,
                     const SoldierAssignment& assignment,
                     float spacing) -> RetainedTarget;

} // namespace Game::Systems::Combat

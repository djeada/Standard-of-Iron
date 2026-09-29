#pragma once

#include <limits>

#include "../../core/component.h"
#include "../formation_combat_geometry.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {

struct StructureFacadeFrame {
  bool attacks_structure{false};
  StructureSurfaceContact facade{};
  float closest_gap{std::numeric_limits<float>::infinity()};
  float render_shift{0.0F};
  float shift_local_x{0.0F};
  float shift_local_z{0.0F};
  float nearest_anchor{std::numeric_limits<float>::infinity()};
};

[[nodiscard]] auto
build_structure_facade(Engine::Core::Entity& attacker,
                       const FormationCombat::FormationLayout& layout,
                       const Engine::Core::TransformComponent* actor,
                       const Engine::Core::UnitTraversalLayoutStateComponent* traversal,
                       Engine::Core::Entity* display_opponent,
                       bool outgoing_melee) -> StructureFacadeFrame;

} // namespace Game::Systems::Combat

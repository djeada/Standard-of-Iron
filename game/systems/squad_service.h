#pragma once

#include <vector>

#include "../core/component.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct SquadDivision {

  Engine::Core::EntityID parent = 0;

  Engine::Core::EntityID detachment = 0;
};

struct SquadMerge {

  Engine::Core::EntityID kept = 0;

  Engine::Core::EntityID absorbed = 0;
};

class SquadService {
public:
  static constexpr float k_merge_radius = 14.0F;

  [[nodiscard]] static auto can_divide(const Engine::Core::World& world,
                                       Engine::Core::EntityID unit_id) -> bool;

  [[nodiscard]] static auto divide(Engine::Core::World& world,
                                   Engine::Core::EntityID unit_id) -> SquadDivision;

  [[nodiscard]] static auto can_merge(const Engine::Core::World& world,
                                      Engine::Core::EntityID kept,
                                      Engine::Core::EntityID absorbed) -> bool;

  [[nodiscard]] static auto merge(Engine::Core::World& world,
                                  Engine::Core::EntityID kept,
                                  Engine::Core::EntityID absorbed) -> bool;

  static auto divide_all(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& units)
      -> std::vector<SquadDivision>;

  static auto merge_all(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& units)
      -> std::vector<SquadMerge>;

  static void apply_strength(Engine::Core::World& world,
                             Engine::Core::EntityID unit_id,
                             int strength);
};

} // namespace Game::Systems

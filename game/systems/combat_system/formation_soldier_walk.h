#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../../core/component.h"
#include "../navigation/pathfinding.h"
#include "formation_foreign_grid.h"

namespace Game::Systems::Combat {

[[nodiscard]] auto foreign_gather_radius(float spacing, bool mounted) -> float;

struct SlotWalk {
  const Engine::Core::TransformComponent& actor;
  const Engine::Core::FormationPresentationComponent& formation;
  const std::vector<Engine::Core::FormationSoldierPresentation>& neighbors;
  const std::vector<ForeignSoldier>& foreign_neighbors;
  float crowd_step_budget{0.0F};
  float squad_speed{0.0F};
  float march_speed{0.0F};
  float spacing{0.0F};
  float body_radius{0.0F};
  int rows{1};
  std::uint32_t seed{0U};
  bool mounted{false};
  bool engaged{false};
  bool external_reform{false};
  bool position_is_authored{false};
  bool walking_to_work_posts{false};
  Pathfinding::Passability passability{Pathfinding::Passability::Light};
  float delta_time{0.0F};
};

void walk_formation_slot(const SlotWalk& walk,
                         const Engine::Core::FormationSoldierPresentation* previous,
                         Engine::Core::FormationSoldierPresentation& soldier);

} // namespace Game::Systems::Combat

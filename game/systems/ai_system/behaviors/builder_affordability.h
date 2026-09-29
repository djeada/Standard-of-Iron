#pragma once

#include "../../resource_types.h"
#include "../ai_types.h"

namespace Game::Systems::AI {

struct AffordabilityVerdict {
  bool blocked = false;
  ResourceType missing = ResourceType::Count;
};

[[nodiscard]] auto affordability_of(const AISnapshot& snapshot,
                                    const char* building_type) -> AffordabilityVerdict;

[[nodiscard]] auto harvest_type_for_resource(ResourceType resource) -> const char*;
[[nodiscard]] auto node_matches_resource(const ResourceNodeSnapshot& node,
                                         ResourceType resource) -> bool;

[[nodiscard]] auto starved_of_food(const AISnapshot& snapshot) -> bool;
[[nodiscard]] auto granary_has_room(const AISnapshot& snapshot) -> bool;

[[nodiscard]] auto planned_wall_wood(const AIContext& context) -> int;

[[nodiscard]] auto neediest_stockpile(const AISnapshot& snapshot,
                                      int building_count,
                                      int planned_wood) -> const char*;

} // namespace Game::Systems::AI

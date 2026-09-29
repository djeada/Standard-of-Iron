#pragma once

#include <vector>

#include "army_formation_types.h"

namespace Game::Formation::planning {

[[nodiscard]] auto
balanced_rows(int total, int max_per_row, int min_per_row) -> std::vector<int>;

[[nodiscard]] auto split_rows(int total,
                              const std::vector<float>& weights) -> std::vector<int>;

[[nodiscard]] auto even_rows(int total, int row_count) -> std::vector<int>;

[[nodiscard]] auto wedge_rows(int total, int growth = 1) -> std::vector<int>;

[[nodiscard]] auto intent_owns_its_rows(ArmyFormationIntent intent) -> bool;

[[nodiscard]] auto silhouette_rows(ArmyFormationIntent intent,
                                   int total,
                                   const ArmyFormationOptions& options,
                                   bool has_rear_tier = false) -> std::vector<int>;

} // namespace Game::Formation::planning

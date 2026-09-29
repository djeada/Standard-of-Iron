#include "formation_row_split.h"

#include <algorithm>
#include <cmath>

namespace Game::Formation::planning {

auto balanced_rows(int total, int max_per_row, int min_per_row) -> std::vector<int> {
  std::vector<int> rows;
  if (total <= 0) {
    return rows;
  }
  int const capped = std::max(1, max_per_row);
  if (total <= capped) {
    rows.push_back(total);
    return rows;
  }
  int const row_count = (total + capped - 1) / capped;
  int const base = total / row_count;
  int const extra = total % row_count;
  int assigned = 0;
  for (int r = 0; r < row_count; ++r) {
    int size = base + (r < extra ? 1 : 0);
    size = std::clamp(size, std::max(1, std::min(min_per_row, capped)), capped);
    size = std::min(size, total - assigned);
    if (size <= 0) {
      break;
    }
    rows.push_back(size);
    assigned += size;
  }
  while (assigned < total && !rows.empty()) {
    rows.back() += total - assigned;
    assigned = total;
  }
  return rows;
}

auto split_rows(int total, const std::vector<float>& weights) -> std::vector<int> {
  std::vector<int> rows;
  float weight_sum = 0.0F;
  for (float const w : weights) {
    weight_sum += w;
  }
  int assigned = 0;
  for (std::size_t r = 0; r < weights.size(); ++r) {
    int size = r + 1U == weights.size()
                   ? total - assigned
                   : static_cast<int>(std::lround(static_cast<float>(total) *
                                                  weights[r] / weight_sum));
    size = std::clamp(size, 0, total - assigned);
    if (size > 0) {
      rows.push_back(size);
    }
    assigned += size;
  }
  return rows;
}

auto even_rows(int total, int row_count) -> std::vector<int> {
  row_count = std::clamp(row_count, 1, std::max(1, total));
  std::vector<int> rows;
  int const base = total / row_count;
  int const extra = total % row_count;
  for (int r = 0; r < row_count; ++r) {
    int const size = base + (r < extra ? 1 : 0);
    if (size > 0) {
      rows.push_back(size);
    }
  }
  return rows;
}

auto wedge_rows(int total, int growth) -> std::vector<int> {
  std::vector<int> rows;
  int placed = 0;
  for (int width = 1; placed + width <= total; width += std::max(1, growth)) {
    rows.push_back(width);
    placed += width;
  }
  if (rows.empty()) {
    return {total};
  }
  if (rows.size() == 1U) {
    rows.front() = total;
    return rows;
  }
  int leftover = total - placed;
  for (std::size_t row = rows.size() - 1U; leftover > 0; --leftover) {
    ++rows[row];
    row = row > 1U ? row - 1U : rows.size() - 1U;
  }
  return rows;
}

auto intent_owns_its_rows(ArmyFormationIntent intent) -> bool {
  return intent == ArmyFormationIntent::Column ||
         intent == ArmyFormationIntent::Assault ||
         intent == ArmyFormationIntent::Defensive;
}

auto silhouette_rows(ArmyFormationIntent intent,
                     int total,
                     const ArmyFormationOptions& options,
                     bool has_rear_tier) -> std::vector<int> {
  float const depth_bias = std::clamp(options.depth_scale, 0.4F, 3.0F) /
                           std::clamp(options.frontage_scale, 0.4F, 3.0F);
  auto scaled = [&](int rows) {
    return std::max(
        1, static_cast<int>(std::lround(static_cast<float>(rows) * depth_bias)));
  };
  switch (intent) {
  case ArmyFormationIntent::Line:
  case ArmyFormationIntent::Encirclement:
    return even_rows(total, total <= 3 ? 1 : scaled(2));
  case ArmyFormationIntent::Column: {
    int const files = std::clamp(static_cast<int>(std::lround((total <= 2   ? 1.0F
                                                               : total <= 6 ? 2.0F
                                                                            : 3.0F) /
                                                              depth_bias)),
                                 1,
                                 std::max(1, total));
    return even_rows(total, (total + files - 1) / files);
  }
  case ArmyFormationIntent::Assault:
    return wedge_rows(total);
  case ArmyFormationIntent::SiegeEscort:
    return total <= 4 ? even_rows(total, 2) : split_rows(total, {7.0F, 5.0F});
  case ArmyFormationIntent::FactionDefault:
  case ArmyFormationIntent::Defensive:
    if (total <= 4 || has_rear_tier) {
      return even_rows(total, scaled(total <= 2 ? 1 : 2));
    }
    return split_rows(total, {5.0F, 5.0F, 3.0F});
  }
  return even_rows(total, 2);
}

} // namespace Game::Formation::planning

#include "layout_manifest.h"

#include <algorithm>
#include <cmath>

#include "individuality_manifest.h"

namespace Animation {

namespace {

[[nodiscard]] auto centered_grid_coordinate(int index, int count) noexcept -> float {
  if (count <= 1) {
    return 0.0F;
  }
  float const normalized = static_cast<float>(index) / static_cast<float>(count - 1);
  return normalized * 2.0F - 1.0F;
}

} // namespace

auto layout_random(std::uint32_t& state) noexcept -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>(state & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

auto resolve_soldier_layout_policy(const SoldierLayoutPolicyInputs& inputs) noexcept
    -> SoldierLayoutPolicy {
  SoldierLayoutPolicy policy{};
  policy.row_index = static_cast<std::uint8_t>(std::clamp(inputs.row, 0, 255));
  policy.col_index = static_cast<std::uint8_t>(std::clamp(inputs.col, 0, 255));
  policy.rank_band = static_cast<std::uint8_t>(
      (inputs.force_single_soldier || inputs.rows <= 1)
          ? 0
          : ((inputs.row <= 0) ? 0 : ((inputs.row + 1 >= inputs.rows) ? 2 : 1)));
  policy.inst_seed = inputs.seed ^ std::uint32_t(inputs.idx * 9176U);

  std::uint32_t rng_state = policy.inst_seed;
  if (!inputs.force_single_soldier) {
    policy.vertical_jitter = (layout_random(rng_state) - 0.5F) * 0.03F;
    policy.yaw_delta = (layout_random(rng_state) - 0.5F) * 5.0F;
    policy.individuality = resolve_soldier_individuality({
        .soldier_seed = policy.inst_seed,
        .row = inputs.row,
        .col = inputs.col,
        .rows = inputs.rows,
        .cols = inputs.cols,
        .single_soldier = false,
    });
  }

  return policy;
}

} // namespace Animation

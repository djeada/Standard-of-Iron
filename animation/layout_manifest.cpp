#include "layout_manifest.h"

#include <algorithm>
#include <cmath>

namespace Animation {

namespace {

[[nodiscard]] auto centered_grid_coordinate(int index, int count) noexcept -> float {
  if (count <= 1) {
    return 0.0F;
  }
  float const normalized = static_cast<float>(index) / static_cast<float>(count - 1);
  return normalized * 2.0F - 1.0F;
}

[[nodiscard]] auto phase_cohort_offset(std::uint32_t inst_seed) noexcept -> float {
  int const cohort = static_cast<int>((inst_seed >> 9U) % 3U) - 1;
  return static_cast<float>(cohort);
}

} // namespace

auto layout_random(std::uint32_t& state) noexcept -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>(state & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

auto structured_layout_phase_offset(
    int row, int col, int rows, int cols, std::uint32_t inst_seed) noexcept -> float {
  float const row_bias = centered_grid_coordinate(row, rows) * 0.040F;
  float const col_bias = centered_grid_coordinate(col, cols) * 0.018F;
  float const checker_bias = (((row + col) & 1) == 0 ? -1.0F : 1.0F) * 0.012F;
  float const cohort_bias = phase_cohort_offset(inst_seed) * 0.026F;

  std::uint32_t rng_state = inst_seed ^ 0xA511E9B3U;
  float const micro_bias = (layout_random(rng_state) - 0.5F) * 0.014F;

  return std::clamp(0.125F + row_bias + col_bias + checker_bias + cohort_bias +
                        micro_bias,
                    0.0F,
                    0.25F);
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
    policy.phase_offset = structured_layout_phase_offset(
        inputs.row, inputs.col, inputs.rows, inputs.cols, policy.inst_seed);
  }

  // Formation roots are authoritative spatial slots.  Combat variation belongs
  // to the skeletal pose, never to root placement; otherwise gameplay contact
  // and the visible soldier positions describe different battlefields.

  return policy;
}

} // namespace Animation

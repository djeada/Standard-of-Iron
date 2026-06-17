#pragma once

#include <cstdint>

namespace Animation {

struct SoldierLayoutPolicyInputs {
  int idx{0};
  int row{0};
  int col{0};
  int rows{1};
  int cols{1};
  float formation_spacing{0.0F};
  std::uint32_t seed{0U};
  bool force_single_soldier{false};
  bool melee_attack{false};
  float sample_time{0.0F};
};

struct SoldierLayoutBase {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float yaw_offset{0.0F};
};

struct SoldierLayoutPolicy {
  float offset_x_delta{0.0F};
  float offset_z_delta{0.0F};
  float vertical_jitter{0.0F};
  float yaw_delta{0.0F};
  float phase_offset{0.0F};
  std::uint8_t rank_band{0U};
  std::uint8_t row_index{0U};
  std::uint8_t col_index{0U};
  std::uint32_t inst_seed{0U};
};

[[nodiscard]] auto layout_random(std::uint32_t& state) noexcept -> float;

[[nodiscard]] auto structured_layout_phase_offset(
    int row, int col, int rows, int cols, std::uint32_t inst_seed) noexcept -> float;

[[nodiscard]] auto resolve_soldier_layout_policy(
    const SoldierLayoutPolicyInputs& inputs) noexcept -> SoldierLayoutPolicy;

} // namespace Animation

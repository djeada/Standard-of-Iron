#pragma once

#include <cstdint>

namespace Animation {

struct SoldierIndividualityInputs {

  std::uint32_t soldier_seed{0U};

  int row{0};
  int col{0};
  int rows{1};
  int cols{1};

  bool single_soldier{false};
};

struct SoldierIndividuality {

  float gait_phase_offset{0.0F};

  float cadence_scale{1.0F};

  float swing_phase_offset{0.0F};

  float swing_tempo_scale{1.0F};

  float swing_plane_offset{0.0F};

  float idle_phase_offset{0.0F};
};

inline constexpr float k_individuality_cadence_spread = 0.035F;

inline constexpr float k_individuality_swing_spread = 0.22F;

[[nodiscard]] auto resolve_soldier_individuality(
    const SoldierIndividualityInputs& inputs) noexcept -> SoldierIndividuality;

} // namespace Animation

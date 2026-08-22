#include "individuality_manifest.h"

#include <algorithm>
#include <cmath>

#include "layout_manifest.h"

namespace Animation {

namespace {

[[nodiscard]] auto cohort_of(std::uint32_t soldier_seed) noexcept -> float {
  return static_cast<float>(static_cast<int>((soldier_seed >> 9U) % 3U) - 1);
}

[[nodiscard]] auto centered(int index, int count) noexcept -> float {
  if (count <= 1) {
    return 0.0F;
  }
  return ((static_cast<float>(index) / static_cast<float>(count - 1)) * 2.0F) - 1.0F;
}

} // namespace

auto resolve_soldier_individuality(const SoldierIndividualityInputs& inputs) noexcept
    -> SoldierIndividuality {
  SoldierIndividuality individuality{};
  if (inputs.single_soldier) {
    return individuality;
  }

  float const cohort = cohort_of(inputs.soldier_seed);
  float const rank = centered(inputs.row, inputs.rows);
  float const file = centered(inputs.col, inputs.cols);
  float const checker = (((inputs.row + inputs.col) & 1) == 0) ? -1.0F : 1.0F;

  std::uint32_t rng_state = inputs.soldier_seed ^ 0xA511E9B3U;
  auto noise = [&rng_state]() {
    return layout_random(rng_state) - 0.5F;
  };

  individuality.gait_phase_offset =
      std::clamp(0.125F + (rank * 0.040F) + (file * 0.018F) + (checker * 0.012F) +
                     (cohort * 0.026F) + (noise() * 0.014F),
                 0.0F,
                 0.25F);

  individuality.cadence_scale =
      1.0F + (((cohort * 0.60F) + (noise() * 0.80F)) * k_individuality_cadence_spread);

  individuality.swing_phase_offset =
      (0.5F + (cohort * 0.22F) + (noise() * 0.9F)) * 0.14F;
  individuality.swing_tempo_scale = 1.0F + ((cohort * 0.05F) + (noise() * 0.10F));
  individuality.swing_plane_offset =
      ((cohort * 0.45F) + (checker * 0.25F) + (noise() * 0.9F)) *
      k_individuality_swing_spread;

  individuality.idle_phase_offset = layout_random(rng_state);

  return individuality;
}

} // namespace Animation

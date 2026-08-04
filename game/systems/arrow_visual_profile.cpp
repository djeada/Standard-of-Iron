#include "arrow_visual_profile.h"

namespace Game::Systems {
namespace {

constexpr float k_unit_float_divisor = 1.0F / 16777215.0F;

[[nodiscard]] auto mix_seed(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

[[nodiscard]] auto unit_float(std::uint32_t seed) -> float {
  return static_cast<float>(mix_seed(seed) & 0x00ffffffU) * k_unit_float_divisor;
}

[[nodiscard]] auto remap_unit(float value, float minimum, float maximum) -> float {
  return minimum + (maximum - minimum) * value;
}

} // namespace

auto make_arrow_visual_profile(ArrowVisualStyle style,
                               std::uint32_t sequence) -> ArrowVisualProfile {
  ArrowVisualProfile profile;
  switch (style) {
  case ArrowVisualStyle::Volley:
    profile.initial_progress =
        -remap_unit(unit_float(sequence ^ 0x13579bdfU), 0.0F, 0.16F);
    profile.scale = remap_unit(unit_float(sequence ^ 0x2468ace0U), 0.92F, 1.18F);
    profile.roll_deg = remap_unit(unit_float(sequence ^ 0x55aa55aaU), -32.0F, 32.0F);
    profile.spin_rate_deg =
        remap_unit(unit_float(sequence ^ 0xa55aa55aU), -280.0F, 280.0F);
    profile.trail_alpha = remap_unit(unit_float(sequence ^ 0x0f1e2d3cU), 0.16F, 0.28F);
    profile.trail_length = remap_unit(unit_float(sequence ^ 0x10293847U), 0.11F, 0.18F);
    profile.brightness = remap_unit(unit_float(sequence ^ 0x56473829U), 0.88F, 1.08F);
    break;
  case ArrowVisualStyle::Javelin:
    profile.scale = 1.30F;
    profile.length_scale = 2.60F;
    profile.arc_scale = 0.30F;
    profile.roll_deg = 0.0F;
    profile.spin_rate_deg = 0.0F;
    profile.trail_alpha = 0.10F;
    profile.trail_length = 0.07F;
    profile.brightness = 1.02F;
    break;
  case ArrowVisualStyle::Marker:
    profile.scale = 1.08F;
    profile.brightness = 1.05F;
    break;
  case ArrowVisualStyle::Aimed:

    profile.scale = 1.78F;
    profile.length_scale = 1.62F;
    profile.arc_scale = 1.0F;
    profile.roll_deg = remap_unit(unit_float(sequence ^ 0x6a09e667U), -6.0F, 6.0F);
    profile.spin_rate_deg =
        remap_unit(unit_float(sequence ^ 0xbb67ae85U), 120.0F, 210.0F);
    profile.trail_alpha = 0.46F;
    profile.trail_length = 0.20F;
    profile.brightness = 1.30F;
    break;
  case ArrowVisualStyle::Focused:
  default:
    profile.initial_progress =
        -remap_unit(unit_float(sequence ^ 0x89abcdefU), 0.0F, 0.05F);
    profile.scale = remap_unit(unit_float(sequence ^ 0x31415926U), 0.97F, 1.08F);
    profile.roll_deg = remap_unit(unit_float(sequence ^ 0x27182818U), -10.0F, 10.0F);
    profile.spin_rate_deg =
        remap_unit(unit_float(sequence ^ 0x42424242U), -80.0F, 80.0F);
    profile.trail_alpha = remap_unit(unit_float(sequence ^ 0xabcdef01U), 0.08F, 0.15F);
    profile.trail_length = remap_unit(unit_float(sequence ^ 0x10fedcbaU), 0.06F, 0.10F);
    profile.brightness = remap_unit(unit_float(sequence ^ 0xc001d00dU), 0.94F, 1.04F);
    break;
  }
  return profile;
}

} // namespace Game::Systems

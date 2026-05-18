#pragma once

#include <QVector3D>

#include "../creature/pipeline/unit_visual_spec.h"

namespace Render::GL::Humanoid {

struct ProportionOffset {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float torso_scale{0.0F};
};

struct ProportionProfile {
  float x{1.0F};
  float y{1.0F};
  float z{1.0F};
  float torso_scale{1.0F};

  [[nodiscard]] constexpr auto as_vector() const -> QVector3D { return {x, y, z}; }

  [[nodiscard]] constexpr auto
  as_pipeline_scaling() const -> Render::Creature::Pipeline::ProportionScaling {
    return {x, y, z};
  }

  [[nodiscard]] constexpr auto
  with_offset(ProportionOffset offset) const -> ProportionProfile {
    return {x + offset.x, y + offset.y, z + offset.z, torso_scale + offset.torso_scale};
  }
};

[[nodiscard]] constexpr auto
make_proportion_profile(float x, float y, float z) -> ProportionProfile {
  return {x, y, z, x};
}

[[nodiscard]] constexpr auto make_proportion_profile(
    float x, float y, float z, float torso_scale) -> ProportionProfile {
  return {x, y, z, torso_scale};
}

inline constexpr ProportionProfile k_ranged_infantry_proportion_profile =
    make_proportion_profile(0.98F, 1.08F, 0.97F);

inline constexpr ProportionProfile k_sword_infantry_proportion_profile =
    make_proportion_profile(0.90F, 1.01F, 0.55F, 0.72F);

inline constexpr ProportionProfile k_polearm_infantry_proportion_profile =
    make_proportion_profile(0.74F, 1.01F, 0.74F, 0.69F);

inline constexpr ProportionProfile k_support_proportion_profile =
    make_proportion_profile(0.87F, 0.99F, 0.90F);

inline constexpr ProportionProfile k_laborer_proportion_profile =
    make_proportion_profile(1.00F, 1.00F, 0.98F);

inline constexpr ProportionProfile k_civilian_proportion_profile =
    make_proportion_profile(0.97F, 0.99F, 0.96F);

inline constexpr ProportionProfile k_mounted_rider_proportion_profile =
    make_proportion_profile(0.84F, 0.91F, 0.90F);

} // namespace Render::GL::Humanoid

#pragma once

#include <cstdint>

namespace Render::Humanoid {

enum class HumanoidPosePolicy : std::uint8_t {

  None = 0,

  SkeletonProportions,

  HealerChannel,

  HealerStaff,

  GravePriestCast,
};

} // namespace Render::Humanoid

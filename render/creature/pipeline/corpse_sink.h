#pragma once

#include <algorithm>

#include "unit_visual_spec.h"

namespace Render::Creature::Pipeline {

[[nodiscard]] constexpr auto corpse_sink_depth(CreatureKind kind) noexcept -> float {
  switch (kind) {
  case CreatureKind::Horse:
    return 1.4F;
  case CreatureKind::Elephant:
    return 2.6F;
  case CreatureKind::Sheep:
  case CreatureKind::Wolf:
    return 0.8F;
  case CreatureKind::Humanoid:
  case CreatureKind::Mounted:
    break;
  }
  return 0.75F;
}

[[nodiscard]] constexpr auto corpse_sink_offset(CreatureKind kind,
                                                float sink_progress) noexcept -> float {
  float const t = std::clamp(sink_progress, 0.0F, 1.0F);
  float const eased = t * t * (3.0F - (2.0F * t));
  return -corpse_sink_depth(kind) * eased;
}

[[nodiscard]] constexpr auto
corpse_shadow_scale(float sink_progress) noexcept -> float {
  return 1.0F - std::clamp(sink_progress, 0.0F, 1.0F);
}

} // namespace Render::Creature::Pipeline

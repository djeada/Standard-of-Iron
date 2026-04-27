#pragma once

#include "../entity/registry.h"
#include "../graphics_settings.h"

namespace Render::GL {

inline auto calculate_humanoid_lod(float distance) -> HumanoidLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.humanoid_full_detail_distance()) {
    return HumanoidLOD::Full;
  }
  if (distance < settings.humanoid_minimal_detail_distance()) {
    return HumanoidLOD::Minimal;
  }
  return HumanoidLOD::Billboard;
}

} // namespace Render::GL

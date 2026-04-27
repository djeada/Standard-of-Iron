#pragma once

#include "../entity/registry.h"
#include "../graphics_settings.h"

namespace Render::GL {

inline auto calculate_elephant_lod(float distance) -> HorseLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.elephant_full_detail_distance()) {
    return HorseLOD::Full;
  }
  if (distance < settings.elephant_minimal_detail_distance()) {
    return HorseLOD::Minimal;
  }
  return HorseLOD::Billboard;
}

} // namespace Render::GL

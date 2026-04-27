#pragma once

#include "../entity/registry.h"
#include "../graphics_settings.h"

namespace Render::GL {

inline auto calculate_horse_lod(float distance) -> HorseLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.horse_full_detail_distance()) {
    return HorseLOD::Full;
  }
  if (distance < settings.horse_reduced_detail_distance()) {
    return HorseLOD::Reduced;
  }
  if (distance < settings.horse_minimal_detail_distance()) {
    return HorseLOD::Minimal;
  }
  return HorseLOD::Billboard;
}

} // namespace Render::GL

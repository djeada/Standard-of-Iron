#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "../../decoration_gpu.h"
#include "../../draw_queue.h"
#include "../../geom/mode_indicator.h"
#include "../../geom/selection_disc.h"
#include "../../geom/selection_ring.h"
#include "../../graphics_settings.h"
#include "../../material.h"
#include "../../primitive_batch.h"
#include "../../rain_gpu.h"
#include "../backend.h"
#include "../buffer.h"
#include "../mesh.h"
#include "../render_constants.h"
#include "../resources.h"
#include "../shader.h"
#include "../state_scopes.h"
#include "../texture.h"
#include "../uniform_helpers.h"
#include "../vertex_attrib_layout.h"
#include "banner_pipeline.h"
#include "character_pipeline.h"
#include "combat_dust_pipeline.h"
#include "cylinder_pipeline.h"
#include "effects_pipeline.h"
#include "healer_aura_pipeline.h"
#include "healing_beam_pipeline.h"
#include "mesh_instancing_pipeline.h"
#include "mode_indicator_pipeline.h"
#include "primitive_batch_pipeline.h"
#include "rain_pipeline.h"
#include "rigged_character_pipeline.h"
#include "terrain_pipeline.h"
#include "vegetation_pipeline.h"
#include "water_pipeline.h"

namespace Render::GL {

inline constexpr float k_fog_start_zoom_scale = 1.25F;
inline constexpr float k_fog_end_zoom_scale = 3.2F;

[[nodiscard]] inline auto
fog_range_for_camera(const Camera& camera) -> std::pair<float, float> {
  const float orbit_distance =
      std::max((camera.get_position() - camera.get_target()).length(), 1.0F);
  const float near_fog_start =
      std::max(camera.get_near() + 5.0F, camera.get_far() * 0.18F);
  const float near_fog_end = std::max(near_fog_start + 1.0F, camera.get_far() * 0.62F);
  return {std::max(near_fog_start, orbit_distance * k_fog_start_zoom_scale),
          std::max(near_fog_end, orbit_distance * k_fog_end_zoom_scale)};
}

} // namespace Render::GL

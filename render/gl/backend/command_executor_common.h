#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "banner_pipeline.h"
#include "character_pipeline.h"
#include "combat_dust_pipeline.h"
#include "cylinder_pipeline.h"
#include "effects_pipeline.h"
#include "ground_marker_pipeline.h"
#include "healer_aura_pipeline.h"
#include "healing_beam_pipeline.h"
#include "mesh_instancing_pipeline.h"
#include "mode_indicator_pipeline.h"
#include "primitive_batch_pipeline.h"
#include "rain_pipeline.h"
#include "render/decoration_gpu.h"
#include "render/draw_commands.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/selection_disc.h"
#include "render/gl/backend.h"
#include "render/gl/buffer.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/resources.h"
#include "render/gl/shader.h"
#include "render/gl/state_scopes.h"
#include "render/gl/texture.h"
#include "render/gl/uniform_helpers.h"
#include "render/gl/vertex_attrib_layout.h"
#include "render/graphics_settings.h"
#include "render/material.h"
#include "render/primitive_batch.h"
#include "render/rain_gpu.h"
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

inline void
bind_visibility_mask(Shader& shader,
                     const TerrainSurfaceCmd::VisibilityResources& visibility) {
  const bool has_mask = visibility.enabled && visibility.texture != nullptr;
  shader.set_uniform(shader.optional_uniform_handle("u_has_visibility"),
                     has_mask ? 1 : 0);
  if (!has_mask) {
    return;
  }
  shader.set_uniform(shader.optional_uniform_handle("u_visibility_size"),
                     visibility.size);
  shader.set_uniform(shader.optional_uniform_handle("u_visibility_tile_size"),
                     visibility.tile_size);
  shader.set_uniform(shader.optional_uniform_handle("u_explored_alpha"),
                     visibility.explored_alpha);
  visibility.texture->bind(TextureUnit::terrain_visibility);
  shader.set_uniform(shader.optional_uniform_handle("u_visibility_tex"),
                     TextureUnit::terrain_visibility);
}

} // namespace Render::GL

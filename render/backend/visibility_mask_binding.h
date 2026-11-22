#pragma once

#include "render/frame/draw_commands.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"

namespace Render::GL {

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

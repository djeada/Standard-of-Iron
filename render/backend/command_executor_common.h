#pragma once

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "render/backend/backend.h"
#include "render/backend/banner_pipeline.h"
#include "render/backend/combat_dust_pipeline.h"
#include "render/backend/cylinder_pipeline.h"
#include "render/backend/effects_pipeline.h"
#include "render/backend/ground_marker_pipeline.h"
#include "render/backend/healer_aura_pipeline.h"
#include "render/backend/healing_beam_pipeline.h"
#include "render/backend/mesh_instancing_pipeline.h"
#include "render/backend/mode_indicator_pipeline.h"
#include "render/backend/primitive_batch_pipeline.h"
#include "render/backend/rain_pipeline.h"
#include "render/backend/rigged_character_pipeline.h"
#include "render/backend/shader_uniform_cache.h"
#include "render/backend/state_scopes.h"
#include "render/backend/terrain_pipeline.h"
#include "render/backend/uniform_helpers.h"
#include "render/backend/vegetation_pipeline.h"
#include "render/backend/water_pipeline.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_commands.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/material.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/rain_gpu.h"
#include "render/geom/mesh.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/selection_disc.h"
#include "render/gl/buffer.h"
#include "render/gl/render_constants.h"
#include "render/gl/resources.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "render/gl/vertex_attrib_layout.h"

namespace Render::GL {

inline void bind_material_detail(Shader& shader,
                                 const ShaderUniformCache::BasicUniforms& uniforms,
                                 ResourceManager* resources) {
  if (uniforms.has_material_detail == Shader::InvalidUniform) {
    return;
  }
  Texture* detail = (resources != nullptr) ? resources->material_detail() : nullptr;
  shader.set_uniform(uniforms.has_material_detail, detail != nullptr);
  if (detail == nullptr) {
    return;
  }
  detail->bind(TextureUnit::material_detail);

  if (auto* context = QOpenGLContext::currentContext()) {
    context->functions()->glActiveTexture(GL_TEXTURE0);
  }
  if (uniforms.material_detail != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.material_detail, TextureUnit::material_detail);
  }
}

} // namespace Render::GL

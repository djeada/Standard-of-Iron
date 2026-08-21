#pragma once

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include "render/gl/render_constants.h"
#include "render/gl/shader.h"

namespace Render::GL {

inline constexpr unsigned int k_texture_3d_target = 0x806FU; // GL_TEXTURE_3D

// The character programs read their wear, coat and hide patterns from one
// shared tiling volume. Bind it to its reserved unit and tell the shader
// whether it is there, so the procedural hash fallback only runs when it is not.
inline void bind_character_wear_volume(Shader& shader,
                                       Shader::UniformHandle sampler_uniform,
                                       Shader::UniformHandle present_uniform,
                                       unsigned int wear_volume) {
  if (present_uniform != Shader::InvalidUniform) {
    shader.set_uniform(present_uniform, wear_volume != 0U);
  }
  if (wear_volume == 0U || sampler_uniform == Shader::InvalidUniform) {
    return;
  }
  auto* context = QOpenGLContext::currentContext();
  if (context == nullptr) {
    return;
  }
  auto* functions = context->functions();
  functions->glActiveTexture(GL_TEXTURE0 + TextureUnit::character_wear_volume);
  functions->glBindTexture(k_texture_3d_target, wear_volume);
  functions->glActiveTexture(GL_TEXTURE0);
  shader.set_uniform(sampler_uniform, TextureUnit::character_wear_volume);
}

} // namespace Render::GL

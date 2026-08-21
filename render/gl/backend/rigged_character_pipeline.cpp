#include "rigged_character_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include <algorithm>
#include <array>
#include <vector>

#include "character_wear_binding.h"
#include "render/bone_palette_arena.h"
#include "render/draw_commands.h"
#include "render/gl/shader_cache.h"
#include "render/gl/texture.h"
#include "render/gl/ubo_bindings.h"
#include "render/rigged_mesh.h"

namespace Render::GL::BackendPipelines {

namespace {

auto gl_funcs() -> QOpenGLFunctions_3_3_Core* {
  auto* ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

void pack_cmd_palette(const RiggedCreatureCmd& cmd, float* dst) {
  if (!cmd.palette_frames_resident || cmd.bone_palette_next == nullptr ||
      cmd.palette_lerp <= 1.0e-4F) {
    BonePaletteArena::pack_palette_for_gpu(cmd.bone_palette, dst);
    return;
  }

  thread_local std::vector<QMatrix4x4> blended;
  const auto bones =
      std::min<std::size_t>(cmd.bone_count, BonePaletteArena::k_palette_width);
  blended.resize(BonePaletteArena::k_palette_width);
  float const weight = std::clamp(cmd.palette_lerp, 0.0F, 1.0F);
  for (std::size_t b = 0; b < bones; ++b) {
    const float* first = cmd.bone_palette[b].constData();
    const float* second = cmd.bone_palette_next[b].constData();
    float* out = blended[b].data();
    for (std::size_t i = 0; i < BonePaletteArena::k_matrix_floats; ++i) {
      out[i] = first[i] + (second[i] - first[i]) * weight;
    }
  }
  for (std::size_t b = bones; b < BonePaletteArena::k_palette_width; ++b) {
    blended[b] = QMatrix4x4{};
  }
  BonePaletteArena::pack_palette_for_gpu(blended.data(), dst);
}

void copy_palette_to_scratch(const RiggedCreatureCmd& cmd,
                             std::vector<float>& scratch) {
  scratch.resize(BonePaletteArena::k_palette_floats);
  pack_cmd_palette(cmd, scratch.data());
}

void flatten_role_colors(
    const RiggedCreatureCmd& cmd,
    std::array<float, RiggedCreatureCmd::k_max_role_colors * 3>& out_flat) {
  out_flat.fill(0.0F);
  const auto role_colors = cmd.role_colors != nullptr ? cmd.role_colors->view()
                                                      : std::span<const QVector3D>{};
  const auto n = std::min<std::size_t>(cmd.role_color_count, role_colors.size());
  for (std::size_t i = 0; i < n; ++i) {
    out_flat[i * 3 + 0] = role_colors[i].x();
    out_flat[i * 3 + 1] = role_colors[i].y();
    out_flat[i * 3 + 2] = role_colors[i].z();
  }
}

void set_role_palette_uniforms(GL::Shader* shader,
                               GL::Shader::UniformHandle colors_handle,
                               GL::Shader::UniformHandle count_handle,
                               const RiggedCreatureCmd& cmd) {
  if (shader == nullptr) {
    return;
  }
  if (count_handle != GL::Shader::InvalidUniform) {
    shader->set_uniform(count_handle, static_cast<int>(cmd.role_color_count));
  }
  if (colors_handle == GL::Shader::InvalidUniform) {
    return;
  }

  std::array<float, RiggedCreatureCmd::k_max_role_colors * 3> flat{};
  flatten_role_colors(cmd, flat);
  shader->set_uniform_vec3_array(
      colors_handle,
      flat.data(),
      static_cast<int>(RiggedCreatureCmd::k_max_role_colors));
}

} // namespace

auto RiggedCharacterPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "RiggedCharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_shader = m_shader_cache->get(QStringLiteral("character_skinned"));
  if (m_shader == nullptr) {
    qWarning() << "RiggedCharacterPipeline: Failed to load character_skinned shader";
    return false;
  }

  m_variant_shaders[0] = m_shader;
  std::size_t slot = 1U;
  for (const auto& [suffix, variant] : ShaderCache::k_character_variants) {
    (void)variant;

    GL::Shader* specialised = m_shader_cache->get(QStringLiteral("character_skinned_") +
                                                  QString::fromLatin1(suffix));
    m_variant_shaders[slot] = (specialised != nullptr) ? specialised : m_shader;
    ++slot;
  }

  cache_uniforms();

  for (GL::Shader* shader : m_variant_shaders) {
    if (shader != nullptr) {
      shader->bind_uniform_block("BonePalette", k_bone_palette_binding_point);
    }
  }

  return is_initialized();
}

auto RiggedCharacterPipeline::variant_for_material(int material_id) -> std::size_t {
  switch (material_id) {
  case 6:
    return 2U;
  case 7:
    return 3U;
  case 8:
    return 4U;
  default:
    return 1U;
  }
}

void RiggedCharacterPipeline::shutdown() {
  m_shader = nullptr;
  m_uniforms = Uniforms{};
  m_variant_shaders = {};
  m_variant_uniforms = {};
  m_last_bound_shader = nullptr;

  auto* fn = gl_funcs();
  if (fn != nullptr) {
    if (m_palette_ubo != 0) {
      fn->glDeleteBuffers(1, &m_palette_ubo);
    }
  }
  m_palette_ubo = 0;
  m_palette_ubo_capacity_bytes = 0;
}

void RiggedCharacterPipeline::cache_uniforms() {
  auto cache_for = [](GL::Shader* shader) {
    Uniforms uniforms{};
    if (shader == nullptr) {
      return uniforms;
    }
    uniforms.view_proj = shader->uniform_handle("u_view_proj");
    uniforms.model = shader->uniform_handle("u_model");
    uniforms.variation_scale = shader->optional_uniform_handle("u_variation_scale");
    uniforms.color = shader->uniform_handle("u_color");
    uniforms.wear_params = shader->optional_uniform_handle("u_wear_params");
    uniforms.alpha = shader->uniform_handle("u_alpha");
    uniforms.use_texture = shader->optional_uniform_handle("u_use_texture");
    uniforms.texture = shader->optional_uniform_handle("u_texture");
    uniforms.material_id = shader->optional_uniform_handle("u_material_id");
    uniforms.role_colors = shader->optional_uniform_handle("u_role_colors[0]");
    uniforms.role_color_count = shader->optional_uniform_handle("u_role_color_count");
    uniforms.light_dir = shader->optional_uniform_handle("u_light_dir");
    uniforms.ambient_strength = shader->optional_uniform_handle("u_ambient_strength");
    uniforms.camera_position = shader->uniform_handle("u_camera_position");
    uniforms.wear_volume = shader->optional_uniform_handle("u_wear_volume");
    uniforms.has_wear_volume = shader->optional_uniform_handle("u_has_wear_volume");
    return uniforms;
  };

  for (std::size_t i = 0; i < k_variant_count; ++i) {
    m_variant_uniforms[i] = cache_for(m_variant_shaders[i]);
  }
  m_uniforms = m_variant_uniforms[0];
}

auto RiggedCharacterPipeline::is_initialized() const -> bool {
  return m_shader != nullptr && m_uniforms.view_proj != GL::Shader::InvalidUniform &&
         m_uniforms.model != GL::Shader::InvalidUniform;
}

auto RiggedCharacterPipeline::draw(const RiggedCreatureCmd& cmd,
                                   const QMatrix4x4& view_proj,
                                   const QVector3D& camera_position) -> bool {
  if (!is_initialized() || cmd.mesh == nullptr) {
    return false;
  }

  const std::size_t variant = variant_for_material(static_cast<int>(cmd.material_id));
  GL::Shader* shader =
      (m_variant_shaders[variant] != nullptr) ? m_variant_shaders[variant] : m_shader;
  const Uniforms& uniforms = (m_variant_shaders[variant] != nullptr)
                                 ? m_variant_uniforms[variant]
                                 : m_uniforms;

  shader->use();
  m_last_bound_shader = shader;
  shader->set_uniform(uniforms.view_proj, view_proj);
  shader->set_uniform(uniforms.model, cmd.world);
  shader->set_uniform(uniforms.light_dir, m_frame_environment->light_direction());
  shader->set_uniform(uniforms.ambient_strength,
                      m_frame_environment->ambient_strength());
  shader->set_uniform(uniforms.camera_position, camera_position);
  if (uniforms.variation_scale != GL::Shader::InvalidUniform) {
    shader->set_uniform(uniforms.variation_scale, cmd.variation_scale);
  }
  shader->set_uniform(uniforms.color, cmd.color);
  if (uniforms.wear_params != GL::Shader::InvalidUniform) {
    shader->set_uniform(uniforms.wear_params, cmd.wear_params);
  }
  shader->set_uniform(uniforms.alpha, cmd.alpha);
  if (uniforms.material_id != GL::Shader::InvalidUniform) {
    shader->set_uniform(uniforms.material_id, static_cast<int>(cmd.material_id));
  }
  bind_character_wear_volume(
      *shader, uniforms.wear_volume, uniforms.has_wear_volume, m_wear_volume);
  set_role_palette_uniforms(
      shader, uniforms.role_colors, uniforms.role_color_count, cmd);

  const bool has_texture =
      (cmd.texture != nullptr) && uniforms.texture != GL::Shader::InvalidUniform;
  if (uniforms.use_texture != GL::Shader::InvalidUniform) {
    shader->set_uniform(uniforms.use_texture, has_texture);
  }
  if (has_texture) {
    cmd.texture->bind(0);
    shader->set_uniform(uniforms.texture, 0);
  }

  auto* fn = gl_funcs();
  if (fn != nullptr) {

    if (cmd.palette_ubo != 0 && cmd.bone_palette == nullptr) {
      fn->glBindBufferRange(GL_UNIFORM_BUFFER,
                            k_bone_palette_binding_point,
                            static_cast<GLuint>(cmd.palette_ubo),
                            static_cast<GLintptr>(cmd.palette_offset),
                            static_cast<GLsizeiptr>(BonePaletteArena::k_palette_bytes));
    } else {
      if (m_palette_ubo == 0) {
        fn->glGenBuffers(1, &m_palette_ubo);
      }
      if (m_palette_ubo != 0) {
        if (BonePaletteArena::k_palette_bytes > m_palette_ubo_capacity_bytes) {
          fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);
          fn->glBufferData(GL_UNIFORM_BUFFER,
                           static_cast<GLsizeiptr>(BonePaletteArena::k_palette_bytes),
                           nullptr,
                           GL_DYNAMIC_DRAW);
          m_palette_ubo_capacity_bytes = BonePaletteArena::k_palette_bytes;
        }
        copy_palette_to_scratch(cmd, m_palette_scratch);
        fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);
        fn->glBufferSubData(GL_UNIFORM_BUFFER,
                            0,
                            static_cast<GLsizeiptr>(BonePaletteArena::k_palette_bytes),
                            m_palette_scratch.data());
        fn->glBindBufferRange(
            GL_UNIFORM_BUFFER,
            k_bone_palette_binding_point,
            m_palette_ubo,
            0,
            static_cast<GLsizeiptr>(BonePaletteArena::k_palette_bytes));
      }
    }
  }

  cmd.mesh->draw();
  return true;
}

} // namespace Render::GL::BackendPipelines

#include "rigged_character_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

#include <algorithm>
#include <array>
#include <vector>

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

  cache_uniforms();

  m_shader->bind_uniform_block("BonePalette", k_bone_palette_binding_point);

  return is_initialized();
}

void RiggedCharacterPipeline::shutdown() {
  m_shader = nullptr;
  m_uniforms = Uniforms{};

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
  if (m_shader == nullptr) {
    return;
  }

  m_uniforms.view_proj = m_shader->uniform_handle("u_view_proj");
  m_uniforms.model = m_shader->uniform_handle("u_model");
  m_uniforms.variation_scale = m_shader->optional_uniform_handle("u_variation_scale");
  m_uniforms.color = m_shader->uniform_handle("u_color");
  m_uniforms.wear_params = m_shader->optional_uniform_handle("u_wear_params");
  m_uniforms.alpha = m_shader->uniform_handle("u_alpha");
  m_uniforms.use_texture = m_shader->optional_uniform_handle("u_use_texture");
  m_uniforms.texture = m_shader->optional_uniform_handle("u_texture");
  m_uniforms.material_id = m_shader->optional_uniform_handle("u_material_id");
  m_uniforms.role_colors = m_shader->optional_uniform_handle("u_role_colors[0]");
  m_uniforms.role_color_count = m_shader->optional_uniform_handle("u_role_color_count");
  m_uniforms.light_dir = m_shader->optional_uniform_handle("u_light_dir");
  m_uniforms.ambient_strength = m_shader->optional_uniform_handle("u_ambient_strength");
  m_uniforms.camera_position = m_shader->uniform_handle("u_camera_position");
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

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.model, cmd.world);
  m_shader->set_uniform(m_uniforms.light_dir, m_frame_environment->light_direction());
  m_shader->set_uniform(m_uniforms.ambient_strength,
                        m_frame_environment->ambient_strength());
  m_shader->set_uniform(m_uniforms.camera_position, camera_position);
  if (m_uniforms.variation_scale != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.variation_scale, cmd.variation_scale);
  }
  m_shader->set_uniform(m_uniforms.color, cmd.color);
  if (m_uniforms.wear_params != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.wear_params, cmd.wear_params);
  }
  m_shader->set_uniform(m_uniforms.alpha, cmd.alpha);
  if (m_uniforms.material_id != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.material_id, static_cast<int>(cmd.material_id));
  }
  set_role_palette_uniforms(
      m_shader, m_uniforms.role_colors, m_uniforms.role_color_count, cmd);

  const bool has_texture =
      (cmd.texture != nullptr) && m_uniforms.texture != GL::Shader::InvalidUniform;
  if (m_uniforms.use_texture != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.use_texture, has_texture);
  }
  if (has_texture) {
    cmd.texture->bind(0);
    m_shader->set_uniform(m_uniforms.texture, 0);
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

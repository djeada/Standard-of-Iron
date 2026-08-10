#include "rigged_cull_pipeline.h"

#include <QDebug>
#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QTextStream>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cstring>

#include "render/draw_commands.h"
#include "render/gl/buffer.h"
#include "render/gl/gl_capabilities.h"
#include "render/gl/shader_cache.h"
#include "render/rigged_mesh.h"
#include "utils/resource_utils.h"

namespace Render::GL::BackendPipelines {

namespace {

constexpr GLuint k_vertex_binding = 0;
constexpr GLuint k_index_binding = 1;
constexpr GLuint k_palette_binding = 2;
constexpr GLuint k_instance_binding = 3;
constexpr GLuint k_out_index_binding = 4;
constexpr GLuint k_command_binding = 5;

constexpr std::size_t k_floats_per_instance = 40;
constexpr std::size_t k_matrix_floats = 16;
constexpr GLuint k_baked_palette_binding = 6;
constexpr std::size_t k_command_words = 7;
constexpr std::size_t k_local_size_x = 64;

constexpr std::size_t k_max_out_triangles = 12u * 1024u * 1024u;

constexpr std::size_t k_min_instances_for_gpu_path = 24;

auto load_shader_text(const QString& resource_path) -> QString {
  QString const resolved = Utils::Resources::resolve_resource_path(resource_path);
  QFile file(resolved);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "RiggedCullPipeline: failed to open" << resolved;
    return {};
  }
  QTextStream stream(&file);
  return stream.readAll();
}

} // namespace

RiggedCullPipeline::~RiggedCullPipeline() {
  if (QOpenGLContext::currentContext() != nullptr) {
    shutdown();
  }
}

auto RiggedCullPipeline::minimum_instances() -> std::size_t {
  return k_min_instances_for_gpu_path;
}

auto RiggedCullPipeline::initialize() -> bool {
  m_available = false;
  if (QOpenGLContext::currentContext() == nullptr) {
    return false;
  }
  initializeOpenGLFunctions();

  if (!GLCapabilities::has_compute_shaders() || !GLCapabilities::has_indirect_draw()) {
    qInfo() << "RiggedCullPipeline: compute or indirect draw unavailable; "
               "GPU crowd culling disabled";
    return false;
  }

  QString const cull_src =
      Shader::preprocess_source(load_shader_text(":/assets/shaders/rigged_cull.comp"));
  QString const finalize_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/rigged_cull_finalize.comp"));
  QString const vert_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/character_skinned_gpudriven.vert"));
  QString const frag_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/character_skinned_instanced.frag"));
  if (cull_src.isEmpty() || finalize_src.isEmpty() || vert_src.isEmpty() ||
      frag_src.isEmpty()) {
    return false;
  }

  m_cull_shader_storage = std::make_unique<Shader>();
  m_cull_shader_storage->set_debug_name(QStringLiteral("rigged_cull"));
  if (!m_cull_shader_storage->load_compute_from_source(cull_src)) {
    m_cull_shader_storage.reset();
    return false;
  }

  m_finalize_shader_storage = std::make_unique<Shader>();
  m_finalize_shader_storage->set_debug_name(QStringLiteral("rigged_cull_finalize"));
  if (!m_finalize_shader_storage->load_compute_from_source(finalize_src)) {
    m_cull_shader_storage.reset();
    m_finalize_shader_storage.reset();
    return false;
  }

  m_draw_shader_storage = std::make_unique<Shader>();
  m_draw_shader_storage->set_debug_name(QStringLiteral("character_skinned_gpudriven"));
  if (!m_draw_shader_storage->load_from_source(vert_src, frag_src)) {
    m_cull_shader_storage.reset();
    m_finalize_shader_storage.reset();
    m_draw_shader_storage.reset();
    return false;
  }
  m_draw_shader = m_draw_shader_storage.get();

  QString const shadow_vert_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/directional_shadow_rigged_gpudriven.vert"));
  QString const shadow_frag_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/directional_shadow_depth.frag"));
  if (!shadow_vert_src.isEmpty() && !shadow_frag_src.isEmpty()) {
    m_shadow_shader_storage = std::make_unique<Shader>();
    m_shadow_shader_storage->set_debug_name(
        QStringLiteral("directional_shadow_rigged_gpudriven"));
    if (m_shadow_shader_storage->load_from_source(shadow_vert_src, shadow_frag_src)) {
      m_shadow_shader = m_shadow_shader_storage.get();
    } else {
      m_shadow_shader_storage.reset();
    }
  }

  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_command_buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_command_buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               static_cast<GLsizeiptr>(k_command_words * sizeof(GLuint)),
               nullptr,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  m_available = m_vao != 0 && m_command_buffer != 0;
  if (m_available) {
    qInfo() << "RiggedCullPipeline: GPU crowd culling enabled";
  }
  return m_available;
}

void RiggedCullPipeline::shutdown() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_available = false;
    return;
  }
  initializeOpenGLFunctions();
  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  for (GLuint* buffer : {&m_palette_ssbo,
                         &m_instance_ssbo,
                         &m_out_index_buffer,
                         &m_command_buffer,
                         &m_role_color_buffer}) {
    if (*buffer != 0) {
      glDeleteBuffers(1, buffer);
      *buffer = 0;
    }
  }
  if (m_role_color_texture != 0) {
    glDeleteTextures(1, &m_role_color_texture);
    m_role_color_texture = 0;
  }
  m_cull_shader_storage.reset();
  m_finalize_shader_storage.reset();
  m_draw_shader_storage.reset();
  m_shadow_shader_storage.reset();
  m_draw_shader = nullptr;
  m_shadow_shader = nullptr;
  m_available = false;
}

auto RiggedCullPipeline::ensure_buffers(std::size_t instance_count,
                                        std::size_t bone_count,
                                        std::size_t candidate_triangles) -> bool {
  std::size_t const palette_bytes = instance_count * bone_count * 16 * sizeof(float);
  if (palette_bytes > m_palette_capacity_bytes || m_palette_ssbo == 0) {
    if (m_palette_ssbo == 0) {
      glGenBuffers(1, &m_palette_ssbo);
    }
    std::size_t const capacity = palette_bytes + (palette_bytes / 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_palette_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_palette_capacity_bytes = capacity;
  }

  std::size_t const instance_bytes =
      instance_count * k_floats_per_instance * sizeof(float);
  if (instance_bytes > m_instance_capacity_bytes || m_instance_ssbo == 0) {
    if (m_instance_ssbo == 0) {
      glGenBuffers(1, &m_instance_ssbo);
    }
    std::size_t const capacity = instance_bytes + (instance_bytes / 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instance_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_instance_capacity_bytes = capacity;
  }

  std::size_t wanted = std::min(candidate_triangles, k_max_out_triangles);
  wanted = std::max<std::size_t>(wanted / 4,
                                 std::min<std::size_t>(candidate_triangles, 1u << 18u));
  if (wanted > m_out_capacity_triangles || m_out_index_buffer == 0) {
    if (m_out_index_buffer == 0) {
      glGenBuffers(1, &m_out_index_buffer);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_out_index_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(wanted * 3 * sizeof(GLuint)),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_out_capacity_triangles = wanted;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_out_index_buffer);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return m_palette_ssbo != 0 && m_instance_ssbo != 0 && m_out_index_buffer != 0;
}

auto RiggedCullPipeline::upload_instances(const RiggedCreatureCmd* const* cmds,
                                          std::size_t count,
                                          std::size_t bone_count) -> bool {
  std::size_t owned_instances = 0;
  for (std::size_t k = 0; k < count; ++k) {
    if (!cmds[k]->palette_frames_resident) {
      ++owned_instances;
    }
  }
  m_palette_scratch.assign(owned_instances * bone_count * k_matrix_floats, 0.0F);
  m_instance_scratch.assign(count * k_floats_per_instance, 0.0F);
  m_stats.resident_instances = static_cast<std::uint32_t>(count - owned_instances);

  constexpr auto k_matrix_bytes =
      static_cast<std::uint32_t>(sizeof(float) * k_matrix_floats);
  std::size_t owned_cursor = 0;
  for (std::size_t k = 0; k < count; ++k) {
    const RiggedCreatureCmd& cmd = *cmds[k];

    std::array<std::uint32_t, 4> palette_ref{0U, 0U, 0U, 0U};
    float blend = 0.0F;
    if (cmd.palette_frames_resident) {
      palette_ref[0] = 1U;
      palette_ref[1] = cmd.palette_offset / k_matrix_bytes;
      palette_ref[2] = cmd.palette_next_offset / k_matrix_bytes;
      blend = cmd.palette_lerp;
    } else {
      if (cmd.bone_palette == nullptr) {
        return false;
      }
      palette_ref[1] = static_cast<std::uint32_t>(owned_cursor * bone_count);
      float* palette_dst =
          m_palette_scratch.data() + (owned_cursor * bone_count * k_matrix_floats);
      for (std::size_t b = 0; b < bone_count; ++b) {
        std::memcpy(palette_dst + (b * k_matrix_floats),
                    cmd.bone_palette[b].constData(),
                    sizeof(float) * k_matrix_floats);
      }
      ++owned_cursor;
    }

    float* dst = m_instance_scratch.data() + k * k_floats_per_instance;
    std::memcpy(dst + 32, palette_ref.data(), sizeof(palette_ref));
    dst[36] = blend;
    std::memcpy(dst, cmd.world.constData(), sizeof(float) * 16);
    dst[16] = cmd.color.x();
    dst[17] = cmd.color.y();
    dst[18] = cmd.color.z();
    dst[19] = cmd.alpha;
    dst[20] = cmd.variation_scale.x();
    dst[21] = cmd.variation_scale.y();
    dst[22] = cmd.variation_scale.z();
    dst[23] = static_cast<float>(cmd.material_id);
    dst[24] = cmd.wear_params.x();
    dst[25] = cmd.wear_params.y();
    dst[26] = cmd.wear_params.z();
    dst[27] = cmd.wear_params.w();
    dst[28] = static_cast<float>(cmd.role_color_count);
  }

  if (!m_palette_scratch.empty()) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_palette_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(m_palette_scratch.size() * sizeof(float)),
                    m_palette_scratch.data());
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instance_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(m_instance_scratch.size() * sizeof(float)),
                  m_instance_scratch.data());
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return true;
}

void RiggedCullPipeline::bind_role_color_texture(const RiggedCreatureCmd* const* cmds,
                                                 std::size_t count) {
  constexpr std::size_t k_roles = RiggedCreatureCmd::k_max_role_colors;
  m_role_color_scratch.assign(count * k_roles * 4, 0.0F);
  for (std::size_t k = 0; k < count; ++k) {
    const RiggedCreatureCmd& cmd = *cmds[k];
    std::size_t const roles =
        std::min<std::size_t>(cmd.role_color_count, cmd.role_colors.size());
    float* dst = m_role_color_scratch.data() + k * k_roles * 4;
    for (std::size_t r = 0; r < roles; ++r) {
      dst[r * 4 + 0] = cmd.role_colors[r].x();
      dst[r * 4 + 1] = cmd.role_colors[r].y();
      dst[r * 4 + 2] = cmd.role_colors[r].z();
    }
  }

  std::size_t const bytes = m_role_color_scratch.size() * sizeof(float);
  if (m_role_color_buffer == 0) {
    glGenBuffers(1, &m_role_color_buffer);
  }
  glBindBuffer(GL_TEXTURE_BUFFER, m_role_color_buffer);
  if (bytes > m_role_color_capacity_bytes) {
    std::size_t const capacity = ((bytes + 4095U) / 4096U) * 4096U;
    glBufferData(
        GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, GL_DYNAMIC_DRAW);
    m_role_color_capacity_bytes = capacity;
  }
  glBufferSubData(GL_TEXTURE_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(bytes),
                  m_role_color_scratch.data());
  if (m_role_color_texture == 0) {
    glGenTextures(1, &m_role_color_texture);
    glBindTexture(GL_TEXTURE_BUFFER, m_role_color_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_role_color_buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_role_color_texture);
}

auto RiggedCullPipeline::draw(const RiggedCreatureCmd* const* cmds,
                              std::size_t count,
                              const QMatrix4x4& view_proj,
                              const QVector3D& camera_position,
                              const QVector2D& viewport) -> bool {
  return dispatch(cmds, count, view_proj, camera_position, viewport, Pass::Color);
}

auto RiggedCullPipeline::draw_shadow(const RiggedCreatureCmd* const* cmds,
                                     std::size_t count,
                                     const QMatrix4x4& light_view_proj,
                                     const QVector2D& shadow_extent) -> bool {
  return dispatch(
      cmds, count, light_view_proj, QVector3D{}, shadow_extent, Pass::Depth);
}

auto RiggedCullPipeline::dispatch(const RiggedCreatureCmd* const* cmds,
                                  std::size_t count,
                                  const QMatrix4x4& view_proj,
                                  const QVector3D& camera_position,
                                  const QVector2D& viewport,
                                  Pass pass) -> bool {
  m_stats = {};
  if (!m_available || cmds == nullptr || count < k_min_instances_for_gpu_path) {
    return false;
  }
  const bool depth_only = pass == Pass::Depth;
  RiggedMesh* mesh = depth_only && cmds[0]->shadow_mesh != nullptr
                         ? cmds[0]->shadow_mesh
                         : cmds[0]->mesh;
  if (mesh == nullptr || cmds[0]->texture != nullptr || cmds[0]->bone_count == 0) {
    return false;
  }
  std::size_t const bone_count = cmds[0]->bone_count;
  GLuint baked_palette_buffer = 0;
  for (std::size_t k = 0; k < count; ++k) {
    RiggedMesh* cmd_mesh =
        depth_only && cmds[k] != nullptr && cmds[k]->shadow_mesh != nullptr
            ? cmds[k]->shadow_mesh
            : (cmds[k] != nullptr ? cmds[k]->mesh : nullptr);
    if (cmds[k] == nullptr || cmd_mesh != mesh || cmds[k]->texture != nullptr ||
        cmds[k]->bone_count != bone_count) {
      return false;
    }
    if (cmds[k]->palette_frames_resident) {

      if (baked_palette_buffer == 0) {
        baked_palette_buffer = cmds[k]->palette_ubo;
      } else if (baked_palette_buffer != cmds[k]->palette_ubo) {
        return false;
      }
    } else if (cmds[k]->bone_palette == nullptr) {
      return false;
    }
  }
  if (!mesh->ensure_gl_buffers()) {
    return false;
  }
  Buffer* vertex_buffer = mesh->vertex_buffer();
  Buffer* index_buffer = mesh->index_buffer();
  if (vertex_buffer == nullptr || index_buffer == nullptr || vertex_buffer->id() == 0 ||
      index_buffer->id() == 0) {
    return false;
  }

  std::size_t const triangle_count = mesh->index_count() / 3;
  if (triangle_count == 0) {
    return false;
  }
  std::size_t const candidate_triangles = triangle_count * count;

  if (!ensure_buffers(count, bone_count, candidate_triangles)) {
    return false;
  }
  if (!upload_instances(cmds, count, bone_count)) {
    return false;
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_vertex_binding, vertex_buffer->id());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_index_binding, index_buffer->id());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_palette_binding, m_palette_ssbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                   k_baked_palette_binding,
                   baked_palette_buffer != 0 ? baked_palette_buffer : m_palette_ssbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_instance_binding, m_instance_ssbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_out_index_binding, m_out_index_buffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_command_binding, m_command_buffer);

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLint cull_mode = GL_BACK;
  GLint front_face = GL_CCW;
  glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
  glGetIntegerv(GL_FRONT_FACE, &front_face);
  bool const cull_backfaces = cull_enabled == GL_TRUE;

  const std::array<GLuint, k_command_words> reset{0U, 1U, 0U, 0U, 0U, 0U, 0U};
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_command_buffer);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(reset.size() * sizeof(GLuint)),
                  reset.data());
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  Shader* finalize = m_finalize_shader_storage.get();
  Shader* cull = m_cull_shader_storage.get();
  cull->use();
  cull->set_uniform(cull->uniform_handle("u_view_proj"), view_proj);
  glUniform1ui(cull->uniform_handle("u_vertex_count"),
               static_cast<GLuint>(mesh->vertex_count()));
  glUniform1ui(cull->uniform_handle("u_bone_count"), static_cast<GLuint>(bone_count));
  glUniform1ui(cull->uniform_handle("u_triangle_count"),
               static_cast<GLuint>(triangle_count));
  glUniform1ui(cull->uniform_handle("u_instance_count"), static_cast<GLuint>(count));
  glUniform1ui(cull->uniform_handle("u_out_capacity_triangles"),
               static_cast<GLuint>(m_out_capacity_triangles));
  cull->set_uniform(cull->uniform_handle("u_viewport"), viewport);
  bool const front_is_ccw = (front_face == GL_CCW) == (cull_mode == GL_BACK);
  cull->set_uniform(cull->uniform_handle("u_front_face_ccw"), front_is_ccw ? 1 : 0);
  cull->set_uniform(cull->uniform_handle("u_cull_backfaces"), cull_backfaces ? 1 : 0);

  auto const groups_x =
      static_cast<GLuint>((triangle_count + k_local_size_x - 1) / k_local_size_x);
  glDispatchCompute(groups_x, static_cast<GLuint>(count), 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  finalize->use();
  glUniform1ui(finalize->uniform_handle("u_out_capacity_triangles"),
               static_cast<GLuint>(m_out_capacity_triangles));
  glUniform1ui(finalize->uniform_handle("u_reset"), 0U);
  glDispatchCompute(1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
                  GL_ELEMENT_ARRAY_BARRIER_BIT);

  Shader* draw_shader = depth_only ? m_shadow_shader : m_draw_shader;
  if (draw_shader == nullptr) {
    return false;
  }
  if (!depth_only) {
    bind_role_color_texture(cmds, count);
  }

  draw_shader->use();
  draw_shader->set_uniform(draw_shader->uniform_handle("u_view_proj"), view_proj);
  glUniform1ui(draw_shader->uniform_handle("u_vertex_count"),
               static_cast<GLuint>(mesh->vertex_count()));
  glUniform1ui(draw_shader->uniform_handle("u_bone_count"),
               static_cast<GLuint>(bone_count));
  if (!depth_only) {
    draw_shader->set_uniform(draw_shader->uniform_handle("u_camera_position"),
                             camera_position);
    auto const role_tbo = draw_shader->optional_uniform_handle("u_role_color_tbo");
    if (role_tbo != Shader::InvalidUniform) {
      draw_shader->set_uniform(role_tbo, 0);
    }
  }

  glBindVertexArray(m_vao);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_command_buffer);
  glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
  glBindVertexArray(0);

  m_stats.dispatched_instances = static_cast<std::uint32_t>(count);
  m_stats.candidate_triangles = static_cast<std::uint32_t>(candidate_triangles);

  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedCullPipeline: GL error" << err;
    return false;
  }
  return true;
}

} // namespace Render::GL::BackendPipelines

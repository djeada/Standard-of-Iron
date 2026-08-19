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
#include <limits>

#include "render/draw_commands.h"
#include "render/gl/buffer.h"
#include "render/gl/gl_capabilities.h"
#include "render/gl/platform_gl.h"
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

constexpr std::size_t k_stream_instance_capacity = 128U * 1024U;
constexpr std::size_t k_stream_role_color_palette_capacity = 8U * 1024U + 1U;
constexpr std::size_t k_stream_owned_palette_instance_capacity = 4U * 1024U;
constexpr std::size_t k_max_owned_bones = 64U;

constexpr std::size_t k_max_stream_bytes = 256U * 1024U * 1024U;

constexpr std::size_t k_min_instances_for_gpu_path = 24;
constexpr std::size_t k_min_instances_for_full_mesh_path = 1;

[[nodiscard]] constexpr auto
output_capacity_for(std::size_t candidate_triangles) noexcept -> std::size_t {
  return candidate_triangles <= k_max_out_triangles ? candidate_triangles : 0U;
}

static_assert(output_capacity_for(k_max_out_triangles) == k_max_out_triangles);
static_assert(output_capacity_for(k_max_out_triangles + 1U) == 0U);

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
      load_shader_text(":/assets/shaders/character_skinned_gpu_instanced.frag"));
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

  QString const full_mesh_vert_src = Shader::preprocess_source(
      load_shader_text(":/assets/shaders/character_skinned_gpu_instanced.vert"));
  if (!full_mesh_vert_src.isEmpty()) {
    m_full_mesh_shader_storage = std::make_unique<Shader>();
    m_full_mesh_shader_storage->set_debug_name(
        QStringLiteral("character_skinned_gpu_instanced"));
    if (m_full_mesh_shader_storage->load_from_source(full_mesh_vert_src, frag_src)) {
      m_full_mesh_shader = m_full_mesh_shader_storage.get();
    } else {
      m_full_mesh_shader_storage.reset();
    }
  }

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

  QString const full_mesh_shadow_vert_src = Shader::preprocess_source(load_shader_text(
      ":/assets/shaders/directional_shadow_rigged_gpu_instanced.vert"));
  if (!full_mesh_shadow_vert_src.isEmpty() && !shadow_frag_src.isEmpty()) {
    m_full_mesh_shadow_shader_storage = std::make_unique<Shader>();
    m_full_mesh_shadow_shader_storage->set_debug_name(
        QStringLiteral("directional_shadow_rigged_gpu_instanced"));
    if (m_full_mesh_shadow_shader_storage->load_from_source(full_mesh_shadow_vert_src,
                                                            shadow_frag_src)) {
      m_full_mesh_shadow_shader = m_full_mesh_shadow_shader_storage.get();
    } else {
      m_full_mesh_shadow_shader_storage.reset();
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
  m_full_mesh_shader_storage.reset();
  m_full_mesh_shadow_shader_storage.reset();
  m_draw_shader = nullptr;
  m_shadow_shader = nullptr;
  m_full_mesh_shader = nullptr;
  m_full_mesh_shadow_shader = nullptr;
  m_available = false;
}

void RiggedCullPipeline::begin_frame() {
  if (!m_available) {
    return;
  }
  m_palette_stream_cursor_bytes = 0U;
  m_instance_stream_cursor_bytes = 0U;
  m_role_color_stream_cursor_bytes = 0U;
  m_role_color_palette_indices.clear();

  const auto orphan_stream = [this](GLenum target,
                                    GLuint& buffer,
                                    std::size_t& capacity,
                                    std::size_t wanted) {
    if (buffer == 0U) {
      glGenBuffers(1, &buffer);
    }
    const std::size_t retained = std::max(wanted, capacity);
    glBindBuffer(target, buffer);
    glBufferData(target, static_cast<GLsizeiptr>(retained), nullptr, GL_STREAM_DRAW);
    capacity = retained;
  };
  orphan_stream(GL_SHADER_STORAGE_BUFFER,
                m_palette_ssbo,
                m_palette_capacity_bytes,
                k_stream_owned_palette_instance_capacity * k_max_owned_bones *
                    k_matrix_floats * sizeof(float));
  orphan_stream(GL_SHADER_STORAGE_BUFFER,
                m_instance_ssbo,
                m_instance_capacity_bytes,
                k_stream_instance_capacity * k_floats_per_instance * sizeof(float));
  orphan_stream(GL_TEXTURE_BUFFER,
                m_role_color_buffer,
                m_role_color_capacity_bytes,
                k_stream_role_color_palette_capacity *
                    RiggedCreatureCmd::k_max_role_colors * 4U * sizeof(float));
  if (m_role_color_texture == 0U) {
    glGenTextures(1, &m_role_color_texture);
  }
  glBindTexture(GL_TEXTURE_BUFFER, m_role_color_texture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_role_color_buffer);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  constexpr std::size_t k_empty_palette_bytes =
      RiggedCreatureCmd::k_max_role_colors * 4U * sizeof(float);
  const std::array<float, RiggedCreatureCmd::k_max_role_colors * 4U> empty_palette{};
  glBufferSubData(GL_TEXTURE_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(k_empty_palette_bytes),
                  empty_palette.data());
  m_role_color_stream_cursor_bytes = k_empty_palette_bytes;
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void RiggedCullPipeline::end_frame() {
}

auto RiggedCullPipeline::ensure_instance_buffers(std::size_t instance_count,
                                                 std::size_t bone_count) -> bool {
  const std::size_t palette_bytes = instance_count * bone_count * 16 * sizeof(float);
  const std::size_t instance_bytes =
      instance_count * k_floats_per_instance * sizeof(float);
  return m_palette_ssbo != 0U && m_instance_ssbo != 0U &&
         palette_bytes <= k_max_stream_bytes && instance_bytes <= k_max_stream_bytes;
}

auto RiggedCullPipeline::ensure_buffers(std::size_t instance_count,
                                        std::size_t bone_count,
                                        std::size_t candidate_triangles) -> bool {
  if (!ensure_instance_buffers(instance_count, bone_count)) {
    return false;
  }

  const std::size_t wanted = output_capacity_for(candidate_triangles);
  if (wanted == 0U) {
    return false;
  }
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
  return m_out_index_buffer != 0;
}

auto RiggedCullPipeline::ensure_stream_capacity(GLuint buffer,
                                                std::size_t& capacity_bytes,
                                                std::size_t& cursor_bytes,
                                                std::size_t wanted_bytes) -> bool {
  if (cursor_bytes + wanted_bytes <= capacity_bytes) {
    return true;
  }
  if (wanted_bytes > k_max_stream_bytes) {
    return false;
  }
  const std::size_t grown = std::min(
      k_max_stream_bytes, std::max(capacity_bytes * 2U, cursor_bytes + wanted_bytes));
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               static_cast<GLsizeiptr>(grown),
               nullptr,
               GL_STREAM_DRAW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  capacity_bytes = grown;
  cursor_bytes = 0U;
  return true;
}

auto RiggedCullPipeline::upload_instances(const RiggedCreatureCmd* const* cmds,
                                          std::size_t count,
                                          std::size_t bone_count,
                                          bool depth_only) -> bool {
  m_owned_palette_slots.clear();
  m_owned_palette_by_cmd.resize(count);
  std::size_t owned_instances = 0;
  std::size_t distinct_owned = 0;
  for (std::size_t k = 0; k < count; ++k) {
    const RiggedCreatureCmd& cmd = *cmds[k];
    if (cmd.palette_frames_resident) {
      continue;
    }
    if (cmd.bone_palette == nullptr) {
      return false;
    }
    ++owned_instances;
    auto const [it, inserted] =
        m_owned_palette_slots.try_emplace(cmd.bone_palette, distinct_owned);
    if (inserted) {
      ++distinct_owned;
    }
    m_owned_palette_by_cmd[k] = it->second;
  }
  m_palette_scratch.resize(distinct_owned * bone_count * k_matrix_floats);
  m_instance_scratch.resize(count * k_floats_per_instance);
  m_stats.resident_instances = static_cast<std::uint32_t>(count - owned_instances);

  for (auto const& [palette, slot] : m_owned_palette_slots) {
    float* palette_dst =
        m_palette_scratch.data() + (slot * bone_count * k_matrix_floats);
    for (std::size_t b = 0; b < bone_count; ++b) {
      std::memcpy(palette_dst + (b * k_matrix_floats),
                  palette[b].constData(),
                  sizeof(float) * k_matrix_floats);
    }
  }

  std::size_t palette_matrix_base = 0U;
  bool streamed_palette = false;
  if (!m_palette_scratch.empty()) {
    const std::size_t bytes = m_palette_scratch.size() * sizeof(float);
    if (!ensure_stream_capacity(m_palette_ssbo,
                                m_palette_capacity_bytes,
                                m_palette_stream_cursor_bytes,
                                bytes)) {
      return false;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_palette_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    static_cast<GLintptr>(m_palette_stream_cursor_bytes),
                    static_cast<GLsizeiptr>(bytes),
                    m_palette_scratch.data());
    palette_matrix_base =
        m_palette_stream_cursor_bytes / (k_matrix_floats * sizeof(float));
    m_palette_stream_cursor_bytes += bytes;
    streamed_palette = true;
  }

  constexpr auto k_matrix_bytes =
      static_cast<std::uint32_t>(sizeof(float) * k_matrix_floats);
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
      palette_ref[1] =
          static_cast<std::uint32_t>((streamed_palette ? palette_matrix_base : 0U) +
                                     m_owned_palette_by_cmd[k] * bone_count);
    }

    float* dst = m_instance_scratch.data() + k * k_floats_per_instance;
    std::memcpy(dst + 32, palette_ref.data(), sizeof(palette_ref));
    dst[36] = blend;
    std::memcpy(dst, cmd.world.constData(), sizeof(float) * 16);
    dst[20] = cmd.variation_scale.x();
    dst[21] = cmd.variation_scale.y();
    dst[22] = cmd.variation_scale.z();
    if (depth_only) {
      continue;
    }
    dst[16] = cmd.color.x();
    dst[17] = cmd.color.y();
    dst[18] = cmd.color.z();
    dst[19] = cmd.alpha;
    dst[23] = static_cast<float>(cmd.material_id);
    dst[24] = cmd.wear_params.x();
    dst[25] = cmd.wear_params.y();
    dst[26] = cmd.wear_params.z();
    dst[27] = cmd.wear_params.w();
    dst[28] = static_cast<float>(cmd.role_color_count);
    dst[29] = static_cast<float>(role_color_palette_index(cmd));
  }

  m_instance_base = 0U;
  const std::size_t instance_bytes = m_instance_scratch.size() * sizeof(float);
  if (!ensure_stream_capacity(m_instance_ssbo,
                              m_instance_capacity_bytes,
                              m_instance_stream_cursor_bytes,
                              instance_bytes)) {
    return false;
  }
  m_instance_base = static_cast<std::uint32_t>(m_instance_stream_cursor_bytes /
                                               (k_floats_per_instance * sizeof(float)));
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instance_ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                  static_cast<GLintptr>(m_instance_stream_cursor_bytes),
                  static_cast<GLsizeiptr>(instance_bytes),
                  m_instance_scratch.data());
  m_instance_stream_cursor_bytes += instance_bytes;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return true;
}

auto RiggedCullPipeline::role_color_palette_index(const RiggedCreatureCmd& cmd)
    -> std::uint32_t {
  if (cmd.role_colors == nullptr || cmd.role_color_count == 0U) {
    return 0U;
  }
  const auto* key = cmd.role_colors.get();
  if (const auto it = m_role_color_palette_indices.find(key);
      it != m_role_color_palette_indices.end()) {
    return it->second;
  }

  constexpr std::size_t k_roles = RiggedCreatureCmd::k_max_role_colors;
  constexpr std::size_t k_floats = k_roles * 4U;
  constexpr std::size_t k_bytes = k_floats * sizeof(float);
  if (m_role_color_stream_cursor_bytes + k_bytes > m_role_color_capacity_bytes) {
    return 0U;
  }
  std::array<float, k_floats> packed{};
  const auto role_colors = cmd.role_colors->view();
  const std::size_t roles =
      std::min<std::size_t>(cmd.role_color_count, role_colors.size());
  for (std::size_t r = 0; r < roles; ++r) {
    packed[r * 4U + 0U] = role_colors[r].x();
    packed[r * 4U + 1U] = role_colors[r].y();
    packed[r * 4U + 2U] = role_colors[r].z();
  }
  const auto palette_index =
      static_cast<std::uint32_t>(m_role_color_stream_cursor_bytes / k_bytes);
  glBindBuffer(GL_TEXTURE_BUFFER, m_role_color_buffer);
  glBufferSubData(GL_TEXTURE_BUFFER,
                  static_cast<GLintptr>(m_role_color_stream_cursor_bytes),
                  static_cast<GLsizeiptr>(k_bytes),
                  packed.data());
  m_role_color_stream_cursor_bytes += k_bytes;
  m_role_color_palette_indices.emplace(key, palette_index);
  return palette_index;
}

auto RiggedCullPipeline::bind_role_color_texture() -> bool {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_role_color_texture);
  return m_role_color_buffer != 0U && m_role_color_texture != 0U;
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

auto RiggedCullPipeline::draw_full_mesh(const RiggedCreatureCmd* const* cmds,
                                        std::size_t count,
                                        const QMatrix4x4& view_proj,
                                        const QVector3D& camera_position) -> bool {
  return draw_full_mesh_pass(cmds, count, view_proj, camera_position, Pass::Color);
}

auto RiggedCullPipeline::draw_full_mesh_shadow(const RiggedCreatureCmd* const* cmds,
                                               std::size_t count,
                                               const QMatrix4x4& light_view_proj)
    -> bool {
  return draw_full_mesh_pass(cmds, count, light_view_proj, QVector3D{}, Pass::Depth);
}

auto RiggedCullPipeline::draw_full_mesh_pass(const RiggedCreatureCmd* const* cmds,
                                             std::size_t count,
                                             const QMatrix4x4& view_proj,
                                             const QVector3D& camera_position,
                                             Pass pass) -> bool {
  m_stats = {};
  if (!m_available || cmds == nullptr || count < k_min_instances_for_full_mesh_path) {
    return false;
  }

  const bool depth_only = pass == Pass::Depth;
  Shader* draw_shader = depth_only ? m_full_mesh_shadow_shader : m_full_mesh_shader;
  if (draw_shader == nullptr) {
    return false;
  }

  RiggedMesh* mesh = depth_only && cmds[0]->shadow_mesh != nullptr
                         ? cmds[0]->shadow_mesh
                         : cmds[0]->mesh;
  if (mesh == nullptr || cmds[0]->texture != nullptr || cmds[0]->bone_count == 0U ||
      mesh->index_count() == 0U) {
    return false;
  }

  const std::size_t bone_count = cmds[0]->bone_count;
  GLuint baked_palette_buffer = 0U;
  for (std::size_t k = 0; k < count; ++k) {
    if (cmds[k] == nullptr) {
      return false;
    }
    RiggedMesh* cmd_mesh = depth_only && cmds[k]->shadow_mesh != nullptr
                               ? cmds[k]->shadow_mesh
                               : cmds[k]->mesh;
    if (cmd_mesh != mesh || cmds[k]->texture != nullptr ||
        cmds[k]->bone_count != bone_count) {
      return false;
    }
    if (cmds[k]->palette_frames_resident) {
      if (baked_palette_buffer == 0U) {
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
  if (vertex_buffer == nullptr || index_buffer == nullptr ||
      vertex_buffer->id() == 0U || index_buffer->id() == 0U ||
      !ensure_instance_buffers(count, bone_count) ||
      !upload_instances(cmds, count, bone_count, depth_only)) {
    return false;
  }

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_vertex_binding, vertex_buffer->id());
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_palette_binding, m_palette_ssbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                   k_baked_palette_binding,
                   baked_palette_buffer != 0U ? baked_palette_buffer : m_palette_ssbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, k_instance_binding, m_instance_ssbo);
  if (!depth_only && !bind_role_color_texture()) {
    return false;
  }

  draw_shader->use();
  draw_shader->set_uniform(draw_shader->uniform_handle("u_view_proj"), view_proj);
  draw_shader->set_uniform(draw_shader->uniform_handle("u_bone_count"),
                           static_cast<GLuint>(bone_count));
  draw_shader->set_uniform(draw_shader->uniform_handle("u_instance_base"),
                           static_cast<GLuint>(m_instance_base));
  const auto rigid_uniform = draw_shader->uniform_handle("u_rigid_skinning");
  if (!depth_only) {
    draw_shader->set_uniform(draw_shader->uniform_handle("u_camera_position"),
                             camera_position);
    const auto role_tbo = draw_shader->optional_uniform_handle("u_role_color_tbo");
    if (role_tbo != Shader::InvalidUniform) {
      draw_shader->set_uniform(role_tbo, 0);
    }
    const auto role_base = draw_shader->optional_uniform_handle("u_role_color_base");
    if (role_base != Shader::InvalidUniform) {
      draw_shader->set_uniform(role_base, 0);
    }
  }

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->id());
  const auto draw_range =
      [&](std::size_t index_offset, std::size_t index_count, bool rigid) {
        if (index_count == 0U) {
          return;
        }
        draw_shader->set_uniform(rigid_uniform, rigid ? 1 : 0);
        glDrawElementsInstanced(
            GL_TRIANGLES,
            static_cast<GLsizei>(index_count),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(index_offset * sizeof(std::uint32_t)),
            static_cast<GLsizei>(count));
        ++m_stats.draw_calls;
      };
  draw_range(0U, mesh->rigid_index_count(), true);
  draw_range(mesh->rigid_index_count(),
             mesh->index_count() - mesh->rigid_index_count(),
             false);
  glBindVertexArray(0);

  m_stats.dispatched_instances = static_cast<std::uint32_t>(count);
  m_stats.candidate_triangles = static_cast<std::uint32_t>(std::min<std::size_t>(
      mesh->index_count() / 3U * count, std::numeric_limits<std::uint32_t>::max()));
#ifndef NDEBUG
  const GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedCullPipeline: full-mesh GL error" << err;
    return false;
  }
#endif
  return true;
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
  if (!upload_instances(cmds, count, bone_count, depth_only)) {
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
  cull->set_uniform(cull->uniform_handle("u_vertex_count"),
                    static_cast<GLuint>(mesh->vertex_count()));
  cull->set_uniform(cull->uniform_handle("u_bone_count"),
                    static_cast<GLuint>(bone_count));
  cull->set_uniform(cull->uniform_handle("u_instance_base"),
                    static_cast<GLuint>(m_instance_base));
  cull->set_uniform(cull->uniform_handle("u_triangle_count"),
                    static_cast<GLuint>(triangle_count));
  cull->set_uniform(cull->uniform_handle("u_instance_count"),
                    static_cast<GLuint>(count));
  cull->set_uniform(cull->uniform_handle("u_out_capacity_triangles"),
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
  finalize->set_uniform(finalize->uniform_handle("u_out_capacity_triangles"),
                        static_cast<GLuint>(m_out_capacity_triangles));
  finalize->set_uniform(finalize->uniform_handle("u_reset"), 0U);
  glDispatchCompute(1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
                  GL_ELEMENT_ARRAY_BARRIER_BIT);

  Shader* draw_shader = depth_only ? m_shadow_shader : m_draw_shader;
  if (draw_shader == nullptr) {
    return false;
  }
  if (!depth_only && !bind_role_color_texture()) {
    return false;
  }

  draw_shader->use();
  draw_shader->set_uniform(draw_shader->uniform_handle("u_view_proj"), view_proj);
  draw_shader->set_uniform(draw_shader->uniform_handle("u_vertex_count"),
                           static_cast<GLuint>(mesh->vertex_count()));
  draw_shader->set_uniform(draw_shader->uniform_handle("u_bone_count"),
                           static_cast<GLuint>(bone_count));
  draw_shader->set_uniform(draw_shader->uniform_handle("u_instance_base"),
                           static_cast<GLuint>(m_instance_base));
  if (!depth_only) {
    draw_shader->set_uniform(draw_shader->uniform_handle("u_camera_position"),
                             camera_position);
    auto const role_tbo = draw_shader->optional_uniform_handle("u_role_color_tbo");
    if (role_tbo != Shader::InvalidUniform) {
      draw_shader->set_uniform(role_tbo, 0);
    }
    const auto role_base = draw_shader->optional_uniform_handle("u_role_color_base");
    if (role_base != Shader::InvalidUniform) {
      draw_shader->set_uniform(role_base, 0);
    }
  }

  glBindVertexArray(m_vao);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_command_buffer);
  glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
  glBindVertexArray(0);

  m_stats.dispatched_instances = static_cast<std::uint32_t>(count);
  m_stats.candidate_triangles = static_cast<std::uint32_t>(candidate_triangles);

#ifndef NDEBUG
  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedCullPipeline: GL error" << err;
    return false;
  }
#endif
  return true;
}

} // namespace Render::GL::BackendPipelines

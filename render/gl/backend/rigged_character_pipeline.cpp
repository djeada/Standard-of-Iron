#include "rigged_character_pipeline.h"

#include "../../bone_palette_arena.h"
#include "../../draw_queue.h"
#include "../../rigged_mesh.h"
#include "../backend.h"
#include "../buffer.h"
#include "../shader_cache.h"
#include "../texture.h"
#include "../ubo_bindings.h"
#include "utils/resource_utils.h"

#include <GL/gl.h>
#include <QDebug>
#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QTextStream>
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace Render::GL::BackendPipelines {

namespace {

auto gl_funcs() -> QOpenGLFunctions_3_3_Core * {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

auto load_shader_source(const QString &resource_path) -> QString {
  QString const resolved = Utils::Resources::resolveResourcePath(resource_path);
  QFile file(resolved);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "RiggedCharacterPipeline: Failed to open shader" << resolved;
    return {};
  }
  QTextStream stream(&file);
  return stream.readAll();
}

auto query_max_uniform_block_size() -> std::size_t {
  auto *fn = gl_funcs();
  if (fn == nullptr) {
    return 0;
  }
  GLint max_block = 0;
  fn->glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_block);
  return (max_block > 0) ? static_cast<std::size_t>(max_block) : 0;
}

void copy_palette_to_scratch(const QMatrix4x4 *palette,
                             std::vector<float> &scratch) {
  scratch.resize(BonePaletteArena::kPaletteFloats);
  BonePaletteArena::pack_palette_for_gpu(palette, scratch.data());
}

} // namespace

auto RiggedCharacterPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "RiggedCharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_shader = m_shader_cache->get(QStringLiteral("character_skinned"));
  if (m_shader == nullptr) {
    qWarning()
        << "RiggedCharacterPipeline: Failed to load character_skinned shader";
    return false;
  }

  cache_uniforms();

  m_shader->bind_uniform_block("BonePalette", kBonePaletteBindingPoint);

  std::size_t const max_block_bytes = query_max_uniform_block_size();
  std::size_t cap = k_instanced_batch_floor;
  if (max_block_bytes > 0) {
    cap = max_block_bytes / BonePaletteArena::kPaletteBytes;
  }
  if (cap < k_instanced_batch_floor) {
    cap = k_instanced_batch_floor;
  }
  if (cap > k_instanced_batch_ceiling) {
    cap = k_instanced_batch_ceiling;
  }
  m_max_instances_per_batch = cap;

  if (!build_instanced_shader_source()) {
    qWarning() << "RiggedCharacterPipeline: instanced shader unavailable; "
                  "falling back to per-cmd draws";
  }

  return is_initialized();
}

auto RiggedCharacterPipeline::build_instanced_shader_source() -> bool {

  QString vert_src = load_shader_source(
      QStringLiteral(":/assets/shaders/character_skinned_instanced.vert"));
  QString frag_src = load_shader_source(
      QStringLiteral(":/assets/shaders/character_skinned_instanced.frag"));
  if (vert_src.isEmpty() || frag_src.isEmpty()) {
    return false;
  }

  QString const define_line =
      QStringLiteral("#define INSTANCED_BATCH_SIZE %1\n")
          .arg(static_cast<qulonglong>(m_max_instances_per_batch));
  int newline = vert_src.indexOf('\n');
  if (newline < 0) {
    newline = 0;
  } else {
    newline += 1;
  }
  vert_src.insert(newline, define_line);

  m_instanced_shader_storage = std::make_unique<Shader>();
  m_instanced_shader_storage->set_debug_name(
      QStringLiteral("character_skinned_instanced"));
  if (!m_instanced_shader_storage->load_from_source(vert_src, frag_src)) {
    qWarning() << "RiggedCharacterPipeline: failed to compile instanced shader";
    m_instanced_shader_storage.reset();
    return false;
  }
  m_instanced_shader = m_instanced_shader_storage.get();
  m_instanced_shader->bind_uniform_block("BonePalette",
                                         kBonePaletteBindingPoint);
  m_instanced_view_proj = m_instanced_shader->uniform_handle("u_view_proj");
  return true;
}

void RiggedCharacterPipeline::shutdown() {
  m_shader = nullptr;
  m_instanced_shader = nullptr;
  m_instanced_shader_storage.reset();
  m_uniforms = Uniforms{};

  auto *fn = gl_funcs();
  if (fn != nullptr) {
    if (m_instance_vbo != 0) {
      fn->glDeleteBuffers(1, &m_instance_vbo);
    }
    if (m_palette_ubo != 0) {
      fn->glDeleteBuffers(1, &m_palette_ubo);
    }
    for (auto &kv : m_instanced_vaos) {
      if (kv.second.vao != 0) {
        fn->glDeleteVertexArrays(1, &kv.second.vao);
      }
    }
  }
  m_instance_vbo = 0;
  m_instance_vbo_capacity_bytes = 0;
  m_palette_ubo = 0;
  m_palette_ubo_capacity_bytes = 0;
  m_instanced_vaos.clear();
}

void RiggedCharacterPipeline::cache_uniforms() {
  if (m_shader == nullptr) {
    return;
  }

  m_uniforms.view_proj = m_shader->uniform_handle("u_view_proj");
  m_uniforms.model = m_shader->uniform_handle("u_model");
  m_uniforms.variation_scale =
      m_shader->optional_uniform_handle("u_variation_scale");
  m_uniforms.color = m_shader->uniform_handle("u_color");
  m_uniforms.alpha = m_shader->uniform_handle("u_alpha");
  m_uniforms.use_texture = m_shader->optional_uniform_handle("u_useTexture");
  m_uniforms.texture = m_shader->optional_uniform_handle("u_texture");
  m_uniforms.material_id = m_shader->optional_uniform_handle("u_materialId");
}

auto RiggedCharacterPipeline::is_initialized() const -> bool {
  return m_shader != nullptr &&
         m_uniforms.view_proj != GL::Shader::InvalidUniform &&
         m_uniforms.model != GL::Shader::InvalidUniform;
}

auto RiggedCharacterPipeline::draw(const RiggedCreatureCmd &cmd,
                                   const QMatrix4x4 &view_proj) -> bool {
  if (!is_initialized() || cmd.mesh == nullptr) {
    return false;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.model, cmd.world);
  if (m_uniforms.variation_scale != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.variation_scale, cmd.variation_scale);
  }
  m_shader->set_uniform(m_uniforms.color, cmd.color);
  m_shader->set_uniform(m_uniforms.alpha, cmd.alpha);
  if (m_uniforms.material_id != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.material_id,
                          static_cast<int>(cmd.material_id));
  }

  const bool has_texture = (cmd.texture != nullptr) &&
                           m_uniforms.texture != GL::Shader::InvalidUniform;
  if (m_uniforms.use_texture != GL::Shader::InvalidUniform) {
    m_shader->set_uniform(m_uniforms.use_texture, has_texture);
  }
  if (has_texture) {
    cmd.texture->bind(0);
    m_shader->set_uniform(m_uniforms.texture, 0);
  }

  auto *fn = gl_funcs();
  if (fn != nullptr) {
    if (cmd.palette_ubo != 0) {
      fn->glBindBufferRange(
          GL_UNIFORM_BUFFER, kBonePaletteBindingPoint,
          static_cast<GLuint>(cmd.palette_ubo),
          static_cast<GLintptr>(cmd.palette_offset),
          static_cast<GLsizeiptr>(BonePaletteArena::kPaletteBytes));
    } else {
      if (m_palette_ubo == 0) {
        fn->glGenBuffers(1, &m_palette_ubo);
      }
      if (m_palette_ubo != 0) {
        if (BonePaletteArena::kPaletteBytes > m_palette_ubo_capacity_bytes) {
          fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);
          fn->glBufferData(
              GL_UNIFORM_BUFFER,
              static_cast<GLsizeiptr>(BonePaletteArena::kPaletteBytes), nullptr,
              GL_DYNAMIC_DRAW);
          m_palette_ubo_capacity_bytes = BonePaletteArena::kPaletteBytes;
        }
        copy_palette_to_scratch(cmd.bone_palette, m_palette_scratch);
        fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);
        fn->glBufferSubData(
            GL_UNIFORM_BUFFER, 0,
            static_cast<GLsizeiptr>(BonePaletteArena::kPaletteBytes),
            m_palette_scratch.data());
        fn->glBindBufferRange(
            GL_UNIFORM_BUFFER, kBonePaletteBindingPoint, m_palette_ubo, 0,
            static_cast<GLsizeiptr>(BonePaletteArena::kPaletteBytes));
      }
    }
  }

  cmd.mesh->draw();
  m_batch_sizes.push_back(1);
  m_last_instance_count = 1;
  return true;
}

void RiggedCharacterPipeline::compute_groups(
    const RiggedCreatureCmd *cmds, std::size_t count, std::size_t cap,
    std::vector<std::size_t> &out_groups) {
  out_groups.clear();
  if (cap == 0) {
    cap = 1;
  }
  std::size_t i = 0;
  while (i < count) {
    const auto &head = cmds[i];
    if (head.texture != nullptr || head.mesh == nullptr) {

      out_groups.push_back(1);
      ++i;
      continue;
    }
    std::size_t j = i + 1;
    while (j < count && (j - i) < cap) {
      const auto &nxt = cmds[j];
      if (nxt.mesh != head.mesh || nxt.material != head.material ||
          nxt.texture != nullptr) {
        break;
      }
      ++j;
    }
    out_groups.push_back(j - i);
    i = j;
  }
}

auto RiggedCharacterPipeline::ensure_instance_vbo(std::size_t bytes_needed)
    -> bool {
  auto *fn = gl_funcs();
  if (fn == nullptr) {
    return false;
  }
  if (m_instance_vbo == 0) {
    fn->glGenBuffers(1, &m_instance_vbo);
  }
  if (bytes_needed > m_instance_vbo_capacity_bytes) {

    std::size_t cap = ((bytes_needed + 4095) / 4096) * 4096;
    fn->glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    fn->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cap), nullptr,
                     GL_DYNAMIC_DRAW);
    m_instance_vbo_capacity_bytes = cap;
  }
  return true;
}

auto RiggedCharacterPipeline::ensure_instanced_vao(RiggedMesh &mesh)
    -> unsigned int {
  auto it = m_instanced_vaos.find(static_cast<void *>(&mesh));
  if (it != m_instanced_vaos.end() && it->second.vao != 0) {
    return it->second.vao;
  }

  auto *fn = gl_funcs();
  if (fn == nullptr) {
    return 0;
  }
  if (!mesh.ensure_gl_buffers()) {
    return 0;
  }
  Buffer *vbo = mesh.vertex_buffer();
  Buffer *ebo = mesh.index_buffer();
  if (vbo == nullptr || ebo == nullptr || m_instance_vbo == 0) {
    return 0;
  }

  GLuint vao = 0;
  fn->glGenVertexArrays(1, &vao);
  fn->glBindVertexArray(vao);

  vbo->bind();
  constexpr GLsizei k_stride = sizeof(RiggedVertex);
  constexpr auto offset_of = [](auto member_ptr) -> std::size_t {
    return reinterpret_cast<std::size_t>(
        &(reinterpret_cast<RiggedVertex const *>(0)->*member_ptr));
  };
  auto const pos_off = offset_of(&RiggedVertex::position_bone_local);
  auto const norm_off = offset_of(&RiggedVertex::normal_bone_local);
  auto const tex_off = offset_of(&RiggedVertex::tex_coord);
  auto const bi_off = offset_of(&RiggedVertex::bone_indices);
  auto const bw_off = offset_of(&RiggedVertex::bone_weights);

  fn->glEnableVertexAttribArray(0);
  fn->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, k_stride,
                            reinterpret_cast<void *>(pos_off));
  fn->glEnableVertexAttribArray(1);
  fn->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, k_stride,
                            reinterpret_cast<void *>(norm_off));
  fn->glEnableVertexAttribArray(2);
  fn->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, k_stride,
                            reinterpret_cast<void *>(tex_off));
  fn->glEnableVertexAttribArray(3);
  fn->glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, k_stride,
                             reinterpret_cast<void *>(bi_off));
  fn->glEnableVertexAttribArray(4);
  fn->glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, k_stride,
                            reinterpret_cast<void *>(bw_off));

  fn->glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
  constexpr GLsizei k_inst_stride = sizeof(InstanceAttrib);
  std::size_t off = 0;
  for (int col = 0; col < 4; ++col) {
    GLuint loc = static_cast<GLuint>(5 + col);
    fn->glEnableVertexAttribArray(loc);
    fn->glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, k_inst_stride,
                              reinterpret_cast<void *>(off));
    fn->glVertexAttribDivisor(loc, 1);
    off += sizeof(float) * 4;
  }
  fn->glEnableVertexAttribArray(9);
  fn->glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, k_inst_stride,
                            reinterpret_cast<void *>(off));
  fn->glVertexAttribDivisor(9, 1);
  off += sizeof(float) * 4;
  fn->glEnableVertexAttribArray(10);
  fn->glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, k_inst_stride,
                            reinterpret_cast<void *>(off));
  fn->glVertexAttribDivisor(10, 1);

  ebo->bind();

  fn->glBindVertexArray(0);
  fn->glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_instanced_vaos[static_cast<void *>(&mesh)] = InstancedVaoEntry{vao};
  return vao;
}

auto RiggedCharacterPipeline::draw_instanced(
    const RiggedCreatureCmd *cmds, std::size_t count,
    const QMatrix4x4 &view_proj) -> bool {
  if (cmds == nullptr || count == 0) {
    return false;
  }
  if (count > m_max_instances_per_batch || m_instanced_shader == nullptr) {
    return false;
  }
  if (cmds[0].mesh == nullptr || cmds[0].texture != nullptr) {
    return false;
  }

  auto *fn = gl_funcs();
  if (fn == nullptr) {
    m_batch_sizes.push_back(count);
    m_last_instance_count = count;
    return true;
  }

  m_instance_scratch.resize(count);
  for (std::size_t k = 0; k < count; ++k) {
    const auto &c = cmds[k];
    InstanceAttrib &ia = m_instance_scratch[k];
    const float *src = c.world.constData();
    std::memcpy(ia.world, src, sizeof(float) * 16);
    ia.color_alpha[0] = c.color.x();
    ia.color_alpha[1] = c.color.y();
    ia.color_alpha[2] = c.color.z();
    ia.color_alpha[3] = c.alpha;
    ia.variation_material[0] = c.variation_scale.x();
    ia.variation_material[1] = c.variation_scale.y();
    ia.variation_material[2] = c.variation_scale.z();
    ia.variation_material[3] = static_cast<float>(c.material_id);
  }

  std::size_t const bytes = count * sizeof(InstanceAttrib);
  if (!ensure_instance_vbo(bytes)) {
    return false;
  }
  fn->glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);

  fn->glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(m_instance_vbo_capacity_bytes),
                   nullptr, GL_DYNAMIC_DRAW);
  fn->glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes),
                      m_instance_scratch.data());

  GLuint const vao = ensure_instanced_vao(*cmds[0].mesh);
  if (vao == 0) {
    return false;
  }

  m_instanced_shader->use();
  if (m_instanced_view_proj != Shader::InvalidUniform) {
    m_instanced_shader->set_uniform(m_instanced_view_proj, view_proj);
  }

  std::size_t const upload_palette_bytes =
      count * BonePaletteArena::kPaletteBytes;
  std::size_t const bound_palette_bytes =
      palette_range_bytes_for_instanced_shader(m_max_instances_per_batch);
  if (m_palette_ubo == 0) {
    fn->glGenBuffers(1, &m_palette_ubo);
  }
  if (bound_palette_bytes > m_palette_ubo_capacity_bytes) {
    fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);
    fn->glBufferData(GL_UNIFORM_BUFFER,
                     static_cast<GLsizeiptr>(bound_palette_bytes), nullptr,
                     GL_DYNAMIC_DRAW);
    m_palette_ubo_capacity_bytes = bound_palette_bytes;
  }

  std::size_t const floats_per_palette = BonePaletteArena::kPaletteFloats;
  m_palette_scratch.resize(count * floats_per_palette);
  for (std::size_t k = 0; k < count; ++k) {
    const auto &c = cmds[k];
    float *dst = m_palette_scratch.data() + k * floats_per_palette;
    BonePaletteArena::pack_palette_for_gpu(c.bone_palette, dst);
  }
  fn->glBindBuffer(GL_UNIFORM_BUFFER, m_palette_ubo);

  fn->glBufferData(GL_UNIFORM_BUFFER,
                   static_cast<GLsizeiptr>(m_palette_ubo_capacity_bytes),
                   nullptr, GL_DYNAMIC_DRAW);
  fn->glBufferSubData(GL_UNIFORM_BUFFER, 0,
                      static_cast<GLsizeiptr>(upload_palette_bytes),
                      m_palette_scratch.data());
  fn->glBindBufferRange(GL_UNIFORM_BUFFER, kBonePaletteBindingPoint,
                        m_palette_ubo, 0,
                        static_cast<GLsizeiptr>(bound_palette_bytes));

  fn->glBindVertexArray(vao);
  GLsizei const idx_count = static_cast<GLsizei>(cmds[0].mesh->index_count());
  fn->glDrawElementsInstanced(GL_TRIANGLES, idx_count, GL_UNSIGNED_INT, nullptr,
                              static_cast<GLsizei>(count));
  fn->glBindVertexArray(0);

  GLenum err = fn->glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedCharacterPipeline::draw_instanced GL error" << err
               << "count" << count;
    return false;
  }

  m_batch_sizes.push_back(count);
  m_last_instance_count = count;
  return true;
}

} // namespace Render::GL::BackendPipelines

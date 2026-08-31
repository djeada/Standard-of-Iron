#include "shader.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <qdebug.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qopenglext.h>
#include <qstringview.h>
#include <qvector2d.h>
#include <qvector3d.h>
#include <qvector4d.h>

#include <algorithm>
#include <mutex>
#include <type_traits>
#include <vector>

#include "platform_gl.h"
#include "render/profiling/asset_counters.h"
#include "render_constants.h"
#include "ubo_bindings.h"
#include "utils/resource_utils.h"

namespace Render::GL {

using namespace Render::GL::BufferCapacity;

template <typename T>
auto Shader::is_uniform_dirty(GLint location, const T& value) -> bool {
  auto it = m_uniform_value_cache.find(location);
  if (it == m_uniform_value_cache.end()) {
    m_uniform_value_cache[location] = value;
    return true;
  }
  auto* cached = std::get_if<T>(&it->second);
  if (cached == nullptr || !(*cached == value)) {
    it->second = value;
    return true;
  }
  return false;
}

namespace {

auto resolve_shader_includes(const QString& source,
                             const QString& base_dir,
                             QSet<QString>& already_included) -> QString {
  QString result;
  result.reserve(source.size());

  const QStringList lines = source.split('\n');
  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith("#include")) {

      int start = trimmed.indexOf('"');
      int end = -1;
      if (start >= 0) {
        end = trimmed.indexOf('"', start + 1);
      } else {
        start = trimmed.indexOf('<');
        if (start >= 0) {
          end = trimmed.indexOf('>', start + 1);
        }
      }

      if (start >= 0 && end > start) {
        const QString include_name = trimmed.mid(start + 1, end - start - 1);

        const QString include_path =
            QStringLiteral(":/assets/shaders/include/") + include_name;
        if (already_included.contains(include_name)) {
          continue;
        }
        const QString resolved = Utils::Resources::resolve_resource_path(include_path);
        QFile include_file(resolved);
        if (include_file.open(QIODevice::ReadOnly)) {
          QTextStream stream(&include_file);
          const QString included_source = stream.readAll();

          already_included.insert(include_name);
          result +=
              resolve_shader_includes(included_source, base_dir, already_included);
          result += '\n';
          continue;
        }
        qWarning() << "Shader #include not found:" << include_path;
      }
    }
    result += line;
    result += '\n';
  }
  return result;
}

auto resolve_shader_includes(const QString& source,
                             const QString& base_dir) -> QString {
  QSet<QString> already_included;
  return resolve_shader_includes(source, base_dir, already_included);
}

auto shader_registry_mutex() -> std::mutex& {
  static std::mutex mutex;
  return mutex;
}

auto live_shaders() -> std::vector<Shader*>& {
  static std::vector<Shader*> shaders;
  return shaders;
}

auto global_defines_storage() -> QString& {
  static QString defines;
  return defines;
}

auto inject_defines(const QString& source, const QString& extra) -> QString {
  QString defines;
  {
    std::lock_guard const lock(shader_registry_mutex());
    defines = global_defines_storage();
  }
  if (!extra.isEmpty()) {
    if (!defines.isEmpty() && !defines.endsWith('\n')) {
      defines += '\n';
    }
    defines += extra;
  }
  if (defines.isEmpty()) {
    return source;
  }
  if (!defines.endsWith('\n')) {
    defines += '\n';
  }
  const int version_at = source.indexOf(QStringLiteral("#version"));
  if (version_at < 0) {
    return defines + source;
  }
  int line_end = source.indexOf('\n', version_at);
  if (line_end < 0) {
    return source + '\n' + defines;
  }
  ++line_end;
  return source.left(line_end) + defines + source.mid(line_end);
}

auto read_text_file(const QString& path, QString& out) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  QTextStream stream(&file);
  out = stream.readAll();
  return true;
}
} // namespace

Shader::Shader() {
  std::lock_guard const lock(shader_registry_mutex());
  live_shaders().push_back(this);
}

Shader::~Shader() {
  {
    std::lock_guard const lock(shader_registry_mutex());
    auto& shaders = live_shaders();
    shaders.erase(std::remove(shaders.begin(), shaders.end(), this), shaders.end());
  }
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

void Shader::set_global_defines(const QString& defines) {
  std::lock_guard const lock(shader_registry_mutex());
  global_defines_storage() = defines;
}

auto Shader::global_defines() -> QString {
  std::lock_guard const lock(shader_registry_mutex());
  return global_defines_storage();
}

auto Shader::reload_all() -> std::size_t {
  std::vector<Shader*> snapshot;
  {
    std::lock_guard const lock(shader_registry_mutex());
    snapshot = live_shaders();
  }
  std::size_t reloaded = 0;
  for (Shader* shader : snapshot) {
    if (shader->m_source_kind != SourceKind::None && shader->reload()) {
      ++reloaded;
    }
  }
  return reloaded;
}

auto Shader::reload() -> bool {
  switch (m_source_kind) {
  case SourceKind::Files: {
    QString vertex_source;
    QString fragment_source;
    if (!read_text_file(m_vertex_path, vertex_source) ||
        !read_text_file(m_fragment_path, fragment_source)) {
      qWarning() << "Shader reload: cannot re-read" << m_debug_name;
      return false;
    }
    const GLuint program = build_graphics_program(
        resolve_shader_includes(vertex_source, QFileInfo(m_vertex_path).path()),
        resolve_shader_includes(fragment_source, QFileInfo(m_fragment_path).path()));
    if (program == 0) {
      qWarning() << "Shader reload: keeping the previous program for" << m_debug_name;
      return false;
    }
    adopt_program(program);
    return true;
  }
  case SourceKind::Sources: {
    const GLuint program = build_graphics_program(m_vertex_source, m_fragment_source);
    if (program == 0) {
      qWarning() << "Shader reload: keeping the previous program for" << m_debug_name;
      return false;
    }
    adopt_program(program);
    return true;
  }
  case SourceKind::Compute: {
    initializeOpenGLFunctions();
    constexpr GLenum k_compute_shader = 0x91B9;
    const GLuint compute_shader = compile_shader(m_compute_source, k_compute_shader);
    if (compute_shader == 0) {
      return false;
    }
    const GLuint program = link_compute_program(compute_shader);
    glDeleteShader(compute_shader);
    if (program == 0) {
      return false;
    }
    adopt_program(program);
    return true;
  }
  case SourceKind::None:
    break;
  }
  return false;
}

auto Shader::load_from_files(const QString& vertex_path,
                             const QString& fragment_path,
                             const QString& variant_defines) -> bool {
  m_variant_defines = variant_defines;
  const QString resolved_vert = Utils::Resources::resolve_resource_path(vertex_path);
  const QString resolved_frag = Utils::Resources::resolve_resource_path(fragment_path);

  QFile vertex_file(resolved_vert);
  QFile fragment_file(resolved_frag);

  if (!vertex_file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open vertex shader file:" << resolved_vert;
    if (resolved_vert != vertex_path) {
      qWarning() << "  Requested path:" << vertex_path;
    }
    return false;
  }

  if (!fragment_file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open fragment shader file:" << resolved_frag;
    if (resolved_frag != fragment_path) {
      qWarning() << "  Requested path:" << fragment_path;
    }
    vertex_file.close();
    return false;
  }

  QTextStream vertex_stream(&vertex_file);
  QTextStream fragment_stream(&fragment_file);

  QString const vertex_source = vertex_stream.readAll();
  QString const fragment_source = fragment_stream.readAll();

  const QString processed_vert =
      resolve_shader_includes(vertex_source, QFileInfo(resolved_vert).path());
  const QString processed_frag =
      resolve_shader_includes(fragment_source, QFileInfo(resolved_frag).path());

  if (!load_from_source(processed_vert, processed_frag)) {
    return false;
  }

  m_source_kind = SourceKind::Files;
  m_vertex_path = resolved_vert;
  m_fragment_path = resolved_frag;
  m_vertex_source.clear();
  m_fragment_source.clear();
  return true;
}

auto Shader::build_graphics_program(const QString& vertex_source,
                                    const QString& fragment_source) -> GLuint {
  initializeOpenGLFunctions();
  GLuint const vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
  GLuint const fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
  if (vertex_shader == 0 || fragment_shader == 0) {
    if (vertex_shader != 0) {
      glDeleteShader(vertex_shader);
    }
    if (fragment_shader != 0) {
      glDeleteShader(fragment_shader);
    }
    return 0;
  }
  const GLuint program = link_program(vertex_shader, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return program;
}

auto Shader::load_from_source(const QString& vertex_source,
                              const QString& fragment_source) -> bool {
  const GLuint program = build_graphics_program(vertex_source, fragment_source);
  if (program == 0) {
    return false;
  }

  m_uniform_cache.clear();
  m_uniform_names.clear();
  m_uniform_locations.clear();
  m_uniform_value_cache.clear();
  m_block_bindings.clear();
  adopt_program(program);
  m_source_kind = SourceKind::Sources;
  m_vertex_source = vertex_source;
  m_fragment_source = fragment_source;
  m_vertex_path.clear();
  m_fragment_path.clear();
  return true;
}

auto Shader::load_compute_from_source(const QString& compute_source) -> bool {
  initializeOpenGLFunctions();
  constexpr GLenum k_compute_shader = 0x91B9;
  GLuint const compute_shader = compile_shader(compute_source, k_compute_shader);
  if (compute_shader == 0) {
    return false;
  }
  const GLuint program = link_compute_program(compute_shader);
  glDeleteShader(compute_shader);
  if (program == 0) {
    return false;
  }
  m_uniform_cache.clear();
  m_uniform_names.clear();
  m_uniform_locations.clear();
  m_uniform_value_cache.clear();
  m_block_bindings.clear();
  adopt_program(program);
  m_source_kind = SourceKind::Compute;
  m_compute_source = compute_source;
  return true;
}

auto Shader::link_compute_program(GLuint compute_shader) -> GLuint {
  initializeOpenGLFunctions();
  const GLuint program = glCreateProgram();
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::GlProgramCreated);
  glAttachShader(program, compute_shader);
  glLinkProgram(program);
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::ProgramLinked);

  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    GLchar info_log[shader_info_log_size];
    glGetProgramInfoLog(program, shader_info_log_size, nullptr, info_log);
    qWarning() << "Shader: compute link failed" << m_debug_name << info_log;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

void Shader::adopt_program(GLuint program) {
  initializeOpenGLFunctions();
  if (m_program != 0 && m_program != program) {
    glDeleteProgram(m_program);
  }
  m_program = program;

  for (std::size_t i = 0; i < m_uniform_names.size(); ++i) {
    m_uniform_locations[i] =
        glGetUniformLocation(m_program, m_uniform_names[i].c_str());
  }
  bind_standard_blocks();
  for (const auto& [block, binding] : m_block_bindings) {
    const GLuint idx = glGetUniformBlockIndex(m_program, block.c_str());
    if (idx != GL_INVALID_INDEX) {
      glUniformBlockBinding(m_program, idx, static_cast<GLuint>(binding));
    }
  }
  replay_uniform_state();
}

void Shader::bind_standard_blocks() {
  const auto bind = [this](const char* block, std::uint32_t binding) {
    const GLuint idx = glGetUniformBlockIndex(m_program, block);
    if (idx != GL_INVALID_INDEX) {
      glUniformBlockBinding(m_program, idx, static_cast<GLuint>(binding));
    }
  };
  bind("FrameData", k_frame_data_binding_point);
  bind("EnvironmentLighting", k_environment_lighting_binding_point);
  bind("LocalLighting", k_local_lighting_binding_point);
  bind("DirectionalShadows", k_directional_shadow_binding_point);
}

void Shader::replay_uniform_state() {
  GLint previous_program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
  glUseProgram(m_program);

  const UniformHandle shadow_sampler =
      optional_uniform_handle("u_directional_shadow_map");
  if (shadow_sampler != InvalidUniform) {
    m_uniform_value_cache[shadow_sampler] =
        static_cast<int>(TextureUnit::directional_shadow_map);
  }
  const UniformHandle far_shadow_sampler =
      optional_uniform_handle("u_directional_shadow_map_far");
  if (far_shadow_sampler != InvalidUniform) {
    m_uniform_value_cache[far_shadow_sampler] =
        static_cast<int>(TextureUnit::directional_shadow_map_far);
  }
  const UniformHandle shadow_depth =
      optional_uniform_handle("u_directional_shadow_depth");
  if (shadow_depth != InvalidUniform) {
    m_uniform_value_cache[shadow_depth] =
        static_cast<int>(TextureUnit::directional_shadow_depth);
  }

  for (const auto& [handle, value] : m_uniform_value_cache) {
    const GLint location = location_for(handle);
    if (location < 0) {
      continue;
    }
    std::visit(
        [&](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, float>) {
            glUniform1f(location, v);
          } else if constexpr (std::is_same_v<T, int>) {
            glUniform1i(location, v);
          } else if constexpr (std::is_same_v<T, unsigned int>) {
            glUniform1ui(location, v);
          } else if constexpr (std::is_same_v<T, QVector2D>) {
            glUniform2f(location, v.x(), v.y());
          } else if constexpr (std::is_same_v<T, QVector3D>) {
            glUniform3f(location, v.x(), v.y(), v.z());
          } else if constexpr (std::is_same_v<T, QVector4D>) {
            glUniform4f(location, v.x(), v.y(), v.z(), v.w());
          } else if constexpr (std::is_same_v<T, QMatrix4x4>) {
            glUniformMatrix4fv(location, 1, GL_FALSE, v.constData());
          }
        },
        value);
  }
  glUseProgram(static_cast<GLuint>(previous_program));
}

auto Shader::location_for(UniformHandle handle) const noexcept -> GLint {
  if (handle < 0 || static_cast<std::size_t>(handle) >= m_uniform_locations.size()) {
    return -1;
  }
  return m_uniform_locations[static_cast<std::size_t>(handle)];
}

auto Shader::preprocess_source(const QString& source) -> QString {
  return resolve_shader_includes(source, QString());
}

void Shader::use() {
  glUseProgram(m_program);
}

void Shader::release() {
  glUseProgram(0);
}

namespace {

auto uniform_handle_impl(QOpenGLFunctions_3_3_Core& fn,
                         GLuint program,
                         std::unordered_map<std::string, Shader::UniformHandle>& cache,
                         std::vector<std::string>& names,
                         std::vector<GLint>& locations,
                         const char* name,
                         bool warn) -> Shader::UniformHandle {
  if ((name == nullptr) || *name == '\0' || program == 0) {
    return Shader::InvalidUniform;
  }

  auto it = cache.find(name);
  if (it != cache.end()) {
    return it->second;
  }

  fn.initializeOpenGLFunctions();
  const GLint location = fn.glGetUniformLocation(program, name);

  if (warn && location < 0) {
    qWarning() << "Shader uniform not found:" << name << "(program:" << program << ")";
  }

  Shader::UniformHandle handle = Shader::InvalidUniform;
  if (location >= 0) {
    handle = static_cast<Shader::UniformHandle>(names.size());
    names.emplace_back(name);
    locations.push_back(location);
  }
  cache.emplace(name, handle);
  return handle;
}
} // namespace

auto Shader::uniform_handle(const char* name) -> Shader::UniformHandle {
  return uniform_handle_impl(*this,
                             m_program,
                             m_uniform_cache,
                             m_uniform_names,
                             m_uniform_locations,
                             name,
                             true);
}

auto Shader::optional_uniform_handle(const char* name) -> Shader::UniformHandle {
  return uniform_handle_impl(*this,
                             m_program,
                             m_uniform_cache,
                             m_uniform_names,
                             m_uniform_locations,
                             name,
                             false);
}

void Shader::set_uniform(UniformHandle handle, float value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform1f(location, value);
}

void Shader::set_uniform(UniformHandle handle, const QVector3D& value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform3f(location, value.x(), value.y(), value.z());
}

void Shader::set_uniform(UniformHandle handle, const QVector4D& value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform4f(location, value.x(), value.y(), value.z(), value.w());
}

void Shader::set_uniform(UniformHandle handle, const QVector2D& value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform2f(location, value.x(), value.y());
}

void Shader::set_uniform(UniformHandle handle, const QMatrix4x4& value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
}

void Shader::set_uniform(UniformHandle handle, int value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform1i(location, value);
}

void Shader::set_uniform(UniformHandle handle, unsigned int value) {
  const GLint location = location_for(handle);
  if (location < 0) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform1ui(location, value);
}

void Shader::set_uniform(UniformHandle handle, bool value) {
  set_uniform(handle, static_cast<int>(value));
}

void Shader::set_uniform_vec3_array(UniformHandle handle,
                                    const float* data,
                                    int count) {
  const GLint location = location_for(handle);
  if (location < 0 || data == nullptr || count <= 0) {
    return;
  }
  glUniform3fv(location, count, data);
}

void Shader::set_uniform(const char* name, float value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, const QVector3D& value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, const QVector4D& value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, const QVector2D& value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, const QMatrix4x4& value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, int value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const char* name, bool value) {
  set_uniform(uniform_handle(name), value);
}

void Shader::set_uniform(const QString& name, float value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, const QVector3D& value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, const QVector4D& value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, const QVector2D& value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, const QMatrix4x4& value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, int value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString& name, bool value) {
  set_uniform(name, static_cast<int>(value));
}

auto Shader::compile_shader(const QString& source, GLenum type) -> GLuint {
  initializeOpenGLFunctions();
  GLuint const shader = glCreateShader(type);

  QByteArray const source_bytes = inject_defines(source, m_variant_defines).toUtf8();
  const char* source_ptr = source_bytes.constData();
  glShaderSource(shader, 1, &source_ptr, nullptr);
  glCompileShader(shader);
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::ShaderCompiled);

  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    GLchar info_log[shader_info_log_size];
    glGetShaderInfoLog(shader, shader_info_log_size, nullptr, info_log);
    qWarning() << "Shader compilation failed:" << info_log;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

auto Shader::link_program(GLuint vertex_shader, GLuint fragment_shader) -> GLuint {
  initializeOpenGLFunctions();
  const GLuint program = glCreateProgram();
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::GlProgramCreated);
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::ProgramLinked);

  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == 0) {
    GLchar info_log[shader_info_log_size];
    glGetProgramInfoLog(program, shader_info_log_size, nullptr, info_log);
    qWarning() << "Shader linking failed:" << m_debug_name << info_log;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

auto Shader::bind_uniform_block(const char* block_name,
                                std::uint32_t binding_point) -> bool {
  if (m_program == 0 || block_name == nullptr) {
    return false;
  }
  initializeOpenGLFunctions();
  GLuint const idx = glGetUniformBlockIndex(m_program, block_name);
  if (idx == GL_INVALID_INDEX) {
    qWarning() << "Shader uniform block not found:" << block_name
               << "(program:" << m_program << ")";
    return false;
  }
  glUniformBlockBinding(m_program, idx, binding_point);
  m_block_bindings.emplace_back(block_name, binding_point);
  return true;
}

auto Shader::optional_bind_uniform_block(const char* block_name,
                                         std::uint32_t binding_point) -> bool {
  if (m_program == 0 || block_name == nullptr) {
    return false;
  }
  initializeOpenGLFunctions();
  const GLuint idx = glGetUniformBlockIndex(m_program, block_name);
  if (idx == GL_INVALID_INDEX) {
    return false;
  }
  glUniformBlockBinding(m_program, idx, binding_point);
  m_block_bindings.emplace_back(block_name, binding_point);
  return true;
}

} // namespace Render::GL

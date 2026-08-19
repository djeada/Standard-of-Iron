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

#include "platform_gl.h"
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
} // namespace

Shader::Shader() = default;

Shader::~Shader() {
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

auto Shader::load_from_files(const QString& vertex_path,
                             const QString& fragment_path) -> bool {
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

  return load_from_source(processed_vert, processed_frag);
}

auto Shader::load_from_source(const QString& vertex_source,
                              const QString& fragment_source) -> bool {
  initializeOpenGLFunctions();
  m_uniform_cache.clear();
  m_uniform_value_cache.clear();
  GLuint const vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
  GLuint const fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);

  if (vertex_shader == 0 || fragment_shader == 0) {
    return false;
  }

  bool const success = link_program(vertex_shader, fragment_shader);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  if (success) {

    (void)optional_bind_uniform_block("EnvironmentLighting",
                                      k_environment_lighting_binding_point);
    (void)optional_bind_uniform_block("LocalLighting", k_local_lighting_binding_point);
    (void)optional_bind_uniform_block("DirectionalShadows",
                                      k_directional_shadow_binding_point);
    const UniformHandle shadow_sampler =
        optional_uniform_handle("u_directional_shadow_map");
    const UniformHandle far_shadow_sampler =
        optional_uniform_handle("u_directional_shadow_map_far");
    if (shadow_sampler != InvalidUniform || far_shadow_sampler != InvalidUniform) {
      use();
      if (shadow_sampler != InvalidUniform) {
        set_uniform(shadow_sampler, TextureUnit::directional_shadow_map);
      }
      if (far_shadow_sampler != InvalidUniform) {
        set_uniform(far_shadow_sampler, TextureUnit::directional_shadow_map_far);
      }
      release();
    }
  }
  return success;
}

auto Shader::load_compute_from_source(const QString& compute_source) -> bool {
  initializeOpenGLFunctions();
  m_uniform_cache.clear();
  m_uniform_value_cache.clear();

  constexpr GLenum k_compute_shader = 0x91B9;
  GLuint const compute_shader = compile_shader(compute_source, k_compute_shader);
  if (compute_shader == 0) {
    return false;
  }

  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
  m_program = glCreateProgram();
  glAttachShader(m_program, compute_shader);
  glLinkProgram(m_program);

  GLint linked = 0;
  glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    GLchar info_log[shader_info_log_size];
    glGetProgramInfoLog(m_program, shader_info_log_size, nullptr, info_log);
    qWarning() << "Shader: compute link failed" << m_debug_name << info_log;
    glDeleteProgram(m_program);
    m_program = 0;
  }

  glDeleteShader(compute_shader);
  return m_program != 0;
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
  Shader::UniformHandle const location = fn.glGetUniformLocation(program, name);

  if (warn && (location == Shader::InvalidUniform)) {
    qWarning() << "Shader uniform not found:" << name << "(program:" << program << ")";
  }

  cache.emplace(name, location);
  return location;
}
} // namespace

auto Shader::uniform_handle(const char* name) -> Shader::UniformHandle {
  return uniform_handle_impl(*this, m_program, m_uniform_cache, name, true);
}

auto Shader::optional_uniform_handle(const char* name) -> Shader::UniformHandle {
  return uniform_handle_impl(*this, m_program, m_uniform_cache, name, false);
}

void Shader::set_uniform(UniformHandle handle, float value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform1f(handle, value);
}

void Shader::set_uniform(UniformHandle handle, const QVector3D& value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform3f(handle, value.x(), value.y(), value.z());
}

void Shader::set_uniform(UniformHandle handle, const QVector4D& value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform4f(handle, value.x(), value.y(), value.z(), value.w());
}

void Shader::set_uniform(UniformHandle handle, const QVector2D& value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform2f(handle, value.x(), value.y());
}

void Shader::set_uniform(UniformHandle handle, const QMatrix4x4& value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniformMatrix4fv(handle, 1, GL_FALSE, value.constData());
}

void Shader::set_uniform(UniformHandle handle, int value) {
  if (handle == InvalidUniform) {
    return;
  }
  if (!is_uniform_dirty(handle, value)) {
    return;
  }
  glUniform1i(handle, value);
}

void Shader::set_uniform(UniformHandle handle, bool value) {
  set_uniform(handle, static_cast<int>(value));
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

  QByteArray const source_bytes = source.toUtf8();
  const char* source_ptr = source_bytes.constData();
  glShaderSource(shader, 1, &source_ptr, nullptr);
  glCompileShader(shader);

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

auto Shader::link_program(GLuint vertex_shader, GLuint fragment_shader) -> bool {
  initializeOpenGLFunctions();
  m_program = glCreateProgram();
  glAttachShader(m_program, vertex_shader);
  glAttachShader(m_program, fragment_shader);
  glLinkProgram(m_program);

  GLint success = 0;
  glGetProgramiv(m_program, GL_LINK_STATUS, &success);
  if (success == 0) {
    GLchar info_log[shader_info_log_size];
    glGetProgramInfoLog(m_program, shader_info_log_size, nullptr, info_log);
    qWarning() << "Shader linking failed:" << info_log;
    glDeleteProgram(m_program);
    m_program = 0;
    return false;
  }

  GLuint const frame_block_idx = glGetUniformBlockIndex(m_program, "FrameData");
  if (frame_block_idx != GL_INVALID_INDEX) {
    glUniformBlockBinding(m_program, frame_block_idx, k_frame_data_binding_point);
  }

  return true;
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
  return true;
}

} // namespace Render::GL

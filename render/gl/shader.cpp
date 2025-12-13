#include "shader.h"
#include "render_constants.h"
#include "utils/resource_utils.h"
#include <GL/gl.h>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <qdebug.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qopenglext.h>
#include <qstringview.h>
#include <qvectornd.h>

namespace Render::GL {

using namespace Render::GL::BufferCapacity;

Shader::Shader() = default;

Shader::~Shader() {
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

auto Shader::loadFromFiles(const QString &vertexPath,
                           const QString &fragmentPath) -> bool {
  const QString resolved_vert =
      Utils::Resources::resolveResourcePath(vertexPath);
  const QString resolved_frag =
      Utils::Resources::resolveResourcePath(fragmentPath);

  QFile vertex_file(resolved_vert);
  QFile fragment_file(resolved_frag);

  if (!vertex_file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open vertex shader file:" << resolved_vert;
    if (resolved_vert != vertexPath) {
      qWarning() << "  Requested path:" << vertexPath;
    }
    return false;
  }

  if (!fragment_file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open fragment shader file:" << resolved_frag;
    if (resolved_frag != fragmentPath) {
      qWarning() << "  Requested path:" << fragmentPath;
    }
    vertex_file.close();
    return false;
  }

  QTextStream vertex_stream(&vertex_file);
  QTextStream fragment_stream(&fragment_file);

  QString const vertex_source = vertex_stream.readAll();
  QString const fragment_source = fragment_stream.readAll();

  return loadFromSource(vertex_source, fragment_source);
}

auto Shader::loadFromSource(const QString &vertex_source,
                            const QString &fragment_source) -> bool {
  initializeOpenGLFunctions();
  m_uniformCache.clear();
  GLuint const vertex_shader = compileShader(vertex_source, GL_VERTEX_SHADER);
  GLuint const fragment_shader =
      compileShader(fragment_source, GL_FRAGMENT_SHADER);

  if (vertex_shader == 0 || fragment_shader == 0) {
    return false;
  }

  bool const success = linkProgram(vertex_shader, fragment_shader);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return success;
}

void Shader::use() {
  initializeOpenGLFunctions();
  glUseProgram(m_program);
}

void Shader::release() {
  initializeOpenGLFunctions();
  glUseProgram(0);
}

namespace {
auto uniformHandleImpl(
    QOpenGLFunctions_3_3_Core &fn, GLuint program,
    std::unordered_map<std::string, Shader::UniformHandle> &cache,
    const char *name, bool warn) -> Shader::UniformHandle {
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
    qWarning() << "Shader uniform not found:" << name << "(program:" << program
               << ")";
  }

  cache.emplace(name, location);
  return location;
}
} // namespace

auto Shader::uniformHandle(const char *name) -> Shader::UniformHandle {
  return uniformHandleImpl(*this, m_program, m_uniformCache, name, true);
}

auto Shader::optionalUniformHandle(const char *name) -> Shader::UniformHandle {
  return uniformHandleImpl(*this, m_program, m_uniformCache, name, false);
}

void Shader::set_uniform(UniformHandle handle, float value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform1f(handle, value);
  }
}

void Shader::set_uniform(UniformHandle handle, const QVector3D &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform3f(handle, value.x(), value.y(), value.z());
  }
}

void Shader::set_uniform(UniformHandle handle, const QVector2D &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform2f(handle, value.x(), value.y());
  }
}

void Shader::set_uniform(UniformHandle handle, const QMatrix4x4 &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniformMatrix4fv(handle, 1, GL_FALSE, value.constData());
  }
}

void Shader::set_uniform(UniformHandle handle, int value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform1i(handle, value);
  }
}

void Shader::set_uniform(UniformHandle handle, bool value) {
  set_uniform(handle, static_cast<int>(value));
}

void Shader::set_uniform(const char *name, float value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const char *name, const QVector3D &value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const char *name, const QVector2D &value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const char *name, const QMatrix4x4 &value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const char *name, int value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const char *name, bool value) {
  set_uniform(uniformHandle(name), value);
}

void Shader::set_uniform(const QString &name, float value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString &name, const QVector3D &value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString &name, const QVector2D &value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString &name, const QMatrix4x4 &value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString &name, int value) {
  const QByteArray utf8 = name.toUtf8();
  set_uniform(utf8.constData(), value);
}

void Shader::set_uniform(const QString &name, bool value) {
  set_uniform(name, static_cast<int>(value));
}

auto Shader::compileShader(const QString &source, GLenum type) -> GLuint {
  initializeOpenGLFunctions();
  GLuint const shader = glCreateShader(type);

  QByteArray const source_bytes = source.toUtf8();
  const char *source_ptr = source_bytes.constData();
  glShaderSource(shader, 1, &source_ptr, nullptr);
  glCompileShader(shader);

  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    GLchar info_log[ShaderInfoLogSize];
    glGetShaderInfoLog(shader, ShaderInfoLogSize, nullptr, info_log);
    qWarning() << "Shader compilation failed:" << info_log;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

auto Shader::linkProgram(GLuint vertex_shader, GLuint fragment_shader) -> bool {
  initializeOpenGLFunctions();
  m_program = glCreateProgram();
  glAttachShader(m_program, vertex_shader);
  glAttachShader(m_program, fragment_shader);
  glLinkProgram(m_program);

  GLint success = 0;
  glGetProgramiv(m_program, GL_LINK_STATUS, &success);
  if (success == 0) {
    GLchar info_log[ShaderInfoLogSize];
    glGetProgramInfoLog(m_program, ShaderInfoLogSize, nullptr, info_log);
    qWarning() << "Shader linking failed:" << info_log;
    glDeleteProgram(m_program);
    m_program = 0;
    return false;
  }

  return true;
}

} // namespace Render::GL

#include "shader.h"
#include "utils/resource_utils.h"
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace Render::GL {

Shader::Shader() {}

Shader::~Shader() {
  if (m_program != 0) {
    glDeleteProgram(m_program);
  }
}

bool Shader::loadFromFiles(const QString &vertexPath,
                           const QString &fragmentPath) {
  const QString resolvedVert =
      Utils::Resources::resolveResourcePath(vertexPath);
  const QString resolvedFrag =
      Utils::Resources::resolveResourcePath(fragmentPath);

  QFile vertexFile(resolvedVert);
  QFile fragmentFile(resolvedFrag);

  if (!vertexFile.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open vertex shader file:" << resolvedVert;
    if (resolvedVert != vertexPath)
      qWarning() << "  Requested path:" << vertexPath;
    return false;
  }

  if (!fragmentFile.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open fragment shader file:" << resolvedFrag;
    if (resolvedFrag != fragmentPath)
      qWarning() << "  Requested path:" << fragmentPath;
    vertexFile.close();
    return false;
  }

  QTextStream vertexStream(&vertexFile);
  QTextStream fragmentStream(&fragmentFile);

  QString vertexSource = vertexStream.readAll();
  QString fragmentSource = fragmentStream.readAll();

  return loadFromSource(vertexSource, fragmentSource);
}

bool Shader::loadFromSource(const QString &vertexSource,
                            const QString &fragmentSource) {
  initializeOpenGLFunctions();
  m_uniformCache.clear();
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

  if (vertexShader == 0 || fragmentShader == 0) {
    return false;
  }

  bool success = linkProgram(vertexShader, fragmentShader);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

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

Shader::UniformHandle Shader::uniformHandle(const char *name) {
  if (!name || *name == '\0' || m_program == 0) {
    return InvalidUniform;
  }

  auto it = m_uniformCache.find(name);
  if (it != m_uniformCache.end()) {
    return it->second;
  }

  initializeOpenGLFunctions();
  UniformHandle location = glGetUniformLocation(m_program, name);
  m_uniformCache.emplace(name, location);
  return location;
}

void Shader::setUniform(UniformHandle handle, float value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform1f(handle, value);
  }
}

void Shader::setUniform(UniformHandle handle, const QVector3D &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform3f(handle, value.x(), value.y(), value.z());
  }
}

void Shader::setUniform(UniformHandle handle, const QVector2D &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform2f(handle, value.x(), value.y());
  }
}

void Shader::setUniform(UniformHandle handle, const QMatrix4x4 &value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniformMatrix4fv(handle, 1, GL_FALSE, value.constData());
  }
}

void Shader::setUniform(UniformHandle handle, int value) {
  initializeOpenGLFunctions();
  if (handle != InvalidUniform) {
    glUniform1i(handle, value);
  }
}

void Shader::setUniform(UniformHandle handle, bool value) {
  setUniform(handle, static_cast<int>(value));
}

void Shader::setUniform(const char *name, float value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const char *name, const QVector3D &value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const char *name, const QVector2D &value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const char *name, const QMatrix4x4 &value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const char *name, int value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const char *name, bool value) {
  setUniform(uniformHandle(name), value);
}

void Shader::setUniform(const QString &name, float value) {
  const QByteArray utf8 = name.toUtf8();
  setUniform(utf8.constData(), value);
}

void Shader::setUniform(const QString &name, const QVector3D &value) {
  const QByteArray utf8 = name.toUtf8();
  setUniform(utf8.constData(), value);
}

void Shader::setUniform(const QString &name, const QVector2D &value) {
  const QByteArray utf8 = name.toUtf8();
  setUniform(utf8.constData(), value);
}

void Shader::setUniform(const QString &name, const QMatrix4x4 &value) {
  const QByteArray utf8 = name.toUtf8();
  setUniform(utf8.constData(), value);
}

void Shader::setUniform(const QString &name, int value) {
  const QByteArray utf8 = name.toUtf8();
  setUniform(utf8.constData(), value);
}

void Shader::setUniform(const QString &name, bool value) {
  setUniform(name, static_cast<int>(value));
}

GLuint Shader::compileShader(const QString &source, GLenum type) {
  initializeOpenGLFunctions();
  GLuint shader = glCreateShader(type);

  QByteArray sourceBytes = source.toUtf8();
  const char *sourcePtr = sourceBytes.constData();
  glShaderSource(shader, 1, &sourcePtr, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    qWarning() << "Shader compilation failed:" << infoLog;
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

bool Shader::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
  initializeOpenGLFunctions();
  m_program = glCreateProgram();
  glAttachShader(m_program, vertexShader);
  glAttachShader(m_program, fragmentShader);
  glLinkProgram(m_program);

  GLint success;
  glGetProgramiv(m_program, GL_LINK_STATUS, &success);
  if (!success) {
    GLchar infoLog[512];
    glGetProgramInfoLog(m_program, 512, nullptr, infoLog);
    qWarning() << "Shader linking failed:" << infoLog;
    glDeleteProgram(m_program);
    m_program = 0;
    return false;
  }

  return true;
}

} // namespace Render::GL

#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <QVector2D>
#include <string>
#include <unordered_map>

namespace Render::GL {

class Shader : protected QOpenGLFunctions_3_3_Core {
public:
  using UniformHandle = GLint;
  static constexpr UniformHandle InvalidUniform = -1;

  Shader();
  ~Shader() override;

  auto loadFromFiles(const QString &vertexPath,
                     const QString &fragmentPath) -> bool;
  auto loadFromSource(const QString &vertex_source,
                      const QString &fragment_source) -> bool;

  void use();
  void release();

  auto uniformHandle(const char *name) -> UniformHandle;

  void setUniform(UniformHandle handle, float value);
  void setUniform(UniformHandle handle, const QVector3D &value);
  void setUniform(UniformHandle handle, const QVector2D &value);
  void setUniform(UniformHandle handle, const QMatrix4x4 &value);
  void setUniform(UniformHandle handle, int value);
  void setUniform(UniformHandle handle, bool value);

  void setUniform(const char *name, float value);
  void setUniform(const char *name, const QVector3D &value);
  void setUniform(const char *name, const QVector2D &value);
  void setUniform(const char *name, const QMatrix4x4 &value);
  void setUniform(const char *name, int value);
  void setUniform(const char *name, bool value);

  void setUniform(const QString &name, float value);
  void setUniform(const QString &name, const QVector3D &value);
  void setUniform(const QString &name, const QVector2D &value);
  void setUniform(const QString &name, const QMatrix4x4 &value);
  void setUniform(const QString &name, int value);
  void setUniform(const QString &name, bool value);

private:
  GLuint m_program = 0;
  auto compileShader(const QString &source, GLenum type) -> GLuint;
  auto linkProgram(GLuint vertex_shader, GLuint fragment_shader) -> bool;

  std::unordered_map<std::string, UniformHandle> m_uniformCache;
};

} // namespace Render::GL
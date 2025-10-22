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
  ~Shader();

  bool loadFromFiles(const QString &vertexPath, const QString &fragmentPath);
  bool loadFromSource(const QString &vertexSource,
                      const QString &fragmentSource);

  void use();
  void release();

  UniformHandle uniformHandle(const char *name);

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
  GLuint compileShader(const QString &source, GLenum type);
  bool linkProgram(GLuint vertexShader, GLuint fragmentShader);

  std::unordered_map<std::string, UniformHandle> m_uniformCache;
};

} 
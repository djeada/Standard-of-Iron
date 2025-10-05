#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>

namespace Render::GL {

class Shader : protected QOpenGLFunctions_3_3_Core {
public:
  Shader();
  ~Shader();

  bool loadFromFiles(const QString &vertexPath, const QString &fragmentPath);
  bool loadFromSource(const QString &vertexSource,
                      const QString &fragmentSource);

  void use();
  void release();

  void setUniform(const QString &name, float value);
  void setUniform(const QString &name, const QVector3D &value);
  void setUniform(const QString &name, const QMatrix4x4 &value);
  void setUniform(const QString &name, int value);
  void setUniform(const QString &name, bool value);

private:
  GLuint m_program = 0;
  GLuint compileShader(const QString &source, GLenum type);
  bool linkProgram(GLuint vertexShader, GLuint fragmentShader);
};

} // namespace Render::GL
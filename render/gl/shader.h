#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

namespace Render::GL {

class Shader : protected QOpenGLFunctions_3_3_Core {
public:
  using UniformHandle = GLint;
  static constexpr UniformHandle InvalidUniform = -1;

  Shader();
  ~Shader() override;

  auto load_from_files(const QString &vertex_path,
                       const QString &fragment_path) -> bool;
  auto load_from_source(const QString &vertex_source,
                        const QString &fragment_source) -> bool;

  void use();
  void release();

  auto uniform_handle(const char *name) -> UniformHandle;
  auto optional_uniform_handle(const char *name) -> UniformHandle;

  void set_uniform(UniformHandle handle, float value);
  void set_uniform(UniformHandle handle, const QVector3D &value);
  void set_uniform(UniformHandle handle, const QVector2D &value);
  void set_uniform(UniformHandle handle, const QMatrix4x4 &value);
  void set_uniform(UniformHandle handle, int value);
  void set_uniform(UniformHandle handle, bool value);

  void set_uniform(const char *name, float value);
  void set_uniform(const char *name, const QVector3D &value);
  void set_uniform(const char *name, const QVector2D &value);
  void set_uniform(const char *name, const QMatrix4x4 &value);
  void set_uniform(const char *name, int value);
  void set_uniform(const char *name, bool value);

  void set_uniform(const QString &name, float value);
  void set_uniform(const QString &name, const QVector3D &value);
  void set_uniform(const QString &name, const QVector2D &value);
  void set_uniform(const QString &name, const QMatrix4x4 &value);
  void set_uniform(const QString &name, int value);
  void set_uniform(const QString &name, bool value);

  void clear_uniform_cache() { m_uniform_value_cache.clear(); }

  // Stage 16.2 — bind a `layout(std140) uniform <name> { ... }` block
  // to a fixed binding point. Safe to call before/after `use()`. Returns
  // false if the block is not present in the program (warning emitted).
  auto bind_uniform_block(const char *block_name, std::uint32_t binding_point)
      -> bool;

private:
  GLuint m_program = 0;
  auto compile_shader(const QString &source, GLenum type) -> GLuint;
  auto link_program(GLuint vertex_shader, GLuint fragment_shader) -> bool;

  std::unordered_map<std::string, UniformHandle> m_uniform_cache;

  // Dirty-tracking for uniform values to skip redundant glUniform calls
  using UniformValue = std::variant<float, int, QVector2D, QVector3D, QVector4D, QMatrix4x4>;
  std::unordered_map<GLint, UniformValue> m_uniform_value_cache;

  template <typename T>
  auto is_uniform_dirty(GLint location, const T &value) -> bool;
};

} // namespace Render::GL

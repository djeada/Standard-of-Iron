#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QString>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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

  void set_debug_name(QString name) { m_debug_name = std::move(name); }
  [[nodiscard]] auto debug_name() const -> const QString & {
    return m_debug_name;
  }

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

  auto bind_uniform_block(const char *block_name,
                          std::uint32_t binding_point) -> bool;

private:
  GLuint m_program = 0;
  QString m_debug_name;
  auto compile_shader(const QString &source, GLenum type) -> GLuint;
  auto link_program(GLuint vertex_shader, GLuint fragment_shader) -> bool;

  std::unordered_map<std::string, UniformHandle> m_uniform_cache;

  using UniformValue =
      std::variant<float, int, QVector2D, QVector3D, QVector4D, QMatrix4x4>;
  std::unordered_map<GLint, UniformValue> m_uniform_value_cache;

  template <typename T>
  auto is_uniform_dirty(GLint location, const T &value) -> bool;
};

struct ShaderBindAuditEntry {
  QString name;
  std::uint64_t bind_count = 0;
};

void set_shader_bind_audit_enabled(bool enabled);
[[nodiscard]] auto shader_bind_audit_enabled() -> bool;
void reset_shader_bind_audit();
[[nodiscard]] auto
shader_bind_audit_snapshot() -> std::vector<ShaderBindAuditEntry>;
[[nodiscard]] auto classify_shader_for_audit(const QString &name) -> QString;
[[nodiscard]] auto format_shader_bind_audit() -> QStringList;

} // namespace Render::GL

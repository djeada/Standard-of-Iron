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

  auto load_from_files(const QString& vertex_path,
                       const QString& fragment_path,
                       const QString& variant_defines = QString()) -> bool;
  auto load_from_source(const QString& vertex_source,
                        const QString& fragment_source) -> bool;
  auto load_compute_from_source(const QString& compute_source) -> bool;

  auto reload() -> bool;

  static auto reload_all() -> std::size_t;

  static void set_global_defines(const QString& defines);
  [[nodiscard]] static auto global_defines() -> QString;

  void use();
  void release();

  void set_debug_name(QString name) { m_debug_name = std::move(name); }
  [[nodiscard]] auto debug_name() const -> const QString& { return m_debug_name; }

  auto uniform_handle(const char* name) -> UniformHandle;
  auto optional_uniform_handle(const char* name) -> UniformHandle;

  void set_uniform(UniformHandle handle, float value);
  void set_uniform(UniformHandle handle, const QVector3D& value);
  void set_uniform(UniformHandle handle, const QVector4D& value);
  void set_uniform(UniformHandle handle, const QVector2D& value);
  void set_uniform(UniformHandle handle, const QMatrix4x4& value);
  void set_uniform(UniformHandle handle, int value);
  void set_uniform(UniformHandle handle, unsigned int value);
  void set_uniform(UniformHandle handle, bool value);

  void set_uniform_vec3_array(UniformHandle handle, const float* data, int count);

  void set_uniform(const char* name, float value);
  void set_uniform(const char* name, const QVector3D& value);
  void set_uniform(const char* name, const QVector4D& value);
  void set_uniform(const char* name, const QVector2D& value);
  void set_uniform(const char* name, const QMatrix4x4& value);
  void set_uniform(const char* name, int value);
  void set_uniform(const char* name, bool value);

  void set_uniform(const QString& name, float value);
  void set_uniform(const QString& name, const QVector3D& value);
  void set_uniform(const QString& name, const QVector4D& value);
  void set_uniform(const QString& name, const QVector2D& value);
  void set_uniform(const QString& name, const QMatrix4x4& value);
  void set_uniform(const QString& name, int value);
  void set_uniform(const QString& name, bool value);

  auto bind_uniform_block(const char* block_name, std::uint32_t binding_point) -> bool;
  auto optional_bind_uniform_block(const char* block_name,
                                   std::uint32_t binding_point) -> bool;
  [[nodiscard]] static auto preprocess_source(const QString& source) -> QString;

private:
  enum class SourceKind : std::uint8_t {
    None,
    Files,
    Sources,
    Compute
  };

  GLuint m_program = 0;
  QString m_debug_name;
  auto compile_shader(const QString& source, GLenum type) -> GLuint;
  auto link_program(GLuint vertex_shader, GLuint fragment_shader) -> GLuint;
  auto link_compute_program(GLuint compute_shader) -> GLuint;
  auto build_graphics_program(const QString& vertex_source,
                              const QString& fragment_source) -> GLuint;
  void adopt_program(GLuint program);
  void bind_standard_blocks();
  void replay_uniform_state();
  [[nodiscard]] auto location_for(UniformHandle handle) const noexcept -> GLint;

  SourceKind m_source_kind{SourceKind::None};
  QString m_vertex_path;
  QString m_fragment_path;
  QString m_vertex_source;
  QString m_fragment_source;
  QString m_compute_source;
  QString m_variant_defines;

  std::unordered_map<std::string, UniformHandle> m_uniform_cache;
  std::vector<std::string> m_uniform_names;
  std::vector<GLint> m_uniform_locations;
  std::vector<std::pair<std::string, std::uint32_t>> m_block_bindings;

  using UniformValue = std::
      variant<float, int, unsigned int, QVector2D, QVector3D, QVector4D, QMatrix4x4>;
  std::unordered_map<GLint, UniformValue> m_uniform_value_cache;

  template <typename T>
  auto is_uniform_dirty(GLint handle, const T& value) -> bool;
};

} // namespace Render::GL

#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <vector>

namespace Render::GL {

class Buffer : protected QOpenGLFunctions_3_3_Core {
public:
  enum class Type { Vertex, Index, Uniform };

  enum class Usage { Static, Dynamic, Stream };

  Buffer(Type type);
  ~Buffer() override;

  void bind();
  void unbind();

  void set_data(const void *data, size_t size, Usage usage = Usage::Static);

  template <typename T>
  void set_data(const std::vector<T> &data, Usage usage = Usage::Static) {
    set_data(data.data(), data.size() * sizeof(T), usage);
  }

private:
  GLuint m_buffer = 0;
  Type m_type;
  [[nodiscard]] auto get_gl_type() const -> GLenum;
  [[nodiscard]] static auto get_gl_usage(Usage usage) -> GLenum;
};

class VertexArray : protected QOpenGLFunctions_3_3_Core {
public:
  VertexArray();
  ~VertexArray() override;

  void bind();
  void unbind();

  void add_vertexBuffer(Buffer &buffer, const std::vector<int> &layout);
  void set_index_buffer(Buffer &buffer);

private:
  GLuint m_vao = 0;
  int m_currentAttribIndex = 0;
};

} // namespace Render::GL
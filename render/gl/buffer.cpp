#include "buffer.h"
#include "opengl_headers.h"
#include <cstddef>
#include <qopenglext.h>
#include <vector>

namespace Render::GL {

Buffer::Buffer(Type type) : m_type(type) {}

Buffer::~Buffer() {
  if (m_buffer != 0) {
    glDeleteBuffers(1, &m_buffer);
  }
}

void Buffer::bind() {
  if (m_buffer == 0u) {
    initializeOpenGLFunctions();
    glGenBuffers(1, &m_buffer);
  }
  glBindBuffer(getGLType(), m_buffer);
}

void Buffer::unbind() { glBindBuffer(getGLType(), 0); }

void Buffer::setData(const void *data, size_t size, Usage usage) {
  bind();
  glBufferData(getGLType(), size, data, getGLUsage(usage));
}

auto Buffer::getGLType() const -> GLenum {
  switch (m_type) {
  case Type::Vertex:
    return GL_ARRAY_BUFFER;
  case Type::Index:
    return GL_ELEMENT_ARRAY_BUFFER;
  case Type::Uniform:
    return GL_UNIFORM_BUFFER;
  }
  return GL_ARRAY_BUFFER;
}

GLenum Buffer::getGLUsage(Usage usage) {
  switch (usage) {
  case Usage::Static:
    return GL_STATIC_DRAW;
  case Usage::Dynamic:
    return GL_DYNAMIC_DRAW;
  case Usage::Stream:
    return GL_STREAM_DRAW;
  }
  return GL_STATIC_DRAW;
}

VertexArray::VertexArray() = default;

VertexArray::~VertexArray() {
  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
  }
}

void VertexArray::bind() {
  if (m_vao == 0u) {
    initializeOpenGLFunctions();
    glGenVertexArrays(1, &m_vao);
  }
  glBindVertexArray(m_vao);
}

void VertexArray::unbind() { glBindVertexArray(0); }

void VertexArray::add_vertexBuffer(Buffer &buffer,
                                   const std::vector<int> &layout) {
  bind();
  buffer.bind();

  int stride = 0;
  for (int const size : layout) {
    stride += size * sizeof(float);
  }

  int offset = 0;
  for (int const size : layout) {
    glEnableVertexAttribArray(m_currentAttribIndex);
    glVertexAttribPointer(m_currentAttribIndex, size, GL_FLOAT, GL_FALSE,
                          stride, reinterpret_cast<void *>(offset));
    offset += size * sizeof(float);
    m_currentAttribIndex++;
  }
}

void VertexArray::setIndexBuffer(Buffer &buffer) {
  bind();
  buffer.bind();
}

} // namespace Render::GL
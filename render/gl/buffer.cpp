#include "buffer.h"
#include <GL/gl.h>
#include <QDebug>
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
  if (m_buffer == 0U) {
    initializeOpenGLFunctions();
    glGenBuffers(1, &m_buffer);
  }
  glBindBuffer(get_gl_type(), m_buffer);
}

void Buffer::unbind() { glBindBuffer(get_gl_type(), 0); }

void Buffer::set_data(const void *data, size_t size, Usage usage) {
  bind();
  glBufferData(get_gl_type(), size, data, get_gl_usage(usage));
}

auto Buffer::get_gl_type() const -> GLenum {
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

auto Buffer::get_gl_usage(Usage usage) -> GLenum {
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
  if (m_vao == 0U) {
    initializeOpenGLFunctions();
    glGenVertexArrays(1, &m_vao);
    GLenum genErr = glGetError();
    if (genErr != GL_NO_ERROR) {
      qWarning() << "VertexArray glGenVertexArrays error" << genErr;
    }
  }

  while (glGetError() != GL_NO_ERROR) {
  }

  glBindVertexArray(m_vao);
  GLenum bindErr = glGetError();
  if (bindErr != GL_NO_ERROR) {
    qWarning() << "VertexArray glBindVertexArray error" << bindErr << "vao"
               << m_vao;
  }
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

void VertexArray::set_index_buffer(Buffer &buffer) {
  bind();
  buffer.bind();
}

} 

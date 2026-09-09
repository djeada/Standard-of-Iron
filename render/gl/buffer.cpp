#include "buffer.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qopenglext.h>

#include <cstddef>
#include <vector>

#include "gl_lifetime.h"
#include "gl_resource_tracking.h"
#include "platform_gl.h"
#include "render/profiling/asset_counters.h"

namespace Render::GL {

Buffer::Buffer(Type type)
    : m_type(type) {
}

Buffer::~Buffer() {
  if (m_buffer == 0) {
    return;
  }
  if (!gl_objects_can_be_released()) {
    defer_gl_delete(DeferredGlObject::Buffer, m_buffer);
    return;
  }
  glDeleteBuffers(1, &m_buffer);
}

void Buffer::bind() {
  if (m_buffer == 0U) {
    initializeOpenGLFunctions();
    glGenBuffers(1, &m_buffer);
    note_buffers_created();
  }
  glBindBuffer(get_gl_type(), m_buffer);
}

void Buffer::unbind() {
  glBindBuffer(get_gl_type(), 0);
}

void Buffer::set_data(const void* data, size_t size, Usage usage) {
  bind();
  glBufferData(get_gl_type(), static_cast<GLsizeiptr>(size), data, get_gl_usage(usage));
  note_buffer_storage(size, data != nullptr);
  m_size_bytes = size;
}

void Buffer::reserve(size_t size, Usage usage) {
  if (m_size_bytes >= size && m_buffer != 0U) {
    return;
  }
  set_data(nullptr, size, usage);
}

void Buffer::update_sub_data(const void* data, size_t size) {
  if (size == 0) {
    return;
  }
  if (size > m_size_bytes || m_buffer == 0U) {
    set_data(data, size, Usage::Dynamic);
    return;
  }
  bind();
  glBufferSubData(get_gl_type(), 0, static_cast<GLsizeiptr>(size), data);
  note_buffer_transfer(size);
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
  if (m_vao == 0) {
    return;
  }
  if (!gl_objects_can_be_released()) {
    defer_gl_delete(DeferredGlObject::VertexArray, m_vao);
    return;
  }
  glDeleteVertexArrays(1, &m_vao);
}

void VertexArray::bind() {
  if (m_vao == 0U) {
    initializeOpenGLFunctions();
#ifndef NDEBUG
    while (glGetError() != GL_NO_ERROR) {
    }
#endif
    glGenVertexArrays(1, &m_vao);
    note_vertex_arrays_created();
#ifndef NDEBUG
    GLenum gen_err = glGetError();
    if (gen_err != GL_NO_ERROR) {
      qWarning() << "VertexArray glGenVertexArrays error" << gen_err;
    }
#endif
  }

#ifndef NDEBUG
  while (glGetError() != GL_NO_ERROR) {
  }
#endif

  glBindVertexArray(m_vao);
#ifndef NDEBUG
  GLenum bind_err = glGetError();
  if (bind_err != GL_NO_ERROR) {
    qWarning() << "VertexArray glBindVertexArray error" << bind_err << "vao" << m_vao;
  }
#endif
}

void VertexArray::unbind() {
  glBindVertexArray(0);
}

void VertexArray::add_vertex_buffer(Buffer& buffer, const std::vector<int>& layout) {
  bind();
  buffer.bind();

  int stride = 0;
  for (int const size : layout) {
    stride += size * sizeof(float);
  }

  int offset = 0;
  for (int const size : layout) {
    glEnableVertexAttribArray(m_current_attrib_index);
    glVertexAttribPointer(m_current_attrib_index,
                          size,
                          GL_FLOAT,
                          GL_FALSE,
                          stride,
                          reinterpret_cast<void*>(offset));
    offset += size * sizeof(float);
    m_current_attrib_index++;
  }
}

void VertexArray::set_index_buffer(Buffer& buffer) {
  bind();
  buffer.bind();
}

} // namespace Render::GL

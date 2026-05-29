#pragma once

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <GL/gl.h>
#include <cstddef>
#include <initializer_list>

namespace Render::GL {

struct VertexAttribSpec {
  GLuint index;
  GLint size;
  GLenum type;
  GLboolean normalized;
  GLsizei stride;
  std::size_t offset;
  GLuint divisor{0};
  bool has_divisor{false};
};

inline void apply_vertex_attrib_layout(std::initializer_list<VertexAttribSpec> specs) {
  auto* gl = QOpenGLContext::currentContext()->extraFunctions();
  for (const VertexAttribSpec& spec : specs) {
    gl->glEnableVertexAttribArray(spec.index);
    gl->glVertexAttribPointer(spec.index,
                              spec.size,
                              spec.type,
                              spec.normalized,
                              spec.stride,
                              reinterpret_cast<void*>(spec.offset));
    if (spec.has_divisor) {
      gl->glVertexAttribDivisor(spec.index, spec.divisor);
    }
  }
}

} // namespace Render::GL

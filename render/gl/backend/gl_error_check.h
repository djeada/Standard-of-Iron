#pragma once

#include <QDebug>

#include <GL/gl.h>

namespace Render::GL::BackendPipelines {

inline void clear_gl_errors() {
#ifndef NDEBUG
  while (glGetError() != GL_NO_ERROR) {
  }
#endif
}

inline auto check_gl_error(const char* pipeline, const char* operation) -> bool {
#ifndef NDEBUG
  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << pipeline << "GL error in" << operation << ":" << err;
    return false;
  }
#else
  Q_UNUSED(pipeline);
  Q_UNUSED(operation);
#endif
  return true;
}

} // namespace Render::GL::BackendPipelines

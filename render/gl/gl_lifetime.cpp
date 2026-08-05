#include "gl_lifetime.h"

#include <QCoreApplication>
#include <QOpenGLContext>

namespace Render::GL {

auto gl_objects_can_be_released() noexcept -> bool {
  if (QCoreApplication::instance() == nullptr) {
    return false;
  }
  return QOpenGLContext::currentContext() != nullptr;
}

} // namespace Render::GL

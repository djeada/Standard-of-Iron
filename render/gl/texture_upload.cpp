#include "texture_upload.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QtGui/qopengl.h>

#include <cstddef>

#include "gl_resource_tracking.h"

namespace Render::GL {

void upload_bound_texture_rgba8(
    int x, int y, int width, int height, const void* texels) {
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return;
  }
  gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl->glTexSubImage2D(
      GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texels);
  note_texture_transfer(texture_transfer_bytes(
      static_cast<std::size_t>(width), static_cast<std::size_t>(height), 4U));
}

void upload_bound_texture_float(int width,
                                int height,
                                FloatTexelLayout layout,
                                const float* texels) {
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return;
  }
  const bool single_channel = layout == FloatTexelLayout::Red;
  gl->glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      0,
                      width,
                      height,
                      single_channel ? GL_RED : GL_RGBA,
                      GL_FLOAT,
                      texels);
  note_texture_transfer(texture_transfer_bytes(static_cast<std::size_t>(width),
                                               static_cast<std::size_t>(height),
                                               single_channel ? 4U : 16U));
}

} // namespace Render::GL

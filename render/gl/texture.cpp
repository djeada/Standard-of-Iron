#include "texture.h"
#include <GL/gl.h>
#include <QDebug>
#include <QImage>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qimage.h>

namespace Render::GL {

Texture::Texture() = default;

Texture::~Texture() {
  if (m_texture != 0) {
    glDeleteTextures(1, &m_texture);
  }
}

auto Texture::load_from_file(const QString &path) -> bool {
  initializeOpenGLFunctions();
  QImage image;
  if (!image.load(path)) {
    qWarning() << "Failed to load texture:" << path;
    return false;
  }

  image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();

  m_width = image.width();
  m_height = image.height();
  m_format = Format::RGBA;

  bind();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image.constBits());

  set_filter(Filter::Linear, Filter::Linear);
  set_wrap(Wrap::Repeat, Wrap::Repeat);

  glGenerateMipmap(GL_TEXTURE_2D);

  unbind();

  return true;
}

auto Texture::create_empty(int width, int height, Format format) -> bool {
  initializeOpenGLFunctions();
  m_width = width;
  m_height = height;
  m_format = format;

  bind();

  GLenum const gl_format = get_gl_format(format);
  GLenum internal_format = gl_format;
  GLenum type = GL_UNSIGNED_BYTE;

  if (format == Format::Depth) {
    internal_format = GL_DEPTH_COMPONENT;
    type = GL_FLOAT;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, gl_format,
               type, nullptr);

  set_filter(Filter::Linear, Filter::Linear);
  set_wrap(Wrap::ClampToEdge, Wrap::ClampToEdge);

  unbind();

  return true;
}

void Texture::bind(int unit) {
  initializeOpenGLFunctions();
  if (m_texture == 0U) {
    glGenTextures(1, &m_texture);
  }
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_texture);
}

void Texture::unbind() {
  initializeOpenGLFunctions();
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::set_filter(Filter min_filter, Filter mag_filter) {
  bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  get_gl_filter(min_filter));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  get_gl_filter(mag_filter));
}

void Texture::set_wrap(Wrap s_wrap, Wrap t_wrap) {
  bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, get_gl_wrap(s_wrap));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, get_gl_wrap(t_wrap));
}

auto Texture::get_gl_format(Format format) -> GLenum {
  switch (format) {
  case Format::RGB:
    return GL_RGB;
  case Format::RGBA:
    return GL_RGBA;
  case Format::Depth:
    return GL_DEPTH_COMPONENT;
  }
  return GL_RGBA;
}

auto Texture::get_gl_filter(Filter filter) -> GLenum {
  switch (filter) {
  case Filter::Nearest:
    return GL_NEAREST;
  case Filter::Linear:
    return GL_LINEAR;
  }
  return GL_LINEAR;
}

auto Texture::get_gl_wrap(Wrap wrap) -> GLenum {
  switch (wrap) {
  case Wrap::Repeat:
    return GL_REPEAT;
  case Wrap::ClampToEdge:
    return GL_CLAMP_TO_EDGE;
  case Wrap::ClampToBorder:
    return GL_CLAMP_TO_BORDER;
  }
  return GL_REPEAT;
}

} // namespace Render::GL
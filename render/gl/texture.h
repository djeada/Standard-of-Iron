#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

namespace Render::GL {

class Texture : protected QOpenGLFunctions_3_3_Core {
public:
  enum class Format { RGB, RGBA, Depth };

  enum class Filter { Nearest, Linear };

  enum class Wrap { Repeat, ClampToEdge, ClampToBorder };

  Texture();
  ~Texture();

  bool loadFromFile(const QString &path);
  bool createEmpty(int width, int height, Format format = Format::RGBA);

  void bind(int unit = 0);
  void unbind();

  void setFilter(Filter minFilter, Filter magFilter);
  void setWrap(Wrap sWrap, Wrap tWrap);

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }

private:
  GLuint m_texture = 0;
  int m_width = 0;
  int m_height = 0;
  Format m_format = Format::RGBA;

  GLenum getGLFormat(Format format) const;
  GLenum getGLFilter(Filter filter) const;
  GLenum getGLWrap(Wrap wrap) const;
};

} // namespace Render::GL
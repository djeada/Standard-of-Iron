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
  ~Texture() override;

  auto loadFromFile(const QString &path) -> bool;
  auto createEmpty(int width, int height, Format format = Format::RGBA) -> bool;

  void bind(int unit = 0);
  void unbind();

  void setFilter(Filter minFilter, Filter magFilter);
  void setWrap(Wrap sWrap, Wrap tWrap);

  [[nodiscard]] auto getWidth() const -> int { return m_width; }
  [[nodiscard]] auto getHeight() const -> int { return m_height; }

private:
  GLuint m_texture = 0;
  int m_width = 0;
  int m_height = 0;
  Format m_format = Format::RGBA;

  [[nodiscard]] static auto getGLFormat(Format format) -> GLenum;
  [[nodiscard]] static auto getGLFilter(Filter filter) -> GLenum;
  [[nodiscard]] static auto getGLWrap(Wrap wrap) -> GLenum;
};

} // namespace Render::GL
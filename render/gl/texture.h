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

  auto load_from_file(const QString &path) -> bool;
  auto create_empty(int width, int height, Format format = Format::RGBA) -> bool;

  void bind(int unit = 0);
  void unbind();

  void set_filter(Filter minFilter, Filter magFilter);
  void set_wrap(Wrap sWrap, Wrap tWrap);

  [[nodiscard]] auto get_width() const -> int { return m_width; }
  [[nodiscard]] auto get_height() const -> int { return m_height; }

private:
  GLuint m_texture = 0;
  int m_width = 0;
  int m_height = 0;
  Format m_format = Format::RGBA;

  [[nodiscard]] static auto get_gl_format(Format format) -> GLenum;
  [[nodiscard]] static auto get_gl_filter(Filter filter) -> GLenum;
  [[nodiscard]] static auto get_gl_wrap(Wrap wrap) -> GLenum;
};

} // namespace Render::GL
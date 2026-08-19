#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>

#include <gtest/gtest.h>

#include "render/gl/shader.h"

namespace {

struct OffscreenGl {
  QOffscreenSurface surface;
  QOpenGLContext context;
  bool ready = false;

  OffscreenGl() {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    surface.setFormat(format);
    surface.create();
    if (!surface.isValid()) {
      return;
    }
    context.setFormat(format);
    if (!context.create() || !context.makeCurrent(&surface)) {
      return;
    }
    ready = true;
  }
  ~OffscreenGl() {
    if (ready) {
      context.doneCurrent();
    }
  }
};

const char* k_vertex = R"(#version 330 core
layout(location = 0) in vec3 a_position;
uniform mat4 u_mvp;
void main() { gl_Position = u_mvp * vec4(a_position, 1.0); }
)";

const char* k_fragment = R"(#version 330 core
#ifndef SOI_QUALITY_TIER
#define SOI_QUALITY_TIER 2
#endif
uniform vec3 u_color;
uniform sampler2D u_texture;
uniform float u_extra;
out vec4 frag_color;
void main() {
#if SOI_QUALITY_TIER >= 2
  frag_color = vec4(u_color * u_extra, 1.0) + texture(u_texture, vec2(0.5));
#else
  frag_color = vec4(u_color, 1.0) + texture(u_texture, vec2(0.5));
#endif
}
)";

} // namespace

TEST(ShaderReload, HandlesAndUniformStateSurviveATierChange) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  QOpenGLFunctions_3_3_Core fn;
  fn.initializeOpenGLFunctions();

  Render::GL::Shader::set_global_defines(
      QStringLiteral("#define SOI_QUALITY_TIER 2\n"));
  Render::GL::Shader shader;
  shader.set_debug_name(QStringLiteral("reload_test"));
  ASSERT_TRUE(shader.load_from_source(QString::fromLatin1(k_vertex),
                                      QString::fromLatin1(k_fragment)));

  const auto color = shader.uniform_handle("u_color");
  const auto extra = shader.uniform_handle("u_extra");
  const auto sampler = shader.uniform_handle("u_texture");
  ASSERT_NE(color, Render::GL::Shader::InvalidUniform);
  ASSERT_NE(extra, Render::GL::Shader::InvalidUniform);
  ASSERT_NE(sampler, Render::GL::Shader::InvalidUniform);
  shader.use();
  shader.set_uniform(sampler, 3);
  shader.set_uniform(color, QVector3D(0.25F, 0.5F, 0.75F));
  shader.release();

  Render::GL::Shader::set_global_defines(
      QStringLiteral("#define SOI_QUALITY_TIER 0\n"));
  ASSERT_TRUE(shader.reload());

  GLint program = 0;
  fn.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  shader.use();
  fn.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  ASSERT_NE(program, 0);

  const GLint color_location = fn.glGetUniformLocation(program, "u_color");
  const GLint sampler_location = fn.glGetUniformLocation(program, "u_texture");
  ASSERT_GE(color_location, 0);
  ASSERT_GE(sampler_location, 0);

  GLint unit = -1;
  fn.glGetUniformiv(program, sampler_location, &unit);
  EXPECT_EQ(unit, 3) << "sampler unit set before the reload was not replayed";
  GLfloat rgb[3]{};
  fn.glGetUniformfv(program, color_location, rgb);
  EXPECT_FLOAT_EQ(rgb[1], 0.5F);

  shader.set_uniform(color, QVector3D(0.1F, 0.2F, 0.3F));
  shader.set_uniform(extra, 4.0F);
  fn.glGetUniformfv(program, color_location, rgb);
  EXPECT_FLOAT_EQ(rgb[2], 0.3F);
  EXPECT_EQ(fn.glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  shader.release();

  Render::GL::Shader::set_global_defines(
      QStringLiteral("#define SOI_QUALITY_TIER 2\n"));
  ASSERT_TRUE(shader.reload());
  shader.use();
  fn.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  shader.set_uniform(extra, 2.5F);
  const GLint extra_location = fn.glGetUniformLocation(program, "u_extra");
  ASSERT_GE(extra_location, 0);
  GLfloat extra_value = 0.0F;
  fn.glGetUniformfv(program, extra_location, &extra_value);
  EXPECT_FLOAT_EQ(extra_value, 2.5F);
  shader.release();

  Render::GL::Shader::set_global_defines(QString());
}

TEST(ShaderReload, ReloadAllCountsLivePrograms) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  Render::GL::Shader::set_global_defines(
      QStringLiteral("#define SOI_QUALITY_TIER 2\n"));
  Render::GL::Shader a;
  Render::GL::Shader b;
  ASSERT_TRUE(a.load_from_source(QString::fromLatin1(k_vertex),
                                 QString::fromLatin1(k_fragment)));
  ASSERT_TRUE(b.load_from_source(QString::fromLatin1(k_vertex),
                                 QString::fromLatin1(k_fragment)));
  Render::GL::Shader never_loaded;
  EXPECT_GE(Render::GL::Shader::reload_all(), 2U);
  Render::GL::Shader::set_global_defines(QString());
}

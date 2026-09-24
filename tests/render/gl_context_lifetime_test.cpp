#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>

#include <gtest/gtest.h>
#include <memory>

#include "render/gl/gl_lifetime.h"
#include "render/gl/mesh.h"
#include "render/gl/resources.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"

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
void main() { gl_Position = vec4(a_position, 1.0); }
)";

const char* k_fragment = R"(#version 330 core
out vec4 frag_color;
void main() { frag_color = vec4(1.0); }
)";

} // namespace

TEST(GlContextLifetime, GlDestructionWithoutCurrentContextNeverCallsGl) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }

  auto texture = std::make_unique<Render::GL::Texture>();
  ASSERT_TRUE(texture->create_empty(4, 4));
  auto shader = std::make_unique<Render::GL::Shader>();
  ASSERT_TRUE(shader->load_from_source(QString::fromLatin1(k_vertex),
                                       QString::fromLatin1(k_fragment)));
  auto resources = std::make_unique<Render::GL::ResourceManager>();
  ASSERT_TRUE(resources->initialize());
  ASSERT_NE(resources->wear_volume(), 0U);

  gl.context.doneCurrent();
  ASSERT_FALSE(Render::GL::gl_objects_can_be_released());

  const std::size_t before = Render::GL::deferred_gl_delete_count();
  texture.reset();
  shader.reset();
  resources.reset();
  constexpr std::size_t k_resource_manager_names = 3U * 3U + 2U + 1U;
  EXPECT_EQ(Render::GL::deferred_gl_delete_count(),
            before + 2U + k_resource_manager_names)
      << "an object destroyed without its context either called GL or leaked its "
         "name instead of deferring it";

  ASSERT_TRUE(gl.context.makeCurrent(&gl.surface));
  Render::GL::drain_deferred_gl_deletes();
  EXPECT_LE(Render::GL::deferred_gl_delete_count(), before);
}

TEST(GlContextLifetime, ResourceManagerReleasesWearVolume) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  QOpenGLFunctions_3_3_Core fn;
  fn.initializeOpenGLFunctions();

  unsigned int wear = 0U;
  {
    Render::GL::ResourceManager resources;
    ASSERT_TRUE(resources.initialize());
    wear = resources.wear_volume();
    ASSERT_NE(wear, 0U);
    ASSERT_EQ(fn.glIsTexture(wear), GL_TRUE);
  }
  EXPECT_EQ(fn.glIsTexture(wear), GL_FALSE) << "the 3D wear texture leaked";
}

TEST(GlContextLifetime, MeshReuploadsWhenDrawnInAnotherShareGroup) {
  OffscreenGl first;
  if (!first.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  auto mesh = Render::GL::create_quad_mesh();
  ASSERT_TRUE(mesh->bind_vao());
  mesh->unbind_vao();
  first.context.doneCurrent();

  OffscreenGl second;
  if (!second.ready) {
    GTEST_SKIP() << "No second OpenGL context available in this environment";
  }
  ASSERT_NE(first.context.shareGroup(), second.context.shareGroup());
  QOpenGLFunctions_3_3_Core fn;
  fn.initializeOpenGLFunctions();
  ASSERT_TRUE(mesh->bind_vao())
      << "a cached mesh reused vertex-array names from another share group";
  GLint bound = 0;
  fn.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &bound);
  EXPECT_EQ(fn.glIsVertexArray(static_cast<GLuint>(bound)), GL_TRUE);
  mesh->unbind_vao();
  mesh.reset();
}

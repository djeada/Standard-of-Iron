#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QSurfaceFormat>

#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "render/geom/instance_block.h"
#include "render/geom/mesh.h"
#include "render/geom/texture_image.h"
#include "render/gl/gl_lifetime.h"
#include "render/gl/instance_buffer_table.h"
#include "render/gl/mesh_gpu_table.h"
#include "render/gl/resources.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "render/gl/texture_table.h"

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
  // Two textures and the wear volume. Its meshes are geometry only; their GL
  // objects belong to whichever MeshGpuTable uploaded them.
  constexpr std::size_t k_resource_manager_names = 2U + 1U;
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

TEST(GlContextLifetime, EachMeshTableUploadsItsOwnCopyOfASharedMesh) {
  OffscreenGl first;
  if (!first.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  auto mesh = Render::GL::create_quad_mesh();
  auto first_table = std::make_unique<Render::GL::MeshGpuTable>();
  ASSERT_TRUE(first_table->bind(*mesh));
  first_table->unbind();
  first.context.doneCurrent();

  OffscreenGl second;
  if (!second.ready) {
    GTEST_SKIP() << "No second OpenGL context available in this environment";
  }
  ASSERT_NE(first.context.shareGroup(), second.context.shareGroup());
  auto second_table = std::make_unique<Render::GL::MeshGpuTable>();
  ASSERT_TRUE(second_table->bind(*mesh))
      << "a second renderer could not draw a mesh the first one already uploaded";
  QOpenGLFunctions_3_3_Core second_fn;
  second_fn.initializeOpenGLFunctions();
  GLint bound = 0;
  second_fn.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &bound);
  EXPECT_EQ(second_fn.glIsVertexArray(static_cast<GLuint>(bound)), GL_TRUE);
  second_table->unbind();
  EXPECT_EQ(first_table->size(), 1U);
  EXPECT_EQ(second_table->size(), 1U);

  second_table.reset();
  ASSERT_TRUE(first.context.makeCurrent(&first.surface));
  first_table.reset();
  Render::GL::drain_deferred_gl_deletes();
}

TEST(GlContextLifetime, ATableDropsTheCopyOfADestroyedMesh) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  Render::GL::MeshGpuTable table;
  {
    auto mesh = Render::GL::create_quad_mesh();
    ASSERT_TRUE(table.prewarm(*mesh));
    EXPECT_EQ(table.size(), 1U);
  }
  table.collect_retired();
  EXPECT_EQ(table.size(), 0U) << "the GPU copy of a destroyed mesh was kept";
}

TEST(GlContextLifetime, ATextureTableUploadsAnImageOnceAndDropsItWithTheImage) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  Render::GL::TextureTable table;
  {
    Render::GL::TextureImage image;
    image.reset(4, 4, Render::GL::TextureImage::Format::Rgba8);
    Render::GL::Texture* first = table.resolve(&image);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->get_width(), 4);
    EXPECT_EQ(table.resolve(&image), first)
        << "an unchanged image must keep its texture";

    const std::array<unsigned char, 4> red{255, 0, 0, 255};
    image.update_region({1, 1, 1, 1}, red.data());
    EXPECT_EQ(table.resolve(&image), first) << "a region update reuses the texture";

    image.reset(8, 2, Render::GL::TextureImage::Format::Rgba8);
    Render::GL::Texture* resized = table.resolve(&image);
    ASSERT_NE(resized, nullptr);
    EXPECT_EQ(resized->get_width(), 8);
    EXPECT_EQ(table.size(), 1U);
  }
  table.collect_retired();
  EXPECT_EQ(table.size(), 0U) << "the texture of a destroyed image was kept";
}

TEST(GlContextLifetime, AnInstanceTableReuploadsOnlyWhenTheBlockChanges) {
  OffscreenGl gl;
  if (!gl.ready) {
    GTEST_SKIP() << "No OpenGL 3.3 context available in this environment";
  }
  Render::GL::InstanceBufferTable table;
  {
    Render::GL::InstanceBlock block;
    EXPECT_EQ(table.resolve(&block), nullptr) << "an empty block has nothing to draw";
    block.reserve_bytes(64);
    ASSERT_TRUE(table.prewarm(block));
    Render::GL::Buffer* buffer = table.resolve(&block);
    ASSERT_NE(buffer, nullptr);
    EXPECT_GE(buffer->size_bytes(), 64U);

    const std::vector<float> values{1.0F, 2.0F, 3.0F, 4.0F};
    block.assign(values);
    EXPECT_EQ(table.resolve(&block), buffer) << "a reserved block updates in place";
    EXPECT_EQ(table.size(), 1U);
  }
  table.collect_retired();
  EXPECT_EQ(table.size(), 0U) << "the buffer of a destroyed block was kept";
}

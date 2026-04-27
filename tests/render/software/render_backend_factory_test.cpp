// Stage 17 — smoke test proving Renderer(ShaderQuality::None) dispatches to
// the SoftwareBackend via RenderBackendFactory without requiring a GL
// context, and that render_software_preview returns a non-empty image.

#include "render/gl/camera.h"
#include "render/render_backend_factory.h"
#include "render/scene_renderer.h"
#include "render/software_backend.h"

#include <QImage>
#include <gtest/gtest.h>

using Render::RenderBackendFactory;
using Render::ShaderQuality;
using Render::GL::Camera;
using Render::GL::Renderer;
using Render::GL::SoftwareBackend;

TEST(RenderBackendFactory, NoneDispatchesToSoftwareBackend) {
  auto backend = RenderBackendFactory::create(ShaderQuality::None);
  ASSERT_NE(backend, nullptr);
  EXPECT_FALSE(backend->supports_shaders());
  EXPECT_EQ(backend->shader_quality(), ShaderQuality::None);
  EXPECT_EQ(backend->frame_tracker(), nullptr);
  EXPECT_NE(dynamic_cast<SoftwareBackend *>(backend.get()), nullptr);
}

TEST(RendererSoftwarePreview, ProducesNonEmptyImage) {
  Renderer renderer(ShaderQuality::None);

  Camera cam;
  cam.set_perspective(60.0F, 1.0F, 0.1F, 100.0F);
  cam.look_at(QVector3D(0, 5, 8), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
  renderer.set_camera(&cam);

  const QImage img = renderer.render_software_preview(64, 64);
  EXPECT_EQ(img.width(), 64);
  EXPECT_EQ(img.height(), 64);
  EXPECT_FALSE(img.isNull());
}

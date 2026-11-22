#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>

#include "render/backend/backend.h"
#include "render/frame/draw_queue.h"
#include "render/gl/gl_handle.h"
#include "render/gl/gl_lifetime.h"
#include "render/gl/resources.h"
#include "render/profiling/frame_profile.h"
#include "scene/camera.h"

namespace {

using Render::GL::DeferredGlObject;

constexpr std::array k_kinds{DeferredGlObject::Buffer,
                             DeferredGlObject::VertexArray,
                             DeferredGlObject::Texture,
                             DeferredGlObject::Program,
                             DeferredGlObject::Shader,
                             DeferredGlObject::Framebuffer,
                             DeferredGlObject::Renderbuffer,
                             DeferredGlObject::Sampler,
                             DeferredGlObject::Query,
                             DeferredGlObject::Sync};

using LiveCounts = std::array<std::int64_t, k_kinds.size()>;

auto live_counts() -> LiveCounts {
  LiveCounts counts{};
  for (std::size_t i = 0; i < k_kinds.size(); ++i) {
    counts[i] = Render::GL::gl_object_census(k_kinds[i]).live();
  }
  return counts;
}

auto describe_leaks(const LiveCounts& before, const LiveCounts& after) -> std::string {
  std::ostringstream out;
  for (std::size_t i = 0; i < k_kinds.size(); ++i) {
    if (after[i] != before[i]) {
      out << "\n  " << Render::GL::gl_object_kind_name(k_kinds[i]) << ": "
          << (after[i] - before[i]) << " still live";
    }
  }
  return out.str();
}

// CI runs this file under Xvfb + Mesa with SOI_REQUIRE_GL=1, where a missing
// context is a failure rather than a skip.
auto gl_required() -> bool {
  return qEnvironmentVariableIntValue("SOI_REQUIRE_GL") != 0;
}

#define SOI_SKIP_WITHOUT_GL(reason)                                                    \
  do {                                                                                 \
    if (gl_required()) {                                                               \
      FAIL() << (reason);                                                              \
    }                                                                                  \
    GTEST_SKIP() << (reason);                                                          \
  } while (false)

struct OffscreenGl {
  QOffscreenSurface surface;
  QOpenGLContext context;
  bool ready = false;

  OffscreenGl() {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
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

void render_frames(Render::GL::Backend& backend, int frames) {
  Render::GL::Camera camera;
  camera.set_perspective(45.0F, 1.0F, 0.5F, 200.0F);
  camera.look_at(QVector3D(0.0F, 12.0F, 16.0F),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));

  Render::GL::DrawQueue queue;
  for (int frame = 0; frame < frames; ++frame) {
    queue.clear();
    Render::GL::MeshCmd mesh;
    mesh.mesh = backend.resources()->unit();
    mesh.texture = backend.resources()->white();
    mesh.model.translate(0.0F, 0.5F, 0.0F);
    queue.submit(mesh);
    Render::GL::CylinderCmd cylinder;
    cylinder.start = QVector3D(2.0F, 0.0F, 0.0F);
    cylinder.end = QVector3D(2.0F, 2.0F, 0.0F);
    cylinder.radius = 0.25F;
    queue.submit(cylinder);
    Render::GL::GridCmd grid;
    queue.submit(grid);
    queue.sort_for_batching();

    backend.begin_frame();
    backend.execute(queue, camera);
  }
}

// One full backend lifetime: initialise every pipeline, draw a few frames with
// profiling on (so timer queries and fences are made), tear down.
auto run_backend_lifetime(int frames) -> bool {
  auto backend = std::make_unique<Render::GL::Backend>();
  if (!backend->initialize()) {
    return false;
  }
  backend->set_viewport(96, 96);
  render_frames(*backend, frames);
  backend.reset();
  return true;
}

class GlOwnershipLeakTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_profiling_was_enabled = Render::Profiling::global_profile().enabled;
    Render::Profiling::global_profile().enabled = true;
  }
  void TearDown() override {
    Render::Profiling::global_profile().enabled = m_profiling_was_enabled;
  }

private:
  bool m_profiling_was_enabled{false};
};

} // namespace

TEST_F(GlOwnershipLeakTest, BackendLoadRenderReloadShutdownReleasesEveryGlObject) {
  OffscreenGl gl;
  if (!gl.ready) {
    SOI_SKIP_WITHOUT_GL("No OpenGL 3.3 context available in this environment");
  }
  Render::GL::drain_deferred_gl_deletes();
  const LiveCounts before = live_counts();

  if (!run_backend_lifetime(4)) {
    SOI_SKIP_WITHOUT_GL("Backend could not initialise here (shaders or GL features)");
  }
  ASSERT_TRUE(run_backend_lifetime(4)) << "a second backend failed after a reload";
  Render::GL::drain_deferred_gl_deletes();

  const LiveCounts after = live_counts();
  EXPECT_EQ(after, before) << "GL objects outlived the backend that made them:"
                           << describe_leaks(before, after);
  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), 0U);
}

TEST_F(GlOwnershipLeakTest, TwoBackendsCoexistAndReleaseEverythingTheyMade) {
  OffscreenGl gl;
  if (!gl.ready) {
    SOI_SKIP_WITHOUT_GL("No OpenGL 3.3 context available in this environment");
  }
  Render::GL::drain_deferred_gl_deletes();
  const LiveCounts before = live_counts();

  auto first = std::make_unique<Render::GL::Backend>();
  auto second = std::make_unique<Render::GL::Backend>();
  if (!first->initialize() || !second->initialize()) {
    SOI_SKIP_WITHOUT_GL("Backend could not initialise here (shaders or GL features)");
  }
  first->set_viewport(96, 96);
  second->set_viewport(64, 64);
  for (int frame = 0; frame < 3; ++frame) {
    render_frames(*first, 1);
    render_frames(*second, 1);
  }
  EXPECT_NE(&first->render_context().meshes(), &second->render_context().meshes());
  EXPECT_GT(first->render_context().meshes().size(), 0U);
  EXPECT_GT(second->render_context().meshes().size(), 0U);

  first.reset();
  render_frames(*second, 1);
  second.reset();
  Render::GL::drain_deferred_gl_deletes();

  const LiveCounts after = live_counts();
  EXPECT_EQ(after, before) << "GL objects outlived the two backends:"
                           << describe_leaks(before, after);
}

TEST(GlOwnership, HandleCreatedAndDestroyedOnContextIsCountedBothWays) {
  OffscreenGl gl;
  if (!gl.ready) {
    SOI_SKIP_WITHOUT_GL("No OpenGL 3.3 context available in this environment");
  }
  const auto before = Render::GL::gl_object_census(DeferredGlObject::Framebuffer);
  {
    auto fbo = Render::GL::FramebufferHandle::create();
    ASSERT_TRUE(fbo.valid());
    EXPECT_EQ(Render::GL::gl_object_census(DeferredGlObject::Framebuffer).live(),
              before.live() + 1);
    Render::GL::FramebufferHandle moved = std::move(fbo);
    EXPECT_FALSE(fbo.valid());
    EXPECT_TRUE(moved.valid());
  }
  EXPECT_EQ(Render::GL::gl_object_census(DeferredGlObject::Framebuffer).live(),
            before.live());
}

TEST(GlOwnership, HandleReleasedOffContextWaitsForTheDrain) {
  OffscreenGl gl;
  if (!gl.ready) {
    SOI_SKIP_WITHOUT_GL("No OpenGL 3.3 context available in this environment");
  }
  const auto before = Render::GL::gl_object_census(DeferredGlObject::Query);
  auto query = Render::GL::QueryHandle::create();
  auto fence = Render::GL::FenceHandle::insert();
  ASSERT_TRUE(query.valid());
  ASSERT_TRUE(fence.valid());

  gl.context.doneCurrent();
  const std::size_t queued = Render::GL::deferred_gl_delete_count();
  query.reset();
  fence.reset();
  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), queued + 2U);
  EXPECT_EQ(Render::GL::gl_object_census(DeferredGlObject::Query).live(),
            before.live() + 1);

  ASSERT_TRUE(gl.context.makeCurrent(&gl.surface));
  Render::GL::drain_deferred_gl_deletes();
  EXPECT_EQ(Render::GL::gl_object_census(DeferredGlObject::Query).live(),
            before.live());
}

TEST(GlOwnership, NamesReleasedIntoADeadShareGroupAreCountedNotQueued) {
  constexpr Render::GL::GlShareGroup k_dead = 0x7fff0001U;
  Render::GL::forget_gl_share_group(k_dead);
  const auto before = Render::GL::gl_object_census(DeferredGlObject::Texture);
  const std::size_t queued = Render::GL::deferred_gl_delete_count();

  Render::GL::defer_gl_delete(DeferredGlObject::Texture, 31337U, k_dead);

  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), queued)
      << "a name from a destroyed context was queued for ever";
  EXPECT_EQ(Render::GL::gl_object_census(DeferredGlObject::Texture).deleted,
            before.deleted + 1U);
}

#include <gtest/gtest.h>

#include "render/gl/gl_lifetime.h"

// A GL object can outlive the thread that owns the context: a match teardown
// runs on the GUI thread while the render thread holds it. Every buffer and
// vertex array freed there used to be abandoned with a warning - 3,620 of them
// in one session of loading two matches, which is a VRAM leak per match. The
// name is kept instead, and deleted by whoever next has a context.
namespace {

class GlDeferredDeleteTest : public ::testing::Test {
protected:
  void SetUp() override { drain(); }
  void TearDown() override { drain(); }

  static void drain() {
    // Without a current context this is a no-op, so clear the queue by hand.
    while (Render::GL::deferred_gl_delete_count() > 0U) {
      break;
    }
  }
};

} // namespace

TEST_F(GlDeferredDeleteTest, AnObjectFreedOffTheContextThreadIsKeptNotDropped) {
  ASSERT_FALSE(Render::GL::gl_objects_can_be_released())
      << "this test has to run without a current GL context";

  const std::size_t before = Render::GL::deferred_gl_delete_count();
  Render::GL::defer_gl_delete(Render::GL::DeferredGlObject::Buffer, 4242U);
  Render::GL::defer_gl_delete(Render::GL::DeferredGlObject::VertexArray, 77U);

  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), before + 2U)
      << "a GL name freed off the context thread was thrown away";
}

TEST_F(GlDeferredDeleteTest, DrainingWithoutAContextKeepsTheQueue) {
  const std::size_t before = Render::GL::deferred_gl_delete_count();
  Render::GL::defer_gl_delete(Render::GL::DeferredGlObject::Texture, 9001U);

  Render::GL::drain_deferred_gl_deletes();

  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), before + 1U)
      << "the queue was emptied without a context to delete into, which is the "
         "leak it exists to prevent";
}

TEST_F(GlDeferredDeleteTest, ANullNameIsNotQueued) {
  const std::size_t before = Render::GL::deferred_gl_delete_count();
  Render::GL::defer_gl_delete(Render::GL::DeferredGlObject::Buffer, 0U);
  EXPECT_EQ(Render::GL::deferred_gl_delete_count(), before);
}

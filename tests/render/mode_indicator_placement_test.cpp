#include <gtest/gtest.h>

#define private public
#include "render/scene_renderer.h"
#undef private

#include "game/core/component.h"
#include "render/draw_queue.h"
#include "render/geom/mode_indicator.h"

namespace {

TEST(ModeIndicatorPlacement, IgnoresTransformScaleForWorldHeight) {
  Render::GL::Renderer renderer;

  Engine::Core::TransformComponent transform{};
  transform.position = {1.0F, 2.0F, 3.0F};
  transform.scale = {0.5F, 0.5F, 0.5F};

  renderer.enqueue_mode_indicator(&transform, nullptr, true, false, false,
                                  false);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const &items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(
      std::holds_alternative<Render::GL::ModeIndicatorCmd>(items.front()));

  auto const &cmd = std::get<Render::GL::ModeIndicatorCmd>(items.front());
  QVector3D const translation = cmd.model.column(3).toVector3D();

  EXPECT_FLOAT_EQ(translation.x(), transform.position.x);
  EXPECT_FLOAT_EQ(translation.y(),
                  transform.position.y + Render::Geom::k_indicator_height_base);
  EXPECT_FLOAT_EQ(translation.z(), transform.position.z);
}

} // namespace

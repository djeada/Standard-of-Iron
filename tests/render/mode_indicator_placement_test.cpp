#include <gtest/gtest.h>

#define private public
#include "render/scene_renderer.h"
#undef private

#include "game/core/component.h"
#include "game/core/world.h"
#include "render/draw_queue.h"
#include "render/entity/combat_dust_renderer.h"
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

TEST(SceneRendererEffects, BloodPoolEnqueuesEffectBatchCommand) {
  Render::GL::Renderer renderer;
  QVector3D const position(1.0F, 0.02F, 3.0F);

  renderer.blood_pool(position, 0.6F, 0.75F, 0.4F, 1.2F, 0.33F);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const &items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(
      std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const &cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BloodPool);
  EXPECT_FLOAT_EQ(cmd.position.x(), position.x());
  EXPECT_FLOAT_EQ(cmd.position.y(), position.y());
  EXPECT_FLOAT_EQ(cmd.position.z(), position.z());
  EXPECT_FLOAT_EQ(cmd.radius, 0.6F);
  EXPECT_FLOAT_EQ(cmd.alpha_scale, 0.75F);
  EXPECT_FLOAT_EQ(cmd.rotation, 0.4F);
  EXPECT_FLOAT_EQ(cmd.aspect_ratio, 1.2F);
  EXPECT_FLOAT_EQ(cmd.seed, 0.33F);
}

TEST(SceneRendererEffects, BloodStainUsesTerrainAlignedHeight) {
  Render::GL::Renderer renderer;
  Engine::Core::World world;

  auto *blood_entity = world.create_entity();
  blood_entity->add_component<Engine::Core::TransformComponent>(4.0F, 1.75F,
                                                                -2.0F);
  blood_entity->add_component<Engine::Core::BloodStainComponent>(
      0.8F, 8.0F, 0.2F, 1.1F, 0.4F);

  Render::GL::render_combat_dust(&renderer, nullptr, &world);

  ASSERT_NE(renderer.m_active_queue, nullptr);
  auto const &items = renderer.m_active_queue->items();
  ASSERT_EQ(items.size(), 1U);
  ASSERT_TRUE(
      std::holds_alternative<Render::GL::EffectBatchCmd>(items.front()));

  auto const &cmd = std::get<Render::GL::EffectBatchCmd>(items.front());
  EXPECT_EQ(cmd.kind, Render::GL::EffectBatchCmd::Kind::BloodPool);
  EXPECT_FLOAT_EQ(cmd.position.x(), 4.0F);
  EXPECT_FLOAT_EQ(cmd.position.y(), 1.77F);
  EXPECT_FLOAT_EQ(cmd.position.z(), -2.0F);
}

} // namespace

#include <gtest/gtest.h>

#include "render/scene_renderer.h"

TEST(RendererVisibilityPolicyTest, ModeConfigUsesTotalVisibilityRules) {
  Render::GL::Renderer renderer;

  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_TRUE(renderer.static_world_visibility_filter_enabled());

  renderer.set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rts);
  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_TRUE(renderer.static_world_visibility_filter_enabled());

  renderer.set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rpg);
  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_FALSE(renderer.static_world_visibility_filter_enabled());
}

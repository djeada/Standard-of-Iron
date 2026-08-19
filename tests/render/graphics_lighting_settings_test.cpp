#include <gtest/gtest.h>

#include "render/graphics_settings.h"
#include "scene/camera.h"

TEST(GraphicsLightingSettingsTest, PresetsScaleDirectionalShadowCost) {
  auto& graphics = Render::GraphicsSettings::instance();

  graphics.set_quality(Render::GraphicsQuality::Low);
  EXPECT_FALSE(graphics.directional_shadows().enabled);
  EXPECT_FALSE(graphics.post_process().bloom);
  EXPECT_FALSE(graphics.post_process().fxaa);

  graphics.set_quality(Render::GraphicsQuality::Medium);
  const auto medium = graphics.directional_shadows();
  EXPECT_TRUE(medium.enabled);
  EXPECT_EQ(medium.cascade_count, 2);
  EXPECT_EQ(medium.resolution, 1024);
  EXPECT_TRUE(graphics.post_process().bloom);
  EXPECT_FALSE(graphics.post_process().godrays);

  graphics.set_quality(Render::GraphicsQuality::High);
  const auto high = graphics.directional_shadows();
  EXPECT_EQ(high.cascade_count, 4);
  EXPECT_EQ(high.resolution, 4096);
  EXPECT_GT(high.distance, medium.distance);
  EXPECT_FALSE(graphics.creature_lod_enabled());
  EXPECT_EQ(graphics.profile().shader_tier, Render::ShaderTier::High);

  graphics.set_quality(Render::GraphicsQuality::Ultra);
  const auto ultra = graphics.directional_shadows();
  EXPECT_EQ(ultra.cascade_count, high.cascade_count);
  EXPECT_EQ(ultra.resolution, high.resolution);
  EXPECT_EQ(graphics.profile().shader_tier, Render::ShaderTier::Ultra);

  EXPECT_GE(graphics.contact_shadow_budget().max_casters, 5000);

  graphics.set_quality(Render::k_default_graphics_quality);
}

TEST(GraphicsLightingSettingsTest, GenerationTicksOncePerPresetChange) {
  auto& graphics = Render::GraphicsSettings::instance();
  const auto before = graphics.generation();
  graphics.set_quality(Render::GraphicsQuality::Low);
  EXPECT_EQ(graphics.generation(), before + 1U);
  graphics.set_quality(Render::GraphicsQuality::Ultra);
  EXPECT_EQ(graphics.generation(), before + 2U);
  EXPECT_EQ(&graphics.profile(),
            &Render::graphics_profile_for(Render::GraphicsQuality::Ultra));
  graphics.set_quality(Render::k_default_graphics_quality);
}

TEST(GraphicsLightingSettingsTest, GroundingReachesTheFullyZoomedOutCamera) {
  auto& graphics = Render::GraphicsSettings::instance();

  for (const auto quality : {Render::GraphicsQuality::Medium,
                             Render::GraphicsQuality::High,
                             Render::GraphicsQuality::Ultra}) {
    graphics.set_quality(quality);
    ASSERT_GT(graphics.contact_shadow_budget().max_casters, 0);
    EXPECT_GE(graphics.shadow_max_distance(),
              Render::GL::CameraDefaults::k_max_rts_distance)
        << "contact shadows are what keep objects planted once the directional "
           "cascades have faded, so their reach has to cover the widest view the "
           "player can pull back to";
  }

  graphics.set_quality(Render::k_default_graphics_quality);
}

TEST(GraphicsLightingSettingsTest, PresetsScaleEdgeAntiAliasing) {
  auto& graphics = Render::GraphicsSettings::instance();

  graphics.set_quality(Render::GraphicsQuality::Low);
  EXPECT_EQ(graphics.presentation().msaa_samples, 0);

  graphics.set_quality(Render::GraphicsQuality::Medium);
  EXPECT_EQ(graphics.presentation().msaa_samples, 2);

  graphics.set_quality(Render::GraphicsQuality::High);
  EXPECT_EQ(graphics.presentation().msaa_samples, 4);

  graphics.set_quality(Render::GraphicsQuality::Ultra);
  EXPECT_EQ(graphics.presentation().msaa_samples, 8);

  graphics.set_quality(Render::k_default_graphics_quality);
}

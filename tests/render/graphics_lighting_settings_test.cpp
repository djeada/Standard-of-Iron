#include <gtest/gtest.h>

#include "render/graphics_settings.h"

TEST(GraphicsLightingSettingsTest, PresetsScaleDirectionalShadowCost) {
  auto& graphics = Render::GraphicsSettings::instance();

  graphics.set_quality(Render::GraphicsQuality::Low);
  EXPECT_FALSE(graphics.directional_shadows().enabled);

  graphics.set_quality(Render::GraphicsQuality::Medium);
  const auto medium = graphics.directional_shadows();
  EXPECT_TRUE(medium.enabled);
  EXPECT_EQ(medium.cascade_count, 2);
  EXPECT_EQ(medium.resolution, 1024);

  graphics.set_quality(Render::GraphicsQuality::High);
  const auto high = graphics.directional_shadows();
  EXPECT_EQ(high.cascade_count, 3);
  EXPECT_EQ(high.resolution, 2048);
  EXPECT_GT(high.distance, medium.distance);

  graphics.set_quality(Render::GraphicsQuality::Ultra);
  const auto ultra = graphics.directional_shadows();
  EXPECT_EQ(ultra.cascade_count, 4);
  EXPECT_EQ(ultra.resolution, 4096);
  EXPECT_EQ(ultra.pcf_radius, 3);
  EXPECT_EQ(graphics.contact_shadow_budget().max_casters, 100);
  EXPECT_EQ(graphics.contact_shadow_budget().max_casters_per_formation, 12);

  graphics.set_quality(Render::GraphicsQuality::High);
}

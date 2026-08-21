#include <gtest/gtest.h>

#include "render/graphics_settings.h"
#include "render/rain_gpu.h"

namespace {

auto density_at(float intensity, float quality_scale, float camera_height) -> float {
  return Render::GL::weather_particle_density({.intensity = intensity,
                                               .quality_scale = quality_scale,
                                               .camera_height = camera_height});
}

} // namespace

TEST(WeatherParticleBudgetTest, LightMediumAndHeavyProduceDistinctDensities) {
  const float light = density_at(Game::Map::k_weather_intensity_light, 1.0F, 0.0F);
  const float medium = density_at(Game::Map::k_weather_intensity_medium, 1.0F, 0.0F);
  const float heavy = density_at(Game::Map::k_weather_intensity_heavy, 1.0F, 0.0F);

  EXPECT_LT(light, medium);
  EXPECT_LT(medium, heavy);
  EXPECT_LE(heavy, 1.0F);
}

TEST(WeatherParticleBudgetTest, QualityPresetsScaleTheParticlePool) {
  auto& graphics = Render::GraphicsSettings::instance();

  graphics.set_quality(Render::GraphicsQuality::Low);
  const float low = graphics.weather_budget().particle_scale;
  graphics.set_quality(Render::GraphicsQuality::Medium);
  const float medium = graphics.weather_budget().particle_scale;
  graphics.set_quality(Render::k_default_graphics_quality);
  const float high = graphics.weather_budget().particle_scale;
  graphics.set_quality(Render::GraphicsQuality::Ultra);
  const float ultra = graphics.weather_budget().particle_scale;

  EXPECT_LT(low, medium);
  EXPECT_LT(medium, high);
  EXPECT_LE(high, ultra);
  EXPECT_FLOAT_EQ(ultra, 1.0F);

  EXPECT_LT(density_at(1.0F, low, 0.0F), density_at(1.0F, ultra, 0.0F));

  graphics.set_quality(Render::k_default_graphics_quality);
}

TEST(WeatherParticleBudgetTest, DensityThinsOutAsTheCameraPullsBack) {
  const float close = density_at(1.0F, 1.0F, Render::GL::k_weather_density_near_height);
  const float far = density_at(1.0F, 1.0F, Render::GL::k_weather_density_far_height);
  const float beyond = density_at(1.0F, 1.0F, 1000.0F);

  EXPECT_FLOAT_EQ(close, 1.0F);
  EXPECT_NEAR(far, Render::GL::k_weather_density_far_scale, 1e-5F);
  EXPECT_FLOAT_EQ(beyond, far) << "density must clamp rather than invert";
  EXPECT_GT(far, 0.0F) << "distant weather thins out but never disappears outright";
}

TEST(WeatherParticleBudgetTest, ClearWeatherSubmitsNothing) {
  EXPECT_FLOAT_EQ(density_at(0.0F, 1.0F, 0.0F), 0.0F);
}

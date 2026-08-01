#include <gtest/gtest.h>

#include "render/contact_shadow.h"

namespace {

auto morning_sun() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState environment;
  environment.primary_direction = QVector3D(0.60F, 0.30F, 0.20F);
  return environment.sanitized();
}

auto midday_sun() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState environment;
  environment.primary_direction = QVector3D(0.05F, 0.99F, 0.05F);
  return environment.sanitized();
}

auto evening_sun() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState environment;
  environment.primary_direction = QVector3D(-0.58F, 0.16F, 0.30F);
  return environment.sanitized();
}

} // namespace

TEST(ContactShadowTest, BlobFollowsTheEnvironmentSunAcrossTheDay) {
  const Render::ContactShadowInputs inputs{.camera_distance = 5.0F,
                                           .fade_distance = 60.0F};

  const auto morning = Render::contact_shadow_placement(morning_sun(), inputs);
  const auto evening = Render::contact_shadow_placement(evening_sun(), inputs);

  EXPECT_LT(morning.direction.x(), 0.0F);
  EXPECT_GT(evening.direction.x(), 0.0F);
  EXPECT_LT(QVector2D::dotProduct(morning.direction, evening.direction), 0.0F)
      << "morning and evening blobs must not point the same way";
}

TEST(ContactShadowTest, LowSunStretchesTheBlobAndHighSunGroundsIt) {
  const Render::ContactShadowInputs inputs{.camera_distance = 5.0F,
                                           .fade_distance = 60.0F};

  const auto midday = Render::contact_shadow_placement(midday_sun(), inputs);
  const auto evening = Render::contact_shadow_placement(evening_sun(), inputs);

  EXPECT_LT(midday.offset_scale, 0.2F);
  EXPECT_GT(evening.offset_scale, midday.offset_scale);
  EXPECT_LE(evening.offset_scale, Render::k_contact_shadow_max_offset_scale);
}

TEST(ContactShadowTest, OpacityFadesOutInsteadOfPoppingAtTheDistanceLimit) {
  const auto environment = morning_sun();
  constexpr float k_fade_distance = 60.0F;

  const auto near_camera = Render::contact_shadow_placement(
      environment, {.camera_distance = 10.0F, .fade_distance = k_fade_distance});
  const auto mid_fade = Render::contact_shadow_placement(
      environment,
      {.camera_distance = k_fade_distance * 0.88F, .fade_distance = k_fade_distance});
  const auto at_limit = Render::contact_shadow_placement(
      environment,
      {.camera_distance = k_fade_distance, .fade_distance = k_fade_distance});

  EXPECT_FLOAT_EQ(near_camera.opacity, 1.0F);
  EXPECT_GT(mid_fade.opacity, 0.0F);
  EXPECT_LT(mid_fade.opacity, near_camera.opacity);
  EXPECT_FLOAT_EQ(at_limit.opacity, 0.0F);
}

TEST(ContactShadowTest, DirectionalShadowsTakeOverSmoothlyNearTheCamera) {
  const auto environment = evening_sun();
  const auto make_inputs = [](float camera_distance) {
    return Render::ContactShadowInputs{.camera_distance = camera_distance,
                                       .fade_distance = 200.0F,
                                       .directional_shadows_enabled = true,
                                       .directional_distance = 80.0F};
  };

  const auto close = Render::contact_shadow_placement(environment, make_inputs(10.0F));
  const auto handing_over =
      Render::contact_shadow_placement(environment, make_inputs(64.0F));
  const auto beyond_directional =
      Render::contact_shadow_placement(environment, make_inputs(90.0F));

  EXPECT_NEAR(close.opacity, Render::k_contact_shadow_grounded_opacity, 1e-5F);
  EXPECT_GT(handing_over.opacity, close.opacity);
  EXPECT_LT(handing_over.opacity, beyond_directional.opacity);
  EXPECT_FLOAT_EQ(beyond_directional.opacity, 1.0F);

  EXPECT_LT(close.offset_scale, beyond_directional.offset_scale)
      << "a real directional shadow should pull the blob under the feet";
}

TEST(ContactShadowTest, DisabledDirectionalShadowsKeepTheBlobAtFullStrength) {
  const auto environment = evening_sun();
  const auto placement =
      Render::contact_shadow_placement(environment,
                                       {.camera_distance = 10.0F,
                                        .fade_distance = 200.0F,
                                        .directional_shadows_enabled = false,
                                        .directional_distance = 80.0F});

  EXPECT_FLOAT_EQ(placement.opacity, 1.0F);
}

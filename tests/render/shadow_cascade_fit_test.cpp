#include <QVector3D>

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <span>

#include "render/gl/shadow_cascade_fit.h"

namespace {

using Render::GL::compute_shadow_cascade_splits;
using Render::GL::fit_shadow_cascade_sphere;
using Render::GL::fit_shadow_distance_range;

auto rts_rays(float pitch_deg, float half_fov_deg = 20.0F) -> std::array<QVector3D, 5> {
  const float pitch = pitch_deg * std::numbers::pi_v<float> / 180.0F;
  const float half = half_fov_deg * std::numbers::pi_v<float> / 180.0F;
  const QVector3D forward(0.0F, -std::sin(pitch), -std::cos(pitch));
  const QVector3D up(0.0F, std::cos(pitch), -std::sin(pitch));
  const QVector3D right(1.0F, 0.0F, 0.0F);
  const float t = std::tan(half);
  std::array<QVector3D, 5> rays{};
  rays[0] = (forward - right * t - up * t).normalized();
  rays[1] = (forward + right * t - up * t).normalized();
  rays[2] = (forward - right * t + up * t).normalized();
  rays[3] = (forward + right * t + up * t).normalized();
  rays[4] = forward;
  return rays;
}

} // namespace

TEST(ShadowCascadeFit, ElevatedCameraSkipsTheEmptyAirAboveTheGround) {
  const QVector3D camera(0.0F, 40.0F, 0.0F);
  const auto rays = rts_rays(50.0F);
  const auto fit = fit_shadow_distance_range(camera, rays, 0.0F, 10.0F, 1.0F, 200.0F);

  EXPECT_GT(fit.near_distance, 20.0F);
  EXPECT_LT(fit.near_distance, 34.0F);

  EXPECT_GT(fit.far_distance, fit.near_distance + 10.0F);
  EXPECT_LT(fit.far_distance, 200.0F);
}

TEST(ShadowCascadeFit, CameraInsideTheSlabKeepsTheNearPlane) {
  const QVector3D camera(0.0F, 2.0F, 0.0F);
  const auto rays = rts_rays(5.0F);
  const auto fit = fit_shadow_distance_range(camera, rays, 0.0F, 12.0F, 0.5F, 120.0F);
  EXPECT_FLOAT_EQ(fit.near_distance, 0.5F);

  EXPECT_FLOAT_EQ(fit.far_distance, 120.0F);
}

TEST(ShadowCascadeFit, LookingAtTheSkyFallsBackToThePlainRange) {
  const QVector3D camera(0.0F, 30.0F, 0.0F);
  const auto rays = rts_rays(-30.0F);
  const auto fit = fit_shadow_distance_range(camera, rays, 0.0F, 5.0F, 1.0F, 90.0F);
  EXPECT_FLOAT_EQ(fit.near_distance, 1.0F);
  EXPECT_FLOAT_EQ(fit.far_distance, 90.0F);
}

TEST(ShadowCascadeFit, RangeIsClampedToTheRequestedBounds) {
  const QVector3D camera(0.0F, 300.0F, 0.0F);
  const auto rays = rts_rays(60.0F);
  const auto fit = fit_shadow_distance_range(camera, rays, 0.0F, 5.0F, 1.0F, 80.0F);
  EXPECT_LE(fit.near_distance, 79.0F);
  EXPECT_LE(fit.far_distance, 80.0F);
  EXPECT_GE(fit.far_distance, fit.near_distance + 1.0F);
}

TEST(ShadowCascadeFit, SplitsAreMonotonicAndEndAtTheFarDistance) {
  const auto splits = compute_shadow_cascade_splits(5.0F, 80.0F, 3);
  EXPECT_GT(splits[0], 5.0F);
  EXPECT_LT(splits[0], splits[1]);
  EXPECT_LT(splits[1], splits[2]);
  EXPECT_FLOAT_EQ(splits[2], 80.0F);
  EXPECT_FLOAT_EQ(splits[3], 80.0F);

  EXPECT_LT(splits[0], 5.0F + (75.0F / 3.0F));
}

TEST(ShadowCascadeFit, SphereContainsTheWholeShellIncludingTheEdgeBulge) {
  const QVector3D camera(3.0F, 25.0F, -4.0F);
  const auto rays = rts_rays(45.0F, 28.0F);
  const float near = 20.0F;
  const float far = 60.0F;
  const auto sphere = fit_shadow_cascade_sphere(
      camera, std::span<const QVector3D, 4>(rays.data(), 4), rays[4], near, far);

  for (float u = 0.0F; u <= 1.0F; u += 0.125F) {
    for (float v = 0.0F; v <= 1.0F; v += 0.125F) {
      const QVector3D bottom = rays[0] * (1.0F - u) + rays[1] * u;
      const QVector3D top = rays[2] * (1.0F - u) + rays[3] * u;
      const QVector3D direction = (bottom * (1.0F - v) + top * v).normalized();
      for (float distance : {near, (near + far) * 0.5F, far}) {
        const QVector3D point = camera + direction * distance;
        EXPECT_LE((point - sphere.center).length(), sphere.radius + 1e-3F)
            << "u=" << u << " v=" << v << " d=" << distance;
      }
    }
  }
}

TEST(ShadowCascadeFit, TighterShellsGetSmallerSpheres) {
  const QVector3D camera(0.0F, 30.0F, 0.0F);
  const auto rays = rts_rays(50.0F);
  const auto rays_span = std::span<const QVector3D, 4>(rays.data(), 4);
  const auto near_sphere =
      fit_shadow_cascade_sphere(camera, rays_span, rays[4], 20.0F, 30.0F);
  const auto far_sphere =
      fit_shadow_cascade_sphere(camera, rays_span, rays[4], 30.0F, 80.0F);
  EXPECT_LT(near_sphere.radius, far_sphere.radius);

  EXPECT_LT(near_sphere.radius, 16.0F);
}

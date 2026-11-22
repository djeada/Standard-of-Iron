#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <span>

#include "render/backend/shadow_cascade_fit.h"

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

namespace {

auto texel_offset_in_light_space(const Render::GL::ShadowCascadeFrame& frame,
                                 const QVector3D& world_point,
                                 int resolution) -> QVector2D {
  const QVector3D clip = frame.light_view_projection.map(world_point);
  const float texels_per_clip_unit = static_cast<float>(resolution) * 0.5F;
  return {clip.x() * texels_per_clip_unit, clip.y() * texels_per_clip_unit};
}

auto low_sun() -> QVector3D {
  return QVector3D(0.7F, 0.35F, 0.4F).normalized();
}

} // namespace

TEST(ShadowLightBasis, IsOrthonormalForAnObliqueSun) {
  const auto basis = Render::GL::shadow_light_basis(low_sun());
  EXPECT_NEAR(basis.right.length(), 1.0F, 1e-5F);
  EXPECT_NEAR(basis.up.length(), 1.0F, 1e-5F);
  EXPECT_NEAR(QVector3D::dotProduct(basis.right, basis.up), 0.0F, 1e-5F);
  EXPECT_NEAR(QVector3D::dotProduct(basis.right, basis.direction), 0.0F, 1e-5F);
  EXPECT_NEAR(QVector3D::dotProduct(basis.up, basis.direction), 0.0F, 1e-5F);
}

TEST(ShadowLightBasis, OverheadSunStillGetsAUsableBasis) {
  const auto basis = Render::GL::shadow_light_basis(QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_NEAR(basis.right.length(), 1.0F, 1e-5F);
  EXPECT_NEAR(basis.up.length(), 1.0F, 1e-5F);
  EXPECT_NEAR(QVector3D::dotProduct(basis.right, basis.up), 0.0F, 1e-5F);
}

TEST(ShadowLightFollow, HoldsTheLightThroughSubArcsecondDrift) {
  const QVector3D held = low_sun();
  const QVector3D drifted = (held + QVector3D(0.0F, 1.0e-4F, 0.0F)).normalized();
  EXPECT_EQ(Render::GL::follow_shadow_light_direction(held, drifted), held);
}

TEST(ShadowLightFollow, FollowsARealSunMoveAndAdoptsTheFirstSun) {
  const QVector3D held = low_sun();
  const QVector3D moved = (held + QVector3D(0.0F, 0.05F, 0.0F)).normalized();
  EXPECT_EQ(Render::GL::follow_shadow_light_direction(held, moved), moved);
  EXPECT_EQ(Render::GL::follow_shadow_light_direction(QVector3D(), moved), moved);
}

TEST(ShadowSunThrow, OverheadSunCastsStraightDownAndLowSunIsCapped) {
  const auto overhead = Render::GL::shadow_sun_throw(QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_FLOAT_EQ(overhead.stretch, 0.0F);
  EXPECT_FLOAT_EQ(overhead.rigged_slack, 6.0F);
  EXPECT_FLOAT_EQ(overhead.throw_scale, 1.0F);

  const auto grazing =
      Render::GL::shadow_sun_throw(QVector3D(1.0F, 0.01F, 0.0F).normalized());
  EXPECT_GT(grazing.stretch, 4.0F);
  EXPECT_FLOAT_EQ(grazing.rigged_slack, 22.0F);
  EXPECT_GT(grazing.throw_scale, grazing.stretch);
}

TEST(ShadowReceiverSlab, EmptyTerrainFallsBackToTheDefaultSlab) {
  const auto slab = Render::GL::finish_shadow_receiver_slab(
      std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
  EXPECT_FLOAT_EQ(slab.min_y, Render::GL::k_shadow_default_slab_min);
  EXPECT_FLOAT_EQ(slab.max_y,
                  Render::GL::k_shadow_default_slab_max +
                      Render::GL::k_shadow_caster_height_allowance);
}

TEST(ShadowReceiverSlab, TerrainSlabGainsHeadroomForCasters) {
  const auto slab = Render::GL::finish_shadow_receiver_slab(1.5F, 4.0F);
  EXPECT_FLOAT_EQ(slab.min_y, 1.5F);
  EXPECT_FLOAT_EQ(slab.max_y, 4.0F + Render::GL::k_shadow_caster_height_allowance);
}

TEST(ShadowViewRays, CornerRaysAreUnitLengthAndForwardIsPassedThrough) {
  QMatrix4x4 projection;
  projection.perspective(45.0F, 16.0F / 9.0F, 0.5F, 300.0F);
  QMatrix4x4 view;
  view.lookAt(QVector3D(0.0F, 30.0F, 20.0F), QVector3D(), QVector3D(0.0F, 1.0F, 0.0F));
  const QVector3D forward = QVector3D(0.0F, -30.0F, -20.0F);
  const auto rays =
      Render::GL::shadow_view_rays((projection * view).inverted(), forward);
  for (std::size_t corner = 0; corner < 4; ++corner) {
    EXPECT_NEAR(rays[corner].length(), 1.0F, 1e-4F);
    EXPECT_GT(QVector3D::dotProduct(rays[corner], forward.normalized()), 0.7F);
  }
  EXPECT_NEAR((rays[4] - forward.normalized()).length(), 0.0F, 1e-6F);
}

TEST(ShadowCascadeFrame, CoversTheSphereAndRoundsTheRadiusUp) {
  const Render::GL::ShadowCascadeSphere sphere{QVector3D(12.0F, 3.0F, -7.0F), 9.97F};
  const auto light = Render::GL::shadow_light_basis(low_sun());
  const auto frame = Render::GL::fit_shadow_cascade_frame(sphere, light, 2048);

  EXPECT_GE(frame.radius, sphere.radius);
  EXPECT_LT(frame.radius, sphere.radius + 1.0F / 16.0F + 1e-5F);
  EXPECT_FLOAT_EQ(frame.world_texel, frame.radius * 2.0F / 2048.0F);
  EXPECT_FLOAT_EQ(frame.depth_span, frame.radius * 6.0F - 0.1F);

  const QVector3D center_clip = frame.light_view_projection.map(sphere.center);
  EXPECT_LT(std::abs(center_clip.x()), 0.01F);
  EXPECT_LT(std::abs(center_clip.y()), 0.01F);
  EXPECT_GT(center_clip.z(), -1.0F);
  EXPECT_LT(center_clip.z(), 1.0F);
}

TEST(ShadowCascadeFrame, SubTexelCameraMovesShiftTheMapByWholeTexels) {
  constexpr int k_resolution = 1024;
  const auto light = Render::GL::shadow_light_basis(low_sun());
  const QVector3D anchor(3.0F, 0.0F, 5.0F);
  const Render::GL::ShadowCascadeSphere base{QVector3D(10.0F, 2.0F, 10.0F), 20.0F};
  const auto base_frame =
      Render::GL::fit_shadow_cascade_frame(base, light, k_resolution);
  const QVector2D base_texel =
      texel_offset_in_light_space(base_frame, anchor, k_resolution);

  for (float step : {0.013F, 0.21F, 0.37F, 1.9F}) {
    Render::GL::ShadowCascadeSphere moved = base;
    moved.center += light.right * step + light.up * (step * 0.5F);
    const auto frame = Render::GL::fit_shadow_cascade_frame(moved, light, k_resolution);
    const QVector2D shift =
        texel_offset_in_light_space(frame, anchor, k_resolution) - base_texel;
    EXPECT_NEAR(shift.x(), std::round(shift.x()), 0.02F) << "step " << step;
    EXPECT_NEAR(shift.y(), std::round(shift.y()), 0.02F) << "step " << step;
  }
}

TEST(ShadowCascadeCull, AcceptsCastersInsideAndRejectsThoseBesideTheCascade) {
  const auto light = Render::GL::shadow_light_basis(low_sun());
  const Render::GL::ShadowCascadeCull cull{
      .center = QVector3D(0.0F, 0.0F, 0.0F),
      .radius = 10.0F,
      .light_right = light.right,
      .light_up = light.up,
      .camera_position = QVector3D(0.0F, 0.0F, 0.0F),
      .receiver_near = 0.0F,
      .receiver_far = 30.0F,
      .receiver_margin = 0.5F,
      .ground_height = 0.0F,
      .shadow_throw = 1.0F,
  };
  EXPECT_TRUE(cull.accepts(QVector3D(2.0F, 0.0F, 2.0F), 1.0F));
  EXPECT_FALSE(cull.accepts(light.right * 25.0F, 1.0F));
  EXPECT_FALSE(cull.accepts(light.up * -25.0F, 1.0F));
}

TEST(ShadowCascadeCull, TallCastersReachReceiversThroughTheirShadowThrow) {
  const auto light = Render::GL::shadow_light_basis(low_sun());
  Render::GL::ShadowCascadeCull cull{
      .center = QVector3D(0.0F, 0.0F, 0.0F),
      .radius = 200.0F,
      .light_right = light.right,
      .light_up = light.up,
      .camera_position = QVector3D(0.0F, 0.0F, 0.0F),
      .receiver_near = 40.0F,
      .receiver_far = 80.0F,
      .receiver_margin = 0.0F,
      .ground_height = 0.0F,
      .shadow_throw = 2.0F,
  };
  const QVector3D short_caster(0.0F, 0.0F, 30.0F);
  EXPECT_FALSE(cull.accepts(short_caster, 1.0F))
      << "a low caster 30 m out cannot shade receivers that start at 40 m";

  const QVector3D tower(0.0F, 8.0F, 30.0F);
  EXPECT_TRUE(cull.accepts(tower, 1.0F))
      << "a 9 m tower throws its shadow into the 40-80 m receiver band";

  cull.shadow_throw = 0.0F;
  EXPECT_FALSE(cull.accepts(tower, 1.0F));
}

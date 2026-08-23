#include <cmath>
#include <gtest/gtest.h>

#include "render/pipeline/screen_metrics.h"

using Render::Pipeline::compute_focal_length_px;
using Render::Pipeline::k_max_apparent_size_scale;
using Render::Pipeline::k_min_apparent_size_scale;
using Render::Pipeline::k_reference_vertical_fov_degrees;
using Render::Pipeline::k_reference_viewport_height_px;
using Render::Pipeline::reference_focal_length_px;
using Render::Pipeline::ScreenMetrics;

TEST(ScreenMetrics, TheReferenceScreenScalesByOne) {
  const auto metrics =
      ScreenMetrics::from_viewport(k_reference_vertical_fov_degrees,
                                   static_cast<int>(k_reference_viewport_height_px));
  EXPECT_TRUE(metrics.known());
  EXPECT_NEAR(metrics.apparent_size_scale(), 1.0F, 1e-5F);
  EXPECT_NEAR(metrics.focal_length_px, reference_focal_length_px(), 1e-3F);
}

TEST(ScreenMetrics, DoublingTheViewportDoublesTheApparentSize) {
  const auto metrics =
      ScreenMetrics::from_viewport(k_reference_vertical_fov_degrees, 2160);
  EXPECT_NEAR(metrics.apparent_size_scale(), 2.0F, 1e-5F);
}

TEST(ScreenMetrics, ANarrowerFieldOfViewEnlargesBodies) {
  const auto wide = ScreenMetrics::from_viewport(90.0F, 1080);
  const auto narrow = ScreenMetrics::from_viewport(30.0F, 1080);
  EXPECT_LT(wide.apparent_size_scale(), 1.0F);
  EXPECT_GT(narrow.apparent_size_scale(), 1.0F);
}

TEST(ScreenMetrics, TheScaleIsClampedToASaneBand) {
  const auto tiny = ScreenMetrics::from_viewport(179.0F, 16);
  const auto huge = ScreenMetrics::from_viewport(1.0F, 32000);
  EXPECT_GE(tiny.apparent_size_scale(), k_min_apparent_size_scale);
  EXPECT_LE(huge.apparent_size_scale(), k_max_apparent_size_scale);
}

TEST(ScreenMetrics, AnUnsetViewportStaysUnknown) {
  const auto metrics = ScreenMetrics::from_viewport(45.0F, 0);
  EXPECT_FALSE(metrics.known());
  EXPECT_FLOAT_EQ(metrics.apparent_size_scale(), 1.0F);
  EXPECT_LT(metrics.projected_radius_px(10.0F, 1.0F), 0.0F);
  EXPECT_FLOAT_EQ(metrics.reference_distance(42.0F), 42.0F);
}

TEST(ScreenMetrics, ANegativeViewportIsAlsoUnknown) {
  EXPECT_FALSE(ScreenMetrics::from_viewport(45.0F, -1080).known());
  EXPECT_FLOAT_EQ(compute_focal_length_px(45.0F, -1080.0F), 0.0F);
}

TEST(ScreenMetrics, ProjectedRadiusFallsOffWithDistance) {
  const auto metrics = ScreenMetrics::from_viewport(45.0F, 1080);
  const float near_px = metrics.projected_radius_px(10.0F, 1.0F);
  const float far_px = metrics.projected_radius_px(20.0F, 1.0F);
  EXPECT_NEAR(near_px, metrics.focal_length_px * 0.1F, 1e-3F);
  EXPECT_NEAR(far_px, near_px * 0.5F, 1e-3F);
}

TEST(ScreenMetrics, AZeroRadiusBodyHasNoProjectedSize) {
  const auto metrics = ScreenMetrics::from_viewport(45.0F, 1080);
  EXPECT_LT(metrics.projected_radius_px(10.0F, 0.0F), 0.0F);
}

TEST(ScreenMetrics, ReferenceDistanceUndoesTheApparentScale) {
  const auto metrics = ScreenMetrics::from_viewport(45.0F, 2160);
  EXPECT_NEAR(metrics.apparent_size_scale(), 2.0F, 1e-5F);
  EXPECT_NEAR(metrics.reference_distance(40.0F), 20.0F, 1e-3F);
  EXPECT_NEAR(metrics.reference_distance_sq(1600.0F), 400.0F, 1e-2F);
}

TEST(ScreenMetrics, AMetreThresholdIsAPixelThreshold) {
  constexpr float k_threshold_m = 12.0F;
  constexpr float k_radius_m = 0.9F;

  const auto reference =
      ScreenMetrics::from_viewport(k_reference_vertical_fov_degrees,
                                   static_cast<int>(k_reference_viewport_height_px));
  const float threshold_px = reference.projected_radius_px(k_threshold_m, k_radius_m);

  for (const auto& metrics : {ScreenMetrics::from_viewport(45.0F, 720),
                              ScreenMetrics::from_viewport(45.0F, 1440),
                              ScreenMetrics::from_viewport(60.0F, 1080),
                              ScreenMetrics::from_viewport(30.0F, 2160)}) {

    const float boundary_m = k_threshold_m * metrics.apparent_size_scale();
    EXPECT_NEAR(metrics.projected_radius_px(boundary_m, k_radius_m),
                threshold_px,
                threshold_px * 1e-3F)
        << "focal " << metrics.focal_length_px;
    EXPECT_NEAR(metrics.reference_distance(boundary_m), k_threshold_m, 1e-2F);
  }
}

TEST(ScreenMetrics, TheSquaredDistanceOverloadAgreesWithTheDirectOne) {
  const auto metrics = ScreenMetrics::from_viewport(45.0F, 1080);
  for (const float distance : {0.0F, 1.0F, 7.5F, 40.0F, 250.0F}) {
    EXPECT_NEAR(metrics.projected_radius_px_from_distance_sq(distance * distance, 0.9F),
                metrics.projected_radius_px(distance, 0.9F),
                1e-2F)
        << "distance " << distance;
  }
  EXPECT_LT(metrics.projected_radius_px_from_distance_sq(100.0F, 0.0F), 0.0F);
  EXPECT_LT(ScreenMetrics{}.projected_radius_px_from_distance_sq(100.0F, 0.9F), 0.0F);
}

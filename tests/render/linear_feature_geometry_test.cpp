#include "render/gl/mesh.h"
#include "render/ground/linear_feature_geometry.h"
#include <gtest/gtest.h>

namespace {

TEST(LinearFeatureGeometryTest, BuildsRoadRibbonMeshWithConfiguredYOffset) {
  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.edge_noise_frequencies = {1.5F, 4.0F, 0.0F};
  settings.edge_noise_weights = {0.6F, 0.4F, 0.0F};
  settings.width_variation_scale = 0.15F;
  settings.meander_length_scale = 0.1F;
  settings.y_offset = 0.02F;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  EXPECT_FALSE(mesh->get_vertices().empty());
  EXPECT_FALSE(mesh->get_indices().empty());
  EXPECT_FLOAT_EQ(mesh->get_vertices().front().position[1], 0.02F);
}

TEST(LinearFeatureGeometryTest, BuildsRiverRibbonMeshWithMeanderSupport) {
  Render::Ground::LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.edge_noise_frequencies = {2.0F, 5.0F, 10.0F};
  settings.edge_noise_weights = {0.5F, 0.3F, 0.2F};
  settings.width_variation_scale = 0.35F;
  settings.meander_frequency = 3.0F;
  settings.meander_length_scale = 0.1F;
  settings.meander_amplitude = 0.3F;

  auto mesh = Render::Ground::build_linear_ribbon_mesh(
      {QVector3D(0.0F, 0.0F, -2.0F), QVector3D(0.0F, 0.0F, 2.0F), 3.0F}, 1.0F,
      settings);

  ASSERT_NE(mesh, nullptr);
  EXPECT_GT(mesh->get_vertices().size(), 8U);
  EXPECT_GT(mesh->get_indices().size(), 6U);
  EXPECT_FLOAT_EQ(mesh->get_vertices().front().position[1], 0.0F);
}

} // namespace

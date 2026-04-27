#include "render/ground/ground_utils.h"
#include <gtest/gtest.h>

namespace {

TEST(GroundUtilsTest, CurvatureShadingResponseIsZeroForFlatTerrain) {
  auto const response = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Flat, 0.18F, 0.32F, 0.5F, 0.1F, 0.0F);

  EXPECT_FLOAT_EQ(response.curvature_emphasis, 0.0F);
  EXPECT_FLOAT_EQ(response.ridge_response, 0.0F);
  EXPECT_FLOAT_EQ(response.gully_response, 0.0F);
}

TEST(GroundUtilsTest, CurvatureShadingResponseGrowsWithCurvature) {
  auto const subtle = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Hill, 0.015F, 0.24F, 0.35F, 0.0F, 0.0F);
  auto const pronounced = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Hill, 0.12F, 0.24F, 0.35F, 0.0F, 0.0F);

  EXPECT_GT(pronounced.curvature_emphasis, subtle.curvature_emphasis);
  EXPECT_GT(pronounced.ridge_response, subtle.ridge_response);
  EXPECT_GT(pronounced.gully_response, subtle.gully_response);
}

TEST(GroundUtilsTest, CurvatureShadingResponseSuppressesShelteredEntrances) {
  auto const exposed = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Hill, 0.10F, 0.30F, 0.55F, 0.0F, 0.0F);
  auto const sheltered = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Hill, 0.10F, 0.30F, 0.55F, 0.45F, 0.35F);

  EXPECT_LT(sheltered.curvature_emphasis, exposed.curvature_emphasis);
  EXPECT_LT(sheltered.ridge_response, exposed.ridge_response);
  EXPECT_LT(sheltered.gully_response, exposed.gully_response);
}

TEST(GroundUtilsTest, MountainsReceiveStrongerCurvatureResponseThanHills) {
  auto const hill = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Hill, 0.10F, 0.36F, 0.60F, 0.0F, 0.0F);
  auto const mountain = Render::Ground::compute_curvature_shading_response(
      Game::Map::TerrainType::Mountain, 0.10F, 0.36F, 0.60F, 0.0F, 0.0F);

  EXPECT_GT(mountain.curvature_emphasis, hill.curvature_emphasis);
  EXPECT_GT(mountain.ridge_response, hill.ridge_response);
  EXPECT_GT(mountain.gully_response, hill.gully_response);
}

} // namespace

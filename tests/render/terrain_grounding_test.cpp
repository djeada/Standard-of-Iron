#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/map/terrain_surface.h"
#include "render/entity/structure_foundation.h"
#include "render/terrain_contact.h"

namespace {

constexpr float k_eps = 1.0e-4F;

auto lcg(std::uint32_t& state) -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>(state >> 8U) / static_cast<float>(1U << 24U);
}

auto hill_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 28;
  map_def.grid.height = 28;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.ground_irregularity_enabled = false;
  map_def.biome.irregularity_amplitude = 0.0F;
  Game::Map::TerrainFeature hill{};
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.radius = 8.0F;
  hill.width = 16.0F;
  hill.depth = 16.0F;
  hill.height = 3.0F;
  map_def.terrain.push_back(hill);
  return map_def;
}

auto flat_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 24;
  map_def.grid.height = 24;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.ground_irregularity_enabled = false;
  map_def.biome.irregularity_amplitude = 0.0F;
  return map_def;
}

auto steepest_point(const Game::Map::TerrainService& terrain) -> QVector3D {
  QVector3D best(0.0F, 0.0F, 0.0F);
  float best_slope = -1.0F;
  for (float z = -9.0F; z <= 9.0F; z += 0.5F) {
    for (float x = -9.0F; x <= 9.0F; x += 0.5F) {
      float const slope = 1.0F - terrain.sample_ground_normal(x, z).y();
      if (slope > best_slope) {
        best_slope = slope;
        best = QVector3D(x, terrain.get_terrain_height(x, z), z);
      }
    }
  }
  return best;
}

} // namespace

TEST(TerrainGroundingTest, TriangulatedHeightHitsVerticesAndIsPlanarPerTriangle) {
  std::array<float, 4> const heights{0.0F, 1.0F, 3.0F, 0.5F};

  EXPECT_NEAR(
      Game::Map::sample_triangulated_height(heights.data(), 2, 2, 0, 0), 0.0F, k_eps);
  EXPECT_NEAR(
      Game::Map::sample_triangulated_height(heights.data(), 2, 2, 1, 0), 1.0F, k_eps);
  EXPECT_NEAR(
      Game::Map::sample_triangulated_height(heights.data(), 2, 2, 0, 1), 3.0F, k_eps);
  EXPECT_NEAR(
      Game::Map::sample_triangulated_height(heights.data(), 2, 2, 1, 1), 0.5F, k_eps);

  EXPECT_NEAR(Game::Map::sample_triangulated_height(heights.data(), 2, 2, 0.5F, 0.5F),
              2.0F,
              k_eps);
  float const bilinear = (0.0F + 1.0F + 3.0F + 0.5F) * 0.25F;
  EXPECT_GT(std::abs(2.0F - bilinear), 0.5F);
}

TEST(TerrainGroundingTest, SubdividedMeshQuadsLieExactlyOnTheCoarseTriangles) {

  std::uint32_t state = 91U;
  for (int trial = 0; trial < 32; ++trial) {
    std::array<float, 4> const heights{
        lcg(state) * 4.0F, lcg(state) * 4.0F, lcg(state) * 4.0F, lcg(state) * 4.0F};
    auto h = [&](float gx, float gz) {
      return Game::Map::sample_triangulated_height(heights.data(), 2, 2, gx, gz);
    };
    for (int sub_z = 0; sub_z < 2; ++sub_z) {
      for (int sub_x = 0; sub_x < 2; ++sub_x) {
        float const x0 = 0.5F * static_cast<float>(sub_x);
        float const z0 = 0.5F * static_cast<float>(sub_z);
        float const a = h(x0, z0);
        float const b = h(x0 + 0.5F, z0);
        float const c = h(x0, z0 + 0.5F);
        float const d = h(x0 + 0.5F, z0 + 0.5F);
        for (int sample = 0; sample < 16; ++sample) {
          float const u = lcg(state);
          float const v = lcg(state);
          float const drawn = (u + v <= 1.0F)
                                  ? a + (b - a) * u + (c - a) * v
                                  : d + (c - d) * (1.0F - u) + (b - d) * (1.0F - v);
          EXPECT_NEAR(drawn, h(x0 + 0.5F * u, z0 + 0.5F * v), 1.0e-4F);
        }
      }
    }
  }
}

TEST(TerrainGroundingTest, GameplayHeightIsTheDrawnSurfaceAndClampsAtTheFarEdge) {
  Game::Map::TerrainHeightMap height_map(20, 20, 1.0F);
  auto map_def = hill_map();
  map_def.terrain.front().center_x = 0.0F;
  map_def.terrain.front().center_z = 0.0F;
  height_map.build_from_features(map_def.terrain);
  const auto& data = height_map.get_height_data();

  std::uint32_t state = 7U;
  for (int i = 0; i < 200; ++i) {
    float const wx = -9.0F + lcg(state) * 18.0F;
    float const wz = -9.0F + lcg(state) * 18.0F;
    float const gx = wx + 9.5F;
    float const gz = wz + 9.5F;
    EXPECT_NEAR(height_map.get_base_height_at(wx, wz),
                Game::Map::sample_triangulated_height(data.data(), 20, 20, gx, gz),
                k_eps);
  }

  EXPECT_NEAR(height_map.get_base_height_at(9.8F, 0.5F),
              Game::Map::sample_triangulated_height(data.data(), 20, 20, 19.0F, 10.0F),
              k_eps);
}

TEST(TerrainGroundingTest, SlopeBedDepthGrowsWithSlopeAndIsCapped) {
  EXPECT_NEAR(
      Game::Map::slope_bed_depth(QVector3D(0.0F, 1.0F, 0.0F), 0.5F, 1.0F), 0.0F, k_eps);
  QVector3D const forty_five = QVector3D(1.0F, 1.0F, 0.0F).normalized();
  EXPECT_NEAR(Game::Map::slope_bed_depth(forty_five, 0.3F, 1.0F), 0.3F, 1.0e-3F);
  EXPECT_NEAR(Game::Map::slope_bed_depth(forty_five, 0.3F, 0.1F), 0.1F, k_eps);
}

TEST(TerrainGroundingTest, RoadSurfaceMatchesTheDrawnRoadEnvelope) {
  auto map_def = hill_map();
  Game::Map::RoadSegment road;
  road.start = QVector3D(-12.0F, 0.0F, 3.5F);
  road.end = QVector3D(12.0F, 0.0F, 3.5F);
  road.width = 4.0F;
  map_def.roads.push_back(road);

  Game::Map::TerrainService terrain;
  terrain.initialize(map_def);
  const auto* height_map = terrain.get_height_map();
  ASSERT_NE(height_map, nullptr);

  int road_samples = 0;
  for (float x = -10.0F; x <= 10.0F; x += 0.37F) {
    for (float z = -10.0F; z <= 10.0F; z += 0.37F) {
      auto const sample = terrain.sample_surface_height(x, z);
      if (sample.kind != Game::Map::SurfaceHeightKind::Road) {
        continue;
      }
      ++road_samples;
      float highest = height_map->get_base_height_at(x, z);
      float const radius = Game::Map::k_road_surface_envelope_tiles;
      highest = std::max(highest, height_map->get_base_height_at(x + radius, z));
      highest = std::max(highest, height_map->get_base_height_at(x - radius, z));
      highest = std::max(highest, height_map->get_base_height_at(x, z + radius));
      highest = std::max(highest, height_map->get_base_height_at(x, z - radius));
      EXPECT_NEAR(
          sample.world_y, highest + Game::Map::k_road_surface_y_offset, 1.0e-3F);
    }
  }
  EXPECT_GT(road_samples, 10);
}

TEST(TerrainGroundingTest, StructuresOnLevelGroundNeedNoFoundation) {
  Game::Map::TerrainService terrain;
  terrain.initialize(flat_map());

  QMatrix4x4 model;
  model.translate(0.0F, terrain.get_terrain_height(0.0F, 0.0F), 0.0F);
  model.scale(1.5F);
  auto const foundation = Render::GL::resolve_structure_foundation(
      terrain, Game::Units::SpawnType::Barracks, model);
  EXPECT_FLOAT_EQ(foundation.depth, 0.0F);
}

TEST(TerrainGroundingTest, StructuresOnASlopeReachDownToTheLowestGround) {
  Game::Map::TerrainService terrain;
  terrain.initialize(hill_map());
  QVector3D const site = steepest_point(terrain);
  ASSERT_LT(terrain.sample_ground_normal(site.x(), site.z()).y(), 0.97F)
      << "fixture hill is too gentle to exercise a foundation";

  QMatrix4x4 model;
  model.translate(site);
  model.rotate(30.0F, 0.0F, 1.0F, 0.0F);
  model.scale(1.2F);
  auto const foundation = Render::GL::resolve_structure_foundation(
      terrain, Game::Units::SpawnType::Home, model);
  ASSERT_GT(foundation.depth, Render::GL::k_structure_foundation_min_depth);

  float lowest = site.y();
  for (float u = -1.0F; u <= 1.0F; u += 0.125F) {
    for (float v = -1.0F; v <= 1.0F; v += 0.125F) {
      QVector3D const world =
          model.map(QVector3D(foundation.center_x + u * foundation.half_width * 0.96F,
                              0.0F,
                              foundation.center_z + v * foundation.half_depth * 0.96F));
      lowest = std::min(lowest, terrain.get_terrain_height(world.x(), world.z()));
    }
  }
  EXPECT_LE(site.y() - foundation.depth - 0.08F, lowest + 1.0e-3F);

  auto const again = Render::GL::resolve_structure_foundation(
      terrain, Game::Units::SpawnType::Home, model);
  EXPECT_FLOAT_EQ(again.depth, foundation.depth);
}

TEST(TerrainGroundingTest, PitchFollowsTheSlopeAlongTheHeading) {
  Game::Map::TerrainService terrain;
  terrain.initialize(hill_map());
  QVector3D const site = steepest_point(terrain);
  QVector3D const normal = terrain.sample_ground_normal(site.x(), site.z());
  QVector3D downhill(normal.x(), 0.0F, normal.z());
  ASSERT_GT(downhill.length(), 0.05F);
  downhill.normalize();

  float const yaw = std::atan2(downhill.x(), downhill.z()) * 180.0F / 3.14159265F;
  QMatrix4x4 model;
  model.translate(site);
  model.rotate(yaw, 0.0F, 1.0F, 0.0F);
  Render::pitch_model_to_ground(model, terrain, 75.0F);

  QVector3D const nose = model.map(QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const tail = model.map(QVector3D(0.0F, 0.0F, -1.0F));
  EXPECT_LT(nose.y(), site.y()) << "facing downhill, the nose must dip";
  EXPECT_GT(tail.y(), site.y());
  float const expected_drop = std::sqrt(1.0F - normal.y() * normal.y()) / normal.y();
  float const drop = (tail.y() - nose.y()) /
                     QVector2D(nose.x() - tail.x(), nose.z() - tail.z()).length();
  EXPECT_NEAR(drop, expected_drop, 0.02F);

  QVector3D const right = model.map(QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const left = model.map(QVector3D(-1.0F, 0.0F, 0.0F));
  EXPECT_NEAR(right.y(), left.y(), 1.0e-3F);
}

TEST(TerrainGroundingTest, TiltLaysTheUpAxisOnTheGroundWithinItsLimit) {
  QVector3D const normal = QVector3D(0.3F, 1.0F, -0.2F).normalized();
  QVector3D const up = Render::ground_tilt_rotation(normal, 45.0F)
                           .rotatedVector(QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_NEAR(QVector3D::dotProduct(up, normal), 1.0F, 1.0e-4F);

  QVector3D const limited = Render::ground_tilt_rotation(normal, 5.0F)
                                .rotatedVector(QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_NEAR(std::acos(std::clamp(limited.y(), -1.0F, 1.0F)) * 180.0F / 3.14159265F,
              5.0F,
              0.05F);

  EXPECT_TRUE(Render::ground_tilt_rotation(normal, 45.0F, 0.0F).isIdentity());
}

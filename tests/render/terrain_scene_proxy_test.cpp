#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/terrain_scene_proxy.h"
#include <gtest/gtest.h>

namespace {

class TerrainSceneProxyServiceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST(TerrainSceneProxyTest, GroupsTerrainPassesInLegacySubmissionOrder) {
  Render::GL::TerrainSurfaceManager surface;
  Render::GL::TerrainFeatureManager features;
  Render::GL::TerrainScatterManager scatter;
  Render::GL::RainRenderer rain;
  Render::GL::FogRenderer fog;

  Render::GL::TerrainSceneProxy proxy(&surface, &features, &scatter, &rain,
                                      &fog);

  const auto &passes = proxy.passes();

  ASSERT_EQ(passes.size(), 14U);
  EXPECT_EQ(proxy.surface(), &surface);
  EXPECT_EQ(proxy.ground(), surface.ground());
  EXPECT_EQ(proxy.terrain(), surface.terrain());
  EXPECT_EQ(proxy.river(), features.river());
  EXPECT_EQ(proxy.road(), features.road());
  EXPECT_EQ(proxy.riverbank(), features.riverbank());
  EXPECT_EQ(proxy.bridge(), features.bridge());
  EXPECT_NE(proxy.biome(), nullptr);
  EXPECT_NE(proxy.stone(), nullptr);
  EXPECT_NE(proxy.plant(), nullptr);
  EXPECT_NE(proxy.pine(), nullptr);
  EXPECT_NE(proxy.olive(), nullptr);
  EXPECT_NE(proxy.firecamp(), nullptr);
  EXPECT_EQ(proxy.rain(), &rain);
  EXPECT_EQ(proxy.fog(), &fog);
  auto river = proxy.river();
  auto road = proxy.road();
  auto riverbank = proxy.riverbank();
  auto bridge = proxy.bridge();

  EXPECT_EQ(passes[0],
            static_cast<Render::GL::IRenderPass *>(surface.ground()));
  EXPECT_EQ(passes[1],
            static_cast<Render::GL::IRenderPass *>(surface.terrain()));
  EXPECT_EQ(passes[2], static_cast<Render::GL::IRenderPass *>(river));
  EXPECT_EQ(passes[3], static_cast<Render::GL::IRenderPass *>(road));
  EXPECT_EQ(passes[4], static_cast<Render::GL::IRenderPass *>(riverbank));
  EXPECT_EQ(passes[5], static_cast<Render::GL::IRenderPass *>(bridge));
  EXPECT_EQ(passes[6], static_cast<Render::GL::IRenderPass *>(proxy.biome()));
  EXPECT_EQ(passes[7], static_cast<Render::GL::IRenderPass *>(proxy.stone()));
  EXPECT_EQ(passes[8], static_cast<Render::GL::IRenderPass *>(proxy.plant()));
  EXPECT_EQ(passes[9], static_cast<Render::GL::IRenderPass *>(proxy.pine()));
  EXPECT_EQ(passes[10], static_cast<Render::GL::IRenderPass *>(proxy.olive()));
  EXPECT_EQ(passes[11],
            static_cast<Render::GL::IRenderPass *>(proxy.firecamp()));
  EXPECT_EQ(passes[12], &rain);
  EXPECT_EQ(passes[13], &fog);
}

TEST_F(TerrainSceneProxyServiceTest, ExposesTerrainFieldAndRoadSegments) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 1.5F});
  map_def.roads.push_back(
      {QVector3D(-1.0F, 0.0F, -1.0F), QVector3D(1.0F, 0.0F, 1.0F), 2.5F});
  map_def.bridges.push_back(
      {QVector3D(-0.5F, 0.0F, 0.0F), QVector3D(0.5F, 0.0F, 0.0F), 2.0F, 0.4F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Render::GL::TerrainSurfaceManager surface;
  Render::GL::TerrainFeatureManager features;
  Render::GL::TerrainScatterManager scatter;
  Render::GL::TerrainSceneProxy proxy(&surface, &features, &scatter, nullptr,
                                      nullptr);

  ASSERT_TRUE(proxy.has_field());
  const auto &field = proxy.field();
  EXPECT_EQ(field.width, 8);
  EXPECT_EQ(field.height, 8);
  ASSERT_EQ(proxy.road_segments().size(), 1U);
  EXPECT_FLOAT_EQ(proxy.road_segments().front().width, 2.5F);

  const auto surfaces = proxy.surface_chunks();
  ASSERT_EQ(surfaces.size(), 2U);
  EXPECT_EQ(surfaces[0].kind, Render::GL::TerrainSurfaceKind::GroundPlane);
  EXPECT_EQ(surfaces[0].pass,
            static_cast<Render::GL::IRenderPass *>(proxy.ground()));
  EXPECT_TRUE(surfaces[0].params.is_ground_plane);
  EXPECT_EQ(surfaces[0].params.field, &field);
  EXPECT_NE(surfaces[0].params.biome_settings, nullptr);
  EXPECT_EQ(surfaces[1].kind, Render::GL::TerrainSurfaceKind::TerrainMesh);
  EXPECT_EQ(surfaces[1].pass,
            static_cast<Render::GL::IRenderPass *>(proxy.terrain()));
  EXPECT_FALSE(surfaces[1].params.is_ground_plane);
  EXPECT_EQ(surfaces[1].params.field, &field);

  const auto feature_chunks = proxy.feature_chunks();
  ASSERT_EQ(feature_chunks.size(), 4U);
  EXPECT_EQ(feature_chunks[0].kind, Render::GL::LinearFeatureKind::River);
  EXPECT_EQ(feature_chunks[0].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::SegmentSampled);
  EXPECT_EQ(feature_chunks[0].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[1].kind, Render::GL::LinearFeatureKind::Road);
  EXPECT_EQ(feature_chunks[1].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[2].kind, Render::GL::LinearFeatureKind::Riverbank);
  EXPECT_EQ(feature_chunks[2].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::TextureDriven);
  EXPECT_EQ(feature_chunks[2].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[3].kind, Render::GL::LinearFeatureKind::Bridge);
  EXPECT_EQ(feature_chunks[3].geometry_count, 1U);

  const auto scatters = proxy.scatter_chunks();
  ASSERT_EQ(scatters.size(), 6U);
  EXPECT_EQ(scatters[0].species, Render::GL::ScatterSpeciesId::Grass);
  EXPECT_EQ(scatters[0].visibility_mode,
            Render::GL::ScatterVisibilityMode::None);
  EXPECT_EQ(scatters[0].instance_count, 0U);
  EXPECT_TRUE(scatters[0].gpu_ready);
  EXPECT_EQ(scatters[1].species, Render::GL::ScatterSpeciesId::Stone);
  EXPECT_EQ(scatters[2].species, Render::GL::ScatterSpeciesId::Plant);
  EXPECT_EQ(scatters[2].visibility_mode,
            Render::GL::ScatterVisibilityMode::InstanceFiltered);
  EXPECT_EQ(scatters[3].species, Render::GL::ScatterSpeciesId::Pine);
  EXPECT_EQ(scatters[4].species, Render::GL::ScatterSpeciesId::Olive);
  EXPECT_EQ(scatters[5].species, Render::GL::ScatterSpeciesId::FireCamp);
  EXPECT_TRUE(scatters[5].gpu_ready);
}

} // namespace

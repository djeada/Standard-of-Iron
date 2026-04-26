#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/terrain_scene_proxy.h"
#include <gtest/gtest.h>

namespace {

class TerrainSceneProxyServiceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST(TerrainSceneProxyTest, GroupsTerrainPassesInLegacySubmissionOrder) {
  Render::GL::GroundRenderer ground;
  Render::GL::TerrainRenderer terrain;
  Render::GL::RiverRenderer river;
  Render::GL::RoadRenderer road;
  Render::GL::RiverbankRenderer riverbank;
  Render::GL::BridgeRenderer bridge;
  Render::GL::BiomeRenderer biome;
  Render::GL::StoneRenderer stone;
  Render::GL::PlantRenderer plant;
  Render::GL::PineRenderer pine;
  Render::GL::OliveRenderer olive;
  Render::GL::FireCampRenderer firecamp;
  Render::GL::RainRenderer rain;
  Render::GL::FogRenderer fog;

  Render::GL::TerrainSceneProxy proxy(
      &ground, &terrain, &river, &road, &riverbank, &bridge, &biome, &stone,
      &plant, &pine, &olive, &firecamp, &rain, &fog);

  const auto &passes = proxy.passes();

  ASSERT_EQ(passes.size(), 14U);
  EXPECT_EQ(proxy.ground(), &ground);
  EXPECT_EQ(proxy.terrain(), &terrain);
  EXPECT_EQ(proxy.river(), &river);
  EXPECT_EQ(proxy.road(), &road);
  EXPECT_EQ(proxy.riverbank(), &riverbank);
  EXPECT_EQ(proxy.bridge(), &bridge);
  EXPECT_EQ(proxy.biome(), &biome);
  EXPECT_EQ(proxy.stone(), &stone);
  EXPECT_EQ(proxy.plant(), &plant);
  EXPECT_EQ(proxy.pine(), &pine);
  EXPECT_EQ(proxy.olive(), &olive);
  EXPECT_EQ(proxy.firecamp(), &firecamp);
  EXPECT_EQ(proxy.rain(), &rain);
  EXPECT_EQ(proxy.fog(), &fog);

  EXPECT_EQ(passes[0], &ground);
  EXPECT_EQ(passes[1], &terrain);
  EXPECT_EQ(passes[2], &river);
  EXPECT_EQ(passes[3], &road);
  EXPECT_EQ(passes[4], &riverbank);
  EXPECT_EQ(passes[5], &bridge);
  EXPECT_EQ(passes[6], &biome);
  EXPECT_EQ(passes[7], &stone);
  EXPECT_EQ(passes[8], &plant);
  EXPECT_EQ(passes[9], &pine);
  EXPECT_EQ(passes[10], &olive);
  EXPECT_EQ(passes[11], &firecamp);
  EXPECT_EQ(passes[12], &rain);
  EXPECT_EQ(passes[13], &fog);
}

TEST_F(TerrainSceneProxyServiceTest, ExposesTerrainFieldAndRoadSegments) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back({QVector3D(-2.0F, 0.0F, 0.0F),
                            QVector3D(2.0F, 0.0F, 0.0F), 1.5F});
  map_def.roads.push_back({QVector3D(-1.0F, 0.0F, -1.0F),
                           QVector3D(1.0F, 0.0F, 1.0F), 2.5F});
  map_def.bridges.push_back({QVector3D(-0.5F, 0.0F, 0.0F),
                             QVector3D(0.5F, 0.0F, 0.0F), 2.0F, 0.4F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Render::GL::TerrainSceneProxy proxy(nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr, nullptr, nullptr,
                                      nullptr, nullptr);

  ASSERT_TRUE(proxy.has_field());
  const auto &field = proxy.field();
  EXPECT_EQ(field.width, 8);
  EXPECT_EQ(field.height, 8);
  ASSERT_EQ(proxy.road_segments().size(), 1U);
  EXPECT_FLOAT_EQ(proxy.road_segments().front().width, 2.5F);

  const auto surfaces = proxy.surface_chunks();
  ASSERT_EQ(surfaces.size(), 2U);
  EXPECT_EQ(surfaces[0].kind, Render::GL::TerrainSurfaceKind::GroundPlane);
  EXPECT_EQ(surfaces[0].pass, proxy.ground());
  EXPECT_TRUE(surfaces[0].params.is_ground_plane);
  EXPECT_EQ(surfaces[0].params.field, &field);
  EXPECT_NE(surfaces[0].params.biome_settings, nullptr);
  EXPECT_EQ(surfaces[1].kind, Render::GL::TerrainSurfaceKind::TerrainMesh);
  EXPECT_EQ(surfaces[1].pass, proxy.terrain());
  EXPECT_FALSE(surfaces[1].params.is_ground_plane);
  EXPECT_EQ(surfaces[1].params.field, &field);

  const auto features = proxy.feature_chunks();
  ASSERT_EQ(features.size(), 4U);
  EXPECT_EQ(features[0].kind, Render::GL::LinearFeatureKind::River);
  EXPECT_EQ(features[0].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::SegmentSampled);
  EXPECT_EQ(features[0].geometry_count, 1U);
  EXPECT_EQ(features[1].kind, Render::GL::LinearFeatureKind::Road);
  EXPECT_EQ(features[1].geometry_count, 1U);
  EXPECT_EQ(features[2].kind, Render::GL::LinearFeatureKind::Riverbank);
  EXPECT_EQ(features[2].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::TextureDriven);
  EXPECT_EQ(features[2].geometry_count, 1U);
  EXPECT_EQ(features[3].kind, Render::GL::LinearFeatureKind::Bridge);
  EXPECT_EQ(features[3].geometry_count, 1U);
}

} // namespace

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

} // namespace

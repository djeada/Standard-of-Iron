#include <algorithm>
#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "render/ground/cursed_gold_vein_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene/terrain_scene_proxy.h"

namespace {

class TerrainSceneProxyServiceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST(TerrainSceneProxyTest, BridgesSubmitBeforeTheFogSheetTheyDissolveWith) {
  Render::GL::TerrainSurfaceManager surface;
  Render::GL::TerrainFeatureManager features;
  Render::GL::TerrainScatterManager scatter;
  Render::GL::RainRenderer rain;
  Render::GL::FogRenderer fog;
  Render::GL::MapBoundaryFogRenderer boundary_fog;

  Render::GL::TerrainSceneProxy const proxy(
      &surface, &features, &scatter, &rain, &fog, &boundary_fog);

  const auto& sources = proxy.sources();
  const auto bridge_at =
      std::find(sources.begin(),
                sources.end(),
                static_cast<Render::GL::IDrawSource*>(proxy.bridge()));
  const auto fog_at = std::find(
      sources.begin(), sources.end(), static_cast<Render::GL::IDrawSource*>(&fog));

  ASSERT_NE(bridge_at, sources.end());
  ASSERT_NE(fog_at, sources.end());
  EXPECT_LT(bridge_at - sources.begin(), fog_at - sources.begin())
      << "the bridge source pulls the fog mask forward; if the fog source ran first the "
         "bridge would dissolve against a frame-old mask";
}

TEST(TerrainSceneProxyTest, GroupsTerrainPassesInLegacySubmissionOrder) {
  Render::GL::TerrainSurfaceManager surface;
  Render::GL::TerrainFeatureManager features;
  Render::GL::TerrainScatterManager scatter;
  Render::GL::RainRenderer rain;
  Render::GL::FogRenderer fog;
  Render::GL::MapBoundaryFogRenderer boundary_fog;

  Render::GL::TerrainSceneProxy const proxy(
      &surface, &features, &scatter, &rain, &fog, &boundary_fog);

  const auto& sources = proxy.sources();

  ASSERT_EQ(sources.size(), 29U);
  EXPECT_EQ(proxy.surface(), &surface);
  EXPECT_EQ(proxy.ground(), surface.ground());
  EXPECT_EQ(proxy.terrain(), surface.terrain());
  EXPECT_EQ(proxy.water(), features.water());
  EXPECT_EQ(proxy.road(), features.road());
  EXPECT_EQ(proxy.shoreline(), features.shoreline());
  EXPECT_EQ(proxy.bridge(), features.bridge());
  EXPECT_NE(proxy.biome(), nullptr);
  EXPECT_NE(proxy.stone(), nullptr);
  EXPECT_NE(proxy.plant(), nullptr);
  EXPECT_NE(proxy.tree(Game::Map::TreeSpecies::Pine), nullptr);
  EXPECT_NE(proxy.tree(Game::Map::TreeSpecies::Olive), nullptr);
  EXPECT_NE(proxy.tree(Game::Map::TreeSpecies::Cypress), nullptr);
  EXPECT_NE(proxy.tree(Game::Map::TreeSpecies::Palm), nullptr);
  EXPECT_NE(proxy.firecamp(), nullptr);
  EXPECT_EQ(proxy.rain(), &rain);
  EXPECT_EQ(proxy.fog(), &fog);
  EXPECT_EQ(proxy.boundary_fog(), &boundary_fog);
  auto* water = proxy.water();
  auto* road = proxy.road();
  auto* shoreline = proxy.shoreline();
  auto* bridge = proxy.bridge();

  EXPECT_EQ(sources[0], static_cast<Render::GL::IDrawSource*>(surface.ground()));
  EXPECT_EQ(sources[1], static_cast<Render::GL::IDrawSource*>(surface.terrain()));
  EXPECT_EQ(sources[2], static_cast<Render::GL::IDrawSource*>(water));
  EXPECT_EQ(sources[3], static_cast<Render::GL::IDrawSource*>(road));
  EXPECT_EQ(sources[4], static_cast<Render::GL::IDrawSource*>(shoreline));
  EXPECT_EQ(sources[5], static_cast<Render::GL::IDrawSource*>(bridge));
  EXPECT_EQ(sources[6], static_cast<Render::GL::IDrawSource*>(proxy.biome()));
  EXPECT_EQ(sources[7], static_cast<Render::GL::IDrawSource*>(proxy.stone()));
  EXPECT_EQ(sources[8], static_cast<Render::GL::IDrawSource*>(proxy.plant()));
  EXPECT_EQ(
      sources[9],
      static_cast<Render::GL::IDrawSource*>(proxy.tree(Game::Map::TreeSpecies::Pine)));
  EXPECT_EQ(
      sources[10],
      static_cast<Render::GL::IDrawSource*>(proxy.tree(Game::Map::TreeSpecies::Olive)));
  EXPECT_EQ(sources[11],
            static_cast<Render::GL::IDrawSource*>(
                proxy.tree(Game::Map::TreeSpecies::Cypress)));
  EXPECT_EQ(
      sources[12],
      static_cast<Render::GL::IDrawSource*>(proxy.tree(Game::Map::TreeSpecies::Palm)));
  EXPECT_EQ(sources[13], static_cast<Render::GL::IDrawSource*>(proxy.firecamp()));
  for (std::size_t i = 14; i <= 23; ++i) {
    EXPECT_NE(sources[i], nullptr);
  }
  EXPECT_EQ(sources[24],
            static_cast<Render::GL::IDrawSource*>(scatter.cursed_gold_vein()));
  EXPECT_EQ(sources[25], &rain);
  EXPECT_EQ(sources[26], &fog);
  EXPECT_EQ(sources[27], &boundary_fog);
  EXPECT_EQ(sources[28], nullptr);
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
  Render::GL::TerrainSceneProxy const proxy(
      &surface, &features, &scatter, nullptr, nullptr, nullptr);

  proxy.set_world_view(Render::WorldView::of(Game::Session::SessionContext::active()));

  ASSERT_TRUE(proxy.has_field());
  const auto& field = proxy.field();
  EXPECT_EQ(field.width, 8);
  EXPECT_EQ(field.height, 8);
  ASSERT_EQ(proxy.road_segments().size(), 1U);
  EXPECT_FLOAT_EQ(proxy.road_segments().front().width, 2.5F);

  const auto surfaces = proxy.surface_chunks();
  ASSERT_EQ(surfaces.size(), 2U);
  EXPECT_EQ(surfaces[0].kind, Render::GL::TerrainSurfaceKind::GroundPlane);
  EXPECT_EQ(surfaces[0].source, static_cast<Render::GL::IDrawSource*>(proxy.ground()));
  EXPECT_TRUE(surfaces[0].params.is_ground_plane);
  EXPECT_EQ(surfaces[0].params.field, &field);
  EXPECT_NE(surfaces[0].params.biome_settings, nullptr);
  EXPECT_EQ(surfaces[1].kind, Render::GL::TerrainSurfaceKind::TerrainMesh);
  EXPECT_EQ(surfaces[1].source, static_cast<Render::GL::IDrawSource*>(proxy.terrain()));
  EXPECT_FALSE(surfaces[1].params.is_ground_plane);
  EXPECT_EQ(surfaces[1].params.field, &field);

  const auto feature_chunks = proxy.feature_chunks();
  ASSERT_EQ(feature_chunks.size(), 4U);
  EXPECT_EQ(feature_chunks[0].kind, Render::GL::LinearFeatureKind::Water);
  EXPECT_EQ(feature_chunks[0].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::SegmentSampled);
  EXPECT_EQ(feature_chunks[0].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[1].kind, Render::GL::LinearFeatureKind::Road);
  EXPECT_EQ(feature_chunks[1].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[2].kind, Render::GL::LinearFeatureKind::Shoreline);
  EXPECT_EQ(feature_chunks[2].visibility_mode,
            Render::GL::LinearFeatureVisibilityMode::TextureDriven);
  EXPECT_EQ(feature_chunks[2].geometry_count, 1U);
  EXPECT_EQ(feature_chunks[3].kind, Render::GL::LinearFeatureKind::Bridge);
  EXPECT_EQ(feature_chunks[3].geometry_count, 1U);

  const auto scatters = proxy.scatter_chunks();
  ASSERT_EQ(scatters.size(), 19U);
  EXPECT_EQ(scatters[0].species, Render::GL::ScatterSpeciesId::Grass);
  EXPECT_EQ(scatters[0].visibility_mode,
            Render::GL::ScatterVisibilityMode::InstanceFiltered);
  EXPECT_EQ(scatters[0].instance_count, 0U);
  EXPECT_TRUE(scatters[0].gpu_ready);
  EXPECT_FALSE(scatters[0].last_sync_stats.did_upload_or_rebuild());
  EXPECT_EQ(scatters[1].species, Render::GL::ScatterSpeciesId::Stone);
  EXPECT_EQ(scatters[1].visibility_mode,
            Render::GL::ScatterVisibilityMode::InstanceFiltered);
  EXPECT_EQ(scatters[2].species, Render::GL::ScatterSpeciesId::Plant);
  EXPECT_EQ(scatters[2].visibility_mode,
            Render::GL::ScatterVisibilityMode::InstanceFiltered);
  EXPECT_EQ(scatters[3].species, Render::GL::ScatterSpeciesId::Pine);
  EXPECT_EQ(scatters[4].species, Render::GL::ScatterSpeciesId::Olive);
  EXPECT_EQ(scatters[5].species, Render::GL::ScatterSpeciesId::Cypress);
  EXPECT_EQ(scatters[6].species, Render::GL::ScatterSpeciesId::Palm);
  EXPECT_EQ(scatters[7].species, Render::GL::ScatterSpeciesId::FireCamp);
  EXPECT_TRUE(scatters[7].gpu_ready);
  EXPECT_EQ(scatters[8].species, Render::GL::ScatterSpeciesId::Tent);
  EXPECT_TRUE(scatters[8].gpu_ready);
  EXPECT_EQ(scatters[9].species, Render::GL::ScatterSpeciesId::SupplyCart);
  EXPECT_TRUE(scatters[9].gpu_ready);
  EXPECT_EQ(scatters[10].species, Render::GL::ScatterSpeciesId::WeaponRack);
  EXPECT_TRUE(scatters[10].gpu_ready);
  EXPECT_EQ(scatters[11].species, Render::GL::ScatterSpeciesId::Ruins);
  EXPECT_TRUE(scatters[11].gpu_ready);
  EXPECT_EQ(scatters[12].species, Render::GL::ScatterSpeciesId::DeadTree);
  EXPECT_TRUE(scatters[12].gpu_ready);
  EXPECT_EQ(scatters[13].species, Render::GL::ScatterSpeciesId::Boulder);
  EXPECT_TRUE(scatters[13].gpu_ready);
  EXPECT_EQ(scatters[14].species, Render::GL::ScatterSpeciesId::IronOre);
  EXPECT_TRUE(scatters[14].gpu_ready);
  EXPECT_EQ(scatters[15].species, Render::GL::ScatterSpeciesId::MagicShrine);
  EXPECT_TRUE(scatters[15].gpu_ready);
  EXPECT_EQ(scatters[16].species, Render::GL::ScatterSpeciesId::AbandonedHome);
  EXPECT_TRUE(scatters[16].gpu_ready);
  EXPECT_EQ(scatters[17].species, Render::GL::ScatterSpeciesId::Statue);
  EXPECT_TRUE(scatters[17].gpu_ready);
  EXPECT_EQ(scatters[18].species, Render::GL::ScatterSpeciesId::CursedGoldVein);
  EXPECT_TRUE(scatters[18].gpu_ready);
  EXPECT_FALSE(scatter.last_sync_stats().did_upload_or_rebuild());
}

} // namespace

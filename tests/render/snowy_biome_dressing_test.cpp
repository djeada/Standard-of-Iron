#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_render_common.h"
#include "render/entity/nations/carthage/barracks_renderer.h"
#include "render/entity/registry.h"
#include "render/ground/biome_renderer.h"
#include "render/world_view.h"

namespace {

using Render::GL::BuildingArchetypeDesc;
using Render::GL::BuildingState;

const QVector3D k_palm_frond{0.30F, 0.46F, 0.20F};
const QVector3D k_palm_frond_dark{0.20F, 0.34F, 0.14F};
const QVector3D k_fir_snow_cap{0.86F, 0.89F, 0.94F};

auto has_part_coloured(const BuildingArchetypeDesc& desc,
                       const QVector3D& color) -> bool {
  return std::any_of(desc.parts().begin(), desc.parts().end(), [&](const auto& part) {
    return (part.color - color).length() < 1.0e-4F;
  });
}

auto has_palm_fronds(const BuildingArchetypeDesc& desc) -> bool {
  return has_part_coloured(desc, k_palm_frond) ||
         has_part_coloured(desc, k_palm_frond_dark);
}

auto map_with_biome(const Game::Map::BiomeSettings& biome) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.grid.width = map.grid.height = 16;
  map.grid.tile_size = 1;
  map.biome = biome;
  return map;
}

TEST(SnowyBiomeDressingTest, CarthageBarracksKeepsItsGreeneryOutOfTheBuilding) {
  for (const auto state :
       {BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed}) {
    EXPECT_FALSE(has_palm_fronds(Render::GL::Carthage::build_barracks_desc(state)))
        << "palms baked into the barracks would stand on every biome";
  }
  const auto palms =
      Render::GL::Carthage::build_barracks_palms_desc(BuildingState::Normal);
  const auto firs =
      Render::GL::Carthage::build_barracks_firs_desc(BuildingState::Normal);
  EXPECT_TRUE(has_palm_fronds(palms));
  EXPECT_FALSE(has_palm_fronds(firs));
  EXPECT_TRUE(has_part_coloured(firs, k_fir_snow_cap));

  bool palms_listed = false;
  bool firs_listed = false;
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    palms_listed = palms_listed || entry.name == "carthage_barracks_palms";
    firs_listed = firs_listed || entry.name == "carthage_barracks_firs";
  }
  EXPECT_TRUE(palms_listed);
  EXPECT_TRUE(firs_listed);
}

TEST(SnowyBiomeDressingTest, GreeneryTurnsColdOnSnowAndAlpineGround) {
  Render::GL::DrawContext ctx;
  EXPECT_FALSE(Render::GL::building_greenery_is_cold(ctx));

  Game::Session::SessionContext temperate;
  temperate.terrain().initialize(map_with_biome(Game::Map::BiomeSettings{}));
  ctx.world_view = Render::WorldView::of(temperate);
  EXPECT_FALSE(Render::GL::building_greenery_is_cold(ctx));

  Game::Map::BiomeSettings snowy;
  snowy.snow_coverage = 0.82F;
  Game::Session::SessionContext frostmere;
  frostmere.terrain().initialize(map_with_biome(snowy));
  ctx.world_view = Render::WorldView::of(frostmere);
  EXPECT_TRUE(Render::GL::building_greenery_is_cold(ctx));

  Game::Map::BiomeSettings alpine;
  Game::Map::apply_ground_type_defaults(alpine, Game::Map::GroundType::AlpineMix);
  alpine.snow_coverage = 0.0F;
  Game::Session::SessionContext alps;
  alps.terrain().initialize(map_with_biome(alpine));
  ctx.world_view = Render::WorldView::of(alps);
  EXPECT_TRUE(Render::GL::building_greenery_is_cold(ctx));
}

TEST(SnowyBiomeDressingTest, SnowThinsTheGrassBlades) {
  Game::Map::TerrainHeightMap const height_map(64, 64, 1.0F);
  auto const blades_for = [&](float snow_coverage) {
    Game::Map::BiomeSettings biome;
    biome.snow_coverage = snow_coverage;
    Render::GL::BiomeRenderer renderer;
    renderer.set_world_view(
        Render::WorldView::of(Game::Session::SessionContext::active()));
    renderer.configure(height_map, biome);
    return renderer.instance_count();
  };

  const auto bare = blades_for(0.0F);
  const auto snowed = blades_for(0.82F);
  ASSERT_GT(bare, 0U);
  EXPECT_LT(snowed, bare / 2U) << "green blades carpet a snowfield";
}

} // namespace

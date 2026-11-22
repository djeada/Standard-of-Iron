#include "render/ground/tree_renderer.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/scatter_world.h"
#include "game/map/scatter/spawn_validator.h"
#include "game/map/scatter/tree_scatter_walk.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_resources.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/terrain_contact.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/math/render_constants.h"

namespace {

auto draw_species(Game::Map::TreeSpecies species)
    -> Render::GL::TerrainScatterCmd::Species {
  using S = Render::GL::TerrainScatterCmd::Species;
  switch (species) {
  case Game::Map::TreeSpecies::Pine:
    return S::Pine;
  case Game::Map::TreeSpecies::Olive:
    return S::Olive;
  case Game::Map::TreeSpecies::Cypress:
    return S::Cypress;
  case Game::Map::TreeSpecies::Palm:
    return S::Palm;
  }
  return S::Pine;
}

} // namespace

namespace Render::GL {

namespace {

auto anchored_of_species(const std::vector<AnchoredTree>& trees,
                         Game::Map::TreeSpecies species) -> std::vector<AnchoredTree> {
  std::vector<AnchoredTree> own;
  for (const AnchoredTree& tree : trees) {
    if (tree.species == species) {
      own.push_back(tree);
    }
  }
  return own;
}

} // namespace

TreeRenderer::TreeRenderer(Game::Map::TreeSpecies species)
    : m_species(species)
    , m_profile(&Render::Ground::tree_scatter_profile(species)) {
}

TreeRenderer::~TreeRenderer() = default;

void TreeRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                             const Game::Map::BiomeSettings& biome_settings,
                             const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  auto& params = m_state.params;
  params.light_direction = m_light_direction;
  params.time = 0.0F;
  params.wind_strength = wind_profile.sway_strength;
  params.wind_speed = wind_profile.sway_speed;
  params.snow_coverage = std::clamp(
      Game::Map::make_climate_profile(m_biome_settings).snow_coverage, 0.0F, 1.0F);

  m_ecology = Render::Ground::shared_ground_ecology(
      world().terrain(), height_map, m_noise_seed);

  rebuild_instances();
}

void TreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_instances();
}

void TreeRenderer::rebuild_instances() {
  adopt_generated(Ground::Scatter::generate_trees({scatter_grid(),
                                                   m_ecology.get(),
                                                   m_biome_settings,
                                                   world().terrain_or_empty(),
                                                   m_world_props},
                                                  m_species,
                                                  m_anchored));
}

void TreeRenderer::assign_anchored_trees(const std::vector<AnchoredTree>& trees) {
  m_anchored = anchored_of_species(trees, m_species);
}

void TreeRenderer::set_anchored_trees(const std::vector<AnchoredTree>& trees) {
  assign_anchored_trees(trees);
  rebuild_instances();
}

void TreeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, QVector3D(0.35F, 0.8F, 0.45F));
}

void TreeRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  submit_filtered_common<true>(
      renderer,
      resources,
      draw_species(m_species),
      [](TerrainScatterCmd& cmd, const FoliageBatchParams& params) {
        cmd.foliage = params;
      });
}

auto TreeRenderer::plan_rebuild(const std::vector<Game::Map::WorldProp>* world_props,
                                const std::vector<AnchoredTree>* anchored,
                                std::shared_ptr<const Game::Map::ScatterWorld> world)
    -> Ground::Scatter::ScatterJob {
  return [this,
          grid = shared_grid(),
          ecology = m_ecology,
          biome = m_biome_settings,
          species = m_species,
          props = world_props != nullptr ? *world_props : m_world_props,
          own = anchored != nullptr ? anchored_of_species(*anchored, m_species)
                                    : m_anchored,
          world = std::move(world)]() mutable -> Ground::Scatter::ScatterAdoption {
    auto instances = Ground::Scatter::generate_trees(
        {grid->view(), ecology.get(), biome, world->terrain(), props, world.get()},
        species,
        own);
    return [this,
            props = std::move(props),
            own = std::move(own),
            instances = std::move(instances)]() mutable {
      m_world_props = std::move(props);
      m_anchored = std::move(own);
      adopt_generated(std::move(instances));
    };
  };
}

} // namespace Render::GL

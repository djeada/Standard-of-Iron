#include "tree_renderer.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "game/map/scatter/tree_scatter_walk.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "render/terrain_contact.h"
#include "scatter_runtime.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

auto resolve_tree_surface_position(const Game::Map::TerrainService& terrain_service,
                                   float world_x,
                                   float world_z,
                                   float fallback_y) -> QVector3D {
  if (terrain_service.is_initialized()) {
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, 0.0F, fallback_y);
  }
  return {world_x, fallback_y, world_z};
}

auto trunk_contact_radius(Game::Map::TreeSpecies species) -> float {
  switch (species) {
  case Game::Map::TreeSpecies::Pine:
    return 0.09F;
  case Game::Map::TreeSpecies::Olive:
    return 0.22F;
  case Game::Map::TreeSpecies::Cypress:
    return 0.065F;
  case Game::Map::TreeSpecies::Palm:
    return 0.10F;
  }
  return 0.10F;
}

auto bed_tree_base(const Game::Map::TerrainService& terrain_service,
                   Game::Map::TreeSpecies species,
                   const QVector3D& surface,
                   float scale) -> float {
  constexpr float k_max_bed_scale = 0.35F;
  float const radius = trunk_contact_radius(species) * scale;
  return Render::bedded_prop_world_y(terrain_service,
                                     surface.x(),
                                     surface.z(),
                                     surface.y(),
                                     radius,
                                     k_max_bed_scale * scale);
}

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

  rebuild_instances();
}

void TreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_instances();
}

void TreeRenderer::rebuild_instances() {
  m_state.instances.clear();
  append_world_prop_trees();
  finish_instance_rebuild();
}

void TreeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, QVector3D(0.35F, 0.8F, 0.45F));
}

void TreeRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_filtered_common<true>(
      renderer,
      resources,
      draw_species(m_species),
      [](TerrainScatterCmd& cmd, const FoliageBatchParams& params) {
        cmd.foliage = params;
      });
}

void TreeRenderer::append_world_prop_trees() {
  const auto& profile = *m_profile;
  const auto& terrain_service = world().terrain_or_empty();

  for (const auto& prop : m_world_props) {
    if (prop.type != profile.prop_type) {
      continue;
    }
    const QVector3D surface = terrain_service.world_prop_world_position(prop);
    const float scale =
        prop.scale * Game::Map::world_prop_render_scale(profile.prop_type);
    const QVector3D pos(surface.x(),
                        bed_tree_base(terrain_service, m_species, surface, scale),
                        surface.z());

    uint32_t var_state = hash_coords(static_cast<int>(std::round(prop.x)),
                                     static_cast<int>(std::round(prop.z)),
                                     m_noise_seed ^ profile.prop_salt);
    const auto look = tree_world_prop_look(profile, var_state);

    TreeInstanceGpu inst;
    inst.pos_scale = QVector4D(pos.x(), pos.y(), pos.z(), scale);
    inst.color_sway =
        QVector4D(look.tint.x(), look.tint.y(), look.tint.z(), look.sway_phase);
    inst.rotation =
        QVector4D(prop.rotation, look.silhouette_seed, look.leaf_seed, look.bark_seed);
    m_state.instances.push_back(inst);
  }
}

} // namespace Render::GL

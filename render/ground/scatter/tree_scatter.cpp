#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/tree_scatter_walk.h"
#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/terrain_contact.h"
#include "render/ground/ground_ecology.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/tree_renderer.h"

namespace Render::Ground::Scatter {

namespace {

using Render::GL::AnchoredTree;
using Render::GL::TreeInstanceGpu;

using std::uint32_t;
using namespace Render::Ground;

constexpr float k_tree_look_cells_per_tile = 8.0F;

auto mix_tree_look_hash(uint32_t value) -> uint32_t {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
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

auto canopy_height_factor(const Render::Ground::GroundEcologyField* ecology,
                          float world_x,
                          float world_z) -> float {
  using Render::Ground::ecology_detail::fbm;
  using Render::Ground::ecology_detail::step_between;
  constexpr std::uint32_t k_canopy_salt = 0x7F4A7C15U;
  float const canopy = step_between(
      0.30F, 0.72F, fbm(world_z * 0.034F, world_x * 0.034F, k_canopy_salt));
  float factor = 0.66F + 0.34F * canopy;
  if (ecology != nullptr && !ecology->empty() && !ecology->coverage.empty()) {
    float const gx = Render::Ground::ecology_grid_from_world(
        world_x, ecology->width, ecology->tile_size);
    float const gz = Render::Ground::ecology_grid_from_world(
        world_z, ecology->height, ecology->tile_size);
    float const authored = ecology->sample(ecology->coverage, gx, gz);
    if (authored > 0.05F) {
      float const stand =
          step_between(0.12F, 0.70F, ecology->sample(ecology->forest, gx, gz));
      factor *= 0.36F + 0.64F * stand;
    }
  }
  return std::clamp(factor, 0.3F, 1.0F);
}

} // namespace

auto generate_trees(const ScatterRequest& request,
                    Game::Map::TreeSpecies species,
                    const std::vector<Render::GL::AnchoredTree>& anchored)
    -> std::vector<TreeInstanceGpu> {
  std::vector<TreeInstanceGpu> instances;
  {
    const auto& profile = Render::Ground::tree_scatter_profile(species);
    const auto& terrain_service = request.terrain;

    for (const auto& prop : request.world_props) {
      if (prop.type != profile.prop_type) {
        continue;
      }
      const QVector3D surface = terrain_service.world_prop_world_position(prop);
      const float scale =
          prop.scale * Game::Map::world_prop_render_scale(profile.prop_type) *
          canopy_height_factor(request.ecology, surface.x(), surface.z());
      const QVector3D pos(surface.x(),
                          bed_tree_base(terrain_service, species, surface, scale),
                          surface.z());

      uint32_t var_state = mix_tree_look_hash(
          hash_coords(static_cast<int>(std::round(prop.x * k_tree_look_cells_per_tile)),
                      static_cast<int>(std::round(prop.z * k_tree_look_cells_per_tile)),
                      request.biome.seed ^ profile.prop_salt));
      const auto look = tree_world_prop_look(profile, var_state);

      TreeInstanceGpu inst;
      inst.pos_scale = QVector4D(pos.x(), pos.y(), pos.z(), scale);
      inst.color_sway =
          QVector4D(look.tint.x(), look.tint.y(), look.tint.z(), look.sway_phase);
      inst.rotation = QVector4D(
          prop.rotation, look.silhouette_seed, look.leaf_seed, look.bark_seed);
      instances.push_back(inst);
    }
  }
  {
    const auto& profile = Render::Ground::tree_scatter_profile(species);
    const auto& terrain_service = request.terrain;
    for (const AnchoredTree& tree : anchored) {
      const QVector3D surface = terrain_service.resolve_surface_world_position(
          tree.position.x(), tree.position.z(), 0.0F, tree.position.y());
      const float scale =
          tree.scale * Game::Map::world_prop_render_scale(profile.prop_type);
      const QVector3D pos(surface.x(),
                          tree.grounded
                              ? bed_tree_base(terrain_service, species, surface, scale)
                              : tree.position.y(),
                          surface.z());
      uint32_t look_state = mix_tree_look_hash(tree.seed ^ profile.prop_salt);
      const auto look = tree_world_prop_look(profile, look_state);
      TreeInstanceGpu inst;
      inst.pos_scale = QVector4D(pos.x(), pos.y(), pos.z(), scale);
      inst.color_sway =
          QVector4D(look.tint.x(), look.tint.y(), look.tint.z(), look.sway_phase);
      inst.rotation = QVector4D(
          tree.rotation, look.silhouette_seed, look.leaf_seed, look.bark_seed);
      instances.push_back(inst);
    }
  }
  return instances;
}

} // namespace Render::Ground::Scatter

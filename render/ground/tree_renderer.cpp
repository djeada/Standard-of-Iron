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

void TreeRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  configure_height_scatter_common(height_map,
                                  biome_settings,
                                  scatter_seed_world_props,
                                  runtime_world_props,
                                  use_world_props_exclusively);

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  auto& params = m_state.params;
  params.light_direction = m_light_direction;
  params.time = 0.0F;
  params.wind_strength = wind_profile.sway_strength;
  params.wind_speed = wind_profile.sway_speed;

  rebuild_instances();
}

void TreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  adopt_runtime_world_props(runtime_world_props, use_world_props_exclusively);
  rebuild_instances();
}

void TreeRenderer::rebuild_instances() {
  m_state.instances.clear();
  append_world_prop_trees();
  if (!m_use_world_props_exclusively) {
    append_procedural_instances(
        [this](std::vector<TreeInstanceGpu>& out) { generate_procedural_trees(out); });
  }
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

  for (const auto& prop : m_runtime_world_props) {
    if (prop.type != profile.prop_type) {
      continue;
    }
    const QVector3D pos = terrain_service.world_prop_world_position(prop);

    uint32_t var_state = hash_coords(static_cast<int>(std::round(prop.x)),
                                     static_cast<int>(std::round(prop.z)),
                                     m_noise_seed ^ profile.prop_salt);
    const auto look = tree_world_prop_look(profile, var_state);

    TreeInstanceGpu inst;
    inst.pos_scale =
        QVector4D(pos.x(),
                  pos.y(),
                  pos.z(),
                  prop.scale * Game::Map::world_prop_render_scale(profile.prop_type));
    inst.color_sway =
        QVector4D(look.tint.x(), look.tint.y(), look.tint.z(), look.sway_phase);
    inst.rotation =
        QVector4D(prop.rotation, look.silhouette_seed, look.leaf_seed, look.bark_seed);
    m_state.instances.push_back(inst);
  }
}

void TreeRenderer::generate_procedural_trees(std::vector<TreeInstanceGpu>& out) const {
  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules = Game::Map::make_scatter_rules(scatter_profile.ground_type);
  const auto& rule = scatter_rules.tree(m_species);
  if (!rule.allowed) {
    return;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  TreeScatterWalkInput input;
  input.terrain_cache = &terrain_cache;
  input.width = m_width;
  input.height = m_height;
  input.tile_size = m_tile_size;
  input.noise_seed = m_noise_seed;
  input.density = tree_scatter_density(scatter_rules, scatter_profile, m_species);
  input.scale_min = rule.scale_min;
  input.scale_max = rule.scale_max;

  const SpawnValidationConfig config = make_tree_scatter_spawn_config(
      *m_profile, input, scatter_profile.spawn_edge_padding);
  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(terrain_cache,
                                        m_width,
                                        m_height,
                                        m_tile_size,
                                        m_biome_settings,
                                        m_scatter_seed_world_props);
  input.validator = &validator;
  input.composition = &composition;

  const auto& terrain_service = world().terrain_or_empty();
  walk_tree_scatter(
      *m_profile,
      input,
      [&](const TreeScatterSample& sample, const ScatterCompositionSample&) {
        QVector3D const world_pos = resolve_tree_surface_position(
            terrain_service,
            sample.world_x,
            sample.world_z,
            terrain_cache.sample_height_at(sample.grid_x, sample.grid_z));

        TreeInstanceGpu instance;
        instance.pos_scale =
            QVector4D(world_pos.x(), world_pos.y(), world_pos.z(), sample.scale);
        instance.color_sway = QVector4D(
            sample.tint.x(), sample.tint.y(), sample.tint.z(), sample.sway_phase);
        instance.rotation = QVector4D(sample.rotation,
                                      sample.silhouette_seed,
                                      sample.leaf_seed,
                                      sample.bark_seed);
        out.push_back(instance);
        return true;
      });
}

} // namespace Render::GL

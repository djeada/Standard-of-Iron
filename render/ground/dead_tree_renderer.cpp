#include "dead_tree_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "gl/render_constants.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

namespace {

using namespace Render::Ground;

constexpr float k_base_color_r = 0.30F;
constexpr float k_base_color_g = 0.27F;
constexpr float k_base_color_b = 0.22F;

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

} // namespace

namespace Render::GL {

DeadTreeRenderer::DeadTreeRenderer() = default;
DeadTreeRenderer::~DeadTreeRenderer() = default;

void DeadTreeRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
    const std::vector<Game::Map::WorldProp>& runtime_world_props) {
  configure_height_scatter_common(
      height_map, biome_settings, scatter_seed_world_props, runtime_world_props, false);
  m_state.params.light_direction = m_light_direction;
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props) {
  adopt_runtime_world_props(runtime_world_props, false);
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::rebuild_dead_tree_instances() {
  m_state.instances.clear();
  append_world_prop_dead_trees();
  append_procedural_instances([this](std::vector<PropInstanceGpu>& out) {
    generate_procedural_dead_trees(out);
  });
  finish_instance_rebuild();
}

void DeadTreeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void DeadTreeRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::DeadTree);
}

void DeadTreeRenderer::append_world_prop_dead_trees() {
  const auto& terrain_service = world().terrain_or_empty();
  const float half_w = static_cast<float>(m_width) * 0.5F;
  const float half_h = static_cast<float>(m_height) * 0.5F;
  const float tile_size = m_tile_size;

  for (const auto& prop : m_runtime_world_props) {
    if (prop.type != Game::Map::WorldProp::Type::DeadTree) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x51A3C7D9U);
    QVector3D const base_color(k_base_color_r, k_base_color_g, k_base_color_b);
    QVector3D const dry_color(0.43F, 0.36F, 0.27F);
    float const color_var = remap(rand_01(state), 0.35F, 0.85F);
    QVector3D const color = base_color * (1.0F - color_var) + dry_color * color_var;

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::DeadTree));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }
}

void DeadTreeRenderer::generate_procedural_dead_trees(
    std::vector<PropInstanceGpu>& out) const {
  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  SpawnValidationConfig config = make_camp_prop_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * 0.55F;
  config.max_slope = 0.42F;
  config.building_clearance = 3.2F;
  config.road_clearance = 1.3F;
  config.river_clearance = 1.4F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(terrain_cache,
                                        m_width,
                                        m_height,
                                        m_tile_size,
                                        m_biome_settings,
                                        m_scatter_seed_world_props);

  auto add_dead_tree = [&](float gx,
                           float gz,
                           float scale_min,
                           float scale_max,
                           uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0x3ED78341U);
    if (scene.fertility > 0.74F && scene.dryness < 0.38F) {
      return false;
    }
    float const chance = scatter_spawn_chance(ScatterRuleSpecies::DeadTree, scene) *
                         (0.18F + scene.dryness * 0.44F + scene.rockiness * 0.22F +
                          scene.cluster_bias * 0.16F);
    if (rand_01(state) > chance) {
      return false;
    }

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    QVector3D const world_pos =
        resolve_tree_surface_position(world().terrain_or_empty(),
                                      world_x,
                                      world_z,
                                      terrain_cache.sample_height_at(gx, gz));

    float const color_var = rand_01(state);
    QVector3D const base_color(k_base_color_r, k_base_color_g, k_base_color_b);
    QVector3D const dry_color(0.43F, 0.36F, 0.27F);
    QVector3D color = base_color * (1.0F - color_var) + dry_color * color_var;
    QVector3D const rain_dark(0.22F, 0.23F, 0.20F);
    float const dampness = (1.0F - scene.dryness) * 0.24F;
    color = color * (1.0F - dampness) + rain_dark * dampness;
    color *= 0.88F + scene.dryness * 0.10F + scene.rockiness * 0.03F;

    PropInstanceGpu inst;
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        scatter_scale_bias(ScatterRuleSpecies::DeadTree, scene);
    inst.pos_scale = QVector4D(world_pos.x(), world_pos.y(), world_pos.z(), scale);
    inst.color_rot = QVector4D(
        color.x(), color.y(), color.z(), rand_01(state) * MathConstants::k_two_pi);
    out.push_back(inst);
    return true;
  };

  {
    float const base_density =
        std::clamp(0.030F + (1.0F - m_biome_settings.moisture_level) * 0.038F +
                       m_biome_settings.rock_exposure * 0.030F,
                   0.018F,
                   0.090F);
    for (int z = 0; z < m_height; z += 10) {
      for (int x = 0; x < m_width; x += 10) {
        int const sample_x = std::min(x + 5, m_width - 1);
        int const sample_z = std::min(z + 5, m_height - 1);
        uint32_t state = hash_coords(x, z, m_biome_settings.seed ^ 0xB91CF237U);
        auto const scene = composition.sample_grid(static_cast<float>(sample_x),
                                                   static_cast<float>(sample_z),
                                                   state ^ 0x62B68DD1U);
        float const density =
            base_density *
            scatter_density_multiplier(ScatterRuleSpecies::DeadTree, scene) *
            (0.45F + scene.cluster_bias * 1.10F);
        if (rand_01(state) > density) {
          continue;
        }
        float const gx = static_cast<float>(x) + rand_01(state) * 10.0F;
        float const gz = static_cast<float>(z) + rand_01(state) * 10.0F;
        add_dead_tree(gx, gz, 1.35F, 2.30F, state);
      }
    }
  }
}

} // namespace Render::GL

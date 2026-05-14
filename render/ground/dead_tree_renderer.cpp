#include "dead_tree_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_composition.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"

namespace {

using namespace Render::Ground;

constexpr float k_base_color_r = 0.58F;
constexpr float k_base_color_g = 0.49F;
constexpr float k_base_color_b = 0.34F;

} // namespace

namespace Render::GL {

DeadTreeRenderer::DeadTreeRenderer() = default;
DeadTreeRenderer::~DeadTreeRenderer() = default;

void DeadTreeRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                 const Game::Map::BiomeSettings& biome_settings,
                                 const std::vector<Game::Map::WorldProp>& world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void DeadTreeRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction =
      dir.isNull() ? DeadTreeBatchParams::default_light_direction() : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void DeadTreeRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state, [](const DeadTreeInstanceGpu& inst) -> const QVector4D& {
        return inst.pos_scale;
      });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  m_state.params.time = renderer.get_animation_time();

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::DeadTree;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.dead_tree = m_state.params;
  renderer.terrain_scatter(cmd);
}

void DeadTreeRenderer::clear() {
  m_state.reset_instances();
}

void DeadTreeRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  auto& terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(height_map.get_height_data(),
                                      height_map.getTerrainTypes(),
                                      width,
                                      map_height,
                                      tile_size);

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  SpawnValidationConfig config = make_camp_prop_spawn_config();
  config.grid_width = width;
  config.grid_height = map_height;
  config.tile_size = tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * 0.55F;
  config.max_slope = 0.42F;
  config.building_clearance = 3.2F;
  config.road_clearance = 1.3F;
  config.river_clearance = 1.4F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, map_height, tile_size, m_biome_settings, world_props);

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
    float const world_y = terrain_cache.sample_height_at(gx, gz);

    float const color_var = rand_01(state);
    QVector3D const base_color(k_base_color_r, k_base_color_g, k_base_color_b);
    QVector3D const dry_color(0.66F, 0.55F, 0.36F);
    QVector3D color = base_color * (1.0F - color_var) + dry_color * color_var;
    color *= 0.88F + scene.dryness * 0.16F;

    DeadTreeInstanceGpu inst;
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        scatter_scale_bias(ScatterRuleSpecies::DeadTree, scene);
    inst.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    inst.color_rot = QVector4D(
        color.x(), color.y(), color.z(), rand_01(state) * MathConstants::k_two_pi);
    m_state.instances.push_back(inst);
    return true;
  };

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::DeadTree) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    DeadTreeInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::DeadTree));
    inst.color_rot =
        QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, prop.rotation);
    m_state.instances.push_back(inst);
  }

  if (width >= 2 && map_height >= 2 && !height_map.get_height_data().empty()) {
    float const base_density =
        std::clamp(0.030F + (1.0F - m_biome_settings.moisture_level) * 0.038F +
                       m_biome_settings.rock_exposure * 0.030F,
                   0.018F,
                   0.090F);
    for (int z = 0; z < map_height; z += 10) {
      for (int x = 0; x < width; x += 10) {
        int const sample_x = std::min(x + 5, width - 1);
        int const sample_z = std::min(z + 5, map_height - 1);
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

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL

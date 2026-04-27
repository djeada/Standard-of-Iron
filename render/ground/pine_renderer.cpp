#include "pine_renderer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

PineRenderer::PineRenderer() = default;
PineRenderer::~PineRenderer() = default;

void PineRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                             const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_pineInstances.clear();
  m_visibleInstances.clear();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;
  m_cachedVisibilityVersion = 0;
  m_visibilityDirty = true;

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  m_pineParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_pineParams.time = 0.0F;
  m_pineParams.wind_strength = wind_profile.sway_strength;
  m_pineParams.wind_speed = wind_profile.sway_speed;

  generate_pine_instances();
}

void PineRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_instances(
      m_pineInstances, m_visibleInstances, m_pineInstanceBuffer,
      m_cachedVisibilityVersion, m_visibilityDirty,
      [](const PineInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_scale;
      });
  if (visible_count == 0 || !m_pineInstanceBuffer) {
    return;
  }

  m_pineInstanceCount = visible_count;
  PineBatchParams params = m_pineParams;
  params.time = renderer.get_animation_time();
  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Pine;
  cmd.instance_buffer = m_pineInstanceBuffer.get();
  cmd.instance_count = visible_count;
  cmd.pine = params;
  renderer.terrain_scatter(cmd);
}

void PineRenderer::clear() {
  m_pineInstances.clear();
  m_visibleInstances.clear();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;
  m_visibilityDirty = true;
  m_cachedVisibilityVersion = 0;
}

void PineRenderer::generate_pine_instances() {
  m_pineInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  const auto scatter_profile =
      Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules =
      Game::Map::make_scatter_rules(scatter_profile.ground_type);
  if (!scatter_rules.allow_pines) {
    m_pineInstancesDirty = false;
    return;
  }

  const float tile_safe = std::max(0.1F, m_tile_size);

  float pine_density = scatter_rules.pine_base_density;
  if (scatter_profile.plant_density > 0.0F) {
    pine_density =
        scatter_profile.plant_density * scatter_rules.pine_density_scale;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_heightData, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  auto add_pine = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = m_heightData[static_cast<size_t>(normal_idx)];

    float const scale = remap(rand_01(state), 3.0F, 6.0F) * tile_safe;

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color(0.15F, 0.35F, 0.20F);
    QVector3D const var_color(0.20F, 0.40F, 0.25F);
    QVector3D tint_color =
        base_color * (1.0F - color_var) + var_color * color_var;

    float const brown_mix = remap(rand_01(state), 0.10F, 0.25F);
    QVector3D const brown_tint(0.35F, 0.30F, 0.20F);
    tint_color = tint_color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    float const silhouette_seed = rand_01(state);
    float const needle_seed = rand_01(state);
    float const bark_seed = rand_01(state);

    PineInstanceGpu instance;

    instance.pos_scale = QVector4D(world_x, world_y, world_z, scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.rotation =
        QVector4D(rotation, silhouette_seed, needle_seed, bark_seed);
    m_pineInstances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 6) {
    for (int x = 0; x < m_width; x += 6) {
      int const idx = z * m_width + x;

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.75F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xAB12CD34U ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);

      float const cluster_noise = value_noise(world_x * 0.03F, world_z * 0.03F,
                                              m_noiseSeed ^ 0x7F8E9D0AU);

      if (cluster_noise < 0.35F) {
        continue;
      }

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 1.2F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.4F;
      }

      float const effective_density = pine_density * density_mult * 0.8F;
      int pine_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(pine_count);
      if (rand_01(state) < frac) {
        pine_count += 1;
      }

      for (int i = 0; i < pine_count; ++i) {
        float const gx = float(x) + rand_01(state) * 6.0F;
        float const gz = float(z) + rand_01(state) * 6.0F;
        add_pine(gx, gz, state);
      }
    }
  }

  m_pineInstanceCount = m_pineInstances.size();
  m_pineInstancesDirty = m_pineInstanceCount > 0;
}

} // namespace Render::GL

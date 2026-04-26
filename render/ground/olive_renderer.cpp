#include "olive_renderer.h"
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

OliveRenderer::OliveRenderer() = default;
OliveRenderer::~OliveRenderer() = default;

void OliveRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_oliveInstances.clear();
  m_visibleInstances.clear();
  m_oliveInstanceBuffer.reset();
  m_oliveInstanceCount = 0;
  m_oliveInstancesDirty = false;
  m_cachedVisibilityVersion = 0;
  m_visibilityDirty = true;

  m_oliveParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_oliveParams.time = 0.0F;
  m_oliveParams.wind_strength = 0.3F;
  m_oliveParams.wind_speed = 0.5F;

  generate_olive_instances();
}

void OliveRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_instances(
      m_oliveInstances, m_visibleInstances, m_oliveInstanceBuffer,
      m_cachedVisibilityVersion, m_visibilityDirty,
      [](const OliveInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_scale;
      });
  if (visible_count == 0 || !m_oliveInstanceBuffer) {
    return;
  }

  m_oliveInstanceCount = visible_count;
  OliveBatchParams params = m_oliveParams;
  params.time = renderer.get_animation_time();
  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Olive;
  cmd.instance_buffer = m_oliveInstanceBuffer.get();
  cmd.instance_count = visible_count;
  cmd.olive = params;
  renderer.terrain_scatter(cmd);
}

void OliveRenderer::clear() {
  m_oliveInstances.clear();
  m_visibleInstances.clear();
  m_oliveInstanceBuffer.reset();
  m_oliveInstanceCount = 0;
  m_oliveInstancesDirty = false;
  m_visibilityDirty = true;
  m_cachedVisibilityVersion = 0;
}

void OliveRenderer::generate_olive_instances() {
  m_oliveInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules = Game::Map::make_scatter_rules(scatter_profile.ground_type);
  if (!scatter_rules.allow_olives) {
    m_oliveInstancesDirty = false;
    return;
  }

  const float tile_safe = std::max(0.1F, m_tile_size);

  float olive_density =
      (scatter_profile.ground_type == Game::Map::GroundType::GrassDry)
          ? scatter_rules.olive_dry_base_density
          : scatter_rules.olive_base_density;
  if (scatter_profile.plant_density > 0.0F) {
    float const density_scale =
        (scatter_profile.ground_type == Game::Map::GroundType::GrassDry)
            ? scatter_rules.olive_density_scale_dry
            : scatter_rules.olive_density_scale_default;
    olive_density = scatter_profile.plant_density * density_scale;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_heightData, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;
  config.max_slope = 0.65F;

  SpawnValidator validator(terrain_cache, config);

  auto add_olive = [&](float gx, float gz, uint32_t &state) -> bool {
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

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color(0.35F, 0.42F, 0.28F);
    QVector3D const var_color(0.38F, 0.45F, 0.32F);
    QVector3D tint_color =
        base_color * (1.0F - color_var) + var_color * color_var;

    float const gray_mix = remap(rand_01(state), 0.08F, 0.18F);
    QVector3D const gray_tint(0.45F, 0.48F, 0.42F);
    tint_color = tint_color * (1.0F - gray_mix) + gray_tint * gray_mix;

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    float const silhouette_seed = rand_01(state);
    float const leaf_seed = rand_01(state);
    float const bark_seed = rand_01(state);

    OliveInstanceGpu instance;

    float const base_scale = remap(rand_01(state), 2.8F, 5.5F) * tile_safe;
    float const dry_scale = remap(rand_01(state), 3.2F, 6.5F) * tile_safe;
    float const chosen_scale =
        (m_biome_settings.ground_type == Game::Map::GroundType::GrassDry)
            ? dry_scale
            : base_scale;

    instance.pos_scale = QVector4D(world_x, world_y, world_z, chosen_scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.rotation =
        QVector4D(rotation, silhouette_seed, leaf_seed, bark_seed);
    m_oliveInstances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 6) {
    for (int x = 0; x < m_width; x += 6) {
      int const idx = z * m_width + x;

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xCD34EF56U ^ static_cast<uint32_t>(idx));

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 1.15F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.5F;
      }

      float const effective_density = olive_density * density_mult;
      int olive_count = static_cast<int>(std::ceil(effective_density));

      for (int i = 0; i < olive_count; ++i) {
        float const gx = float(x) + rand_01(state) * 6.0F;
        float const gz = float(z) + rand_01(state) * 6.0F;
        add_olive(gx, gz, state);
      }
    }
  }

  m_oliveInstanceCount = m_oliveInstances.size();
  m_oliveInstancesDirty = m_oliveInstanceCount > 0;
}

} // namespace Render::GL

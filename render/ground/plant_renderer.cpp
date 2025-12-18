#include "plant_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/plant_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
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

inline auto valueNoise(float x, float z, uint32_t salt = 0U) -> float {
  int const x0 = int(std::floor(x));
  int const z0 = int(std::floor(z));
  int const x1 = x0 + 1;
  int const z1 = z0 + 1;
  float const tx = x - float(x0);
  float const tz = z - float(z0);
  float const n00 = hash_to_01(hash_coords(x0, z0, salt));
  float const n10 = hash_to_01(hash_coords(x1, z0, salt));
  float const n01 = hash_to_01(hash_coords(x0, z1, salt));
  float const n11 = hash_to_01(hash_coords(x1, z1, salt));
  float const nx0 = n00 * (1 - tx) + n10 * tx;
  float const nx1 = n01 * (1 - tx) + n11 * tx;
  return nx0 * (1 - tz) + nx1 * tz;
}

} // namespace

namespace Render::GL {

PlantRenderer::PlantRenderer() = default;
PlantRenderer::~PlantRenderer() = default;

void PlantRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_plantInstances.clear();
  m_plantInstanceBuffer.reset();
  m_plantInstanceCount = 0;
  m_plantInstancesDirty = false;

  m_plantParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_plantParams.time = 0.0F;
  m_plantParams.wind_strength = m_biome_settings.sway_strength;
  m_plantParams.wind_speed = m_biome_settings.sway_speed;

  generatePlantInstances();
}

void PlantRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_plantInstanceCount = static_cast<uint32_t>(m_plantInstances.size());

  if (m_plantInstanceCount == 0) {
    m_plantInstanceBuffer.reset();
    m_visibleInstances.clear();
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();
  const std::uint64_t current_version =
      use_visibility ? visibility.version() : 0;

  const bool needs_visibility_update =
      m_visibilityDirty || (current_version != m_cachedVisibilityVersion);

  if (needs_visibility_update) {
    m_visibleInstances.clear();

    if (use_visibility) {
      m_visibleInstances.reserve(m_plantInstanceCount);
      for (const auto &instance : m_plantInstances) {
        float const world_x = instance.pos_scale.x();
        float const world_z = instance.pos_scale.z();
        if (visibility.isVisibleWorld(world_x, world_z)) {
          m_visibleInstances.push_back(instance);
        }
      }
    } else {
      m_visibleInstances = m_plantInstances;
    }

    m_cachedVisibilityVersion = current_version;
    m_visibilityDirty = false;

    if (!m_visibleInstances.empty()) {
      if (!m_visibleInstanceBuffer) {
        m_visibleInstanceBuffer =
            std::make_unique<Buffer>(Buffer::Type::Vertex);
      }
      m_visibleInstanceBuffer->set_data(m_visibleInstances,
                                        Buffer::Usage::Static);
    }
  }

  const auto visible_count = static_cast<uint32_t>(m_visibleInstances.size());
  if (visible_count == 0 || !m_visibleInstanceBuffer) {
    return;
  }

  PlantBatchParams params = m_plantParams;
  params.time = renderer.get_animation_time();
  renderer.plant_batch(m_visibleInstanceBuffer.get(), visible_count, params);
}

void PlantRenderer::clear() {
  m_plantInstances.clear();
  m_visibleInstances.clear();
  m_plantInstanceBuffer.reset();
  m_visibleInstanceBuffer.reset();
  m_plantInstanceCount = 0;
  m_plantInstancesDirty = false;
  m_visibilityDirty = true;
  m_cachedVisibilityVersion = 0;
}

void PlantRenderer::generatePlantInstances() {
  m_plantInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    m_plantInstanceCount = 0;
    m_plantInstancesDirty = false;
    return;
  }

  const float plant_density =
      std::clamp(m_biome_settings.plant_density, 0.0F, 2.0F);

  if (plant_density < 0.01F) {
    m_plantInstanceCount = 0;
    m_plantInstancesDirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_heightData, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = m_biome_settings.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  auto add_plant = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), 0.30F, 0.80F) * tile_safe;

    float const plant_type = std::floor(rand_01(state) * 4.0F);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color = m_biome_settings.grass_primary * 0.7F;
    QVector3D const var_color = m_biome_settings.grass_secondary * 0.8F;
    QVector3D tint_color =
        base_color * (1.0F - color_var) + var_color * color_var;

    float const brown_mix = remap(rand_01(state), 0.15F, 0.35F);
    QVector3D const brown_tint(0.55F, 0.50F, 0.35F);
    tint_color = tint_color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;
    float const sway_strength = remap(rand_01(state), 0.6F, 1.2F);
    float const sway_speed = remap(rand_01(state), 0.8F, 1.3F);

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    PlantInstanceGpu instance;

    instance.pos_scale = QVector4D(world_x, world_y + 0.05F, world_z, scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.type_params =
        QVector4D(plant_type, rotation, sway_strength, sway_speed);
    m_plantInstances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 3) {
    for (int x = 0; x < m_width; x += 3) {
      int const idx = z * m_width + x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      if (terrain_type == Game::Map::TerrainType::Mountain ||
          terrain_type == Game::Map::TerrainType::River) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0x8F3C5A7EU ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);

      float const cluster_noise = valueNoise(world_x * 0.05F, world_z * 0.05F,
                                             m_noiseSeed ^ 0x4B9D2F1AU);

      if (cluster_noise < 0.45F) {
        continue;
      }

      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 0.6F;
      }

      float const effective_density = plant_density * density_mult * 0.8F;
      int plant_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(plant_count);
      if (rand_01(state) < frac) {
        plant_count += 1;
      }

      for (int i = 0; i < plant_count; ++i) {
        float const gx = float(x) + rand_01(state) * 3.0F;
        float const gz = float(z) + rand_01(state) * 3.0F;
        add_plant(gx, gz, state);
      }
    }
  }

  m_plantInstanceCount = m_plantInstances.size();
  m_plantInstancesDirty = m_plantInstanceCount > 0;
}

} // namespace Render::GL

#include "stone_renderer.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/stone_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "spawn_validator.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qelapsedtimer.h>
#include <qglobal.h>
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

StoneRenderer::StoneRenderer() = default;
StoneRenderer::~StoneRenderer() = default;

void StoneRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_stoneInstances.clear();
  m_stoneInstanceBuffer.reset();
  m_stoneInstanceCount = 0;
  m_stoneInstancesDirty = false;

  m_stoneParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_stoneParams.time = 0.0F;

  generateStoneInstances();
}

void StoneRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  Q_UNUSED(resources);
  if (m_stoneInstanceCount > 0) {
    if (!m_stoneInstanceBuffer) {
      m_stoneInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (m_stoneInstancesDirty && m_stoneInstanceBuffer) {
      m_stoneInstanceBuffer->set_data(m_stoneInstances, Buffer::Usage::Static);
      m_stoneInstancesDirty = false;
    }
  } else {
    m_stoneInstanceBuffer.reset();
    return;
  }

  renderer.stone_batch(m_stoneInstanceBuffer.get(), m_stoneInstanceCount,
                       m_stoneParams);
}

void StoneRenderer::clear() {
  m_stoneInstances.clear();
  m_stoneInstanceBuffer.reset();
  m_stoneInstanceCount = 0;
  m_stoneInstancesDirty = false;
}

void StoneRenderer::generateStoneInstances() {
  QElapsedTimer timer;
  timer.start();

  m_stoneInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    m_stoneInstanceCount = 0;
    m_stoneInstancesDirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_heightData, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = m_biome_settings.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  auto add_stone = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), 0.08F, 0.25F) * tile_safe;

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = m_biome_settings.rock_low;
    QVector3D const high_rock = m_biome_settings.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;

    float const brown_mix = remap(rand_01(state), 0.0F, 0.4F);
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    StoneInstanceGpu instance;
    instance.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    instance.color_rot = QVector4D(color.x(), color.y(), color.z(), rotation);
    m_stoneInstances.push_back(instance);
    return true;
  };

  const float stone_density = 0.15F;

  for (int z = 0; z < m_height; z += 2) {
    for (int x = 0; x < m_width; x += 2) {
      int const idx = z * m_width + x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      if (terrain_type != Game::Map::TerrainType::Flat) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.15F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xABCDEF12U ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);
      float const cluster_noise = valueNoise(world_x * 0.03F, world_z * 0.03F,
                                             m_noiseSeed ^ 0x7F3A9B2CU);

      if (cluster_noise < 0.6F) {
        continue;
      }

      int stone_count = static_cast<int>(std::floor(stone_density));
      float const frac = stone_density - float(stone_count);
      if (rand_01(state) < frac) {
        stone_count += 1;
      }

      for (int i = 0; i < stone_count; ++i) {
        float const gx = float(x) + rand_01(state) * 2.0F;
        float const gz = float(z) + rand_01(state) * 2.0F;
        add_stone(gx, gz, state);
      }
    }
  }

  m_stoneInstanceCount = m_stoneInstances.size();
  m_stoneInstancesDirty = m_stoneInstanceCount > 0;
}

} // namespace Render::GL

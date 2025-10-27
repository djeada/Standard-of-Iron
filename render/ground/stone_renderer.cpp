#include "stone_renderer.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/resources.h"
#include "ground/stone_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <qelapsedtimer.h>
#include <qglobal.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

inline auto valueNoise(float x, float z, uint32_t salt = 0U) -> float {
  int x0 = int(std::floor(x));
  int z0 = int(std::floor(z));
  int x1 = x0 + 1;
  int z1 = z0 + 1;
  float tx = x - float(x0);
  float tz = z - float(z0);
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
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

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
      m_stoneInstanceBuffer->setData(m_stoneInstances, Buffer::Usage::Static);
      m_stoneInstancesDirty = false;
    }
  } else {
    m_stoneInstanceBuffer.reset();
    return;
  }

  renderer.stoneBatch(m_stoneInstanceBuffer.get(), m_stoneInstanceCount,
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

  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;
  const float tile_safe = std::max(0.001F, m_tile_size);

  const float edge_padding =
      std::clamp(m_biomeSettings.spawnEdgePadding, 0.0F, 0.5F);
  const float edge_margin_x = static_cast<float>(m_width) * edge_padding;
  const float edge_margin_z = static_cast<float>(m_height) * edge_padding;

  std::vector<QVector3D> normals(m_width * m_height,
                                 QVector3D(0.0F, 1.0F, 0.0F));

  auto sample_height_at = [&](float gx, float gz) -> float {
    gx = std::clamp(gx, 0.0F, float(m_width - 1));
    gz = std::clamp(gz, 0.0F, float(m_height - 1));
    int const x0 = int(std::floor(gx));
    int const z0 = int(std::floor(gz));
    int const x1 = std::min(x0 + 1, m_width - 1);
    int const z1 = std::min(z0 + 1, m_height - 1);
    float const tx = gx - float(x0);
    float const tz = gz - float(z0);
    float const h00 = m_heightData[z0 * m_width + x0];
    float const h10 = m_heightData[z0 * m_width + x1];
    float const h01 = m_heightData[z1 * m_width + x0];
    float const h11 = m_heightData[z1 * m_width + x1];
    float const h0 = h00 * (1.0F - tx) + h10 * tx;
    float const h1 = h01 * (1.0F - tx) + h11 * tx;
    return h0 * (1.0F - tz) + h1 * tz;
  };

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int const idx = z * m_width + x;
      float const gx0 = std::clamp(float(x) - 1.0F, 0.0F, float(m_width - 1));
      float const gx1 = std::clamp(float(x) + 1.0F, 0.0F, float(m_width - 1));
      float const gz0 = std::clamp(float(z) - 1.0F, 0.0F, float(m_height - 1));
      float const gz1 = std::clamp(float(z) + 1.0F, 0.0F, float(m_height - 1));

      float const hL = sample_height_at(gx0, float(z));
      float const hR = sample_height_at(gx1, float(z));
      float const hD = sample_height_at(float(x), gz0);
      float const hU = sample_height_at(float(x), gz1);

      QVector3D const dx(2.0F * m_tile_size, hR - hL, 0.0F);
      QVector3D const dz(0.0F, hU - hD, 2.0F * m_tile_size);
      QVector3D n = QVector3D::crossProduct(dz, dx);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto add_stone = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edge_margin_x || gx > m_width - 1 - edge_margin_x ||
        gz < edge_margin_z || gz > m_height - 1 - edge_margin_z) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    if (m_terrain_types[normal_idx] != Game::Map::TerrainType::Flat) {
      return false;
    }

    constexpr int k_river_margin = 1;
    for (int dz = -k_river_margin; dz <= k_river_margin; ++dz) {
      for (int dx = -k_river_margin; dx <= k_river_margin; ++dx) {
        int const nx = ix + dx;
        int const nz = iz + dz;
        if (nx >= 0 && nx < m_width && nz >= 0 && nz < m_height) {
          int const nIdx = nz * m_width + nx;
          if (m_terrain_types[nIdx] == Game::Map::TerrainType::River) {
            return false;
          }
        }
      }
    }

    QVector3D const normal = normals[normal_idx];
    float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);

    if (slope > 0.15F) {
      return false;
    }

    float const world_x = (gx - half_width) * m_tile_size;
    float const world_z = (gz - half_height) * m_tile_size;
    float const world_y = sample_height_at(sgx, sgz);

    auto &building_registry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (building_registry.isPointInBuilding(world_x, world_z)) {
      return false;
    }

    float const scale = remap(rand_01(state), 0.08F, 0.25F) * tile_safe;

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = m_biomeSettings.rockLow;
    QVector3D const high_rock = m_biomeSettings.rockHigh;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;

    float const brown_mix = remap(rand_01(state), 0.0F, 0.4F);
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const rotation = rand_01(state) * math_constants::k_two_pi;

    StoneInstanceGpu instance;
    instance.posScale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    instance.colorRot = QVector4D(color.x(), color.y(), color.z(), rotation);
    m_stoneInstances.push_back(instance);
    return true;
  };

  const float stone_density = 0.15F;

  for (int z = 0; z < m_height; z += 2) {
    for (int x = 0; x < m_width; x += 2) {
      int const idx = z * m_width + x;

      if (m_terrain_types[idx] != Game::Map::TerrainType::Flat) {
        continue;
      }

      QVector3D const normal = normals[idx];
      float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
      if (slope > 0.15F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xABCDEF12U ^ static_cast<uint32_t>(idx));

      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;
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

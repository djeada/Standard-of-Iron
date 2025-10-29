#include "plant_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/plant_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
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
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_plantInstances.clear();
  m_plantInstanceBuffer.reset();
  m_plantInstanceCount = 0;
  m_plantInstancesDirty = false;

  m_plantParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_plantParams.time = 0.0F;
  m_plantParams.windStrength = m_biomeSettings.sway_strength;
  m_plantParams.windSpeed = m_biomeSettings.sway_speed;

  generatePlantInstances();
}

void PlantRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_plantInstanceCount = static_cast<uint32_t>(m_plantInstances.size());

  if (m_plantInstanceCount > 0) {
    if (!m_plantInstanceBuffer) {
      m_plantInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (m_plantInstancesDirty && m_plantInstanceBuffer) {
      m_plantInstanceBuffer->setData(m_plantInstances, Buffer::Usage::Static);
      m_plantInstancesDirty = false;
    }
  } else {
    m_plantInstanceBuffer.reset();
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.isInitialized();

  if (use_visibility) {

    std::vector<PlantInstanceGpu> visible_instances;
    visible_instances.reserve(m_plantInstances.size());

    for (const auto &instance : m_plantInstances) {
      float const world_x = instance.posScale.x();
      float const world_z = instance.posScale.z();

      if (visibility.isVisibleWorld(world_x, world_z)) {
        visible_instances.push_back(instance);
      }
    }

    if (visible_instances.empty()) {
      return;
    }

    if (!m_visibleInstanceBuffer) {
      m_visibleInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    m_visibleInstanceBuffer->setData(visible_instances, Buffer::Usage::Stream);

    PlantBatchParams params = m_plantParams;
    params.time = renderer.getAnimationTime();
    renderer.plantBatch(m_visibleInstanceBuffer.get(),
                        static_cast<uint32_t>(visible_instances.size()),
                        params);
  } else {

    if (m_plantInstanceBuffer && m_plantInstanceCount > 0) {
      PlantBatchParams params = m_plantParams;
      params.time = renderer.getAnimationTime();
      renderer.plantBatch(m_plantInstanceBuffer.get(), m_plantInstanceCount,
                          params);
    }
  }
}

void PlantRenderer::clear() {
  m_plantInstances.clear();
  m_plantInstanceBuffer.reset();
  m_visibleInstanceBuffer.reset();
  m_plantInstanceCount = 0;
  m_plantInstancesDirty = false;
}

void PlantRenderer::generatePlantInstances() {
  m_plantInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    m_plantInstanceCount = 0;
    m_plantInstancesDirty = false;
    return;
  }

  const float plant_density =
      std::clamp(m_biomeSettings.plant_density, 0.0F, 2.0F);

  if (plant_density < 0.01F) {
    m_plantInstanceCount = 0;
    m_plantInstancesDirty = false;
    return;
  }

  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;
  const float tile_safe = std::max(0.001F, m_tile_size);

  const float edge_padding =
      std::clamp(m_biomeSettings.spawnEdgePadding, 0.0F, 0.5F);
  const float edge_margin_x = static_cast<float>(m_width) * edge_padding;
  const float edge_margin_z = static_cast<float>(m_height) * edge_padding;

  std::vector<QVector3D> normals(static_cast<qsizetype>(m_width * m_height),
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

      float const h_l = sample_height_at(gx0, float(z));
      float const h_r = sample_height_at(gx1, float(z));
      float const h_d = sample_height_at(float(x), gz0);
      float const h_u = sample_height_at(float(x), gz1);

      QVector3D const dx(2.0F * m_tile_size, h_r - h_l, 0.0F);
      QVector3D const dz(0.0F, h_u - h_d, 2.0F * m_tile_size);
      QVector3D n = QVector3D::crossProduct(dz, dx);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto add_plant = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edge_margin_x || gx > m_width - 1 - edge_margin_x ||
        gz < edge_margin_z || gz > m_height - 1 - edge_margin_z) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    if (m_terrain_types[normal_idx] == Game::Map::TerrainType::Mountain) {
      return false;
    }

    if (m_terrain_types[normal_idx] == Game::Map::TerrainType::River) {
      return false;
    }

    constexpr int k_river_margin = 1;
    for (int dz = -k_river_margin; dz <= k_river_margin; ++dz) {
      for (int dx = -k_river_margin; dx <= k_river_margin; ++dx) {
        if (dx == 0 && dz == 0) {
          continue;
        }
        int const nx = ix + dx;
        int const nz = iz + dz;
        if (nx >= 0 && nx < m_width && nz >= 0 && nz < m_height) {
          int const n_idx = nz * m_width + nx;
          if (m_terrain_types[n_idx] == Game::Map::TerrainType::River) {
            return false;
          }
        }
      }
    }

    QVector3D const normal = normals[normal_idx];
    float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);

    if (slope > 0.65F) {
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

    float const scale = remap(rand_01(state), 0.30F, 0.80F) * tile_safe;

    float const plant_type = std::floor(rand_01(state) * 4.0F);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color = m_biomeSettings.grassPrimary * 0.7F;
    QVector3D const var_color = m_biomeSettings.grassSecondary * 0.8F;
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

    instance.posScale = QVector4D(world_x, world_y + 0.05F, world_z, scale);
    instance.colorSway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.typeParams =
        QVector4D(plant_type, rotation, sway_strength, sway_speed);
    m_plantInstances.push_back(instance);
    return true;
  };

  int cells_checked = 0;
  int cells_passed = 0;
  int plants_added = 0;

  for (int z = 0; z < m_height; z += 3) {
    for (int x = 0; x < m_width; x += 3) {
      cells_checked++;
      int const idx = z * m_width + x;

      if (m_terrain_types[idx] == Game::Map::TerrainType::Mountain ||
          m_terrain_types[idx] == Game::Map::TerrainType::River) {
        continue;
      }

      QVector3D const normal = normals[idx];
      float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0x8F3C5A7EU ^ static_cast<uint32_t>(idx));

      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;

      float const cluster_noise = valueNoise(world_x * 0.05F, world_z * 0.05F,
                                             m_noiseSeed ^ 0x4B9D2F1AU);

      if (cluster_noise < 0.45F) {
        continue;
      }

      cells_passed++;

      float density_mult = 1.0F;
      if (m_terrain_types[idx] == Game::Map::TerrainType::Hill) {
        density_mult = 0.6F;
      }

      float const effective_density = plant_density * density_mult * 2.0F;
      int plant_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(plant_count);
      if (rand_01(state) < frac) {
        plant_count += 1;
      }

      for (int i = 0; i < plant_count; ++i) {
        float const gx = float(x) + rand_01(state) * 3.0F;
        float const gz = float(z) + rand_01(state) * 3.0F;
        if (add_plant(gx, gz, state)) {
          plants_added++;
        }
      }
    }
  }

  m_plantInstanceCount = m_plantInstances.size();
  m_plantInstancesDirty = m_plantInstanceCount > 0;
}

} // namespace Render::GL

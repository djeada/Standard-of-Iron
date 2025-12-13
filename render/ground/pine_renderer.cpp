#include "pine_renderer.h"
#include "../../game/map/terrain_service.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/pine_gpu.h"
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

PineRenderer::PineRenderer() = default;
PineRenderer::~PineRenderer() = default;

void PineRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                             const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_pineInstances.clear();
  m_pineInstanceBuffer.reset();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;

  m_pineParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_pineParams.time = 0.0F;
  m_pineParams.wind_strength = 0.3F;
  m_pineParams.wind_speed = 0.5F;

  generatePineInstances();
}

void PineRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_pineInstanceCount = static_cast<uint32_t>(m_pineInstances.size());

  if (m_pineInstanceCount == 0) {
    m_pineInstanceBuffer.reset();
    m_visibleInstances.clear();
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.isInitialized();
  const std::uint64_t current_version =
      use_visibility ? visibility.version() : 0;

  const bool needs_visibility_update =
      m_visibilityDirty || (current_version != m_cachedVisibilityVersion);

  if (needs_visibility_update) {
    m_visibleInstances.clear();

    if (use_visibility) {
      m_visibleInstances.reserve(m_pineInstanceCount);
      for (const auto &instance : m_pineInstances) {
        float const world_x = instance.pos_scale.x();
        float const world_z = instance.pos_scale.z();
        if (visibility.isVisibleWorld(world_x, world_z)) {
          m_visibleInstances.push_back(instance);
        }
      }
    } else {
      m_visibleInstances = m_pineInstances;
    }

    m_cachedVisibilityVersion = current_version;
    m_visibilityDirty = false;

    if (!m_visibleInstances.empty()) {
      if (!m_pineInstanceBuffer) {
        m_pineInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
      }
      m_pineInstanceBuffer->set_data(m_visibleInstances, Buffer::Usage::Static);
    }
  }

  const auto visible_count = static_cast<uint32_t>(m_visibleInstances.size());
  if (visible_count == 0 || !m_pineInstanceBuffer) {
    return;
  }

  PineBatchParams params = m_pineParams;
  params.time = renderer.get_animation_time();
  renderer.pine_batch(m_pineInstanceBuffer.get(), visible_count, params);
}

void PineRenderer::clear() {
  m_pineInstances.clear();
  m_visibleInstances.clear();
  m_pineInstanceBuffer.reset();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;
  m_visibilityDirty = true;
  m_cachedVisibilityVersion = 0;
}

void PineRenderer::generatePineInstances() {
  m_pineInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  if (m_biomeSettings.ground_type == Game::Map::GroundType::GrassDry) {
    m_pineInstancesDirty = false;
    return;
  }

  const float half_width = static_cast<float>(m_width) * 0.5F;
  const float half_height = static_cast<float>(m_height) * 0.5F;
  const float tile_safe = std::max(0.1F, m_tile_size);

  const float edge_padding =
      std::clamp(m_biomeSettings.spawn_edge_padding, 0.0F, 0.5F);
  const float edge_margin_x = static_cast<float>(m_width) * edge_padding;
  const float edge_margin_z = static_cast<float>(m_height) * edge_padding;

  float pine_density = 0.2F;
  if (m_biomeSettings.plant_density > 0.0F) {

    pine_density = m_biomeSettings.plant_density * 0.3F;
  }

  std::vector<QVector3D> normals(static_cast<qsizetype>(m_width * m_height),
                                 QVector3D(0, 1, 0));
  for (int z = 1; z < m_height - 1; ++z) {
    for (int x = 1; x < m_width - 1; ++x) {
      int const idx = z * m_width + x;
      float const h_l = m_heightData[(z)*m_width + (x - 1)];
      float const h_r = m_heightData[(z)*m_width + (x + 1)];
      float const h_d = m_heightData[(z - 1) * m_width + (x)];
      float const h_u = m_heightData[(z + 1) * m_width + (x)];

      QVector3D n = QVector3D(h_l - h_r, 2.0F * tile_safe, h_d - h_u);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto add_pine = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edge_margin_x || gx > m_width - 1 - edge_margin_x ||
        gz < edge_margin_z || gz > m_height - 1 - edge_margin_z) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    QVector3D const normal = normals[normal_idx];
    float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);

    if (slope > 0.75F) {
      return false;
    }

    float const world_x = (gx - half_width) * m_tile_size;
    float const world_z = (gz - half_height) * m_tile_size;
    float const world_y = m_heightData[normal_idx];

    auto &building_registry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (building_registry.isPointInBuilding(world_x, world_z)) {
      return false;
    }

    auto &terrain_service = Game::Map::TerrainService::instance();
    if (terrain_service.is_point_on_road(world_x, world_z)) {
      return false;
    }

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

      QVector3D const normal = normals[idx];
      float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
      if (slope > 0.75F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xAB12CD34U ^ static_cast<uint32_t>(idx));

      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;

      float const cluster_noise = valueNoise(world_x * 0.03F, world_z * 0.03F,
                                             m_noiseSeed ^ 0x7F8E9D0AU);

      if (cluster_noise < 0.35F) {
        continue;
      }

      float density_mult = 1.0F;
      if (m_terrain_types[idx] == Game::Map::TerrainType::Hill) {
        density_mult = 1.2F;
      } else if (m_terrain_types[idx] == Game::Map::TerrainType::Mountain) {
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

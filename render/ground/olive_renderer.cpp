#include "olive_renderer.h"
#include "../../game/map/terrain_service.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/olive_gpu.h"
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

OliveRenderer::OliveRenderer() = default;
OliveRenderer::~OliveRenderer() = default;

void OliveRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_oliveInstances.clear();
  m_oliveInstanceBuffer.reset();
  m_oliveInstanceCount = 0;
  m_oliveInstancesDirty = false;

  m_oliveParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_oliveParams.time = 0.0F;
  m_oliveParams.wind_strength = 0.3F;
  m_oliveParams.wind_speed = 0.5F;

  generate_olive_instances();
}

void OliveRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_oliveInstanceCount = static_cast<uint32_t>(m_oliveInstances.size());

  if (m_oliveInstanceCount == 0) {
    m_oliveInstanceBuffer.reset();
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
      m_visibleInstances.reserve(m_oliveInstanceCount);
      for (const auto &instance : m_oliveInstances) {
        float const world_x = instance.pos_scale.x();
        float const world_z = instance.pos_scale.z();
        if (visibility.isVisibleWorld(world_x, world_z)) {
          m_visibleInstances.push_back(instance);
        }
      }
    } else {
      m_visibleInstances = m_oliveInstances;
    }

    m_cachedVisibilityVersion = current_version;
    m_visibilityDirty = false;

    if (!m_visibleInstances.empty()) {
      if (!m_oliveInstanceBuffer) {
        m_oliveInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
      }
      m_oliveInstanceBuffer->setData(m_visibleInstances, Buffer::Usage::Static);
    }
  }

  const auto visible_count = static_cast<uint32_t>(m_visibleInstances.size());
  if (visible_count == 0 || !m_oliveInstanceBuffer) {
    return;
  }

  OliveBatchParams params = m_oliveParams;
  params.time = renderer.getAnimationTime();
  renderer.oliveBatch(m_oliveInstanceBuffer.get(), visible_count, params);
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

  const float half_width = static_cast<float>(m_width) * 0.5F;
  const float half_height = static_cast<float>(m_height) * 0.5F;
  const float tile_safe = std::max(0.1F, m_tile_size);

  const float edge_padding =
      std::clamp(m_biomeSettings.spawn_edge_padding, 0.0F, 0.5F);
  const float edge_margin_x = static_cast<float>(m_width) * edge_padding;
  const float edge_margin_z = static_cast<float>(m_height) * edge_padding;

  float olive_density =
      (m_biomeSettings.ground_type == Game::Map::GroundType::GrassDry) ? 0.12F
                                                                       : 0.05F;
  if (m_biomeSettings.plant_density > 0.0F) {
    float const density_mult =
        (m_biomeSettings.ground_type == Game::Map::GroundType::GrassDry)
            ? 0.15F
            : 0.08F;
    olive_density = m_biomeSettings.plant_density * density_mult;
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

  auto add_olive = [&](float gx, float gz, uint32_t &state) -> bool {
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
        (m_biomeSettings.ground_type == Game::Map::GroundType::GrassDry)
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

      QVector3D const normal = normals[idx];
      float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xCD34EF56U ^ static_cast<uint32_t>(idx));

      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;

      float density_mult = 1.0F;
      if (m_terrain_types[idx] == Game::Map::TerrainType::Hill) {
        density_mult = 1.15F;
      } else if (m_terrain_types[idx] == Game::Map::TerrainType::Mountain) {
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

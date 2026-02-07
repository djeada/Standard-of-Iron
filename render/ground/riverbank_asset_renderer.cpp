#include "riverbank_asset_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/resources.h"
#include "ground/riverbank_asset_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include <QDebug>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qglobal.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

inline auto value_noise(float x, float z, uint32_t salt = 0U) -> float {
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

RiverbankAssetRenderer::RiverbankAssetRenderer() = default;
RiverbankAssetRenderer::~RiverbankAssetRenderer() = default;

void RiverbankAssetRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &riverSegments,
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings) {
  m_riverSegments = riverSegments;
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_assetInstances.clear();
  m_assetInstanceBuffer.reset();
  m_assetInstanceCount = 0;
  m_assetInstancesDirty = false;

  m_assetParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_assetParams.time = 0.0F;

  generate_asset_instances();
}

void RiverbankAssetRenderer::submit(Renderer &, ResourceManager *resources) {
  Q_UNUSED(resources);

  if (m_assetInstanceCount == 0) {
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();
  const std::uint64_t current_version =
      use_visibility ? visibility.version() : 0;

  const bool needs_visibility_update =
      m_visibilityDirty || m_assetInstancesDirty ||
      (use_visibility && current_version != m_cachedVisibilityVersion);

  if (needs_visibility_update) {
    Game::Map::VisibilityService::Snapshot visibility_snapshot;
    if (use_visibility) {
      visibility_snapshot = visibility.snapshot();
    }

    m_visibleInstances.clear();
    m_visibleInstances.reserve(m_assetInstances.size());

    for (const auto &instance : m_assetInstances) {
      bool should_render = true;

      if (use_visibility) {
        float const world_x = instance.position[0];
        float const world_z = instance.position[2];

        if (!visibility_snapshot.isVisibleWorld(world_x, world_z)) {
          should_render = false;
        }
      }

      if (should_render) {
        m_visibleInstances.push_back(instance);
      }
    }

    if (!m_assetInstanceBuffer) {
      m_assetInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (!m_visibleInstances.empty()) {
      m_assetInstanceBuffer->set_data(m_visibleInstances,
                                      Buffer::Usage::Dynamic);
    }

    m_cachedVisibilityVersion = current_version;
    m_visibilityDirty = false;
    m_assetInstancesDirty = false;
  }

  if (!m_visibleInstances.empty()) {
    qDebug() << "RiverbankAssetRenderer: Would render"
             << m_visibleInstances.size() << "of" << m_assetInstanceCount
             << "riverbank assets (fog of war applied)";
  }
}

void RiverbankAssetRenderer::clear() {
  m_assetInstances.clear();
  m_assetInstanceBuffer.reset();
  m_assetInstanceCount = 0;
  m_assetInstancesDirty = false;
  m_visibleInstances.clear();
  m_cachedVisibilityVersion = 0;
  m_visibilityDirty = true;
}

void RiverbankAssetRenderer::generate_asset_instances() {
  m_assetInstances.clear();

  if (m_riverSegments.empty() || m_width < 2 || m_height < 2) {
    m_assetInstanceCount = 0;
    m_assetInstancesDirty = false;
    return;
  }

  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;

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

  for (size_t seg_idx = 0; seg_idx < m_riverSegments.size(); ++seg_idx) {
    const auto &segment = m_riverSegments[seg_idx];

    QVector3D dir = segment.end - segment.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const half_river_width = segment.width * 0.5F;
    float const bank_zone_width = 1.5F;

    int const num_steps = static_cast<int>(length / 0.8F) + 1;

    constexpr uint32_t k_rng_segment_multiplier = 1000;
    uint32_t rng =
        m_noiseSeed + static_cast<uint32_t>(seg_idx * k_rng_segment_multiplier);

    for (int i = 0; i < num_steps; ++i) {
      float const t = static_cast<float>(i) /
                      static_cast<float>(std::max(num_steps - 1, 1));
      QVector3D const center_pos = segment.start + dir * (length * t);

      for (int side = 0; side < 2; ++side) {
        float const side_sign = (side == 0) ? -1.0F : 1.0F;

        if (rand_01(rng) > 0.3F) {
          continue;
        }

        float const dist_from_water =
            half_river_width + rand_01(rng) * bank_zone_width;
        float const along_river = (rand_01(rng) - 0.5F) * 0.6F;

        QVector3D const asset_pos =
            center_pos + perpendicular * (side_sign * dist_from_water) +
            dir * along_river;

        float const gx = (asset_pos.x() / m_tile_size) + half_width;
        float const gz = (asset_pos.z() / m_tile_size) + half_height;

        if (gx < 0 || gx >= m_width - 1 || gz < 0 || gz >= m_height - 1) {
          continue;
        }

        int const ix = static_cast<int>(gx);
        int const iz = static_cast<int>(gz);
        int const idx = iz * m_width + ix;

        if (m_terrain_types[idx] != Game::Map::TerrainType::Flat) {
          continue;
        }

        float const world_y = sample_height_at(gx, gz);

        RiverbankAssetInstanceGpu instance{};
        instance.position[0] = asset_pos.x();
        instance.position[1] = world_y;
        instance.position[2] = asset_pos.z();

        float const type_rand = rand_01(rng);
        if (type_rand < 0.7F) {

          instance.asset_type = 0.0F;
          float const size = 0.05F + rand_01(rng) * 0.1F;
          instance.scale[0] = size * (0.8F + rand_01(rng) * 0.4F);
          instance.scale[1] = size * (0.6F + rand_01(rng) * 0.3F);
          instance.scale[2] = size * (0.8F + rand_01(rng) * 0.4F);

          float const color_var = 0.3F + rand_01(rng) * 0.4F;
          instance.color[0] = color_var;
          instance.color[1] = color_var * 0.9F;
          instance.color[2] = color_var * 0.85F;
        } else if (type_rand < 0.9F) {

          instance.asset_type = 1.0F;
          float const size = 0.1F + rand_01(rng) * 0.15F;
          instance.scale[0] = size;
          instance.scale[1] = size * (0.7F + rand_01(rng) * 0.4F);
          instance.scale[2] = size;

          float const color_var = 0.35F + rand_01(rng) * 0.25F;
          instance.color[0] = color_var;
          instance.color[1] = color_var * 0.95F;
          instance.color[2] = color_var * 0.9F;
        } else {

          if (dist_from_water > half_river_width + 0.5F) {
            continue;
          }

          instance.asset_type = 2.0F;
          float const size = 0.3F + rand_01(rng) * 0.4F;
          instance.scale[0] = size * 0.3F;
          instance.scale[1] = size;
          instance.scale[2] = size * 0.3F;

          instance.color[0] = 0.25F + rand_01(rng) * 0.15F;
          instance.color[1] = 0.35F + rand_01(rng) * 0.25F;
          instance.color[2] = 0.15F + rand_01(rng) * 0.1F;
        }

        float const angle = rand_01(rng) * 6.28318F;
        instance.rotation[0] = 0.0F;
        instance.rotation[1] = std::sin(angle * 0.5F);
        instance.rotation[2] = 0.0F;
        instance.rotation[3] = std::cos(angle * 0.5F);

        m_assetInstances.push_back(instance);
      }
    }
  }

  m_assetInstanceCount = m_assetInstances.size();
  m_assetInstancesDirty = true;

  qDebug() << "Generated" << m_assetInstanceCount << "riverbank assets";
}

} // namespace Render::GL

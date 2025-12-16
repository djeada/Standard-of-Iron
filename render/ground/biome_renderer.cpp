#include "biome_renderer.h"
#include "../gl/buffer.h"
#include "../gl/render_constants.h"
#include "../scene_renderer.h"
#include "gl/resources.h"
#include "ground/grass_gpu.h"
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
#include <optional>
#include <qelapsedtimer.h>
#include <qglobal.h>
#include <qvectornd.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;
using namespace Render::GL::Geometry;

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

inline auto section_for(Game::Map::TerrainType type) -> int {
  switch (type) {
  case Game::Map::TerrainType::Mountain:
    return 2;
  case Game::Map::TerrainType::Hill:
    return 1;
  case Game::Map::TerrainType::Flat:
  default:
    return 0;
  }
}

} // namespace

namespace Render::GL {

BiomeRenderer::BiomeRenderer() = default;
BiomeRenderer::~BiomeRenderer() = default;

void BiomeRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_grassInstances.clear();
  m_grassInstanceBuffer.reset();
  m_grassInstanceCount = 0;
  m_grassInstancesDirty = false;

  m_grassParams.soil_color = m_biome_settings.soil_color;
  m_grassParams.wind_strength = m_biome_settings.sway_strength;
  m_grassParams.wind_speed = m_biome_settings.sway_speed;
  m_grassParams.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  m_grassParams.time = 0.0F;

  generate_grass_instances();
}

void BiomeRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  Q_UNUSED(resources);
  if (m_grassInstanceCount > 0) {
    if (!m_grassInstanceBuffer) {
      m_grassInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (m_grassInstancesDirty && m_grassInstanceBuffer) {
      m_grassInstanceBuffer->set_data(m_grassInstances, Buffer::Usage::Static);
      m_grassInstancesDirty = false;
    }
  } else {
    m_grassInstanceBuffer.reset();
    return;
  }

  if (m_grassInstanceBuffer && m_grassInstanceCount > 0) {
    GrassBatchParams params = m_grassParams;
    params.time = renderer.get_animation_time();
    renderer.grass_batch(m_grassInstanceBuffer.get(), m_grassInstanceCount,
                         params);
  }
}

void BiomeRenderer::clear() {
  m_grassInstances.clear();
  m_grassInstanceBuffer.reset();
  m_grassInstanceCount = 0;
  m_grassInstancesDirty = false;
}

void BiomeRenderer::refreshGrass() { generate_grass_instances(); }

void BiomeRenderer::generate_grass_instances() {
  QElapsedTimer timer;
  timer.start();

  m_grassInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    m_grassInstanceCount = 0;
    m_grassInstancesDirty = false;
    return;
  }

  if (m_biome_settings.patch_density < 0.01F) {
    m_grassInstanceCount = 0;
    m_grassInstancesDirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);

  // Build terrain cache for spawn validation
  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_heightData, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  // Configure spawn validator for grass
  SpawnValidationConfig config = make_grass_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = m_biome_settings.spawn_edge_padding;
  // Note: We handle river margin specially for grass (with reduced density
  // near rivers) so we disable the strict river margin check in the validator
  config.check_river_margin = false;

  SpawnValidator validator(terrain_cache, config);

  // Helper to check river margin with special riverbank density logic
  auto check_riverbank = [&](int ix, int iz, uint32_t &state) -> bool {
    constexpr int k_river_margin = 1;
    int near_river_count = 0;
    for (int dz = -k_river_margin; dz <= k_river_margin; ++dz) {
      for (int dx = -k_river_margin; dx <= k_river_margin; ++dx) {
        if (dx == 0 && dz == 0) {
          continue;
        }
        int const nx = ix + dx;
        int const nz = iz + dz;
        if (nx >= 0 && nx < m_width && nz >= 0 && nz < m_height) {
          if (terrain_cache.get_terrain_type_at(nx, nz) ==
              Game::Map::TerrainType::River) {
            near_river_count++;
          }
        }
      }
    }

    if (near_river_count > 0) {
      // Allow some grass near riverbanks with reduced density
      float const riverbank_density = 0.15F;
      if (rand_01(state) > riverbank_density) {
        return false;
      }
    }
    return true;
  };

  auto add_grass_blade = [&](float gx, float gz, uint32_t &state) {
    // Use unified spawn validator for most checks
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);

    // Special riverbank handling for grass
    if (!check_riverbank(ix, iz, state)) {
      return false;
    }

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const slope = terrain_cache.get_slope_at(ix, iz);

    float const lush_noise =
        valueNoise(world_x * 0.06F, world_z * 0.06F, m_noiseSeed ^ 0x9235U);
    float const dryness_noise =
        valueNoise(world_x * 0.12F, world_z * 0.12F, m_noiseSeed ^ 0x47d2U);
    float const dryness =
        std::clamp(dryness_noise * 0.6F + slope * 0.4F, 0.0F, 1.0F);
    QVector3D const lush_mix =
        m_biome_settings.grass_primary * (1.0F - lush_noise) +
        m_biome_settings.grass_secondary * lush_noise;
    QVector3D const color =
        lush_mix * (1.0F - dryness) + m_biome_settings.grass_dry * dryness;

    float const height =
        remap(rand_01(state), m_biome_settings.blade_height_min,
              m_biome_settings.blade_height_max) *
        tile_safe * 0.5F;
    float const width = remap(rand_01(state), m_biome_settings.blade_width_min,
                              m_biome_settings.blade_width_max) *
                        tile_safe;

    float const sway_strength = remap(rand_01(state), 0.75F, 1.25F);
    float const sway_speed = remap(rand_01(state), 0.85F, 1.15F);
    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;
    float const orientation = rand_01(state) * MathConstants::k_two_pi;

    GrassInstanceGpu instance;
    instance.pos_height = QVector4D(world_x, world_y, world_z, height);
    instance.color_width = QVector4D(color.x(), color.y(), color.z(), width);
    instance.sway_params =
        QVector4D(sway_strength, sway_speed, sway_phase, orientation);
    m_grassInstances.push_back(instance);
    return true;
  };

  auto quad_section = [&](Game::Map::TerrainType a, Game::Map::TerrainType b,
                          Game::Map::TerrainType c, Game::Map::TerrainType d) {
    int const priority_a = section_for(a);
    int const priority_b = section_for(b);
    int const priority_c = section_for(c);
    int const priority_d = section_for(d);
    int result = priority_a;
    result = std::max(result, priority_b);
    result = std::max(result, priority_c);
    result = std::max(result, priority_d);
    return result;
  };

  const int chunk_size = DefaultChunkSize;

  for (int chunk_z = 0; chunk_z < m_height - 1; chunk_z += chunk_size) {
    int const chunk_max_z = std::min(chunk_z + chunk_size, m_height - 1);
    for (int chunk_x = 0; chunk_x < m_width - 1; chunk_x += chunk_size) {
      int const chunk_max_x = std::min(chunk_x + chunk_size, m_width - 1);

      int flat_count = 0;
      int hill_count = 0;
      int mountain_count = 0;
      float chunk_height_sum = 0.0F;
      float chunk_slope_sum = 0.0F;
      int sample_count = 0;

      for (int z = chunk_z; z < chunk_max_z && z < m_height - 1; ++z) {
        for (int x = chunk_x; x < chunk_max_x && x < m_width - 1; ++x) {
          Game::Map::TerrainType const t0 =
              terrain_cache.get_terrain_type_at(x, z);
          Game::Map::TerrainType const t1 =
              terrain_cache.get_terrain_type_at(x + 1, z);
          Game::Map::TerrainType const t2 =
              terrain_cache.get_terrain_type_at(x, z + 1);
          Game::Map::TerrainType const t3 =
              terrain_cache.get_terrain_type_at(x + 1, z + 1);

          if (t0 == Game::Map::TerrainType::Mountain ||
              t1 == Game::Map::TerrainType::Mountain ||
              t2 == Game::Map::TerrainType::Mountain ||
              t3 == Game::Map::TerrainType::Mountain ||
              t0 == Game::Map::TerrainType::River ||
              t1 == Game::Map::TerrainType::River ||
              t2 == Game::Map::TerrainType::River ||
              t3 == Game::Map::TerrainType::River) {
            mountain_count++;
          } else if (t0 == Game::Map::TerrainType::Hill ||
                     t1 == Game::Map::TerrainType::Hill ||
                     t2 == Game::Map::TerrainType::Hill ||
                     t3 == Game::Map::TerrainType::Hill) {
            hill_count++;
          } else {
            flat_count++;
          }

          int const idx0 = z * m_width + x;
          int const idx1 = idx0 + 1;
          int const idx2 = (z + 1) * m_width + x;
          int const idx3 = idx2 + 1;

          float const quad_height = (m_heightData[static_cast<size_t>(idx0)] +
                                     m_heightData[static_cast<size_t>(idx1)] +
                                     m_heightData[static_cast<size_t>(idx2)] +
                                     m_heightData[static_cast<size_t>(idx3)]) *
                                    0.25F;
          chunk_height_sum += quad_height;

          float const slope0 = terrain_cache.get_slope_at(x, z);
          float const slope1 = terrain_cache.get_slope_at(x + 1, z);
          float const slope2 = terrain_cache.get_slope_at(x, z + 1);
          float const slope3 = terrain_cache.get_slope_at(x + 1, z + 1);
          float const avg_slope = (slope0 + slope1 + slope2 + slope3) * 0.25F;
          chunk_slope_sum += avg_slope;
          sample_count++;
        }
      }

      if (sample_count == 0) {
        continue;
      }

      const float usable_coverage =
          sample_count > 0
              ? float(flat_count + hill_count) / float(sample_count)
              : 0.0F;
      if (usable_coverage < 0.05F) {
        continue;
      }

      float const avg_slope = chunk_slope_sum / float(sample_count);

      uint32_t state = hash_coords(chunk_x, chunk_z, m_noiseSeed ^ 0xC915872BU);
      float const slope_penalty =
          1.0F - std::clamp(avg_slope * 1.35F, 0.0F, 0.75F);

      float const type_bias = 1.0F;
      constexpr float k_cluster_boost = 1.35F;
      float const expected_clusters =
          std::max(0.0F, m_biome_settings.patch_density * k_cluster_boost *
                             slope_penalty * type_bias * usable_coverage);
      int cluster_count = static_cast<int>(std::floor(expected_clusters));
      float const frac = expected_clusters - float(cluster_count);
      if (rand_01(state) < frac) {
        cluster_count += 1;
      }

      if (cluster_count > 0) {
        auto chunk_span_x = float(chunk_max_x - chunk_x + 1);
        auto chunk_span_z = float(chunk_max_z - chunk_z + 1);
        float const scatter_base =
            std::max(0.25F, m_biome_settings.patch_jitter);

        auto pick_cluster_center =
            [&](uint32_t &rng) -> std::optional<QVector2D> {
          constexpr int k_max_attempts = 8;
          for (int attempt = 0; attempt < k_max_attempts; ++attempt) {
            float const candidate_gx =
                float(chunk_x) + rand_01(rng) * chunk_span_x;
            float const candidate_gz =
                float(chunk_z) + rand_01(rng) * chunk_span_z;

            int const cx =
                std::clamp(int(std::round(candidate_gx)), 0, m_width - 1);
            int const cz =
                std::clamp(int(std::round(candidate_gz)), 0, m_height - 1);
            Game::Map::TerrainType const center_terrain_type =
                terrain_cache.get_terrain_type_at(cx, cz);
            if (center_terrain_type == Game::Map::TerrainType::Mountain ||
                center_terrain_type == Game::Map::TerrainType::River) {
              continue;
            }

            float const center_slope = terrain_cache.get_slope_at(cx, cz);
            if (center_slope > 0.92F) {
              continue;
            }

            return QVector2D(candidate_gx, candidate_gz);
          }
          return std::nullopt;
        };

        for (int cluster = 0; cluster < cluster_count; ++cluster) {
          auto center = pick_cluster_center(state);
          if (!center) {
            continue;
          }

          float const center_gx = center->x();
          float const center_gz = center->y();

          int blades = 6 + static_cast<int>(rand_01(state) * 6.0F);
          blades = std::max(
              4, int(std::round(blades * (0.85F + 0.3F * rand_01(state)))));
          float const scatter_radius =
              (0.45F + 0.55F * rand_01(state)) * scatter_base * tile_safe;

          for (int blade = 0; blade < blades; ++blade) {
            float const angle = rand_01(state) * MathConstants::k_two_pi;
            float const radius = scatter_radius * std::sqrt(rand_01(state));
            float const gx = center_gx + std::cos(angle) * radius / tile_safe;
            float const gz = center_gz + std::sin(angle) * radius / tile_safe;
            add_grass_blade(gx, gz, state);
          }
        }
      }
    }
  }

  const float background_density =
      std::max(0.0F, m_biome_settings.background_blade_density);
  if (background_density > 0.0F) {
    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        Game::Map::TerrainType const terrain_type =
            terrain_cache.get_terrain_type_at(x, z);
        if (terrain_type == Game::Map::TerrainType::Mountain ||
            terrain_type == Game::Map::TerrainType::Hill ||
            terrain_type == Game::Map::TerrainType::River) {
          continue;
        }

        float const slope = terrain_cache.get_slope_at(x, z);
        if (slope > 0.95F) {
          continue;
        }

        int const idx = z * m_width + x;
        uint32_t state = hash_coords(
            x, z, m_noiseSeed ^ 0x51bda7U ^ static_cast<uint32_t>(idx));
        int base_count = static_cast<int>(std::floor(background_density));
        float const frac = background_density - float(base_count);
        if (rand_01(state) < frac) {
          base_count += 1;
        }

        for (int i = 0; i < base_count; ++i) {
          float const gx = float(x) + rand_01(state);
          float const gz = float(z) + rand_01(state);
          add_grass_blade(gx, gz, state);
        }
      }
    }
  }

  m_grassInstanceCount = m_grassInstances.size();
  m_grassInstancesDirty = m_grassInstanceCount > 0;
}

} // namespace Render::GL

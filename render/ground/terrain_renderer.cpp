#include "terrain_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground/terrain_gpu.h"
#include "map/terrain.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QQuaternion>
#include <QVector2D>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <qelapsedtimer.h>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::GL::BitShift;
using namespace Render::GL::Geometry;

const QMatrix4x4 k_identity_matrix;

inline auto hashCoords(int x, int z, uint32_t salt = 0U) -> uint32_t {
  auto const ux = static_cast<uint32_t>(x * 73856093);
  auto const uz = static_cast<uint32_t>(z * 19349663);
  return ux ^ uz ^ (salt * 83492791U);
}

inline auto rand01(uint32_t &state) -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>((state >> Shift8) & Mask24Bit) / Mask24BitFloat;
  static_cast<float>(0xFFFFFF);
}

inline auto remap(float value, float minOut, float maxOut) -> float {
  return minOut + (maxOut - minOut) * value;
}

inline auto applyTint(const QVector3D &color, float tint) -> QVector3D {
  QVector3D const c = color * tint;
  return {std::clamp(c.x(), 0.0F, 1.0F), std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}

inline auto clamp01(const QVector3D &c) -> QVector3D {
  return {std::clamp(c.x(), 0.0F, 1.0F), std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}

inline auto hashTo01(uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return (h & 0x00FFFFFFU) / float(0x01000000);
}

inline auto linstep(float a, float b, float x) -> float {
  return std::clamp((x - a) / std::max(1e-6F, (b - a)), 0.0F, 1.0F);
}
inline auto smooth(float a, float b, float x) -> float {
  float const t = linstep(a, b, x);
  return t * t * (3.0F - 2.0F * t);
}

inline auto valueNoise(float x, float z, uint32_t salt = 0U) -> float {
  int x0 = int(std::floor(x));
  int z0 = int(std::floor(z));
  int x1 = x0 + 1;
  int z1 = z0 + 1;
  float tx = x - float(x0);
  float tz = z - float(z0);
  float const n00 = hashTo01(hashCoords(x0, z0, salt));
  float const n10 = hashTo01(hashCoords(x1, z0, salt));
  float const n01 = hashTo01(hashCoords(x0, z1, salt));
  float const n11 = hashTo01(hashCoords(x1, z1, salt));
  float const nx0 = n00 * (1 - tx) + n10 * tx;
  float const nx1 = n01 * (1 - tx) + n11 * tx;
  return nx0 * (1 - tz) + nx1 * tz;
}

} // namespace

namespace Render::GL {

TerrainRenderer::TerrainRenderer() = default;
TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                                const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();

  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;
  buildMeshes();
}

void TerrainRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_chunks.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.isInitialized();

  for (const auto &chunk : m_chunks) {
    if (!chunk.mesh) {
      continue;
    }

    if (use_visibility) {
      bool any_visible = false;
      for (int gz = chunk.minZ; gz <= chunk.maxZ && !any_visible; ++gz) {
        for (int gx = chunk.minX; gx <= chunk.maxX; ++gx) {
          if (visibility.stateAt(gx, gz) ==
              Game::Map::VisibilityState::Visible) {
            any_visible = true;
            break;
          }
        }
      }
      if (!any_visible) {
        continue;
      }
    }

    renderer.terrainChunk(chunk.mesh.get(), k_identity_matrix, chunk.params,
                          0x0080U, true, 0.0F);
  }
}

int TerrainRenderer::sectionFor(Game::Map::TerrainType type) {
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

void TerrainRenderer::buildMeshes() {
  QElapsedTimer timer;
  timer.start();

  m_chunks.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  float min_h = std::numeric_limits<float>::infinity();
  float max_h = -std::numeric_limits<float>::infinity();
  for (float const h : m_heightData) {
    min_h = std::min(min_h, h);
    max_h = std::max(max_h, h);
  }
  const float height_range = std::max(1e-4F, max_h - min_h);

  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;
  const int vertex_count = m_width * m_height;

  std::vector<QVector3D> positions(vertex_count);
  std::vector<QVector3D> normals(vertex_count, QVector3D(0.0F, 0.0F, 0.0F));
  std::vector<QVector3D> face_accum(vertex_count, QVector3D(0, 0, 0));

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int const idx = z * m_width + x;
      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;
      positions[idx] = QVector3D(world_x, m_heightData[idx], world_z);
    }
  }

  auto accumulate_normal = [&](int i0, int i1, int i2) {
    const QVector3D &v0 = positions[i0];
    const QVector3D &v1 = positions[i1];
    const QVector3D &v2 = positions[i2];
    QVector3D const normal = QVector3D::crossProduct(v1 - v0, v2 - v0);
    normals[i0] += normal;
    normals[i1] += normal;
    normals[i2] += normal;
  };

  auto sample_height_at = [&](float gx, float gz) {
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

  auto normal_from_heights_at = [&](float gx, float gz) {
    float const gx0 = std::clamp(gx - 1.0F, 0.0F, float(m_width - 1));
    float const gx1 = std::clamp(gx + 1.0F, 0.0F, float(m_width - 1));
    float const gz0 = std::clamp(gz - 1.0F, 0.0F, float(m_height - 1));
    float const gz1 = std::clamp(gz + 1.0F, 0.0F, float(m_height - 1));
    float const hL = sample_height_at(gx0, gz);
    float const hR = sample_height_at(gx1, gz);
    float const hD = sample_height_at(gx, gz0);
    float const hU = sample_height_at(gx, gz1);
    QVector3D const dx(2.0F * m_tile_size, hR - hL, 0.0F);
    QVector3D const dz(0.0F, hU - hD, 2.0F * m_tile_size);
    QVector3D n = QVector3D::crossProduct(dz, dx);
    if (n.lengthSquared() > 0.0F) {
      n.normalize();
    }
    return n.isNull() ? QVector3D(0, 1, 0) : n;
  };

  for (int z = 0; z < m_height - 1; ++z) {
    for (int x = 0; x < m_width - 1; ++x) {
      int const idx0 = z * m_width + x;
      int const idx1 = idx0 + 1;
      int const idx2 = (z + 1) * m_width + x;
      int const idx3 = idx2 + 1;
      accumulate_normal(idx0, idx1, idx2);
      accumulate_normal(idx2, idx1, idx3);
    }
  }

  for (int i = 0; i < vertex_count; ++i) {
    normals[i].normalize();
    if (normals[i].isNull()) {
      normals[i] = QVector3D(0.0F, 1.0F, 0.0F);
    }
    face_accum[i] = normals[i];
  }

  {
    std::vector<QVector3D> filtered = normals;
    auto getN = [&](int x, int z) -> QVector3D & {
      return normals[z * m_width + x];
    };

    for (int z = 1; z < m_height - 1; ++z) {
      for (int x = 1; x < m_width - 1; ++x) {
        const int idx = z * m_width + x;
        const float h0 = m_heightData[idx];
        const float nh = (h0 - min_h) / height_range;

        const float hL = m_heightData[z * m_width + (x - 1)];
        const float hR = m_heightData[z * m_width + (x + 1)];
        const float hD = m_heightData[(z - 1) * m_width + x];
        const float hU = m_heightData[(z + 1) * m_width + x];
        const float avg_nbr = 0.25F * (hL + hR + hD + hU);
        const float convexity = h0 - avg_nbr;

        const QVector3D n0 = normals[idx];
        const float slope = 1.0F - std::clamp(n0.y(), 0.0F, 1.0F);

        const float ridge_s = smooth(0.35F, 0.70F, slope);
        const float ridge_c = smooth(0.00F, 0.20F, convexity);
        const float ridge_factor =
            std::clamp(0.5F * ridge_s + 0.5F * ridge_c, 0.0F, 1.0F);
        const float base_boost = 0.6F * (1.0F - nh);

        QVector3D acc(0, 0, 0);
        float wsum = 0.0F;
        for (int dz = -1; dz <= 1; ++dz) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx;
            const int nz = z + dz;
            const int nIdx = nz * m_width + nx;

            const float dh = std::abs(m_heightData[nIdx] - h0);
            const QVector3D nn = getN(nx, nz);
            const float ndot = std::max(0.0F, QVector3D::dotProduct(n0, nn));

            const float w_h = 1.0F / (1.0F + 2.0F * dh);
            const float w_n = std::pow(ndot, 8.0F);
            const float w_b = 1.0F + base_boost;
            const float w_r = 1.0F - ridge_factor * 0.85F;

            const float w = w_h * w_n * w_b * w_r;
            acc += nn * w;
            wsum += w;
          }
        }

        QVector3D n_filtered = (wsum > 0.0F) ? (acc / wsum) : n0;
        n_filtered.normalize();

        const QVector3D n_orig = face_accum[idx];
        const QVector3D n_final =
            (ridge_factor > 0.0F)
                ? (n_filtered * (1.0F - ridge_factor) + n_orig * ridge_factor)
                : n_filtered;

        filtered[idx] = n_final.normalized();
      }
    }
    normals.swap(filtered);
  }

  auto quad_section = [&](Game::Map::TerrainType a, Game::Map::TerrainType b,
                          Game::Map::TerrainType c, Game::Map::TerrainType d) {
    int const priority_a = sectionFor(a);
    int const priority_b = sectionFor(b);
    int const priority_c = sectionFor(c);
    int const priority_d = sectionFor(d);
    int result = priority_a;
    result = std::max(result, priority_b);
    result = std::max(result, priority_c);
    result = std::max(result, priority_d);
    return result;
  };

  const int chunk_size = DefaultChunkSize;
  std::size_t total_triangles = 0;

  for (int chunk_z = 0; chunk_z < m_height - 1; chunk_z += chunk_size) {
    int const chunk_max_z = std::min(chunk_z + chunk_size, m_height - 1);
    for (int chunk_x = 0; chunk_x < m_width - 1; chunk_x += chunk_size) {
      int const chunk_max_x = std::min(chunk_x + chunk_size, m_width - 1);

      struct SectionData {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::unordered_map<int, unsigned int> remap;
        float heightSum = 0.0F;
        int heightCount = 0;
        float rotationDeg = 0.0F;
        bool flipU = false;
        float tint = 1.0F;
        QVector3D normalSum = QVector3D(0, 0, 0);
        float slopeSum = 0.0F;
        float heightVarSum = 0.0F;
        int statCount = 0;

        float aoSum = 0.0F;
        int aoCount = 0;
      };

      SectionData sections[3];

      uint32_t const chunk_seed = hashCoords(chunk_x, chunk_z, m_noiseSeed);
      uint32_t const variant_seed = chunk_seed ^ 0x9e3779b9U;
      float const rotation_step =
          static_cast<float>((variant_seed >> 5) & 3) * 90.0F;
      bool const flip = ((variant_seed >> 7) & 1U) != 0U;
      static const float tint_variants[7] = {0.9F,  0.94F, 0.97F, 1.0F,
                                             1.03F, 1.06F, 1.1F};
      float const tint = tint_variants[(variant_seed >> 12) % 7];

      for (auto &section : sections) {
        section.rotationDeg = rotation_step;
        section.flipU = flip;
        section.tint = tint;
      }

      auto ensure_vertex = [&](SectionData &section,
                               int globalIndex) -> unsigned int {
        auto it = section.remap.find(globalIndex);
        if (it != section.remap.end()) {
          return it->second;
        }
        Vertex v{};
        const QVector3D &pos = positions[globalIndex];
        const QVector3D &normal = normals[globalIndex];
        v.position[0] = pos.x();
        v.position[1] = pos.y();
        v.position[2] = pos.z();
        v.normal[0] = normal.x();
        v.normal[1] = normal.y();
        v.normal[2] = normal.z();

        float const tex_scale = 0.2F / std::max(1.0F, m_tile_size);
        float uu = pos.x() * tex_scale;
        float const vv = pos.z() * tex_scale;

        if (section.flipU) {
          uu = -uu;
        }
        float ru = uu;
        float rv = vv;
        switch (static_cast<int>(section.rotationDeg)) {
        case 90: {
          float const t = ru;
          ru = -rv;
          rv = t;
        } break;
        case 180:
          ru = -ru;
          rv = -rv;
          break;
        case 270: {
          float const t = ru;
          ru = rv;
          rv = -t;
        } break;
        default:
          break;
        }
        v.tex_coord[0] = ru;
        v.tex_coord[1] = rv;

        section.vertices.push_back(v);
        auto const local_index =
            static_cast<unsigned int>(section.vertices.size() - 1);
        section.remap.emplace(globalIndex, local_index);
        section.normalSum += normal;
        return local_index;
      };

      for (int z = chunk_z; z < chunk_max_z; ++z) {
        for (int x = chunk_x; x < chunk_max_x; ++x) {
          int const idx0 = z * m_width + x;
          int const idx1 = idx0 + 1;
          int const idx2 = (z + 1) * m_width + x;
          int const idx3 = idx2 + 1;

          int const section_index =
              quad_section(m_terrain_types[idx0], m_terrain_types[idx1],
                           m_terrain_types[idx2], m_terrain_types[idx3]);

          if (section_index > 0) {
            SectionData &section = sections[section_index];
            unsigned int const v0 = ensure_vertex(section, idx0);
            unsigned int const v1 = ensure_vertex(section, idx1);
            unsigned int const v2 = ensure_vertex(section, idx2);
            unsigned int const v3 = ensure_vertex(section, idx3);
            section.indices.push_back(v0);
            section.indices.push_back(v1);
            section.indices.push_back(v2);
            section.indices.push_back(v2);
            section.indices.push_back(v1);
            section.indices.push_back(v3);

            float const quad_height =
                (m_heightData[idx0] + m_heightData[idx1] + m_heightData[idx2] +
                 m_heightData[idx3]) *
                0.25F;
            section.heightSum += quad_height;
            section.heightCount += 1;

            float const nY = (normals[idx0].y() + normals[idx1].y() +
                              normals[idx2].y() + normals[idx3].y()) *
                             0.25F;
            float const slope = 1.0F - std::clamp(nY, 0.0F, 1.0F);
            section.slopeSum += slope;

            float const hmin =
                std::min(std::min(m_heightData[idx0], m_heightData[idx1]),
                         std::min(m_heightData[idx2], m_heightData[idx3]));
            float const hmax =
                std::max(std::max(m_heightData[idx0], m_heightData[idx1]),
                         std::max(m_heightData[idx2], m_heightData[idx3]));
            section.heightVarSum += (hmax - hmin);
            section.statCount += 1;

            auto H = [&](int gx, int gz) {
              gx = std::clamp(gx, 0, m_width - 1);
              gz = std::clamp(gz, 0, m_height - 1);
              return m_heightData[gz * m_width + gx];
            };
            int cx = x;
            int cz = z;
            float const hC = quad_height;
            float ao = 0.0F;
            ao += std::max(0.0F, H(cx - 1, cz) - hC);
            ao += std::max(0.0F, H(cx + 1, cz) - hC);
            ao += std::max(0.0F, H(cx, cz - 1) - hC);
            ao += std::max(0.0F, H(cx, cz + 1) - hC);
            ao = std::clamp(ao * 0.15F, 0.0F, 1.0F);
            section.aoSum += ao;
            section.aoCount += 1;
          }
        }
      }

      for (int i = 0; i < 3; ++i) {
        SectionData const &section = sections[i];
        if (section.indices.empty()) {
          continue;
        }

        auto mesh = std::make_unique<Mesh>(section.vertices, section.indices);
        if (!mesh) {
          continue;
        }

        ChunkMesh chunk;
        chunk.mesh = std::move(mesh);
        chunk.minX = chunk_x;
        chunk.maxX = chunk_max_x - 1;
        chunk.minZ = chunk_z;
        chunk.maxZ = chunk_max_z - 1;
        chunk.type = (i == 0)   ? Game::Map::TerrainType::Flat
                     : (i == 1) ? Game::Map::TerrainType::Hill
                                : Game::Map::TerrainType::Mountain;
        chunk.averageHeight =
            (section.heightCount > 0)
                ? section.heightSum / float(section.heightCount)
                : 0.0F;

        const float nh_chunk = (chunk.averageHeight - min_h) / height_range;
        const float avg_slope =
            (section.statCount > 0)
                ? (section.slopeSum / float(section.statCount))
                : 0.0F;
        const float roughness =
            (section.statCount > 0)
                ? (section.heightVarSum / float(section.statCount))
                : 0.0F;

        const float center_gx = 0.5F * (chunk.minX + chunk.maxX);
        const float center_gz = 0.5F * (chunk.minZ + chunk.maxZ);
        auto hgrid = [&](int gx, int gz) {
          gx = std::clamp(gx, 0, m_width - 1);
          gz = std::clamp(gz, 0, m_height - 1);
          return m_heightData[gz * m_width + gx];
        };
        const int cxi = int(center_gx);
        const int czi = int(center_gz);
        const float hC = hgrid(cxi, czi);
        const float hL = hgrid(cxi - 1, czi);
        const float hR = hgrid(cxi + 1, czi);
        const float hD = hgrid(cxi, czi - 1);
        const float hU = hgrid(cxi, czi + 1);
        const float convexity = hC - 0.25F * (hL + hR + hD + hU);

        const float edge_factor = smooth(0.25F, 0.55F, avg_slope);
        const float entrance_factor =
            (1.0F - edge_factor) * smooth(0.00F, 0.15F, -convexity);
        const float plateau_flat = 1.0F - smooth(0.10F, 0.25F, avg_slope);
        const float plateau_height = smooth(0.60F, 0.80F, nh_chunk);
        const float plateau_factor = plateau_flat * plateau_height;

        QVector3D const base_color =
            getTerrainColor(chunk.type, chunk.averageHeight);
        QVector3D const rock_tint = m_biomeSettings.rockLow;

        float slope_mix = std::clamp(
            avg_slope * ((chunk.type == Game::Map::TerrainType::Flat) ? 0.30F
                         : (chunk.type == Game::Map::TerrainType::Hill)
                             ? 0.60F
                             : 0.90F),
            0.0F, 1.0F);

        slope_mix += 0.15F * edge_factor;
        slope_mix -= 0.10F * entrance_factor;
        slope_mix -= 0.08F * plateau_factor;
        slope_mix = std::clamp(slope_mix, 0.0F, 1.0F);

        float const center_wx = (center_gx - half_width) * m_tile_size;
        float const center_wz = (center_gz - half_height) * m_tile_size;
        float const macro = valueNoise(center_wx * 0.02F, center_wz * 0.02F,
                                       m_noiseSeed ^ 0x51C3U);
        float const macro_shade = 0.9F + 0.2F * macro;

        float const ao_avg = (section.aoCount > 0)
                                 ? (section.aoSum / float(section.aoCount))
                                 : 0.0F;
        float const ao_shade = 1.0F - 0.35F * ao_avg;

        QVector3D avgN = section.normalSum;
        if (avgN.lengthSquared() > 0.0F) {
          avgN.normalize();
        }
        QVector3D const north(0, 0, 1);
        float const northness = std::clamp(
            QVector3D::dotProduct(avgN, north) * 0.5F + 0.5F, 0.0F, 1.0F);
        QVector3D const cool_tint(0.96F, 1.02F, 1.04F);
        QVector3D const warm_tint(1.03F, 1.0F, 0.97F);
        QVector3D const aspect_tint =
            cool_tint * northness + warm_tint * (1.0F - northness);

        float const feature_bright =
            1.0F + 0.08F * plateau_factor - 0.05F * entrance_factor;
        QVector3D const feature_tint =
            QVector3D(1.0F + 0.03F * plateau_factor - 0.03F * entrance_factor,
                      1.0F + 0.01F * plateau_factor - 0.01F * entrance_factor,
                      1.0F - 0.02F * plateau_factor + 0.03F * entrance_factor);

        chunk.tint = section.tint;

        QVector3D color =
            base_color * (1.0F - slope_mix) + rock_tint * slope_mix;
        color = applyTint(color, chunk.tint);
        color *= macro_shade;
        color.setX(color.x() * aspect_tint.x() * feature_tint.x());
        color.setY(color.y() * aspect_tint.y() * feature_tint.y());
        color.setZ(color.z() * aspect_tint.z() * feature_tint.z());
        color *= ao_shade * feature_bright;
        color = color * 0.96F + QVector3D(0.04F, 0.04F, 0.04F);
        chunk.color = clamp01(color);

        TerrainChunkParams params;
        auto tint_color = [&](const QVector3D &base) {
          return clamp01(applyTint(base, chunk.tint));
        };
        params.grassPrimary = tint_color(m_biomeSettings.grassPrimary);
        params.grassSecondary = tint_color(m_biomeSettings.grassSecondary);
        params.grassDry = tint_color(m_biomeSettings.grassDry);
        params.soilColor = tint_color(m_biomeSettings.soilColor);
        params.rockLow = tint_color(m_biomeSettings.rockLow);
        params.rockHigh = tint_color(m_biomeSettings.rockHigh);

        params.tile_size = std::max(0.001F, m_tile_size);
        params.macroNoiseScale = m_biomeSettings.terrainMacroNoiseScale;
        params.detail_noiseScale = m_biomeSettings.terrainDetailNoiseScale;

        float slope_threshold = m_biomeSettings.terrainRockThreshold;
        float sharpness_mul = 1.0F;
        if (chunk.type == Game::Map::TerrainType::Hill) {
          slope_threshold -= 0.08F;
          sharpness_mul = 1.25F;
        } else if (chunk.type == Game::Map::TerrainType::Mountain) {
          slope_threshold -= 0.16F;
          sharpness_mul = 1.60F;
        }
        slope_threshold -= 0.05F * edge_factor;
        slope_threshold += 0.04F * entrance_factor;
        slope_threshold = std::clamp(
            slope_threshold - std::clamp(avg_slope * 0.20F, 0.0F, 0.12F), 0.05F,
            0.9F);

        params.slopeRockThreshold = slope_threshold;
        params.slopeRockSharpness = std::max(
            1.0F, m_biomeSettings.terrainRockSharpness * sharpness_mul);

        float soil_height = m_biomeSettings.terrainSoilHeight;
        if (chunk.type == Game::Map::TerrainType::Hill) {
          soil_height -= 0.06F;
        } else if (chunk.type == Game::Map::TerrainType::Mountain) {
          soil_height -= 0.12F;
        }
        soil_height += 0.05F * entrance_factor - 0.03F * plateau_factor;
        params.soilBlendHeight = soil_height;

        params.soilBlendSharpness =
            std::max(0.75F, m_biomeSettings.terrainSoilSharpness *
                                (chunk.type == Game::Map::TerrainType::Mountain
                                     ? 0.80F
                                     : 0.95F));

        const uint32_t noise_key_a =
            hashCoords(chunk.minX, chunk.minZ, m_noiseSeed ^ 0xB5297A4DU);
        const uint32_t noise_key_b =
            hashCoords(chunk.minX, chunk.minZ, m_noiseSeed ^ 0x68E31DA4U);
        params.noiseOffset = QVector2D(hashTo01(noise_key_a) * 256.0F,
                                       hashTo01(noise_key_b) * 256.0F);

        float base_amp =
            m_biomeSettings.heightNoiseAmplitude *
            (0.7F + 0.3F * std::clamp(roughness * 0.6F, 0.0F, 1.0F));
        if (chunk.type == Game::Map::TerrainType::Mountain) {
          base_amp *= 1.25F;
        }
        base_amp *= (1.0F + 0.10F * edge_factor - 0.08F * plateau_factor -
                     0.06F * entrance_factor);
        params.heightNoiseStrength = base_amp;
        params.heightNoiseFrequency = m_biomeSettings.heightNoiseFrequency;

        params.ambientBoost =
            m_biomeSettings.terrainAmbientBoost *
            ((chunk.type == Game::Map::TerrainType::Mountain) ? 0.90F : 0.95F);
        params.rockDetailStrength =
            m_biomeSettings.terrainRockDetailStrength *
            (0.75F + 0.35F * std::clamp(avg_slope * 1.2F, 0.0F, 1.0F) +
             0.15F * edge_factor - 0.10F * plateau_factor -
             0.08F * entrance_factor);

        params.tint = clamp01(QVector3D(chunk.tint, chunk.tint, chunk.tint));
        params.light_direction = QVector3D(0.35F, 0.8F, 0.45F);

        chunk.params = params;

        total_triangles += chunk.mesh->getIndices().size() / 3;
        m_chunks.push_back(std::move(chunk));
      }
    }
  }
}

auto TerrainRenderer::getTerrainColor(Game::Map::TerrainType type,
                                      float height) const -> QVector3D {
  switch (type) {
  case Game::Map::TerrainType::Mountain:
    if (height > 4.0F) {
      return m_biomeSettings.rockHigh;
    }
    return m_biomeSettings.rockLow;
  case Game::Map::TerrainType::Hill: {
    float const t = std::clamp(height / 3.0F, 0.0F, 1.0F);
    QVector3D const grass = m_biomeSettings.grassSecondary * (1.0F - t) +
                            m_biomeSettings.grassDry * t;
    QVector3D const rock =
        m_biomeSettings.rockLow * (1.0F - t) + m_biomeSettings.rockHigh * t;
    float const rock_blend = std::clamp(0.25F + 0.5F * t, 0.0F, 0.75F);
    return grass * (1.0F - rock_blend) + rock * rock_blend;
  }
  case Game::Map::TerrainType::Flat:
  default: {
    float const moisture = std::clamp((height - 0.5F) * 0.2F, 0.0F, 0.4F);
    QVector3D const base = m_biomeSettings.grassPrimary * (1.0F - moisture) +
                           m_biomeSettings.grassSecondary * moisture;
    float const dry_blend = std::clamp((height - 2.0F) * 0.12F, 0.0F, 0.3F);
    return base * (1.0F - dry_blend) + m_biomeSettings.grassDry * dry_blend;
  }
  }
}

} // namespace Render::GL

#include "terrain_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/buffer.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QQuaternion>
#include <QVector2D>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

using std::uint32_t;

const QMatrix4x4 kIdentityMatrix;

inline uint32_t hashCoords(int x, int z, uint32_t salt = 0u) {
  uint32_t ux = static_cast<uint32_t>(x * 73856093);
  uint32_t uz = static_cast<uint32_t>(z * 19349663);
  return ux ^ uz ^ (salt * 83492791u);
}

inline float rand01(uint32_t &state) {
  state = state * 1664525u + 1013904223u;
  return static_cast<float>((state >> 8) & 0xFFFFFF) /
         static_cast<float>(0xFFFFFF);
}

inline float remap(float value, float minOut, float maxOut) {
  return minOut + (maxOut - minOut) * value;
}

inline QVector3D applyTint(const QVector3D &color, float tint) {
  QVector3D c = color * tint;
  return QVector3D(std::clamp(c.x(), 0.0f, 1.0f), std::clamp(c.y(), 0.0f, 1.0f),
                   std::clamp(c.z(), 0.0f, 1.0f));
}

inline QVector3D clamp01(const QVector3D &c) {
  return QVector3D(std::clamp(c.x(), 0.0f, 1.0f), std::clamp(c.y(), 0.0f, 1.0f),
                   std::clamp(c.z(), 0.0f, 1.0f));
}

inline float hashTo01(uint32_t h) {
  h ^= h >> 17;
  h *= 0xed5ad4bbu;
  h ^= h >> 11;
  h *= 0xac4c1b51u;
  h ^= h >> 15;
  h *= 0x31848babu;
  h ^= h >> 14;
  return (h & 0x00FFFFFFu) / float(0x01000000);
}

inline float linstep(float a, float b, float x) {
  return std::clamp((x - a) / std::max(1e-6f, (b - a)), 0.0f, 1.0f);
}
inline float smooth(float a, float b, float x) {
  float t = linstep(a, b, x);
  return t * t * (3.0f - 2.0f * t);
}

inline float valueNoise(float x, float z, uint32_t salt = 0u) {
  int x0 = int(std::floor(x)), z0 = int(std::floor(z));
  int x1 = x0 + 1, z1 = z0 + 1;
  float tx = x - float(x0), tz = z - float(z0);
  float n00 = hashTo01(hashCoords(x0, z0, salt));
  float n10 = hashTo01(hashCoords(x1, z0, salt));
  float n01 = hashTo01(hashCoords(x0, z1, salt));
  float n11 = hashTo01(hashCoords(x1, z1, salt));
  float nx0 = n00 * (1 - tx) + n10 * tx;
  float nx1 = n01 * (1 - tx) + n11 * tx;
  return nx0 * (1 - tz) + nx1 * tz;
}

} // namespace

namespace Render::GL {

TerrainRenderer::TerrainRenderer() = default;
TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::configure(const Game::Map::TerrainHeightMap &heightMap,
                                const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();

  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;
  buildMeshes();

  qDebug() << "TerrainRenderer configured:" << m_width << "x" << m_height
           << "grid";
}

void TerrainRenderer::submit(Renderer &renderer, ResourceManager &resources) {
  if (m_chunks.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();

  for (const auto &chunk : m_chunks) {
    if (!chunk.mesh)
      continue;

    if (useVisibility) {
      bool anyVisible = false;
      for (int gz = chunk.minZ; gz <= chunk.maxZ && !anyVisible; ++gz) {
        for (int gx = chunk.minX; gx <= chunk.maxX; ++gx) {
          if (visibility.stateAt(gx, gz) ==
              Game::Map::VisibilityState::Visible) {
            anyVisible = true;
            break;
          }
        }
      }
      if (!anyVisible)
        continue;
    }

    renderer.terrainChunk(chunk.mesh.get(), kIdentityMatrix, chunk.params,
                          0x0080u, true, 0.0f);
  }
}

int TerrainRenderer::sectionFor(Game::Map::TerrainType type) const {
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

  float minH = std::numeric_limits<float>::infinity();
  float maxH = -std::numeric_limits<float>::infinity();
  for (float h : m_heightData) {
    minH = std::min(minH, h);
    maxH = std::max(maxH, h);
  }
  const float heightRange = std::max(1e-4f, maxH - minH);

  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;
  const int vertexCount = m_width * m_height;

  std::vector<QVector3D> positions(vertexCount);
  std::vector<QVector3D> normals(vertexCount, QVector3D(0.0f, 0.0f, 0.0f));
  std::vector<QVector3D> faceAccum(vertexCount, QVector3D(0, 0, 0));

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int idx = z * m_width + x;
      float worldX = (x - halfWidth) * m_tileSize;
      float worldZ = (z - halfHeight) * m_tileSize;
      positions[idx] = QVector3D(worldX, m_heightData[idx], worldZ);
    }
  }

  auto accumulateNormal = [&](int i0, int i1, int i2) {
    const QVector3D &v0 = positions[i0];
    const QVector3D &v1 = positions[i1];
    const QVector3D &v2 = positions[i2];
    QVector3D normal = QVector3D::crossProduct(v1 - v0, v2 - v0);
    normals[i0] += normal;
    normals[i1] += normal;
    normals[i2] += normal;
  };

  auto sampleHeightAt = [&](float gx, float gz) {
    gx = std::clamp(gx, 0.0f, float(m_width - 1));
    gz = std::clamp(gz, 0.0f, float(m_height - 1));
    int x0 = int(std::floor(gx));
    int z0 = int(std::floor(gz));
    int x1 = std::min(x0 + 1, m_width - 1);
    int z1 = std::min(z0 + 1, m_height - 1);
    float tx = gx - float(x0);
    float tz = gz - float(z0);
    float h00 = m_heightData[z0 * m_width + x0];
    float h10 = m_heightData[z0 * m_width + x1];
    float h01 = m_heightData[z1 * m_width + x0];
    float h11 = m_heightData[z1 * m_width + x1];
    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;
    return h0 * (1.0f - tz) + h1 * tz;
  };

  auto normalFromHeightsAt = [&](float gx, float gz) {
    float gx0 = std::clamp(gx - 1.0f, 0.0f, float(m_width - 1));
    float gx1 = std::clamp(gx + 1.0f, 0.0f, float(m_width - 1));
    float gz0 = std::clamp(gz - 1.0f, 0.0f, float(m_height - 1));
    float gz1 = std::clamp(gz + 1.0f, 0.0f, float(m_height - 1));
    float hL = sampleHeightAt(gx0, gz);
    float hR = sampleHeightAt(gx1, gz);
    float hD = sampleHeightAt(gx, gz0);
    float hU = sampleHeightAt(gx, gz1);
    QVector3D dx(2.0f * m_tileSize, hR - hL, 0.0f);
    QVector3D dz(0.0f, hU - hD, 2.0f * m_tileSize);
    QVector3D n = QVector3D::crossProduct(dz, dx);
    if (n.lengthSquared() > 0.0f)
      n.normalize();
    return n.isNull() ? QVector3D(0, 1, 0) : n;
  };

  for (int z = 0; z < m_height - 1; ++z) {
    for (int x = 0; x < m_width - 1; ++x) {
      int idx0 = z * m_width + x;
      int idx1 = idx0 + 1;
      int idx2 = (z + 1) * m_width + x;
      int idx3 = idx2 + 1;
      accumulateNormal(idx0, idx1, idx2);
      accumulateNormal(idx2, idx1, idx3);
    }
  }

  for (int i = 0; i < vertexCount; ++i) {
    normals[i].normalize();
    if (normals[i].isNull())
      normals[i] = QVector3D(0.0f, 1.0f, 0.0f);
    faceAccum[i] = normals[i];
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
        const float nh = (h0 - minH) / heightRange;

        const float hL = m_heightData[z * m_width + (x - 1)];
        const float hR = m_heightData[z * m_width + (x + 1)];
        const float hD = m_heightData[(z - 1) * m_width + x];
        const float hU = m_heightData[(z + 1) * m_width + x];
        const float avgNbr = 0.25f * (hL + hR + hD + hU);
        const float convexity = h0 - avgNbr;

        const QVector3D n0 = normals[idx];
        const float slope = 1.0f - std::clamp(n0.y(), 0.0f, 1.0f);

        const float ridgeS = smooth(0.35f, 0.70f, slope);
        const float ridgeC = smooth(0.00f, 0.20f, convexity);
        const float ridgeFactor =
            std::clamp(0.5f * ridgeS + 0.5f * ridgeC, 0.0f, 1.0f);
        const float baseBoost = 0.6f * (1.0f - nh);

        QVector3D acc(0, 0, 0);
        float wsum = 0.0f;
        for (int dz = -1; dz <= 1; ++dz) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx;
            const int nz = z + dz;
            const int nIdx = nz * m_width + nx;

            const float dh = std::abs(m_heightData[nIdx] - h0);
            const QVector3D nn = getN(nx, nz);
            const float ndot = std::max(0.0f, QVector3D::dotProduct(n0, nn));

            const float w_h = 1.0f / (1.0f + 2.0f * dh);
            const float w_n = std::pow(ndot, 8.0f);
            const float w_b = 1.0f + baseBoost;
            const float w_r = 1.0f - ridgeFactor * 0.85f;

            const float w = w_h * w_n * w_b * w_r;
            acc += nn * w;
            wsum += w;
          }
        }

        QVector3D nFiltered = (wsum > 0.0f) ? (acc / wsum) : n0;
        nFiltered.normalize();

        const QVector3D nOrig = faceAccum[idx];
        const QVector3D nFinal =
            (ridgeFactor > 0.0f)
                ? (nFiltered * (1.0f - ridgeFactor) + nOrig * ridgeFactor)
                : nFiltered;

        filtered[idx] = nFinal.normalized();
      }
    }
    normals.swap(filtered);
  }

  auto quadSection = [&](Game::Map::TerrainType a, Game::Map::TerrainType b,
                         Game::Map::TerrainType c, Game::Map::TerrainType d) {
    int priorityA = sectionFor(a);
    int priorityB = sectionFor(b);
    int priorityC = sectionFor(c);
    int priorityD = sectionFor(d);
    int result = priorityA;
    result = std::max(result, priorityB);
    result = std::max(result, priorityC);
    result = std::max(result, priorityD);
    return result;
  };

  const int chunkSize = 16;
  std::size_t totalTriangles = 0;

  for (int chunkZ = 0; chunkZ < m_height - 1; chunkZ += chunkSize) {
    int chunkMaxZ = std::min(chunkZ + chunkSize, m_height - 1);
    for (int chunkX = 0; chunkX < m_width - 1; chunkX += chunkSize) {
      int chunkMaxX = std::min(chunkX + chunkSize, m_width - 1);

      struct SectionData {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::unordered_map<int, unsigned int> remap;
        float heightSum = 0.0f;
        int heightCount = 0;
        float rotationDeg = 0.0f;
        bool flipU = false;
        float tint = 1.0f;
        QVector3D normalSum = QVector3D(0, 0, 0);
        float slopeSum = 0.0f;
        float heightVarSum = 0.0f;
        int statCount = 0;

        float aoSum = 0.0f;
        int aoCount = 0;
      };

      SectionData sections[3];

      uint32_t chunkSeed = hashCoords(chunkX, chunkZ, m_noiseSeed);
      uint32_t variantSeed = chunkSeed ^ 0x9e3779b9u;
      float rotationStep = static_cast<float>((variantSeed >> 5) & 3) * 90.0f;
      bool flip = ((variantSeed >> 7) & 1u) != 0u;
      static const float tintVariants[7] = {0.9f,  0.94f, 0.97f, 1.0f,
                                            1.03f, 1.06f, 1.1f};
      float tint = tintVariants[(variantSeed >> 12) % 7];

      for (auto &section : sections) {
        section.rotationDeg = rotationStep;
        section.flipU = flip;
        section.tint = tint;
      }

      auto ensureVertex = [&](SectionData &section,
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

        float texScale = 0.2f / std::max(1.0f, m_tileSize);
        float uu = pos.x() * texScale;
        float vv = pos.z() * texScale;

        if (section.flipU)
          uu = -uu;
        float ru = uu, rv = vv;
        switch (static_cast<int>(section.rotationDeg)) {
        case 90: {
          float t = ru;
          ru = -rv;
          rv = t;
        } break;
        case 180:
          ru = -ru;
          rv = -rv;
          break;
        case 270: {
          float t = ru;
          ru = rv;
          rv = -t;
        } break;
        default:
          break;
        }
        v.texCoord[0] = ru;
        v.texCoord[1] = rv;

        section.vertices.push_back(v);
        unsigned int localIndex =
            static_cast<unsigned int>(section.vertices.size() - 1);
        section.remap.emplace(globalIndex, localIndex);
        section.normalSum += normal;
        return localIndex;
      };

      for (int z = chunkZ; z < chunkMaxZ; ++z) {
        for (int x = chunkX; x < chunkMaxX; ++x) {
          int idx0 = z * m_width + x;
          int idx1 = idx0 + 1;
          int idx2 = (z + 1) * m_width + x;
          int idx3 = idx2 + 1;

          int sectionIndex =
              quadSection(m_terrainTypes[idx0], m_terrainTypes[idx1],
                          m_terrainTypes[idx2], m_terrainTypes[idx3]);

          if (sectionIndex > 0) {
            SectionData &section = sections[sectionIndex];
            unsigned int v0 = ensureVertex(section, idx0);
            unsigned int v1 = ensureVertex(section, idx1);
            unsigned int v2 = ensureVertex(section, idx2);
            unsigned int v3 = ensureVertex(section, idx3);
            section.indices.push_back(v0);
            section.indices.push_back(v1);
            section.indices.push_back(v2);
            section.indices.push_back(v2);
            section.indices.push_back(v1);
            section.indices.push_back(v3);

            float quadHeight = (m_heightData[idx0] + m_heightData[idx1] +
                                m_heightData[idx2] + m_heightData[idx3]) *
                               0.25f;
            section.heightSum += quadHeight;
            section.heightCount += 1;

            float nY = (normals[idx0].y() + normals[idx1].y() +
                        normals[idx2].y() + normals[idx3].y()) *
                       0.25f;
            float slope = 1.0f - std::clamp(nY, 0.0f, 1.0f);
            section.slopeSum += slope;

            float hmin =
                std::min(std::min(m_heightData[idx0], m_heightData[idx1]),
                         std::min(m_heightData[idx2], m_heightData[idx3]));
            float hmax =
                std::max(std::max(m_heightData[idx0], m_heightData[idx1]),
                         std::max(m_heightData[idx2], m_heightData[idx3]));
            section.heightVarSum += (hmax - hmin);
            section.statCount += 1;

            auto H = [&](int gx, int gz) {
              gx = std::clamp(gx, 0, m_width - 1);
              gz = std::clamp(gz, 0, m_height - 1);
              return m_heightData[gz * m_width + gx];
            };
            int cx = x, cz = z;
            float hC = quadHeight;
            float ao = 0.0f;
            ao += std::max(0.0f, H(cx - 1, cz) - hC);
            ao += std::max(0.0f, H(cx + 1, cz) - hC);
            ao += std::max(0.0f, H(cx, cz - 1) - hC);
            ao += std::max(0.0f, H(cx, cz + 1) - hC);
            ao = std::clamp(ao * 0.15f, 0.0f, 1.0f);
            section.aoSum += ao;
            section.aoCount += 1;
          }
        }
      }

      for (int i = 0; i < 3; ++i) {
        SectionData &section = sections[i];
        if (section.indices.empty())
          continue;

        auto mesh = std::make_unique<Mesh>(section.vertices, section.indices);
        if (!mesh)
          continue;

        ChunkMesh chunk;
        chunk.mesh = std::move(mesh);
        chunk.minX = chunkX;
        chunk.maxX = chunkMaxX - 1;
        chunk.minZ = chunkZ;
        chunk.maxZ = chunkMaxZ - 1;
        chunk.type = (i == 0)   ? Game::Map::TerrainType::Flat
                     : (i == 1) ? Game::Map::TerrainType::Hill
                                : Game::Map::TerrainType::Mountain;
        chunk.averageHeight =
            (section.heightCount > 0)
                ? section.heightSum / float(section.heightCount)
                : 0.0f;

        const float nhChunk = (chunk.averageHeight - minH) / heightRange;
        const float avgSlope =
            (section.statCount > 0)
                ? (section.slopeSum / float(section.statCount))
                : 0.0f;
        const float roughness =
            (section.statCount > 0)
                ? (section.heightVarSum / float(section.statCount))
                : 0.0f;

        const float centerGX = 0.5f * (chunk.minX + chunk.maxX);
        const float centerGZ = 0.5f * (chunk.minZ + chunk.maxZ);
        auto Hgrid = [&](int gx, int gz) {
          gx = std::clamp(gx, 0, m_width - 1);
          gz = std::clamp(gz, 0, m_height - 1);
          return m_heightData[gz * m_width + gx];
        };
        const int cxi = int(centerGX);
        const int czi = int(centerGZ);
        const float hC = Hgrid(cxi, czi);
        const float hL = Hgrid(cxi - 1, czi);
        const float hR = Hgrid(cxi + 1, czi);
        const float hD = Hgrid(cxi, czi - 1);
        const float hU = Hgrid(cxi, czi + 1);
        const float convexity = hC - 0.25f * (hL + hR + hD + hU);

        const float edgeFactor = smooth(0.25f, 0.55f, avgSlope);
        const float entranceFactor =
            (1.0f - edgeFactor) * smooth(0.00f, 0.15f, -convexity);
        const float plateauFlat = 1.0f - smooth(0.10f, 0.25f, avgSlope);
        const float plateauHeight = smooth(0.60f, 0.80f, nhChunk);
        const float plateauFactor = plateauFlat * plateauHeight;

        QVector3D baseColor = getTerrainColor(chunk.type, chunk.averageHeight);
        QVector3D rockTint = m_biomeSettings.rockLow;

        float slopeMix = std::clamp(
            avgSlope * ((chunk.type == Game::Map::TerrainType::Flat)   ? 0.30f
                        : (chunk.type == Game::Map::TerrainType::Hill) ? 0.60f
                                                                       : 0.90f),
            0.0f, 1.0f);

        slopeMix += 0.15f * edgeFactor;
        slopeMix -= 0.10f * entranceFactor;
        slopeMix -= 0.08f * plateauFactor;
        slopeMix = std::clamp(slopeMix, 0.0f, 1.0f);

        float centerWX = (centerGX - halfWidth) * m_tileSize;
        float centerWZ = (centerGZ - halfHeight) * m_tileSize;
        float macro = valueNoise(centerWX * 0.02f, centerWZ * 0.02f,
                                 m_noiseSeed ^ 0x51C3u);
        float macroShade = 0.9f + 0.2f * macro;

        float aoAvg = (section.aoCount > 0)
                          ? (section.aoSum / float(section.aoCount))
                          : 0.0f;
        float aoShade = 1.0f - 0.35f * aoAvg;

        QVector3D avgN = section.normalSum;
        if (avgN.lengthSquared() > 0.0f)
          avgN.normalize();
        QVector3D north(0, 0, 1);
        float northness = std::clamp(
            QVector3D::dotProduct(avgN, north) * 0.5f + 0.5f, 0.0f, 1.0f);
        QVector3D coolTint(0.96f, 1.02f, 1.04f);
        QVector3D warmTint(1.03f, 1.0f, 0.97f);
        QVector3D aspectTint =
            coolTint * northness + warmTint * (1.0f - northness);

        float featureBright =
            1.0f + 0.08f * plateauFactor - 0.05f * entranceFactor;
        QVector3D featureTint =
            QVector3D(1.0f + 0.03f * plateauFactor - 0.03f * entranceFactor,
                      1.0f + 0.01f * plateauFactor - 0.01f * entranceFactor,
                      1.0f - 0.02f * plateauFactor + 0.03f * entranceFactor);

        chunk.tint = section.tint;

        QVector3D color = baseColor * (1.0f - slopeMix) + rockTint * slopeMix;
        color = applyTint(color, chunk.tint);
        color *= macroShade;
        color.setX(color.x() * aspectTint.x() * featureTint.x());
        color.setY(color.y() * aspectTint.y() * featureTint.y());
        color.setZ(color.z() * aspectTint.z() * featureTint.z());
        color *= aoShade * featureBright;
        color = color * 0.96f + QVector3D(0.04f, 0.04f, 0.04f);
        chunk.color = clamp01(color);

        TerrainChunkParams params;
        auto tintColor = [&](const QVector3D &base) {
          return clamp01(applyTint(base, chunk.tint));
        };
        params.grassPrimary = tintColor(m_biomeSettings.grassPrimary);
        params.grassSecondary = tintColor(m_biomeSettings.grassSecondary);
        params.grassDry = tintColor(m_biomeSettings.grassDry);
        params.soilColor = tintColor(m_biomeSettings.soilColor);
        params.rockLow = tintColor(m_biomeSettings.rockLow);
        params.rockHigh = tintColor(m_biomeSettings.rockHigh);

        params.tileSize = std::max(0.001f, m_tileSize);
        params.macroNoiseScale = m_biomeSettings.terrainMacroNoiseScale;
        params.detailNoiseScale = m_biomeSettings.terrainDetailNoiseScale;

        float slopeThreshold = m_biomeSettings.terrainRockThreshold;
        float sharpnessMul = 1.0f;
        if (chunk.type == Game::Map::TerrainType::Hill) {
          slopeThreshold -= 0.08f;
          sharpnessMul = 1.25f;
        } else if (chunk.type == Game::Map::TerrainType::Mountain) {
          slopeThreshold -= 0.16f;
          sharpnessMul = 1.60f;
        }
        slopeThreshold -= 0.05f * edgeFactor;
        slopeThreshold += 0.04f * entranceFactor;
        slopeThreshold = std::clamp(
            slopeThreshold - std::clamp(avgSlope * 0.20f, 0.0f, 0.12f), 0.05f,
            0.9f);

        params.slopeRockThreshold = slopeThreshold;
        params.slopeRockSharpness =
            std::max(1.0f, m_biomeSettings.terrainRockSharpness * sharpnessMul);

        float soilHeight = m_biomeSettings.terrainSoilHeight;
        if (chunk.type == Game::Map::TerrainType::Hill)
          soilHeight -= 0.06f;
        else if (chunk.type == Game::Map::TerrainType::Mountain)
          soilHeight -= 0.12f;
        soilHeight += 0.05f * entranceFactor - 0.03f * plateauFactor;
        params.soilBlendHeight = soilHeight;

        params.soilBlendSharpness =
            std::max(0.75f, m_biomeSettings.terrainSoilSharpness *
                                (chunk.type == Game::Map::TerrainType::Mountain
                                     ? 0.80f
                                     : 0.95f));

        const uint32_t noiseKeyA =
            hashCoords(chunk.minX, chunk.minZ, m_noiseSeed ^ 0xB5297A4Du);
        const uint32_t noiseKeyB =
            hashCoords(chunk.minX, chunk.minZ, m_noiseSeed ^ 0x68E31DA4u);
        params.noiseOffset = QVector2D(hashTo01(noiseKeyA) * 256.0f,
                                       hashTo01(noiseKeyB) * 256.0f);

        float baseAmp =
            m_biomeSettings.heightNoiseAmplitude *
            (0.7f + 0.3f * std::clamp(roughness * 0.6f, 0.0f, 1.0f));
        if (chunk.type == Game::Map::TerrainType::Mountain)
          baseAmp *= 1.25f;
        baseAmp *= (1.0f + 0.10f * edgeFactor - 0.08f * plateauFactor -
                    0.06f * entranceFactor);
        params.heightNoiseStrength = baseAmp;
        params.heightNoiseFrequency = m_biomeSettings.heightNoiseFrequency;

        params.ambientBoost =
            m_biomeSettings.terrainAmbientBoost *
            ((chunk.type == Game::Map::TerrainType::Mountain) ? 0.90f : 0.95f);
        params.rockDetailStrength =
            m_biomeSettings.terrainRockDetailStrength *
            (0.75f + 0.35f * std::clamp(avgSlope * 1.2f, 0.0f, 1.0f) +
             0.15f * edgeFactor - 0.10f * plateauFactor -
             0.08f * entranceFactor);

        params.tint = clamp01(QVector3D(chunk.tint, chunk.tint, chunk.tint));
        params.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);

        chunk.params = params;

        totalTriangles += chunk.mesh->getIndices().size() / 3;
        m_chunks.push_back(std::move(chunk));
      }
    }
  }

  qDebug() << "TerrainRenderer: built" << m_chunks.size() << "chunks in"
           << timer.elapsed() << "ms" << "triangles:" << totalTriangles;
}

QVector3D TerrainRenderer::getTerrainColor(Game::Map::TerrainType type,
                                           float height) const {
  switch (type) {
  case Game::Map::TerrainType::Mountain:
    if (height > 4.0f) {
      return m_biomeSettings.rockHigh;
    }
    return m_biomeSettings.rockLow;
  case Game::Map::TerrainType::Hill: {
    float t = std::clamp(height / 3.0f, 0.0f, 1.0f);
    QVector3D grass = m_biomeSettings.grassSecondary * (1.0f - t) +
                      m_biomeSettings.grassDry * t;
    QVector3D rock =
        m_biomeSettings.rockLow * (1.0f - t) + m_biomeSettings.rockHigh * t;
    float rockBlend = std::clamp(0.25f + 0.5f * t, 0.0f, 0.75f);
    return grass * (1.0f - rockBlend) + rock * rockBlend;
  }
  case Game::Map::TerrainType::Flat:
  default: {
    float moisture = std::clamp((height - 0.5f) * 0.2f, 0.0f, 0.4f);
    QVector3D base = m_biomeSettings.grassPrimary * (1.0f - moisture) +
                     m_biomeSettings.grassSecondary * moisture;
    float dryBlend = std::clamp((height - 2.0f) * 0.12f, 0.0f, 0.3f);
    return base * (1.0f - dryBlend) + m_biomeSettings.grassDry * dryBlend;
  }
  }
}

} // namespace Render::GL

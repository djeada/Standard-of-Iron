#include "biome_renderer.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QVector2D>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace {

using std::uint32_t;

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

inline int sectionFor(Game::Map::TerrainType type) {
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

void BiomeRenderer::configure(const Game::Map::TerrainHeightMap &heightMap,
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_grassInstances.clear();
  m_grassInstanceBuffer.reset();
  m_grassInstanceCount = 0;
  m_grassInstancesDirty = false;

  m_grassParams.soilColor = m_biomeSettings.soilColor;
  m_grassParams.windStrength = m_biomeSettings.swayStrength;
  m_grassParams.windSpeed = m_biomeSettings.swaySpeed;
  m_grassParams.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);
  m_grassParams.time = 0.0f;

  generateGrassInstances();
}

void BiomeRenderer::submit(Renderer &renderer) {
  if (m_grassInstanceCount > 0) {
    if (!m_grassInstanceBuffer) {
      m_grassInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (m_grassInstancesDirty && m_grassInstanceBuffer) {
      m_grassInstanceBuffer->setData(m_grassInstances, Buffer::Usage::Static);
      m_grassInstancesDirty = false;
    }
  } else {
    m_grassInstanceBuffer.reset();
    return;
  }

  if (m_grassInstanceBuffer && m_grassInstanceCount > 0) {
    GrassBatchParams params = m_grassParams;
    params.time = renderer.getAnimationTime();
    renderer.grassBatch(m_grassInstanceBuffer.get(), m_grassInstanceCount,
                        params);
  }
}

void BiomeRenderer::clear() {
  m_grassInstances.clear();
  m_grassInstanceBuffer.reset();
  m_grassInstanceCount = 0;
  m_grassInstancesDirty = false;
}

void BiomeRenderer::refreshGrass() { generateGrassInstances(); }

void BiomeRenderer::generateGrassInstances() {
  QElapsedTimer timer;
  timer.start();

  m_grassInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    m_grassInstanceCount = 0;
    m_grassInstancesDirty = false;
    return;
  }

  if (m_biomeSettings.patchDensity < 0.01f) {
    m_grassInstanceCount = 0;
    m_grassInstancesDirty = false;
    return;
  }

  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;
  const float tileSafe = std::max(0.001f, m_tileSize);

  std::vector<QVector3D> normals(m_width * m_height,
                                 QVector3D(0.0f, 1.0f, 0.0f));

  auto sampleHeightAt = [&](float gx, float gz) -> float {
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

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int idx = z * m_width + x;
      float gx0 = std::clamp(float(x) - 1.0f, 0.0f, float(m_width - 1));
      float gx1 = std::clamp(float(x) + 1.0f, 0.0f, float(m_width - 1));
      float gz0 = std::clamp(float(z) - 1.0f, 0.0f, float(m_height - 1));
      float gz1 = std::clamp(float(z) + 1.0f, 0.0f, float(m_height - 1));

      float hL = sampleHeightAt(gx0, float(z));
      float hR = sampleHeightAt(gx1, float(z));
      float hD = sampleHeightAt(float(x), gz0);
      float hU = sampleHeightAt(float(x), gz1);

      QVector3D dx(2.0f * m_tileSize, hR - hL, 0.0f);
      QVector3D dz(0.0f, hU - hD, 2.0f * m_tileSize);
      QVector3D n = QVector3D::crossProduct(dz, dx);
      if (n.lengthSquared() > 0.0f) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto addGrassBlade = [&](float gx, float gz, uint32_t &state) {
    float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
    float sgz = std::clamp(gz, 0.0f, float(m_height - 1));

    int ix = std::clamp(int(std::floor(sgx + 0.5f)), 0, m_width - 1);
    int iz = std::clamp(int(std::floor(sgz + 0.5f)), 0, m_height - 1);
    int normalIdx = iz * m_width + ix;

    if (m_terrainTypes[normalIdx] == Game::Map::TerrainType::Mountain ||
        m_terrainTypes[normalIdx] == Game::Map::TerrainType::Hill)
      return false;

    QVector3D normal = normals[normalIdx];
    float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
    if (slope > 0.92f)
      return false;

    float worldX = (gx - halfWidth) * m_tileSize;
    float worldZ = (gz - halfHeight) * m_tileSize;
    float worldY = sampleHeightAt(sgx, sgz);

    auto &buildingRegistry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (buildingRegistry.isPointInBuilding(worldX, worldZ)) {
      return false;
    }

    float lushNoise =
        valueNoise(worldX * 0.06f, worldZ * 0.06f, m_noiseSeed ^ 0x9235u);
    float drynessNoise =
        valueNoise(worldX * 0.12f, worldZ * 0.12f, m_noiseSeed ^ 0x47d2u);
    float dryness = std::clamp(drynessNoise * 0.6f + slope * 0.4f, 0.0f, 1.0f);
    QVector3D lushMix = m_biomeSettings.grassPrimary * (1.0f - lushNoise) +
                        m_biomeSettings.grassSecondary * lushNoise;
    QVector3D color =
        lushMix * (1.0f - dryness) + m_biomeSettings.grassDry * dryness;

    float height = remap(rand01(state), m_biomeSettings.bladeHeightMin,
                         m_biomeSettings.bladeHeightMax) *
                   tileSafe * 0.5f;
    float width = remap(rand01(state), m_biomeSettings.bladeWidthMin,
                        m_biomeSettings.bladeWidthMax) *
                  tileSafe;

    float swayStrength = remap(rand01(state), 0.75f, 1.25f);
    float swaySpeed = remap(rand01(state), 0.85f, 1.15f);
    float swayPhase = rand01(state) * 6.2831853f;
    float orientation = rand01(state) * 6.2831853f;

    GrassInstanceGpu instance;
    instance.posHeight = QVector4D(worldX, worldY, worldZ, height);
    instance.colorWidth = QVector4D(color.x(), color.y(), color.z(), width);
    instance.swayParams =
        QVector4D(swayStrength, swaySpeed, swayPhase, orientation);
    m_grassInstances.push_back(instance);
    return true;
  };

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

  for (int chunkZ = 0; chunkZ < m_height - 1; chunkZ += chunkSize) {
    int chunkMaxZ = std::min(chunkZ + chunkSize, m_height - 1);
    for (int chunkX = 0; chunkX < m_width - 1; chunkX += chunkSize) {
      int chunkMaxX = std::min(chunkX + chunkSize, m_width - 1);

      int flatCount = 0;
      int hillCount = 0;
      int mountainCount = 0;
      float chunkHeightSum = 0.0f;
      float chunkSlopeSum = 0.0f;
      int sampleCount = 0;

      for (int z = chunkZ; z < chunkMaxZ && z < m_height - 1; ++z) {
        for (int x = chunkX; x < chunkMaxX && x < m_width - 1; ++x) {
          int idx0 = z * m_width + x;
          int idx1 = idx0 + 1;
          int idx2 = (z + 1) * m_width + x;
          int idx3 = idx2 + 1;

          if (m_terrainTypes[idx0] == Game::Map::TerrainType::Mountain ||
              m_terrainTypes[idx1] == Game::Map::TerrainType::Mountain ||
              m_terrainTypes[idx2] == Game::Map::TerrainType::Mountain ||
              m_terrainTypes[idx3] == Game::Map::TerrainType::Mountain) {
            mountainCount++;
          } else if (m_terrainTypes[idx0] == Game::Map::TerrainType::Hill ||
                     m_terrainTypes[idx1] == Game::Map::TerrainType::Hill ||
                     m_terrainTypes[idx2] == Game::Map::TerrainType::Hill ||
                     m_terrainTypes[idx3] == Game::Map::TerrainType::Hill) {
            hillCount++;
          } else {
            flatCount++;
          }

          float quadHeight = (m_heightData[idx0] + m_heightData[idx1] +
                              m_heightData[idx2] + m_heightData[idx3]) *
                             0.25f;
          chunkHeightSum += quadHeight;

          float nY = (normals[idx0].y() + normals[idx1].y() +
                      normals[idx2].y() + normals[idx3].y()) *
                     0.25f;
          chunkSlopeSum += 1.0f - std::clamp(nY, 0.0f, 1.0f);
          sampleCount++;
        }
      }

      if (sampleCount == 0)
        continue;

      const float usableCoverage =
          sampleCount > 0 ? float(flatCount + hillCount) / float(sampleCount)
                          : 0.0f;
      if (usableCoverage < 0.05f)
        continue;

      bool isPrimarilyFlat = flatCount >= hillCount;

      float avgSlope = chunkSlopeSum / float(sampleCount);

      uint32_t state = hashCoords(chunkX, chunkZ, m_noiseSeed ^ 0xC915872Bu);
      float slopePenalty = 1.0f - std::clamp(avgSlope * 1.35f, 0.0f, 0.75f);

      float typeBias = 1.0f;
      constexpr float kClusterBoost = 1.35f;
      float expectedClusters =
          std::max(0.0f, m_biomeSettings.patchDensity * kClusterBoost *
                             slopePenalty * typeBias * usableCoverage);
      int clusterCount = static_cast<int>(std::floor(expectedClusters));
      float frac = expectedClusters - float(clusterCount);
      if (rand01(state) < frac)
        clusterCount += 1;

      if (clusterCount > 0) {
        float chunkSpanX = float(chunkMaxX - chunkX + 1);
        float chunkSpanZ = float(chunkMaxZ - chunkZ + 1);
        float scatterBase = std::max(0.25f, m_biomeSettings.patchJitter);

        auto pickClusterCenter =
            [&](uint32_t &rng) -> std::optional<QVector2D> {
          constexpr int kMaxAttempts = 8;
          for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            float candidateGX = float(chunkX) + rand01(rng) * chunkSpanX;
            float candidateGZ = float(chunkZ) + rand01(rng) * chunkSpanZ;

            int cx = std::clamp(int(std::round(candidateGX)), 0, m_width - 1);
            int cz = std::clamp(int(std::round(candidateGZ)), 0, m_height - 1);
            int centerIdx = cz * m_width + cx;
            if (m_terrainTypes[centerIdx] == Game::Map::TerrainType::Mountain)
              continue;

            QVector3D centerNormal = normals[centerIdx];
            float centerSlope = 1.0f - std::clamp(centerNormal.y(), 0.0f, 1.0f);
            if (centerSlope > 0.92f)
              continue;

            return QVector2D(candidateGX, candidateGZ);
          }
          return std::nullopt;
        };

        for (int cluster = 0; cluster < clusterCount; ++cluster) {
          auto center = pickClusterCenter(state);
          if (!center)
            continue;

          float centerGX = center->x();
          float centerGZ = center->y();

          int blades = 6 + static_cast<int>(rand01(state) * 6.0f);
          blades = std::max(
              4, int(std::round(blades * (0.85f + 0.3f * rand01(state)))));
          float scatterRadius =
              (0.45f + 0.55f * rand01(state)) * scatterBase * tileSafe;

          for (int blade = 0; blade < blades; ++blade) {
            float angle = rand01(state) * 6.2831853f;
            float radius = scatterRadius * std::sqrt(rand01(state));
            float gx = centerGX + std::cos(angle) * radius / tileSafe;
            float gz = centerGZ + std::sin(angle) * radius / tileSafe;
            addGrassBlade(gx, gz, state);
          }
        }
      }
    }
  }

  const float backgroundDensity =
      std::max(0.0f, m_biomeSettings.backgroundBladeDensity);
  if (backgroundDensity > 0.0f) {
    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        int idx = z * m_width + x;

        if (m_terrainTypes[idx] == Game::Map::TerrainType::Mountain ||
            m_terrainTypes[idx] == Game::Map::TerrainType::Hill)
          continue;

        QVector3D normal = normals[idx];
        float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
        if (slope > 0.95f)
          continue;

        uint32_t state = hashCoords(
            x, z, m_noiseSeed ^ 0x51bda7u ^ static_cast<uint32_t>(idx));
        int baseCount = static_cast<int>(std::floor(backgroundDensity));
        float frac = backgroundDensity - float(baseCount);
        if (rand01(state) < frac)
          baseCount += 1;

        for (int i = 0; i < baseCount; ++i) {
          float gx = float(x) + rand01(state);
          float gz = float(z) + rand01(state);
          addGrassBlade(gx, gz, state);
        }
      }
    }
  }

  m_grassInstanceCount = m_grassInstances.size();
  m_grassInstancesDirty = m_grassInstanceCount > 0;

  int debugFlatCount = 0;
  int debugHillCount = 0;
  int debugMountainCount = 0;
  for (const auto &type : m_terrainTypes) {
    if (type == Game::Map::TerrainType::Flat)
      debugFlatCount++;
    else if (type == Game::Map::TerrainType::Hill)
      debugHillCount++;
    else if (type == Game::Map::TerrainType::Mountain)
      debugMountainCount++;
  }
}

} // namespace Render::GL

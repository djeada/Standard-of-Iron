#include "stone_renderer.h"
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

} // namespace

namespace Render::GL {

StoneRenderer::StoneRenderer() = default;
StoneRenderer::~StoneRenderer() = default;

void StoneRenderer::configure(const Game::Map::TerrainHeightMap &heightMap,
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_stoneInstances.clear();
  m_stoneInstanceBuffer.reset();
  m_stoneInstanceCount = 0;
  m_stoneInstancesDirty = false;

  m_stoneParams.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);
  m_stoneParams.time = 0.0f;

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

  auto addStone = [&](float gx, float gz, uint32_t &state) -> bool {
    float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
    float sgz = std::clamp(gz, 0.0f, float(m_height - 1));

    int ix = std::clamp(int(std::floor(sgx + 0.5f)), 0, m_width - 1);
    int iz = std::clamp(int(std::floor(sgz + 0.5f)), 0, m_height - 1);
    int normalIdx = iz * m_width + ix;

    if (m_terrainTypes[normalIdx] != Game::Map::TerrainType::Flat)
      return false;

    QVector3D normal = normals[normalIdx];
    float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);

    if (slope > 0.15f)
      return false;

    float worldX = (gx - halfWidth) * m_tileSize;
    float worldZ = (gz - halfHeight) * m_tileSize;
    float worldY = sampleHeightAt(sgx, sgz);

    auto &buildingRegistry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (buildingRegistry.isPointInBuilding(worldX, worldZ)) {
      return false;
    }

    float scale = remap(rand01(state), 0.08f, 0.25f) * tileSafe;

    float colorVar = remap(rand01(state), 0.0f, 1.0f);
    QVector3D baseRock = m_biomeSettings.rockLow;
    QVector3D highRock = m_biomeSettings.rockHigh;
    QVector3D color = baseRock * (1.0f - colorVar) + highRock * colorVar;

    float brownMix = remap(rand01(state), 0.0f, 0.4f);
    QVector3D brownTint(0.45f, 0.38f, 0.30f);
    color = color * (1.0f - brownMix) + brownTint * brownMix;

    float rotation = rand01(state) * 6.2831853f;

    StoneInstanceGpu instance;
    instance.posScale = QVector4D(worldX, worldY + 0.01f, worldZ, scale);
    instance.colorRot = QVector4D(color.x(), color.y(), color.z(), rotation);
    m_stoneInstances.push_back(instance);
    return true;
  };

  const float stoneDensity = 0.15f;

  for (int z = 0; z < m_height; z += 2) {
    for (int x = 0; x < m_width; x += 2) {
      int idx = z * m_width + x;

      if (m_terrainTypes[idx] != Game::Map::TerrainType::Flat)
        continue;

      QVector3D normal = normals[idx];
      float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
      if (slope > 0.15f)
        continue;

      uint32_t state = hashCoords(
          x, z, m_noiseSeed ^ 0xABCDEF12u ^ static_cast<uint32_t>(idx));

      float worldX = (x - halfWidth) * m_tileSize;
      float worldZ = (z - halfHeight) * m_tileSize;
      float clusterNoise =
          valueNoise(worldX * 0.03f, worldZ * 0.03f, m_noiseSeed ^ 0x7F3A9B2Cu);

      if (clusterNoise < 0.6f)
        continue;

      int stoneCount = static_cast<int>(std::floor(stoneDensity));
      float frac = stoneDensity - float(stoneCount);
      if (rand01(state) < frac)
        stoneCount += 1;

      for (int i = 0; i < stoneCount; ++i) {
        float gx = float(x) + rand01(state) * 2.0f;
        float gz = float(z) + rand01(state) * 2.0f;
        addStone(gx, gz, state);
      }
    }
  }

  m_stoneInstanceCount = m_stoneInstances.size();
  m_stoneInstancesDirty = m_stoneInstanceCount > 0;
}

} // namespace Render::GL

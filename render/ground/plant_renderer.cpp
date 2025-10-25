#include "plant_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
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

PlantRenderer::PlantRenderer() = default;
PlantRenderer::~PlantRenderer() = default;

void PlantRenderer::configure(const Game::Map::TerrainHeightMap &heightMap,
                              const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_plantInstances.clear();
  m_plantInstanceBuffer.reset();
  m_plantInstanceCount = 0;
  m_plantInstancesDirty = false;

  m_plantParams.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);
  m_plantParams.time = 0.0f;
  m_plantParams.windStrength = m_biomeSettings.swayStrength;
  m_plantParams.windSpeed = m_biomeSettings.swaySpeed;

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
  const bool useVisibility = visibility.isInitialized();

  if (useVisibility) {

    std::vector<PlantInstanceGpu> visibleInstances;
    visibleInstances.reserve(m_plantInstances.size());

    for (const auto &instance : m_plantInstances) {
      float worldX = instance.posScale.x();
      float worldZ = instance.posScale.z();

      if (visibility.isVisibleWorld(worldX, worldZ)) {
        visibleInstances.push_back(instance);
      }
    }

    if (visibleInstances.empty()) {
      return;
    }

    if (!m_visibleInstanceBuffer) {
      m_visibleInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    m_visibleInstanceBuffer->setData(visibleInstances, Buffer::Usage::Stream);

    PlantBatchParams params = m_plantParams;
    params.time = renderer.getAnimationTime();
    renderer.plantBatch(m_visibleInstanceBuffer.get(),
                        static_cast<uint32_t>(visibleInstances.size()), params);
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

  const float plantDensity =
      std::clamp(m_biomeSettings.plantDensity, 0.0f, 2.0f);

  if (plantDensity < 0.01f) {
    m_plantInstanceCount = 0;
    m_plantInstancesDirty = false;
    return;
  }

  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;
  const float tileSafe = std::max(0.001f, m_tileSize);

  const float edgePadding =
      std::clamp(m_biomeSettings.spawnEdgePadding, 0.0f, 0.5f);
  const float edgeMarginX = static_cast<float>(m_width) * edgePadding;
  const float edgeMarginZ = static_cast<float>(m_height) * edgePadding;

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

  auto addPlant = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edgeMarginX || gx > m_width - 1 - edgeMarginX ||
        gz < edgeMarginZ || gz > m_height - 1 - edgeMarginZ) {
      return false;
    }

    float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
    float sgz = std::clamp(gz, 0.0f, float(m_height - 1));

    int ix = std::clamp(int(std::floor(sgx + 0.5f)), 0, m_width - 1);
    int iz = std::clamp(int(std::floor(sgz + 0.5f)), 0, m_height - 1);
    int normalIdx = iz * m_width + ix;

    if (m_terrainTypes[normalIdx] == Game::Map::TerrainType::Mountain) {
      return false;
    }

    if (m_terrainTypes[normalIdx] == Game::Map::TerrainType::River) {
      return false;
    }

    constexpr int kRiverMargin = 1;
    for (int dz = -kRiverMargin; dz <= kRiverMargin; ++dz) {
      for (int dx = -kRiverMargin; dx <= kRiverMargin; ++dx) {
        if (dx == 0 && dz == 0) {
          continue;
        }
        int nx = ix + dx;
        int nz = iz + dz;
        if (nx >= 0 && nx < m_width && nz >= 0 && nz < m_height) {
          int nIdx = nz * m_width + nx;
          if (m_terrainTypes[nIdx] == Game::Map::TerrainType::River) {
            return false;
          }
        }
      }
    }

    QVector3D normal = normals[normalIdx];
    float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);

    if (slope > 0.65f) {
      return false;
    }

    float worldX = (gx - halfWidth) * m_tileSize;
    float worldZ = (gz - halfHeight) * m_tileSize;
    float worldY = sampleHeightAt(sgx, sgz);

    auto &buildingRegistry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (buildingRegistry.isPointInBuilding(worldX, worldZ)) {
      return false;
    }

    float scale = remap(rand01(state), 0.30f, 0.80f) * tileSafe;

    float plantType = std::floor(rand01(state) * 4.0f);

    float colorVar = remap(rand01(state), 0.0f, 1.0f);
    QVector3D baseColor = m_biomeSettings.grassPrimary * 0.7f;
    QVector3D varColor = m_biomeSettings.grassSecondary * 0.8f;
    QVector3D tintColor = baseColor * (1.0f - colorVar) + varColor * colorVar;

    float brownMix = remap(rand01(state), 0.15f, 0.35f);
    QVector3D brownTint(0.55f, 0.50f, 0.35f);
    tintColor = tintColor * (1.0f - brownMix) + brownTint * brownMix;

    float swayPhase = rand01(state) * 6.2831853f;
    float swayStrength = remap(rand01(state), 0.6f, 1.2f);
    float swaySpeed = remap(rand01(state), 0.8f, 1.3f);

    float rotation = rand01(state) * 6.2831853f;

    PlantInstanceGpu instance;

    instance.posScale = QVector4D(worldX, worldY + 0.05f, worldZ, scale);
    instance.colorSway =
        QVector4D(tintColor.x(), tintColor.y(), tintColor.z(), swayPhase);
    instance.typeParams =
        QVector4D(plantType, rotation, swayStrength, swaySpeed);
    m_plantInstances.push_back(instance);
    return true;
  };

  int cellsChecked = 0;
  int cellsPassed = 0;
  int plantsAdded = 0;

  for (int z = 0; z < m_height; z += 3) {
    for (int x = 0; x < m_width; x += 3) {
      cellsChecked++;
      int idx = z * m_width + x;

      if (m_terrainTypes[idx] == Game::Map::TerrainType::Mountain ||
          m_terrainTypes[idx] == Game::Map::TerrainType::River) {
        continue;
      }

      QVector3D normal = normals[idx];
      float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
      if (slope > 0.65f) {
        continue;
      }

      uint32_t state = hashCoords(
          x, z, m_noiseSeed ^ 0x8F3C5A7Eu ^ static_cast<uint32_t>(idx));

      float worldX = (x - halfWidth) * m_tileSize;
      float worldZ = (z - halfHeight) * m_tileSize;

      float clusterNoise =
          valueNoise(worldX * 0.05f, worldZ * 0.05f, m_noiseSeed ^ 0x4B9D2F1Au);

      if (clusterNoise < 0.45f) {
        continue;
      }

      cellsPassed++;

      float densityMult = 1.0f;
      if (m_terrainTypes[idx] == Game::Map::TerrainType::Hill) {
        densityMult = 0.6f;
      }

      float effectiveDensity = plantDensity * densityMult * 2.0f;
      int plantCount = static_cast<int>(std::floor(effectiveDensity));
      float frac = effectiveDensity - float(plantCount);
      if (rand01(state) < frac) {
        plantCount += 1;
      }

      for (int i = 0; i < plantCount; ++i) {
        float gx = float(x) + rand01(state) * 3.0f;
        float gz = float(z) + rand01(state) * 3.0f;
        if (addPlant(gx, gz, state)) {
          plantsAdded++;
        }
      }
    }
  }

  m_plantInstanceCount = m_plantInstances.size();
  m_plantInstancesDirty = m_plantInstanceCount > 0;
}

} // namespace Render::GL

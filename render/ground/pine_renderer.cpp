#include "pine_renderer.h"
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

inline float valueNoise(float x, float z, uint32_t seed) {
  int ix = static_cast<int>(std::floor(x));
  int iz = static_cast<int>(std::floor(z));
  float fx = x - static_cast<float>(ix);
  float fz = z - static_cast<float>(iz);

  fx = fx * fx * (3.0f - 2.0f * fx);
  fz = fz * fz * (3.0f - 2.0f * fz);

  uint32_t s00 = hashCoords(ix, iz, seed);
  uint32_t s10 = hashCoords(ix + 1, iz, seed);
  uint32_t s01 = hashCoords(ix, iz + 1, seed);
  uint32_t s11 = hashCoords(ix + 1, iz + 1, seed);

  float v00 = rand01(s00);
  float v10 = rand01(s10);
  float v01 = rand01(s01);
  float v11 = rand01(s11);

  float v0 = v00 * (1.0f - fx) + v10 * fx;
  float v1 = v01 * (1.0f - fx) + v11 * fx;
  return v0 * (1.0f - fz) + v1 * fz;
}

} 

namespace Render::GL {

PineRenderer::PineRenderer() = default;
PineRenderer::~PineRenderer() = default;

void PineRenderer::configure(const Game::Map::TerrainHeightMap &heightMap,
                             const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_pineInstances.clear();
  m_pineInstanceBuffer.reset();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;

  m_pineParams.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);
  m_pineParams.time = 0.0f;
  m_pineParams.windStrength = 0.3f;
  m_pineParams.windSpeed = 0.5f;

  generatePineInstances();
}

void PineRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_pineInstanceCount = static_cast<uint32_t>(m_pineInstances.size());

  if (m_pineInstanceCount == 0) {
    m_pineInstanceBuffer.reset();
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();

  std::vector<PineInstanceGpu> visibleInstances;
  if (useVisibility) {
    visibleInstances.reserve(m_pineInstanceCount);
    for (const auto &instance : m_pineInstances) {
      float worldX = instance.posScale.x();
      float worldZ = instance.posScale.z();
      if (visibility.isVisibleWorld(worldX, worldZ)) {
        visibleInstances.push_back(instance);
      }
    }
  } else {
    visibleInstances = m_pineInstances;
  }

  const uint32_t visibleCount = static_cast<uint32_t>(visibleInstances.size());
  if (visibleCount == 0) {
    m_pineInstanceBuffer.reset();
    return;
  }

  if (!m_pineInstanceBuffer) {
    m_pineInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
  }

  m_pineInstanceBuffer->setData(visibleInstances, Buffer::Usage::Static);

  PineBatchParams params = m_pineParams;
  params.time = renderer.getAnimationTime();
  renderer.pineBatch(m_pineInstanceBuffer.get(), visibleCount, params);
}

void PineRenderer::clear() {
  m_pineInstances.clear();
  m_pineInstanceBuffer.reset();
  m_pineInstanceCount = 0;
  m_pineInstancesDirty = false;
}

void PineRenderer::generatePineInstances() {
  m_pineInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  const float halfWidth = static_cast<float>(m_width) * 0.5f;
  const float halfHeight = static_cast<float>(m_height) * 0.5f;
  const float tileSafe = std::max(0.1f, m_tileSize);

  const float edgePadding =
      std::clamp(m_biomeSettings.spawnEdgePadding, 0.0f, 0.5f);
  const float edgeMarginX = static_cast<float>(m_width) * edgePadding;
  const float edgeMarginZ = static_cast<float>(m_height) * edgePadding;

  float pineDensity = 0.2f;
  if (m_biomeSettings.plantDensity > 0.0f) {

    pineDensity = m_biomeSettings.plantDensity * 0.3f;
  }

  std::vector<QVector3D> normals(m_width * m_height, QVector3D(0, 1, 0));
  for (int z = 1; z < m_height - 1; ++z) {
    for (int x = 1; x < m_width - 1; ++x) {
      int idx = z * m_width + x;
      float hL = m_heightData[(z)*m_width + (x - 1)];
      float hR = m_heightData[(z)*m_width + (x + 1)];
      float hD = m_heightData[(z - 1) * m_width + (x)];
      float hU = m_heightData[(z + 1) * m_width + (x)];

      QVector3D n = QVector3D(hL - hR, 2.0f * tileSafe, hD - hU);
      if (n.lengthSquared() > 0.0f) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto addPine = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edgeMarginX || gx > m_width - 1 - edgeMarginX ||
        gz < edgeMarginZ || gz > m_height - 1 - edgeMarginZ) {
      return false;
    }

    float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
    float sgz = std::clamp(gz, 0.0f, float(m_height - 1));

    int ix = std::clamp(int(std::floor(sgx + 0.5f)), 0, m_width - 1);
    int iz = std::clamp(int(std::floor(sgz + 0.5f)), 0, m_height - 1);
    int normalIdx = iz * m_width + ix;

    QVector3D normal = normals[normalIdx];
    float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);

    if (slope > 0.75f)
      return false;

    float worldX = (gx - halfWidth) * m_tileSize;
    float worldZ = (gz - halfHeight) * m_tileSize;
    float worldY = m_heightData[normalIdx];

    auto &buildingRegistry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (buildingRegistry.isPointInBuilding(worldX, worldZ)) {
      return false;
    }

    float scale = remap(rand01(state), 3.0f, 6.0f) * tileSafe;

    float colorVar = remap(rand01(state), 0.0f, 1.0f);
    QVector3D baseColor(0.15f, 0.35f, 0.20f);
    QVector3D varColor(0.20f, 0.40f, 0.25f);
    QVector3D tintColor = baseColor * (1.0f - colorVar) + varColor * colorVar;

    float brownMix = remap(rand01(state), 0.10f, 0.25f);
    QVector3D brownTint(0.35f, 0.30f, 0.20f);
    tintColor = tintColor * (1.0f - brownMix) + brownTint * brownMix;

    float swayPhase = rand01(state) * 6.2831853f;

    float rotation = rand01(state) * 6.2831853f;

    float silhouetteSeed = rand01(state);
    float needleSeed = rand01(state);
    float barkSeed = rand01(state);

    PineInstanceGpu instance;

    instance.posScale = QVector4D(worldX, worldY, worldZ, scale);
    instance.colorSway =
        QVector4D(tintColor.x(), tintColor.y(), tintColor.z(), swayPhase);
    instance.rotation =
        QVector4D(rotation, silhouetteSeed, needleSeed, barkSeed);
    m_pineInstances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 6) {
    for (int x = 0; x < m_width; x += 6) {
      int idx = z * m_width + x;

      QVector3D normal = normals[idx];
      float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
      if (slope > 0.75f)
        continue;

      uint32_t state = hashCoords(
          x, z, m_noiseSeed ^ 0xAB12CD34u ^ static_cast<uint32_t>(idx));

      float worldX = (x - halfWidth) * m_tileSize;
      float worldZ = (z - halfHeight) * m_tileSize;

      float clusterNoise =
          valueNoise(worldX * 0.03f, worldZ * 0.03f, m_noiseSeed ^ 0x7F8E9D0Au);

      if (clusterNoise < 0.35f)
        continue;

      float densityMult = 1.0f;
      if (m_terrainTypes[idx] == Game::Map::TerrainType::Hill) {
        densityMult = 1.2f;
      } else if (m_terrainTypes[idx] == Game::Map::TerrainType::Mountain) {
        densityMult = 0.4f;
      }

      float effectiveDensity = pineDensity * densityMult * 0.8f;
      int pineCount = static_cast<int>(std::floor(effectiveDensity));
      float frac = effectiveDensity - float(pineCount);
      if (rand01(state) < frac)
        pineCount += 1;

      for (int i = 0; i < pineCount; ++i) {
        float gx = float(x) + rand01(state) * 6.0f;
        float gz = float(z) + rand01(state) * 6.0f;
        addPine(gx, gz, state);
      }
    }
  }

  m_pineInstanceCount = m_pineInstances.size();
  m_pineInstancesDirty = m_pineInstanceCount > 0;
}

} 

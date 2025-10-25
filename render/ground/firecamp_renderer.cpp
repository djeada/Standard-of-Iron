#include "firecamp_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include <QDebug>
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

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(
    const Game::Map::TerrainHeightMap &heightMap,
    const Game::Map::BiomeSettings &biomeSettings) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_fireCampInstances.clear();
  m_fireCampInstanceBuffer.reset();
  m_fireCampInstanceCount = 0;
  m_fireCampInstancesDirty = false;

  m_fireCampParams.time = 0.0f;
  m_fireCampParams.flickerSpeed = 5.0f;
  m_fireCampParams.flickerAmount = 0.02f;
  m_fireCampParams.glowStrength = 1.1f;

  generateFireCampInstances();
}

void FireCampRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_fireCampInstanceCount = static_cast<uint32_t>(m_fireCampInstances.size());

  if (m_fireCampInstanceCount == 0) {
    m_fireCampInstanceBuffer.reset();
    qWarning() << "FireCampRenderer: No instances to render";
    return;
  }

  qDebug() << "FireCampRenderer: Submitting" << m_fireCampInstanceCount
           << "fire camps";

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();

  std::vector<FireCampInstanceGpu> visibleInstances;
  if (useVisibility) {
    visibleInstances.reserve(m_fireCampInstanceCount);
    for (const auto &instance : m_fireCampInstances) {
      float worldX = instance.posIntensity.x();
      float worldZ = instance.posIntensity.z();
      if (visibility.isVisibleWorld(worldX, worldZ)) {
        visibleInstances.push_back(instance);
      }
    }
  } else {
    visibleInstances = m_fireCampInstances;
  }

  const uint32_t visibleCount = static_cast<uint32_t>(visibleInstances.size());
  if (visibleCount == 0) {
    m_fireCampInstanceBuffer.reset();
    return;
  }

  if (!m_fireCampInstanceBuffer) {
    m_fireCampInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
  }

  m_fireCampInstanceBuffer->setData(visibleInstances, Buffer::Usage::Static);

  FireCampBatchParams params = m_fireCampParams;
  params.time = renderer.getAnimationTime();
  params.flickerAmount = m_fireCampParams.flickerAmount *
                         (0.9f + 0.25f * std::sin(params.time * 1.3f));
  params.glowStrength = m_fireCampParams.glowStrength *
                        (0.85f + 0.2f * std::sin(params.time * 1.7f + 1.2f));
  renderer.firecampBatch(m_fireCampInstanceBuffer.get(), visibleCount, params);

  const QVector3D logColor(0.26f, 0.15f, 0.08f);
  const QVector3D charColor(0.08f, 0.05f, 0.03f);

  for (const auto &instance : visibleInstances) {
    const QVector4D posIntensity = instance.posIntensity;
    const QVector4D radiusPhase = instance.radiusPhase;

    const QVector3D campPos = posIntensity.toVector3D();
    const float intensity = std::clamp(posIntensity.w(), 0.6f, 1.6f);
    const float baseRadius = std::max(radiusPhase.x(), 1.0f);

    uint32_t state = hashCoords(static_cast<int>(std::floor(campPos.x())),
                                static_cast<int>(std::floor(campPos.z())),
                                static_cast<uint32_t>(radiusPhase.y() * 37.0f));

    const float time = params.time;
    const float charAmount =
        std::clamp(time * 0.015f + rand01(state) * 0.05f, 0.0f, 1.0f);

    const QVector3D blendedLogColor =
        logColor * (1.0f - charAmount) + charColor * (charAmount + 0.15f);

    const float logLength = std::clamp(baseRadius * 0.85f, 0.45f, 1.1f);
    const float logRadius = std::clamp(baseRadius * 0.08f, 0.03f, 0.08f);

    const float baseYaw = (rand01(state) - 0.5f) * 0.35f;
    const float cosBase = std::cos(baseYaw);
    const float sinBase = std::sin(baseYaw);
    const QVector3D axisA(cosBase, 0.0f, sinBase);
    const QVector3D axisB(-axisA.z(), 0.0f, axisA.x());

    const QVector3D baseCenter = campPos + QVector3D(0.0f, -0.02f, 0.0f);
    const QVector3D baseHalfA = axisA * (logLength * 0.5f);
    const QVector3D baseHalfB = axisB * (logLength * 0.45f);

    renderer.cylinder(baseCenter - baseHalfA, baseCenter + baseHalfA, logRadius,
                      blendedLogColor, 1.0f);
    renderer.cylinder(baseCenter - baseHalfB, baseCenter + baseHalfB, logRadius,
                      blendedLogColor, 1.0f);

    if (rand01(state) > 0.25f) {
      float topYaw = baseYaw + 0.6f + (rand01(state) - 0.5f) * 0.35f;
      QVector3D topAxis(std::cos(topYaw), 0.0f, std::sin(topYaw));
      QVector3D topHalf = topAxis * (logLength * 0.35f);
      QVector3D topCenter = campPos + QVector3D(0.0f, logRadius * 1.6f, 0.0f);
      float topRadius = logRadius * 0.85f;
      renderer.cylinder(topCenter - topHalf, topCenter + topHalf, topRadius,
                        blendedLogColor, 1.0f);
    }
  }
}

void FireCampRenderer::clear() {
  m_fireCampInstances.clear();
  m_fireCampInstanceBuffer.reset();
  m_fireCampInstanceCount = 0;
  m_fireCampInstancesDirty = false;
  m_explicitPositions.clear();
  m_explicitIntensities.clear();
  m_explicitRadii.clear();
}

void FireCampRenderer::setExplicitFireCamps(
    const std::vector<QVector3D> &positions,
    const std::vector<float> &intensities, const std::vector<float> &radii) {
  m_explicitPositions = positions;
  m_explicitIntensities = intensities;
  m_explicitRadii = radii;
  m_fireCampInstancesDirty = true;
  if (m_width > 0 && m_height > 0 && !m_heightData.empty()) {
    generateFireCampInstances();
  }
}

void FireCampRenderer::addExplicitFireCamps() {
  if (m_explicitPositions.empty()) {
    return;
  }

  for (size_t i = 0; i < m_explicitPositions.size(); ++i) {
    const QVector3D &pos = m_explicitPositions[i];

    float intensity = 1.0f;
    if (i < m_explicitIntensities.size()) {
      intensity = m_explicitIntensities[i];
    }

    float radius = 3.0f;
    if (i < m_explicitRadii.size()) {
      radius = m_explicitRadii[i];
    }

    float phase = static_cast<float>(i) * 1.234567f;

    FireCampInstanceGpu instance;
    instance.posIntensity = QVector4D(pos.x(), pos.y(), pos.z(), intensity);
    instance.radiusPhase = QVector4D(radius, phase, 1.0f, 0.0f);
    m_fireCampInstances.push_back(instance);
  }
}

void FireCampRenderer::generateFireCampInstances() {
  m_fireCampInstances.clear();

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

  float fireCampDensity = 0.02f;

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

  auto addFireCamp = [&](float gx, float gz, uint32_t &state) -> bool {
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

    if (slope > 0.3f) {
      return false;
    }

    float worldX = (gx - halfWidth) * m_tileSize;
    float worldZ = (gz - halfHeight) * m_tileSize;
    float worldY = m_heightData[normalIdx];

    auto &buildingRegistry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (buildingRegistry.isPointInBuilding(worldX, worldZ)) {
      return false;
    }

    float intensity = remap(rand01(state), 0.8f, 1.2f);
    float radius = remap(rand01(state), 2.0f, 4.0f) * tileSafe;

    float phase = rand01(state) * 6.2831853f;

    float duration = 1.0f;

    FireCampInstanceGpu instance;
    instance.posIntensity = QVector4D(worldX, worldY, worldZ, intensity);
    instance.radiusPhase = QVector4D(radius, phase, duration, 0.0f);
    m_fireCampInstances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 20) {
    for (int x = 0; x < m_width; x += 20) {
      int idx = z * m_width + x;

      QVector3D normal = normals[idx];
      float slope = 1.0f - std::clamp(normal.y(), 0.0f, 1.0f);
      if (slope > 0.3f) {
        continue;
      }

      uint32_t state = hashCoords(
          x, z, m_noiseSeed ^ 0xF12ECA3Fu ^ static_cast<uint32_t>(idx));

      float worldX = (x - halfWidth) * m_tileSize;
      float worldZ = (z - halfHeight) * m_tileSize;

      float clusterNoise =
          valueNoise(worldX * 0.02f, worldZ * 0.02f, m_noiseSeed ^ 0xCA3F12E0u);

      if (clusterNoise < 0.4f) {
        continue;
      }

      float densityMult = 1.0f;
      if (m_terrainTypes[idx] == Game::Map::TerrainType::Hill) {
        densityMult = 0.5f;
      } else if (m_terrainTypes[idx] == Game::Map::TerrainType::Mountain) {
        densityMult = 0.0f;
      }

      float effectiveDensity = fireCampDensity * densityMult;
      if (rand01(state) < effectiveDensity) {
        float gx = float(x) + rand01(state) * 20.0f;
        float gz = float(z) + rand01(state) * 20.0f;
        addFireCamp(gx, gz, state);
      }
    }
  }

  addExplicitFireCamps();

  m_fireCampInstanceCount = m_fireCampInstances.size();
  m_fireCampInstancesDirty = m_fireCampInstanceCount > 0;

  qDebug() << "FireCampRenderer: Generated" << m_fireCampInstanceCount
           << "total instances";
}

} // namespace Render::GL

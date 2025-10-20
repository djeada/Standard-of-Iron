#include "riverbank_asset_renderer.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QVector2D>
#include <algorithm>
#include <cmath>

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

RiverbankAssetRenderer::RiverbankAssetRenderer() = default;
RiverbankAssetRenderer::~RiverbankAssetRenderer() = default;

void RiverbankAssetRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &riverSegments,
    const Game::Map::TerrainHeightMap &heightMap,
    const Game::Map::BiomeSettings &biomeSettings) {
  m_riverSegments = riverSegments;
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();
  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_assetInstances.clear();
  m_assetInstanceBuffer.reset();
  m_assetInstanceCount = 0;
  m_assetInstancesDirty = false;

  m_assetParams.lightDirection = QVector3D(0.35f, 0.8f, 0.45f);
  m_assetParams.time = 0.0f;

  generateAssetInstances();
}

void RiverbankAssetRenderer::submit(Renderer &renderer,
                                   ResourceManager *resources) {
  Q_UNUSED(resources);
  
  if (m_assetInstanceCount > 0) {
    if (!m_assetInstanceBuffer) {
      m_assetInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    if (m_assetInstancesDirty && m_assetInstanceBuffer) {
      m_assetInstanceBuffer->setData(m_assetInstances, Buffer::Usage::Static);
      m_assetInstancesDirty = false;
    }
  } else {
    m_assetInstanceBuffer.reset();
    return;
  }

  // For now, we'll render these using the stone batch renderer
  // In a full implementation, you'd create a custom shader for riverbank assets
  // This is a simplified version that reuses existing rendering infrastructure
  if (m_assetInstanceBuffer && m_assetInstanceCount > 0) {
    // Note: This would require extending the renderer to support riverbank assets
    // For minimal changes, we'll just prepare the data
    qDebug() << "RiverbankAssetRenderer: Would render" << m_assetInstanceCount << "riverbank assets";
  }
}

void RiverbankAssetRenderer::clear() {
  m_assetInstances.clear();
  m_assetInstanceBuffer.reset();
  m_assetInstanceCount = 0;
  m_assetInstancesDirty = false;
}

void RiverbankAssetRenderer::generateAssetInstances() {
  m_assetInstances.clear();

  if (m_riverSegments.empty() || m_width < 2 || m_height < 2) {
    m_assetInstanceCount = 0;
    m_assetInstancesDirty = false;
    return;
  }

  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;

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

  // Generate assets along each river segment
  for (size_t segIdx = 0; segIdx < m_riverSegments.size(); ++segIdx) {
    const auto &segment = m_riverSegments[segIdx];
    
    QVector3D dir = segment.end - segment.start;
    float length = dir.length();
    if (length < 0.01f)
      continue;

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfRiverWidth = segment.width * 0.5f;
    float bankZoneWidth = 1.5f; // Zone where we place assets

    // Number of potential asset positions along the river
    int numSteps = static_cast<int>(length / 0.8f) + 1;
    
    uint32_t rng = m_noiseSeed + static_cast<uint32_t>(segIdx * 1000);

    for (int i = 0; i < numSteps; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(std::max(numSteps - 1, 1));
      QVector3D centerPos = segment.start + dir * (length * t);

      // Random placement on either side of river
      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        
        // Randomly decide if we place an asset here
        if (rand01(rng) > 0.3f) continue; // 30% chance to place asset
        
        // Random position within bank zone
        float distFromWater = halfRiverWidth + rand01(rng) * bankZoneWidth;
        float alongRiver = (rand01(rng) - 0.5f) * 0.6f;
        
        QVector3D assetPos = centerPos + 
                            perpendicular * (sideSign * distFromWater) +
                            dir * alongRiver;

        // Convert to grid coordinates
        float gx = (assetPos.x() / m_tileSize) + halfWidth;
        float gz = (assetPos.z() / m_tileSize) + halfHeight;

        if (gx < 0 || gx >= m_width - 1 || gz < 0 || gz >= m_height - 1)
          continue;

        int ix = static_cast<int>(gx);
        int iz = static_cast<int>(gz);
        int idx = iz * m_width + ix;

        // Don't place on rivers, mountains, or hills
        if (m_terrainTypes[idx] != Game::Map::TerrainType::Flat)
          continue;

        float worldY = sampleHeightAt(gx, gz);
        
        RiverbankAssetInstanceGpu instance;
        instance.position[0] = assetPos.x();
        instance.position[1] = worldY;
        instance.position[2] = assetPos.z();

        // Random asset type
        float typeRand = rand01(rng);
        if (typeRand < 0.7f) {
          // Pebble (most common)
          instance.assetType = 0.0f;
          float size = 0.05f + rand01(rng) * 0.1f;
          instance.scale[0] = size * (0.8f + rand01(rng) * 0.4f);
          instance.scale[1] = size * (0.6f + rand01(rng) * 0.3f);
          instance.scale[2] = size * (0.8f + rand01(rng) * 0.4f);
          
          // Gray/brown pebble colors
          float colorVar = 0.3f + rand01(rng) * 0.4f;
          instance.color[0] = colorVar;
          instance.color[1] = colorVar * 0.9f;
          instance.color[2] = colorVar * 0.85f;
        } else if (typeRand < 0.9f) {
          // Small rock
          instance.assetType = 1.0f;
          float size = 0.1f + rand01(rng) * 0.15f;
          instance.scale[0] = size;
          instance.scale[1] = size * (0.7f + rand01(rng) * 0.4f);
          instance.scale[2] = size;
          
          // Rock colors
          float colorVar = 0.35f + rand01(rng) * 0.25f;
          instance.color[0] = colorVar;
          instance.color[1] = colorVar * 0.95f;
          instance.color[2] = colorVar * 0.9f;
        } else {
          // Reed cluster (near water)
          if (distFromWater > halfRiverWidth + 0.5f)
            continue; // Only near water edge
            
          instance.assetType = 2.0f;
          float size = 0.3f + rand01(rng) * 0.4f;
          instance.scale[0] = size * 0.3f;
          instance.scale[1] = size;
          instance.scale[2] = size * 0.3f;
          
          // Green/brown reed colors
          instance.color[0] = 0.25f + rand01(rng) * 0.15f;
          instance.color[1] = 0.35f + rand01(rng) * 0.25f;
          instance.color[2] = 0.15f + rand01(rng) * 0.1f;
        }

        // Random rotation
        float angle = rand01(rng) * 6.28318f; // 2*PI
        instance.rotation[0] = 0.0f;
        instance.rotation[1] = std::sin(angle * 0.5f);
        instance.rotation[2] = 0.0f;
        instance.rotation[3] = std::cos(angle * 0.5f);

        m_assetInstances.push_back(instance);
      }
    }
  }

  m_assetInstanceCount = m_assetInstances.size();
  m_assetInstancesDirty = true;
  
  qDebug() << "Generated" << m_assetInstanceCount << "riverbank assets";
}

} // namespace Render::GL

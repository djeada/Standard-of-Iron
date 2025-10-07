#include "terrain.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace Game::Map {

TerrainHeightMap::TerrainHeightMap(int width, int height, float tileSize)
    : m_width(width), m_height(height), m_tileSize(tileSize) {
  const int count = width * height;
  m_heights.resize(count, 0.0f);
  m_terrainTypes.resize(count, TerrainType::Flat);
  m_hillEntrances.resize(count, false);
  m_hillWalkable.resize(count, false);
}

void TerrainHeightMap::buildFromFeatures(
    const std::vector<TerrainFeature> &features) {

  std::fill(m_heights.begin(), m_heights.end(), 0.0f);
  std::fill(m_terrainTypes.begin(), m_terrainTypes.end(), TerrainType::Flat);
  std::fill(m_hillEntrances.begin(), m_hillEntrances.end(), false);
  std::fill(m_hillWalkable.begin(), m_hillWalkable.end(), false);

  qDebug() << "Building terrain from" << features.size() << "features";

  const float gridHalfWidth = m_width * 0.5f - 0.5f;
  const float gridHalfHeight = m_height * 0.5f - 0.5f;

  for (const auto &feature : features) {
    qDebug() << "  Feature:"
             << (feature.type == TerrainType::Mountain ? "Mountain"
                 : feature.type == TerrainType::Hill   ? "Hill"
                                                       : "Flat")
             << "at (" << feature.centerX << "," << feature.centerZ << ")"
             << "radius:" << feature.radius << "height:" << feature.height;

    const float gridCenterX = (feature.centerX / m_tileSize) + gridHalfWidth;
    const float gridCenterZ = (feature.centerZ / m_tileSize) + gridHalfHeight;
    const float gridRadius = std::max(feature.radius / m_tileSize, 1.0f);

    if (feature.type == TerrainType::Mountain) {
      const float majorRadius = std::max(gridRadius * 1.8f, gridRadius + 3.0f);
      const float minorRadius = std::max(gridRadius * 0.22f, 0.8f);
      const float bound = std::max(majorRadius, minorRadius) + 2.0f;
      const int minX = std::max(0, int(std::floor(gridCenterX - bound)));
      const int maxX =
          std::min(m_width - 1, int(std::ceil(gridCenterX + bound)));
      const int minZ = std::max(0, int(std::floor(gridCenterZ - bound)));
      const int maxZ =
          std::min(m_height - 1, int(std::ceil(gridCenterZ + bound)));

      const float angleRad = feature.rotationDeg * float(M_PI / 180.0f);
      const float cosA = std::cos(angleRad);
      const float sinA = std::sin(angleRad);

      for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
          const float localX = float(x) - gridCenterX;
          const float localZ = float(z) - gridCenterZ;

          const float rotatedX = localX * cosA + localZ * sinA;
          const float rotatedZ = -localX * sinA + localZ * cosA;

          const float norm =
              std::sqrt((rotatedX * rotatedX) / (majorRadius * majorRadius) +
                        (rotatedZ * rotatedZ) / (minorRadius * minorRadius));

          if (norm <= 1.0f) {
            float blend = std::clamp(1.0f - norm, 0.0f, 1.0f);

            float height = feature.height * std::pow(blend, 3.5f);
            if (blend > 0.92f) {
              height = feature.height;
            }

            if (height > 0.01f) {
              int idx = indexAt(x, z);
              if (height > m_heights[idx]) {
                m_heights[idx] = height;
                m_terrainTypes[idx] = TerrainType::Mountain;
              }
            }
          }
        }
      }
      continue;
    }

    if (feature.type == TerrainType::Hill) {
      const float plateauRadius = std::max(1.5f, gridRadius * 0.45f);
      const float slopeRadius = std::max(plateauRadius + 1.5f, gridRadius);
      const int minX =
          std::max(0, int(std::floor(gridCenterX - slopeRadius - 1.0f)));
      const int maxX = std::min(
          m_width - 1, int(std::ceil(gridCenterX + slopeRadius + 1.0f)));
      const int minZ =
          std::max(0, int(std::floor(gridCenterZ - slopeRadius - 1.0f)));
      const int maxZ = std::min(
          m_height - 1, int(std::ceil(gridCenterZ + slopeRadius + 1.0f)));

      std::vector<int> plateauCells;
      plateauCells.reserve(int(M_PI * plateauRadius * plateauRadius));

      const float slopeSpan = std::max(1.0f, slopeRadius - plateauRadius);

      for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
          const float dx = float(x) - gridCenterX;
          const float dz = float(z) - gridCenterZ;
          const float dist = std::sqrt(dx * dx + dz * dz);

          if (dist > slopeRadius) {
            continue;
          }

          const int idx = indexAt(x, z);

          float height = 0.0f;
          if (dist <= plateauRadius) {
            height = feature.height;
            plateauCells.push_back(idx);
          } else {
            float t =
                std::clamp((dist - plateauRadius) / slopeSpan, 0.0f, 1.0f);
            float smooth = 0.5f * (1.0f + std::cos(t * float(M_PI)));
            height = feature.height * smooth;
          }

          if (height > m_heights[idx]) {
            m_heights[idx] = height;
            m_terrainTypes[idx] = TerrainType::Hill;
          }
        }
      }

      for (int idx : plateauCells) {
        m_hillWalkable[idx] = true;
      }

      for (const auto &entrance : feature.entrances) {
        int ex = int(std::round(entrance.x()));
        int ez = int(std::round(entrance.z()));
        if (!inBounds(ex, ez)) {
          continue;
        }

        const int entranceIdx = indexAt(ex, ez);
        m_hillEntrances[entranceIdx] = true;
        m_hillWalkable[entranceIdx] = true;

        float dirX = gridCenterX - float(ex);
        float dirZ = gridCenterZ - float(ez);
        float length = std::sqrt(dirX * dirX + dirZ * dirZ);
        if (length < 0.001f) {
          continue;
        }

        dirX /= length;
        dirZ /= length;

        float curX = float(ex);
        float curZ = float(ez);
        const int steps = int(length) + 3;

        for (int step = 0; step < steps; ++step) {
          int ix = int(std::round(curX));
          int iz = int(std::round(curZ));
          if (!inBounds(ix, iz)) {
            break;
          }

          const int idx = indexAt(ix, iz);
          float cellDist =
              std::sqrt((float(ix) - gridCenterX) * (float(ix) - gridCenterX) +
                        (float(iz) - gridCenterZ) * (float(iz) - gridCenterZ));
          if (cellDist > slopeRadius + 1.0f) {
            break;
          }

          m_hillWalkable[idx] = true;
          if (m_terrainTypes[idx] != TerrainType::Mountain) {
            m_terrainTypes[idx] = TerrainType::Hill;
          }

          if (m_heights[idx] < feature.height * 0.25f) {
            float t = std::clamp(cellDist / slopeRadius, 0.0f, 1.0f);
            float rampHeight = feature.height * (1.0f - t * 0.85f);
            m_heights[idx] = std::max(m_heights[idx], rampHeight);
          }

          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              if (ox == 0 && oz == 0)
                continue;
              int nx = ix + ox;
              int nz = iz + oz;
              if (!inBounds(nx, nz))
                continue;

              float neighborDist = std::sqrt(
                  (float(nx) - gridCenterX) * (float(nx) - gridCenterX) +
                  (float(nz) - gridCenterZ) * (float(nz) - gridCenterZ));
              if (neighborDist <= slopeRadius + 0.5f) {
                int nIdx = indexAt(nx, nz);
                if (m_terrainTypes[nIdx] != TerrainType::Mountain) {
                  m_hillWalkable[nIdx] = true;
                  if (m_terrainTypes[nIdx] == TerrainType::Flat) {
                    m_terrainTypes[nIdx] = TerrainType::Hill;
                  }
                  if (m_heights[nIdx] < m_heights[idx] * 0.8f) {
                    m_heights[nIdx] =
                        std::max(m_heights[nIdx], m_heights[idx] * 0.7f);
                  }
                }
              }
            }
          }

          if (cellDist <= plateauRadius + 0.5f) {
            break;
          }

          curX += dirX;
          curZ += dirZ;
        }
      }

      continue;
    }

    const float flatRadius = gridRadius;
    const int minX = std::max(0, int(std::floor(gridCenterX - flatRadius)));
    const int maxX =
        std::min(m_width - 1, int(std::ceil(gridCenterX + flatRadius)));
    const int minZ = std::max(0, int(std::floor(gridCenterZ - flatRadius)));
    const int maxZ =
        std::min(m_height - 1, int(std::ceil(gridCenterZ + flatRadius)));

    for (int z = minZ; z <= maxZ; ++z) {
      for (int x = minX; x <= maxX; ++x) {
        const float dx = float(x) - gridCenterX;
        const float dz = float(z) - gridCenterZ;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > flatRadius)
          continue;

        float t = dist / std::max(flatRadius, 0.0001f);
        float height = feature.height * (1.0f - t);
        if (height <= 0.0f)
          continue;

        int idx = indexAt(x, z);
        if (height > m_heights[idx]) {
          m_heights[idx] = height;
          m_terrainTypes[idx] = TerrainType::Flat;
        }
      }
    }
  }
}

float TerrainHeightMap::getHeightAt(float worldX, float worldZ) const {

  const float gridHalfWidth = m_width * 0.5f - 0.5f;
  const float gridHalfHeight = m_height * 0.5f - 0.5f;

  float gx = worldX / m_tileSize + gridHalfWidth;
  float gz = worldZ / m_tileSize + gridHalfHeight;

  int x0 = int(std::floor(gx));
  int z0 = int(std::floor(gz));
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  if (!inBounds(x0, z0))
    return 0.0f;

  float tx = gx - x0;
  float tz = gz - z0;

  float h00 = inBounds(x0, z0) ? m_heights[indexAt(x0, z0)] : 0.0f;
  float h10 = inBounds(x1, z0) ? m_heights[indexAt(x1, z0)] : 0.0f;
  float h01 = inBounds(x0, z1) ? m_heights[indexAt(x0, z1)] : 0.0f;
  float h11 = inBounds(x1, z1) ? m_heights[indexAt(x1, z1)] : 0.0f;

  float h0 = h00 * (1.0f - tx) + h10 * tx;
  float h1 = h01 * (1.0f - tx) + h11 * tx;

  return h0 * (1.0f - tz) + h1 * tz;
}

float TerrainHeightMap::getHeightAtGrid(int gridX, int gridZ) const {
  if (!inBounds(gridX, gridZ))
    return 0.0f;
  return m_heights[indexAt(gridX, gridZ)];
}

bool TerrainHeightMap::isWalkable(int gridX, int gridZ) const {
  if (!inBounds(gridX, gridZ))
    return false;

  TerrainType type = m_terrainTypes[indexAt(gridX, gridZ)];

  if (type == TerrainType::Mountain)
    return false;

  if (type == TerrainType::Hill) {
    return m_hillWalkable[indexAt(gridX, gridZ)];
  }

  return true;
}

bool TerrainHeightMap::isHillEntrance(int gridX, int gridZ) const {
  if (!inBounds(gridX, gridZ))
    return false;
  return m_hillEntrances[indexAt(gridX, gridZ)];
}

TerrainType TerrainHeightMap::getTerrainType(int gridX, int gridZ) const {
  if (!inBounds(gridX, gridZ))
    return TerrainType::Flat;
  return m_terrainTypes[indexAt(gridX, gridZ)];
}

int TerrainHeightMap::indexAt(int x, int z) const { return z * m_width + x; }

bool TerrainHeightMap::inBounds(int x, int z) const {
  return x >= 0 && x < m_width && z >= 0 && z < m_height;
}

float TerrainHeightMap::calculateFeatureHeight(const TerrainFeature &feature,
                                               float worldX,
                                               float worldZ) const {
  float dx = worldX - feature.centerX;
  float dz = worldZ - feature.centerZ;
  float dist = std::sqrt(dx * dx + dz * dz);

  if (dist > feature.radius)
    return 0.0f;

  float t = dist / feature.radius;
  float heightFactor = (std::cos(t * M_PI) + 1.0f) * 0.5f;

  return feature.height * heightFactor;
}

} // namespace Game::Map

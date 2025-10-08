#include "terrain_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QQuaternion>
#include <algorithm>
#include <cmath>
#include <unordered_map>

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

void TerrainRenderer::configure(const Game::Map::TerrainHeightMap &heightMap) {
  m_width = heightMap.getWidth();
  m_height = heightMap.getHeight();
  m_tileSize = heightMap.getTileSize();

  m_heightData = heightMap.getHeightData();
  m_terrainTypes = heightMap.getTerrainTypes();
  buildMeshes();

  qDebug() << "TerrainRenderer configured:" << m_width << "x" << m_height
           << "grid";
}

void TerrainRenderer::submit(Renderer &renderer, ResourceManager &resources) {
  if (m_chunks.empty()) {
    return;
  }

  Texture *white = resources.white();
  if (!white)
    return;

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();

  Mesh *unitMesh = resources.unit();
  Mesh *quadMesh = resources.quad();
  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;

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

  auto normalFromHeights = [&](float gx, float gz) {
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

    QMatrix4x4 model;
    renderer.mesh(chunk.mesh.get(), model, chunk.color, white, 1.0f);
  }

  for (const auto &prop : m_props) {
    if (useVisibility) {
      int gridX = static_cast<int>(
          std::floor(prop.position.x() / m_tileSize + halfWidth + 0.5f));
      int gridZ = static_cast<int>(
          std::floor(prop.position.z() / m_tileSize + halfHeight + 0.5f));
      gridX = std::clamp(gridX, 0, m_width - 1);
      gridZ = std::clamp(gridZ, 0, m_height - 1);
      if (visibility.stateAt(gridX, gridZ) !=
          Game::Map::VisibilityState::Visible)
        continue;
    }

    Mesh *mesh = nullptr;
    switch (prop.type) {
    case PropType::Pebble:
      mesh = unitMesh;
      break;
    case PropType::Tuft:
      mesh = unitMesh;
      break;
    case PropType::Stick:
      mesh = unitMesh;
      break;
    }
    if (!mesh)
      continue;

    float gx = prop.position.x() / m_tileSize + halfWidth;
    float gz = prop.position.z() / m_tileSize + halfHeight;
    QVector3D n = normalFromHeights(gx, gz);
    float slope = 1.0f - std::clamp(n.y(), 0.0f, 1.0f);

    QQuaternion tilt = QQuaternion::rotationTo(QVector3D(0, 1, 0), n);

    QMatrix4x4 model;
    model.translate(prop.position);
    model.rotate(tilt);
    model.rotate(prop.rotationDeg, 0.0f, 1.0f, 0.0f);

    QVector3D scale = prop.scale;
    float along = 1.0f + 0.25f * (1.0f - slope);
    float across = 1.0f - 0.15f * (1.0f - slope);
    scale.setX(scale.x() * across);
    scale.setZ(scale.z() * along);
    model.scale(scale);

    QVector3D propColor = prop.color;
    float shade = 0.9f + 0.2f * (1.0f - slope);
    propColor *= shade;
    renderer.mesh(mesh, model, clamp01(propColor), white, prop.alpha);

    if (quadMesh) {
      QMatrix4x4 decal;
      decal.translate(prop.position.x(), prop.position.y() + 0.01f,
                      prop.position.z());
      decal.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
      decal.rotate(tilt);
      float scaleBoost = (prop.type == PropType::Tuft)
                             ? 1.25f
                             : (prop.type == PropType::Pebble ? 1.6f : 1.4f);
      decal.scale(prop.scale.x() * scaleBoost, prop.scale.z() * scaleBoost,
                  1.0f);
      float ao = (prop.type == PropType::Tuft)
                     ? 0.22f
                     : (prop.type == PropType::Pebble ? 0.42f : 0.35f);
      ao = std::clamp(ao + 0.15f * slope, 0.18f, 0.55f);
      QVector3D aoColor(0.05f, 0.05f, 0.048f);
      renderer.mesh(quadMesh, decal, aoColor, white, ao);
    }
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
  m_props.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  const float halfWidth = m_width * 0.5f - 0.5f;
  const float halfHeight = m_height * 0.5f - 0.5f;
  const int vertexCount = m_width * m_height;

  std::vector<QVector3D> positions(vertexCount);
  std::vector<QVector3D> normals(vertexCount, QVector3D(0.0f, 0.0f, 0.0f));

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
    if (normals[i].isNull()) {
      normals[i] = QVector3D(0.0f, 1.0f, 0.0f);
    }
  }

  {
    std::vector<QVector3D> smoothed = normals;
    auto getN = [&](int x, int z) -> QVector3D & {
      return normals[z * m_width + x];
    };
    for (int z = 1; z < m_height - 1; ++z) {
      for (int x = 1; x < m_width - 1; ++x) {
        QVector3D acc(0, 0, 0);
        for (int dz = -1; dz <= 1; ++dz)
          for (int dx = -1; dx <= 1; ++dx)
            acc += getN(x + dx, z + dz);
        acc.normalize();
        smoothed[z * m_width + x] = acc;
      }
    }
    normals.swap(smoothed);
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

      uint32_t chunkSeed = hashCoords(chunkX, chunkZ);
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
        int gx = globalIndex % m_width;
        int gz = globalIndex / m_width;
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

          float maxHeight =
              std::max(std::max(m_heightData[idx0], m_heightData[idx1]),
                       std::max(m_heightData[idx2], m_heightData[idx3]));
          if (maxHeight <= 0.05f) {
            continue;
          }

          int sectionIndex =
              quadSection(m_terrainTypes[idx0], m_terrainTypes[idx1],
                          m_terrainTypes[idx2], m_terrainTypes[idx3]);

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

      for (int i = 0; i < 3; ++i) {
        SectionData &section = sections[i];
        if (section.indices.empty()) {
          continue;
        }

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

        QVector3D baseColor = getTerrainColor(chunk.type, chunk.averageHeight);
        float avgSlope = (section.statCount > 0)
                             ? (section.slopeSum / float(section.statCount))
                             : 0.0f;
        float roughness =
            (section.statCount > 0)
                ? (section.heightVarSum / float(section.statCount))
                : 0.0f;
        QVector3D rockTint(0.52f, 0.49f, 0.47f);
        float slopeMix = std::clamp(
            avgSlope *
                ((chunk.type == Game::Map::TerrainType::Flat)
                     ? 0.3f
                     : (chunk.type == Game::Map::TerrainType::Hill ? 0.6f
                                                                   : 0.85f)),
            0.0f, 1.0f);

        QVector3D avgN = section.normalSum;
        if (avgN.lengthSquared() > 0.0f)
          avgN.normalize();
        QVector3D lightDir = QVector3D(0.35f, 0.8f, 0.45f);
        lightDir.normalize();
        float ndl = std::clamp(
            QVector3D::dotProduct(avgN, lightDir) * 0.5f + 0.5f, 0.0f, 1.0f);
        float dirShade = 0.9f + 0.25f * ndl;

        float valleyShade =
            1.0f -
            std::clamp((4.0f - chunk.averageHeight) * 0.06f, 0.0f, 0.15f);
        float roughShade = 1.0f - std::clamp(roughness * 0.35f, 0.0f, 0.2f);

        QVector3D north(0, 0, 1);
        float northness = std::clamp(
            QVector3D::dotProduct(avgN, north) * 0.5f + 0.5f, 0.0f, 1.0f);
        QVector3D coolTint(0.95f, 1.03f, 1.05f);
        QVector3D warmTint(1.05f, 1.0f, 0.95f);
        QVector3D aspectTint =
            coolTint * northness + warmTint * (1.0f - northness);

        float centerGX = 0.5f * (chunk.minX + chunk.maxX);
        float centerGZ = 0.5f * (chunk.minZ + chunk.maxZ);
        float centerWX = (centerGX - halfWidth) * m_tileSize;
        float centerWZ = (centerGZ - halfHeight) * m_tileSize;
        float macro = valueNoise(centerWX * 0.02f, centerWZ * 0.02f, 1337u);
        float macroShade = 0.9f + 0.2f * macro;

        float aoAvg = (section.aoCount > 0)
                          ? (section.aoSum / float(section.aoCount))
                          : 0.0f;
        float aoShade = 1.0f - 0.35f * aoAvg;

        chunk.tint = section.tint;
        QVector3D color = baseColor * (1.0f - slopeMix) + rockTint * slopeMix;
        color = applyTint(color, chunk.tint);
        color *= dirShade * valleyShade * roughShade * macroShade;
        color.setX(color.x() * aspectTint.x());
        color.setY(color.y() * aspectTint.y());
        color.setZ(color.z() * aspectTint.z());
        color *= aoShade;
        color = color * 0.96f + QVector3D(0.04f, 0.04f, 0.04f);
        chunk.color = clamp01(color);

        if (chunk.type != Game::Map::TerrainType::Mountain) {
          uint32_t propSeed = hashCoords(chunk.minX, chunk.minZ,
                                         static_cast<uint32_t>(chunk.type));
          uint32_t state = propSeed ^ 0x6d2b79f5u;
          float spawnChance = rand01(state);
          int clusterCount = 0;
          if (spawnChance > 0.58f) {
            clusterCount = 1;
            if (rand01(state) > 0.7f)
              clusterCount += 1;
            if (rand01(state) > 0.9f)
              clusterCount += 1;
          }

          for (int cluster = 0; cluster < clusterCount; ++cluster) {
            float gridSpanX = float(chunk.maxX - chunk.minX + 1);
            float gridSpanZ = float(chunk.maxZ - chunk.minZ + 1);
            float centerGX2 = float(chunk.minX) + rand01(state) * gridSpanX;
            float centerGZ2 = float(chunk.minZ) + rand01(state) * gridSpanZ;

            int propsPerCluster = 2 + static_cast<int>(rand01(state) * 4.0f);
            float scatterRadius =
                remap(rand01(state), 0.25f, 0.85f) * m_tileSize;

            for (int p = 0; p < propsPerCluster; ++p) {
              float angle = rand01(state) * 6.2831853f;
              float radius = scatterRadius * std::sqrt(rand01(state));
              float gx = centerGX2 + std::cos(angle) * radius / m_tileSize;
              float gz = centerGZ2 + std::sin(angle) * radius / m_tileSize;

              float worldX = (gx - halfWidth) * m_tileSize;
              float worldZ = (gz - halfHeight) * m_tileSize;
              float worldY = 0.0f;
              {
                float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
                float sgz = std::clamp(gz, 0.0f, float(m_height - 1));
                int x0 = int(std::floor(sgx));
                int z0 = int(std::floor(sgz));
                int x1 = std::min(x0 + 1, m_width - 1);
                int z1 = std::min(z0 + 1, m_height - 1);
                float tx = sgx - float(x0);
                float tz = sgz - float(z0);
                float h00 = m_heightData[z0 * m_width + x0];
                float h10 = m_heightData[z0 * m_width + x1];
                float h01 = m_heightData[z1 * m_width + x0];
                float h11 = m_heightData[z1 * m_width + x1];
                float h0 = h00 * (1.0f - tx) + h10 * tx;
                float h1 = h01 * (1.0f - tx) + h11 * tx;
                worldY = h0 * (1.0f - tz) + h1 * tz;
              }

              QVector3D n;
              {
                float sgx = std::clamp(gx, 0.0f, float(m_width - 1));
                float sgz = std::clamp(gz, 0.0f, float(m_height - 1));
                float gx0 = std::clamp(sgx - 1.0f, 0.0f, float(m_width - 1));
                float gx1 = std::clamp(sgx + 1.0f, 0.0f, float(m_width - 1));
                float gz0 = std::clamp(sgz - 1.0f, 0.0f, float(m_height - 1));
                float gz1 = std::clamp(sgz + 1.0f, 0.0f, float(m_height - 1));
                auto h = [&](float x, float z) {
                  int xi = int(std::round(x));
                  int zi = int(std::round(z));
                  return m_heightData[zi * m_width + xi];
                };
                QVector3D dx(2.0f * m_tileSize, h(gx1, sgz) - h(gx0, sgz),
                             0.0f);
                QVector3D dz(0.0f, h(sgx, gz1) - h(sgx, gz0),
                             2.0f * m_tileSize);
                n = QVector3D::crossProduct(dz, dx);
                if (n.lengthSquared() > 0.0f)
                  n.normalize();
                if (n.isNull())
                  n = QVector3D(0, 1, 0);
              }
              float slope = 1.0f - std::clamp(n.y(), 0.0f, 1.0f);

              float northnessP = std::clamp(
                  QVector3D::dotProduct(n, QVector3D(0, 0, 1)) * 0.5f + 0.5f,
                  0.0f, 1.0f);
              auto Hs = [&](int ix, int iz) {
                ix = std::clamp(ix, 0, m_width - 1);
                iz = std::clamp(iz, 0, m_height - 1);
                return m_heightData[iz * m_width + ix];
              };
              int ix = int(std::round(gx)), iz = int(std::round(gz));
              float concav = std::max(0.0f, (Hs(ix - 1, iz) + Hs(ix + 1, iz) +
                                             Hs(ix, iz - 1) + Hs(ix, iz + 1)) *
                                                    0.25f -
                                                Hs(ix, iz));
              concav = std::clamp(concav * 0.15f, 0.0f, 1.0f);

              float tuftAffinity = (1.0f - slope) * (0.6f + 0.4f * northnessP) *
                                   (0.7f + 0.3f * concav);
              float pebbleAffinity =
                  (0.3f + 0.7f * slope) * (0.6f + 0.4f * concav);

              float r = rand01(state);
              PropInstance instance;
              if (r < 0.55f * tuftAffinity && slope < 0.6f) {
                instance.type = PropType::Tuft;
                instance.color = applyTint(QVector3D(0.28f, 0.62f, 0.24f),
                                           remap(rand01(state), 0.9f, 1.15f));
                instance.scale = QVector3D(remap(rand01(state), 0.20f, 0.32f),
                                           remap(rand01(state), 0.45f, 0.7f),
                                           remap(rand01(state), 0.20f, 0.32f));
                instance.alpha = 1.0f;
              } else if (r < 0.85f * pebbleAffinity) {
                instance.type = PropType::Pebble;
                instance.color = applyTint(QVector3D(0.44f, 0.42f, 0.40f),
                                           remap(rand01(state), 0.85f, 1.08f));
                instance.scale = QVector3D(remap(rand01(state), 0.12f, 0.26f),
                                           remap(rand01(state), 0.06f, 0.12f),
                                           remap(rand01(state), 0.12f, 0.26f));
                instance.alpha = 1.0f;
              } else {
                instance.type = PropType::Stick;
                instance.color = applyTint(QVector3D(0.36f, 0.25f, 0.13f),
                                           remap(rand01(state), 0.95f, 1.12f));
                instance.scale = QVector3D(remap(rand01(state), 0.06f, 0.1f),
                                           remap(rand01(state), 0.35f, 0.6f),
                                           remap(rand01(state), 0.06f, 0.1f));
                instance.alpha = 1.0f;
              }
              instance.rotationDeg = rand01(state) * 360.0f;
              instance.position = QVector3D(worldX, worldY, worldZ);

              if (slope < 0.95f)
                m_props.push_back(instance);
            }
          }
        }

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
      return QVector3D(0.66f, 0.68f, 0.72f);
    } else {
      return QVector3D(0.50f, 0.48f, 0.46f);
    }
  case Game::Map::TerrainType::Hill: {
    float t = std::clamp(height / 3.0f, 0.0f, 1.0f);
    QVector3D lushGrass(0.34f, 0.66f, 0.30f);
    QVector3D sunKissed(0.58f, 0.49f, 0.35f);
    return lushGrass * (1.0f - t) + sunKissed * t;
  }
  case Game::Map::TerrainType::Flat:
  default:
    return QVector3D(0.27f, 0.58f, 0.30f);
  }
}

} // namespace Render::GL

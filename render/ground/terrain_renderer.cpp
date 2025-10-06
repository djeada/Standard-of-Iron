#include "terrain_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QElapsedTimer>
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
          std::round(prop.position.x() / m_tileSize + halfWidth));
      int gridZ = static_cast<int>(
          std::round(prop.position.z() / m_tileSize + halfHeight));
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

    QMatrix4x4 model;
    model.translate(prop.position);
    model.rotate(prop.rotationDeg, 0.0f, 1.0f, 0.0f);
    model.scale(prop.scale);
    renderer.mesh(mesh, model, prop.color, white, prop.alpha);

    if (quadMesh) {
      QMatrix4x4 decal;
      decal.translate(prop.position.x(), prop.position.y() + 0.01f,
                      prop.position.z());
      decal.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
      decal.scale(prop.scale.x() * 1.4f, prop.scale.z() * 1.4f, 1.0f);
      QVector3D aoColor(0.06f, 0.06f, 0.055f);
      renderer.mesh(quadMesh, decal, aoColor, white, 0.35f);
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
      };

      SectionData sections[3];

      uint32_t chunkSeed = hashCoords(chunkX, chunkZ);
      uint32_t variantSeed = chunkSeed ^ 0x9e3779b9u;
      float rotationStep = static_cast<float>((variantSeed >> 5) & 3) * 90.0f;
      bool flip = ((variantSeed >> 7) & 1u) != 0u;
      static const float tintVariants[5] = {0.92f, 0.96f, 1.0f, 1.04f, 1.08f};
      float tint = tintVariants[(variantSeed >> 12) % 5];

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
        float uu = gx / float(std::max(m_width - 1, 1));
        float vv = gz / float(std::max(m_height - 1, 1));
        if (section.flipU)
          uu = 1.0f - uu;
        float ru = uu - 0.5f;
        float rv = vv - 0.5f;
        switch (static_cast<int>(section.rotationDeg)) {
        case 90:
          std::swap(ru, rv);
          ru = -ru;
          break;
        case 180:
          ru = -ru;
          rv = -rv;
          break;
        case 270:
          std::swap(ru, rv);
          rv = -rv;
          break;
        default:
          break;
        }
        uu = ru + 0.5f;
        vv = rv + 0.5f;
        v.texCoord[0] = uu;
        v.texCoord[1] = vv;
        section.vertices.push_back(v);
        unsigned int localIndex =
            static_cast<unsigned int>(section.vertices.size() - 1);
        section.remap.emplace(globalIndex, localIndex);
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
        chunk.tint = section.tint;
        chunk.color = applyTint(baseColor, section.tint);
        chunk.color = 0.88f * chunk.color + QVector3D(0.07f, 0.07f, 0.07f);

        if (chunk.type != Game::Map::TerrainType::Mountain) {
          uint32_t propSeed = hashCoords(chunk.minX, chunk.minZ,
                                         static_cast<uint32_t>(chunk.type));
          uint32_t state = propSeed ^ 0x6d2b79f5u;
          float spawnChance = rand01(state);
          int clusterCount = 0;
          if (spawnChance > 0.65f) {
            clusterCount = 1;
            if (rand01(state) > 0.8f)
              clusterCount += 1;
          }

          for (int cluster = 0; cluster < clusterCount; ++cluster) {
            float gridSpanX = float(chunk.maxX - chunk.minX + 1);
            float gridSpanZ = float(chunk.maxZ - chunk.minZ + 1);
            float centerGX = float(chunk.minX) + rand01(state) * gridSpanX;
            float centerGZ = float(chunk.minZ) + rand01(state) * gridSpanZ;

            int propsPerCluster = 2 + static_cast<int>(rand01(state) * 3.0f);
            float scatterRadius = remap(rand01(state), 0.2f, 0.8f) * m_tileSize;

            for (int p = 0; p < propsPerCluster; ++p) {
              float angle = rand01(state) * 6.2831853f;
              float radius = scatterRadius * rand01(state);
              float gx = centerGX + std::cos(angle) * radius / m_tileSize;
              float gz = centerGZ + std::sin(angle) * radius / m_tileSize;

              int sampleGX =
                  std::clamp(static_cast<int>(std::round(gx)), 0, m_width - 1);
              int sampleGZ =
                  std::clamp(static_cast<int>(std::round(gz)), 0, m_height - 1);
              float worldX = (gx - halfWidth) * m_tileSize;
              float worldZ = (gz - halfHeight) * m_tileSize;
              float worldY = m_heightData[sampleGZ * m_width + sampleGX];

              float pick = rand01(state);
              PropInstance instance;
              if (pick < 0.45f) {
                instance.type = PropType::Tuft;
                instance.color = applyTint(QVector3D(0.26f, 0.6f, 0.22f),
                                           remap(rand01(state), 0.9f, 1.15f));
                instance.scale = QVector3D(remap(rand01(state), 0.18f, 0.28f),
                                           remap(rand01(state), 0.4f, 0.6f),
                                           remap(rand01(state), 0.18f, 0.28f));
                instance.alpha = 1.0f;
              } else if (pick < 0.8f) {
                instance.type = PropType::Pebble;
                instance.color = applyTint(QVector3D(0.42f, 0.41f, 0.39f),
                                           remap(rand01(state), 0.85f, 1.05f));
                instance.scale = QVector3D(remap(rand01(state), 0.12f, 0.22f),
                                           remap(rand01(state), 0.06f, 0.1f),
                                           remap(rand01(state), 0.12f, 0.22f));
                instance.alpha = 1.0f;
              } else {
                instance.type = PropType::Stick;
                instance.color = applyTint(QVector3D(0.35f, 0.24f, 0.12f),
                                           remap(rand01(state), 0.95f, 1.1f));
                instance.scale = QVector3D(remap(rand01(state), 0.05f, 0.09f),
                                           remap(rand01(state), 0.35f, 0.55f),
                                           remap(rand01(state), 0.05f, 0.09f));
                instance.alpha = 1.0f;
              }
              instance.rotationDeg = rand01(state) * 360.0f;
              instance.position = QVector3D(worldX, worldY, worldZ);
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
      return QVector3D(0.68f, 0.69f, 0.72f);
    } else {
      return QVector3D(0.52f, 0.49f, 0.47f);
    }

  case Game::Map::TerrainType::Hill:

  {
    float t = std::clamp(height / 3.0f, 0.0f, 1.0f);
    QVector3D lushGrass(0.36f, 0.65f, 0.30f);
    QVector3D sunKissed(0.58f, 0.48f, 0.34f);
    return lushGrass * (1.0f - t) + sunKissed * t;
  }

  case Game::Map::TerrainType::Flat:
  default:
    return QVector3D(0.26f, 0.56f, 0.29f);
  }
}

} // namespace Render::GL

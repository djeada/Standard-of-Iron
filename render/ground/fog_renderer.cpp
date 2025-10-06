#include "fog_renderer.h"

#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QElapsedTimer>
#include <algorithm>

namespace Render::GL {

void FogRenderer::updateMask(int width, int height, float tileSize,
                             const std::vector<std::uint8_t> &cells) {
  m_width = std::max(0, width);
  m_height = std::max(0, height);
  m_tileSize = std::max(0.0001f, tileSize);
  m_halfWidth = m_width * 0.5f - 0.5f;
  m_halfHeight = m_height * 0.5f - 0.5f;
  m_cells = cells;
  buildChunks();
}

void FogRenderer::submit(Renderer &renderer, ResourceManager &resources) {
  if (!m_enabled)
    return;
  if (m_width <= 0 || m_height <= 0)
    return;
  if (static_cast<int>(m_cells.size()) != m_width * m_height)
    return;

  Texture *white = resources.white();
  if (!white)
    return;

  QMatrix4x4 model;

  for (const auto &chunk : m_chunks) {
    if (!chunk.mesh)
      continue;
    renderer.mesh(chunk.mesh.get(), model, chunk.color, white, chunk.alpha);
  }
}

void FogRenderer::buildChunks() {
  m_chunks.clear();

  if (m_width <= 0 || m_height <= 0)
    return;
  if (static_cast<int>(m_cells.size()) != m_width * m_height)
    return;

  QElapsedTimer timer;
  timer.start();

  const float halfTile = m_tileSize * 0.5f;
  const int chunkSize = 16;
  std::size_t totalQuads = 0;

  for (int chunkZ = 0; chunkZ < m_height; chunkZ += chunkSize) {
    int chunkMaxZ = std::min(chunkZ + chunkSize, m_height);
    for (int chunkX = 0; chunkX < m_width; chunkX += chunkSize) {
      int chunkMaxX = std::min(chunkX + chunkSize, m_width);

      struct SectionData {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
      };

      SectionData sections[2];

      auto appendQuad = [&](SectionData &section, float centerX,
                            float centerZ) {
        Vertex v0{};
        Vertex v1{};
        Vertex v2{};
        Vertex v3{};

        v0.position[0] = centerX - halfTile;
        v0.position[1] = 0.25f;
        v0.position[2] = centerZ - halfTile;
        v1.position[0] = centerX + halfTile;
        v1.position[1] = 0.25f;
        v1.position[2] = centerZ - halfTile;
        v2.position[0] = centerX - halfTile;
        v2.position[1] = 0.25f;
        v2.position[2] = centerZ + halfTile;
        v3.position[0] = centerX + halfTile;
        v3.position[1] = 0.25f;
        v3.position[2] = centerZ + halfTile;

        v0.normal[0] = v1.normal[0] = v2.normal[0] = v3.normal[0] = 0.0f;
        v0.normal[1] = v1.normal[1] = v2.normal[1] = v3.normal[1] = 1.0f;
        v0.normal[2] = v1.normal[2] = v2.normal[2] = v3.normal[2] = 0.0f;

        v0.texCoord[0] = 0.0f;
        v0.texCoord[1] = 0.0f;
        v1.texCoord[0] = 1.0f;
        v1.texCoord[1] = 0.0f;
        v2.texCoord[0] = 0.0f;
        v2.texCoord[1] = 1.0f;
        v3.texCoord[0] = 1.0f;
        v3.texCoord[1] = 1.0f;

        const unsigned int base =
            static_cast<unsigned int>(section.vertices.size());
        section.vertices.push_back(v0);
        section.vertices.push_back(v1);
        section.vertices.push_back(v2);
        section.vertices.push_back(v3);

        section.indices.push_back(base + 0);
        section.indices.push_back(base + 1);
        section.indices.push_back(base + 2);
        section.indices.push_back(base + 2);
        section.indices.push_back(base + 1);
        section.indices.push_back(base + 3);
      };

      for (int z = chunkZ; z < chunkMaxZ; ++z) {
        for (int x = chunkX; x < chunkMaxX; ++x) {
          const std::uint8_t state = m_cells[z * m_width + x];
          if (state >= 2)
            continue;

          const float worldX = (x - m_halfWidth) * m_tileSize;
          const float worldZ = (z - m_halfHeight) * m_tileSize;

          SectionData &section = sections[std::min<int>(state, 1)];
          appendQuad(section, worldX, worldZ);
        }
      }

      if (!sections[0].indices.empty()) {
        FogChunk chunk;
        chunk.mesh =
            std::make_unique<Mesh>(sections[0].vertices, sections[0].indices);
        chunk.color = QVector3D(0.02f, 0.02f, 0.05f);
        chunk.alpha = 0.9f;
        totalQuads += sections[0].indices.size() / 6;
        m_chunks.push_back(std::move(chunk));
      }
      if (!sections[1].indices.empty()) {
        FogChunk chunk;
        chunk.mesh =
            std::make_unique<Mesh>(sections[1].vertices, sections[1].indices);
        chunk.color = QVector3D(0.05f, 0.05f, 0.05f);
        chunk.alpha = 0.45f;
        totalQuads += sections[1].indices.size() / 6;
        m_chunks.push_back(std::move(chunk));
      }
    }
  }

  qDebug() << "FogRenderer: built" << m_chunks.size() << "chunks in"
           << timer.elapsed() << "ms" << "quads:" << totalQuads;
}

} // namespace Render::GL

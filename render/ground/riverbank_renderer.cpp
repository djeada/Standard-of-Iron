#include "riverbank_renderer.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QVector2D>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

RiverbankRenderer::RiverbankRenderer() = default;
RiverbankRenderer::~RiverbankRenderer() = default;

void RiverbankRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &riverSegments,
    const Game::Map::TerrainHeightMap &heightMap) {
  qDebug() << "RiverbankRenderer::configure called with" << riverSegments.size() << "river segments";
  m_riverSegments = riverSegments;
  m_tileSize = heightMap.getTileSize();
  m_gridWidth = heightMap.getWidth();
  m_gridHeight = heightMap.getHeight();
  m_heights = heightMap.getHeightData();
  qDebug() << "  Grid size:" << m_gridWidth << "x" << m_gridHeight << "tile size:" << m_tileSize;
  buildMeshes();
}

void RiverbankRenderer::buildMeshes() {
  qDebug() << "RiverbankRenderer::buildMeshes called";
  if (m_riverSegments.empty()) {
    qDebug() << "  No river segments - clearing mesh";
    m_mesh.reset();
    return;
  }

  qDebug() << "  Processing" << m_riverSegments.size() << "river segments";
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  auto noiseHash = [](float x, float y) -> float {
    float n = std::sin(x * 127.1f + y * 311.7f) * 43758.5453123f;
    return n - std::floor(n);
  };

  auto noise = [&noiseHash](float x, float y) -> float {
    float ix = std::floor(x);
    float iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;

    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float a = noiseHash(ix, iy);
    float b = noiseHash(ix + 1.0f, iy);
    float c = noiseHash(ix, iy + 1.0f);
    float d = noiseHash(ix + 1.0f, iy + 1.0f);

    return a * (1.0f - fx) * (1.0f - fy) + b * fx * (1.0f - fy) +
           c * (1.0f - fx) * fy + d * fx * fy;
  };

  // Helper to sample terrain height at world position
  auto sampleHeight = [&](float worldX, float worldZ) -> float {
    if (m_heights.empty() || m_gridWidth == 0 || m_gridHeight == 0)
      return 0.0f;
    
    float halfWidth = m_gridWidth * 0.5f - 0.5f;
    float halfHeight = m_gridHeight * 0.5f - 0.5f;
    float gx = (worldX / m_tileSize) + halfWidth;
    float gz = (worldZ / m_tileSize) + halfHeight;
    
    gx = std::clamp(gx, 0.0f, float(m_gridWidth - 1));
    gz = std::clamp(gz, 0.0f, float(m_gridHeight - 1));
    
    int x0 = int(std::floor(gx));
    int z0 = int(std::floor(gz));
    int x1 = std::min(x0 + 1, m_gridWidth - 1);
    int z1 = std::min(z0 + 1, m_gridHeight - 1);
    
    float tx = gx - float(x0);
    float tz = gz - float(z0);
    
    float h00 = m_heights[z0 * m_gridWidth + x0];
    float h10 = m_heights[z0 * m_gridWidth + x1];
    float h01 = m_heights[z1 * m_gridWidth + x0];
    float h11 = m_heights[z1 * m_gridWidth + x1];
    
    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;
    return h0 * (1.0f - tz) + h1 * tz;
  };

  // Create riverbank transition zones on both sides of each river segment
  for (const auto &segment : m_riverSegments) {
    QVector3D dir = segment.end - segment.start;
    float length = dir.length();
    if (length < 0.01f)
      continue;

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfWidth = segment.width * 0.5f;
    
    // Riverbank transition zone width (extends beyond water edge)
    float bankWidth = 1.2f;

    int lengthSteps =
        static_cast<int>(std::ceil(length / (m_tileSize * 0.5f))) + 1;
    lengthSteps = std::max(lengthSteps, 8);

    unsigned int baseIndex = static_cast<unsigned int>(vertices.size());

    for (int i = 0; i < lengthSteps; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(lengthSteps - 1);
      QVector3D centerPos = segment.start + dir * (length * t);

      // Edge variation using noise
      float noiseFreq1 = 2.0f;
      float noiseFreq2 = 5.0f;
      float noiseFreq3 = 10.0f;

      float edgeNoise1 =
          noise(centerPos.x() * noiseFreq1, centerPos.z() * noiseFreq1);
      float edgeNoise2 =
          noise(centerPos.x() * noiseFreq2, centerPos.z() * noiseFreq2);
      float edgeNoise3 =
          noise(centerPos.x() * noiseFreq3, centerPos.z() * noiseFreq3);

      float combinedNoise =
          edgeNoise1 * 0.5f + edgeNoise2 * 0.3f + edgeNoise3 * 0.2f;
      combinedNoise = (combinedNoise - 0.5f) * 2.0f;

      float widthVariation = combinedNoise * halfWidth * 0.35f;

      // Meander for natural curves
      float meander = noise(t * 3.0f, length * 0.1f) * 0.3f;
      QVector3D centerOffset = perpendicular * meander;
      centerPos += centerOffset;

      // Create riverbank zones on both sides
      // Inner edge (water edge)
      QVector3D innerLeft = centerPos - perpendicular * (halfWidth + widthVariation);
      QVector3D innerRight = centerPos + perpendicular * (halfWidth + widthVariation);
      
      // Outer edge (land edge) - with additional variation
      float outerVariation = noise(centerPos.x() * 8.0f, centerPos.z() * 8.0f) * 0.5f;
      QVector3D outerLeft = innerLeft - perpendicular * (bankWidth + outerVariation);
      QVector3D outerRight = innerRight + perpendicular * (bankWidth + outerVariation);

      float normal[3] = {0.0f, 1.0f, 0.0f};

      // Left bank strip (4 vertices per segment)
      Vertex leftInner, leftOuter;
      float heightInnerLeft = sampleHeight(innerLeft.x(), innerLeft.z());
      float heightOuterLeft = sampleHeight(outerLeft.x(), outerLeft.z());
      
      leftInner.position[0] = innerLeft.x();
      leftInner.position[1] = heightInnerLeft + 0.05f; // Slightly above terrain
      leftInner.position[2] = innerLeft.z();
      leftInner.normal[0] = normal[0];
      leftInner.normal[1] = normal[1];
      leftInner.normal[2] = normal[2];
      leftInner.texCoord[0] = 0.0f; // Inner = wet
      leftInner.texCoord[1] = t;
      vertices.push_back(leftInner);

      leftOuter.position[0] = outerLeft.x();
      leftOuter.position[1] = heightOuterLeft + 0.05f;
      leftOuter.position[2] = outerLeft.z();
      leftOuter.normal[0] = normal[0];
      leftOuter.normal[1] = normal[1];
      leftOuter.normal[2] = normal[2];
      leftOuter.texCoord[0] = 1.0f; // Outer = dry
      leftOuter.texCoord[1] = t;
      vertices.push_back(leftOuter);

      // Right bank strip
      Vertex rightInner, rightOuter;
      float heightInnerRight = sampleHeight(innerRight.x(), innerRight.z());
      float heightOuterRight = sampleHeight(outerRight.x(), outerRight.z());
      
      rightInner.position[0] = innerRight.x();
      rightInner.position[1] = heightInnerRight + 0.05f;
      rightInner.position[2] = innerRight.z();
      rightInner.normal[0] = normal[0];
      rightInner.normal[1] = normal[1];
      rightInner.normal[2] = normal[2];
      rightInner.texCoord[0] = 0.0f; // Inner = wet
      rightInner.texCoord[1] = t;
      vertices.push_back(rightInner);

      rightOuter.position[0] = outerRight.x();
      rightOuter.position[1] = heightOuterRight + 0.05f;
      rightOuter.position[2] = outerRight.z();
      rightOuter.normal[0] = normal[0];
      rightOuter.normal[1] = normal[1];
      rightOuter.normal[2] = normal[2];
      rightOuter.texCoord[0] = 1.0f; // Outer = dry
      rightOuter.texCoord[1] = t;
      vertices.push_back(rightOuter);

      if (i < lengthSteps - 1) {
        unsigned int idx0 = baseIndex + i * 4;
        
        // Left bank triangles
        indices.push_back(idx0 + 0);     // left inner
        indices.push_back(idx0 + 4);     // next left inner
        indices.push_back(idx0 + 1);     // left outer
        
        indices.push_back(idx0 + 1);     // left outer
        indices.push_back(idx0 + 4);     // next left inner
        indices.push_back(idx0 + 5);     // next left outer

        // Right bank triangles
        indices.push_back(idx0 + 2);     // right inner
        indices.push_back(idx0 + 3);     // right outer
        indices.push_back(idx0 + 6);     // next right inner
        
        indices.push_back(idx0 + 3);     // right outer
        indices.push_back(idx0 + 7);     // next right outer
        indices.push_back(idx0 + 6);     // next right inner
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    qDebug() << "  No vertices or indices generated - clearing mesh";
    m_mesh.reset();
    return;
  }

  qDebug() << "  Created mesh with" << vertices.size() << "vertices and" << indices.size() << "indices";
  m_mesh = std::make_unique<Mesh>(vertices, indices);
}

void RiverbankRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  static int callCount = 0;
  if (callCount < 5) {
    qDebug() << "RiverbankRenderer::submit called (call" << callCount << ")";
    callCount++;
  }
  
  if (!m_mesh || m_riverSegments.empty()) {
    if (callCount <= 5) {
      qDebug() << "  Skipping: mesh=" << (m_mesh ? "exists" : "null") 
               << "segments=" << m_riverSegments.size();
    }
    return;
  }

  Q_UNUSED(resources);

  auto shader = renderer.getShader("riverbank");
  if (!shader) {
    static bool warned = false;
    if (!warned) {
      qWarning() << "  Riverbank shader not found!";
      warned = true;
    }
    return;
  }
  
  if (callCount <= 5) {
    qDebug() << "  Rendering riverbank with shader";
  }

  renderer.setCurrentShader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  renderer.mesh(m_mesh.get(), model, QVector3D(1.0f, 1.0f, 1.0f), nullptr,
                1.0f);

  renderer.setCurrentShader(nullptr);
}

} // namespace Render::GL

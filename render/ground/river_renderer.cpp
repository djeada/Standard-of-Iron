#include "river_renderer.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QVector2D>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

RiverRenderer::RiverRenderer() = default;
RiverRenderer::~RiverRenderer() = default;

void RiverRenderer::configure(
    const std::vector<Game::Map::RiverSegment> &riverSegments, float tileSize) {
  m_riverSegments = riverSegments;
  m_tileSize = tileSize;
  buildMeshes();
}

void RiverRenderer::buildMeshes() {
  if (m_riverSegments.empty()) {
    m_mesh.reset();
    return;
  }

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

  for (const auto &segment : m_riverSegments) {
    QVector3D dir = segment.end - segment.start;
    float length = dir.length();
    if (length < 0.01f)
      continue;

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfWidth = segment.width * 0.5f;

    int lengthSteps =
        static_cast<int>(std::ceil(length / (m_tileSize * 0.5f))) + 1;
    lengthSteps = std::max(lengthSteps, 8);

    unsigned int baseIndex = static_cast<unsigned int>(vertices.size());

    for (int i = 0; i < lengthSteps; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(lengthSteps - 1);
      QVector3D centerPos = segment.start + dir * (length * t);

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

      float meander = noise(t * 3.0f, length * 0.1f) * 0.3f;
      QVector3D centerOffset = perpendicular * meander;
      centerPos += centerOffset;

      QVector3D left = centerPos - perpendicular * (halfWidth + widthVariation);
      QVector3D right =
          centerPos + perpendicular * (halfWidth + widthVariation);

      float normal[3] = {0.0f, 1.0f, 0.0f};

      Vertex leftVertex;
      leftVertex.position[0] = left.x();
      leftVertex.position[1] = left.y();
      leftVertex.position[2] = left.z();
      leftVertex.normal[0] = normal[0];
      leftVertex.normal[1] = normal[1];
      leftVertex.normal[2] = normal[2];
      leftVertex.texCoord[0] = 0.0f;
      leftVertex.texCoord[1] = t;
      vertices.push_back(leftVertex);

      Vertex rightVertex;
      rightVertex.position[0] = right.x();
      rightVertex.position[1] = right.y();
      rightVertex.position[2] = right.z();
      rightVertex.normal[0] = normal[0];
      rightVertex.normal[1] = normal[1];
      rightVertex.normal[2] = normal[2];
      rightVertex.texCoord[0] = 1.0f;
      rightVertex.texCoord[1] = t;
      vertices.push_back(rightVertex);

      if (i < lengthSteps - 1) {
        unsigned int idx0 = baseIndex + i * 2;
        unsigned int idx1 = idx0 + 1;
        unsigned int idx2 = idx0 + 2;
        unsigned int idx3 = idx0 + 3;

        indices.push_back(idx0);
        indices.push_back(idx2);
        indices.push_back(idx1);

        indices.push_back(idx1);
        indices.push_back(idx2);
        indices.push_back(idx3);
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    m_mesh.reset();
    return;
  }

  m_mesh = std::make_unique<Mesh>(vertices, indices);
}

void RiverRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (!m_mesh || m_riverSegments.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto shader = renderer.getShader("river");
  if (!shader) {
    return;
  }

  renderer.setCurrentShader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  renderer.mesh(m_mesh.get(), model, QVector3D(1.0f, 1.0f, 1.0f), nullptr,
                1.0f);

  renderer.setCurrentShader(nullptr);
}

} // namespace Render::GL

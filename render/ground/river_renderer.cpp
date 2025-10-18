#include "river_renderer.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
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

  std::vector<float> vertices;
  std::vector<unsigned int> indices;

  for (const auto &segment : m_riverSegments) {
    QVector3D dir = segment.end - segment.start;
    float length = dir.length();
    if (length < 0.01f)
      continue;

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfWidth = segment.width * 0.5f;

    int lengthSteps = static_cast<int>(std::ceil(length / m_tileSize)) + 1;
    lengthSteps = std::max(lengthSteps, 2);

    unsigned int baseIndex = static_cast<unsigned int>(vertices.size() / 5);

    for (int i = 0; i < lengthSteps; ++i) {
      float t =
          static_cast<float>(i) / static_cast<float>(lengthSteps - 1);
      QVector3D centerPos = segment.start + dir * (length * t);

      QVector3D left = centerPos - perpendicular * halfWidth;
      QVector3D right = centerPos + perpendicular * halfWidth;

      // Left vertex
      vertices.push_back(left.x());
      vertices.push_back(left.y());
      vertices.push_back(left.z());
      vertices.push_back(0.0f);
      vertices.push_back(t);

      // Right vertex
      vertices.push_back(right.x());
      vertices.push_back(right.y());
      vertices.push_back(right.z());
      vertices.push_back(1.0f);
      vertices.push_back(t);

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

  auto shader = resources->getShader("river");
  if (!shader) {
    return;
  }

  shader->bind();

  QMatrix4x4 model;
  model.setToIdentity();

  shader->setUniformValue("model", model);
  shader->setUniformValue("view", renderer.getViewMatrix());
  shader->setUniformValue("projection", renderer.getProjectionMatrix());
  shader->setUniformValue("time", renderer.getAnimationTime());

  m_mesh->draw();

  shader->release();
}

} // namespace Render::GL

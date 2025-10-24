#include "bridge_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "terrain_gpu.h"
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

BridgeRenderer::BridgeRenderer() = default;
BridgeRenderer::~BridgeRenderer() = default;

void BridgeRenderer::configure(const std::vector<Game::Map::Bridge> &bridges,
                               float tileSize) {
  m_bridges = bridges;
  m_tileSize = tileSize;
  buildMeshes();
}

void BridgeRenderer::buildMeshes() {
  m_meshes.clear();

  if (m_bridges.empty()) {
    return;
  }

  for (const auto &bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float length = dir.length();
    if (length < 0.01f) {
      m_meshes.push_back(nullptr);
      continue;
    }

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfWidth = bridge.width * 0.5f;

    int lengthSegments =
        static_cast<int>(std::ceil(length / (m_tileSize * 0.3f)));
    lengthSegments = std::max(lengthSegments, 8);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const int vertsPerSegment = 12;
    const float deckThickness = std::clamp(bridge.width * 0.25f, 0.35f, 0.8f);
    const float parapetHeight = std::clamp(bridge.width * 0.25f, 0.25f, 0.55f);
    const float parapetOffset = halfWidth * 1.05f;

    auto addVertex = [&](const QVector3D &position, const QVector3D &normal,
                         float u, float v) {
      Vertex vtx{};
      vtx.position[0] = position.x();
      vtx.position[1] = position.y();
      vtx.position[2] = position.z();
      QVector3D n = normal.normalized();
      vtx.normal[0] = n.x();
      vtx.normal[1] = n.y();
      vtx.normal[2] = n.z();
      vtx.texCoord[0] = u;
      vtx.texCoord[1] = v;
      vertices.push_back(vtx);
    };

    auto pushQuad = [&](unsigned int a, unsigned int b, unsigned int c,
                        unsigned int d) {
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(a);
      indices.push_back(c);
      indices.push_back(d);
    };

    for (int i = 0; i <= lengthSegments; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(lengthSegments);
      QVector3D centerPos = bridge.start + dir * (length * t);

      float archCurve = 4.0f * t * (1.0f - t);
      float archHeight = bridge.height * archCurve * 0.8f;

      float deckHeight = bridge.start.y() + bridge.height + archHeight * 0.3f;

      float stoneNoise = std::sin(centerPos.x() * 3.0f) *
                         std::cos(centerPos.z() * 2.5f) * 0.02f;

      float deckY = deckHeight + stoneNoise;
      float undersideY =
          deckHeight - deckThickness - archCurve * bridge.height * 0.55f;
      float railTopY = deckY + parapetHeight;

      QVector3D leftNormal = (-perpendicular).normalized();
      QVector3D rightNormal = perpendicular.normalized();

      QVector3D topLeft = centerPos + perpendicular * (-halfWidth);
      topLeft.setY(deckY);
      QVector3D topRight = centerPos + perpendicular * (halfWidth);
      topRight.setY(deckY);

      QVector3D bottomLeft = topLeft;
      bottomLeft.setY(undersideY);
      QVector3D bottomRight = topRight;
      bottomRight.setY(undersideY);

      QVector3D sideLeftTop = topLeft;
      QVector3D sideLeftBottom = bottomLeft;
      QVector3D sideRightTop = topRight;
      QVector3D sideRightBottom = bottomRight;

      QVector3D parapetLeftBottom =
          centerPos + perpendicular * (-parapetOffset);
      parapetLeftBottom.setY(deckY);
      QVector3D parapetLeftTop = parapetLeftBottom;
      parapetLeftTop.setY(railTopY);

      QVector3D parapetRightBottom =
          centerPos + perpendicular * (parapetOffset);
      parapetRightBottom.setY(deckY);
      QVector3D parapetRightTop = parapetRightBottom;
      parapetRightTop.setY(railTopY);

      float texU0 = 0.0f;
      float texU1 = 1.0f;
      float texV = t * length * 0.4f;

      addVertex(topLeft, QVector3D(0.0f, 1.0f, 0.0f), texU0, texV);
      addVertex(topRight, QVector3D(0.0f, 1.0f, 0.0f), texU1, texV);
      addVertex(bottomLeft, QVector3D(0.0f, -1.0f, 0.0f), texU0, texV);
      addVertex(bottomRight, QVector3D(0.0f, -1.0f, 0.0f), texU1, texV);
      addVertex(sideLeftTop, leftNormal, texU0, texV);
      addVertex(sideLeftBottom, leftNormal, texU0, texV);
      addVertex(sideRightTop, rightNormal, texU1, texV);
      addVertex(sideRightBottom, rightNormal, texU1, texV);
      addVertex(parapetLeftTop, leftNormal, texU0, texV);
      addVertex(parapetLeftBottom, leftNormal, texU0, texV);
      addVertex(parapetRightTop, rightNormal, texU1, texV);
      addVertex(parapetRightBottom, rightNormal, texU1, texV);

      if (i < lengthSegments) {
        unsigned int baseIdx = static_cast<unsigned int>(i * vertsPerSegment);
        unsigned int nextIdx = baseIdx + vertsPerSegment;

        pushQuad(baseIdx + 0, baseIdx + 1, nextIdx + 1, nextIdx + 0);

        pushQuad(nextIdx + 3, nextIdx + 2, baseIdx + 2, baseIdx + 3);

        pushQuad(baseIdx + 4, baseIdx + 5, nextIdx + 5, nextIdx + 4);

        pushQuad(baseIdx + 6, baseIdx + 7, nextIdx + 7, nextIdx + 6);

        pushQuad(baseIdx + 9, baseIdx + 8, nextIdx + 8, nextIdx + 9);

        pushQuad(baseIdx + 11, baseIdx + 10, nextIdx + 10, nextIdx + 11);
      }
    }

    if (!vertices.empty()) {
      unsigned int startIdx = 0;
      unsigned int endIdx =
          static_cast<unsigned int>(lengthSegments * vertsPerSegment);

      QVector3D forwardNormal = dir;

      auto addCap = [&](unsigned int topL, unsigned int topR,
                        unsigned int bottomR, unsigned int bottomL,
                        const QVector3D &normal) {
        unsigned int capStart = static_cast<unsigned int>(vertices.size());
        auto copyVertex = [&](unsigned int source, const QVector3D &norm) {
          const Vertex &src = vertices[source];
          Vertex vtx = src;
          QVector3D n = norm.normalized();
          vtx.normal[0] = n.x();
          vtx.normal[1] = n.y();
          vtx.normal[2] = n.z();
          vertices.push_back(vtx);
        };
        copyVertex(topL, normal);
        copyVertex(topR, normal);
        copyVertex(bottomR, normal);
        copyVertex(bottomL, normal);
        pushQuad(capStart + 0, capStart + 1, capStart + 2, capStart + 3);
      };

      addCap(startIdx + 0, startIdx + 1, startIdx + 3, startIdx + 2,
             -forwardNormal);
      addCap(endIdx + 0, endIdx + 1, endIdx + 3, endIdx + 2, forwardNormal);
    }

    if (!vertices.empty() && !indices.empty()) {
      m_meshes.push_back(std::make_unique<Mesh>(vertices, indices));
    } else {
      m_meshes.push_back(nullptr);
    }
  }
}

void BridgeRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (m_meshes.empty() || m_bridges.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();

  auto shader = renderer.getShader("bridge");
  if (!shader) {
    shader = renderer.getShader("basic");
    if (!shader) {
      return;
    }
  }

  renderer.setCurrentShader(shader);

  QMatrix4x4 model;
  model.setToIdentity();

  QVector3D stoneColor(0.55f, 0.52f, 0.48f);

  size_t meshIndex = 0;
  for (const auto &bridge : m_bridges) {
    if (meshIndex >= m_meshes.size())
      break;

    auto *mesh = m_meshes[meshIndex].get();
    ++meshIndex;

    if (!mesh) {
      continue;
    }

    QVector3D dir = bridge.end - bridge.start;
    float length = dir.length();

    float alpha = 1.0f;
    QVector3D colorMultiplier(1.0f, 1.0f, 1.0f);

    if (useVisibility) {
      int maxVisibilityState = 0;
      dir.normalize();

      int samplesPerBridge = 5;
      for (int i = 0; i < samplesPerBridge; ++i) {
        float t =
            static_cast<float>(i) / static_cast<float>(samplesPerBridge - 1);
        QVector3D pos = bridge.start + dir * (length * t);

        if (visibility.isVisibleWorld(pos.x(), pos.z())) {
          maxVisibilityState = 2;
          break;
        } else if (visibility.isExploredWorld(pos.x(), pos.z())) {
          maxVisibilityState = std::max(maxVisibilityState, 1);
        }
      }

      if (maxVisibilityState == 0) {
        continue;
      } else if (maxVisibilityState == 1) {
        alpha = 0.5f;
        colorMultiplier = QVector3D(0.4f, 0.4f, 0.45f);
      }
    }

    QVector3D finalColor(stoneColor.x() * colorMultiplier.x(),
                         stoneColor.y() * colorMultiplier.y(),
                         stoneColor.z() * colorMultiplier.z());

    renderer.mesh(mesh, model, finalColor, nullptr, alpha);
  }

  renderer.setCurrentShader(nullptr);
}

} // namespace Render::GL

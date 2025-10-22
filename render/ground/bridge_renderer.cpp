#include "bridge_renderer.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "terrain_gpu.h"
#include <QVector2D>
#include <QVector3D>
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

    for (int i = 0; i <= lengthSegments; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(lengthSegments);
      QVector3D centerPos = bridge.start + dir * (length * t);

      float archCurve = 4.0f * t * (1.0f - t);
      float archHeight = bridge.height * archCurve * 0.8f;

      float deckHeight = bridge.start.y() + bridge.height + archHeight * 0.3f;

      float stoneNoise = std::sin(centerPos.x() * 3.0f) *
                         std::cos(centerPos.z() * 2.5f) * 0.02f;

      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        QVector3D edgePos = centerPos + perpendicular * (halfWidth * sideSign);
        edgePos.setY(deckHeight + stoneNoise);

        Vertex deckVertex;
        deckVertex.position[0] = edgePos.x();
        deckVertex.position[1] = edgePos.y();
        deckVertex.position[2] = edgePos.z();
        deckVertex.normal[0] = 0.0f;
        deckVertex.normal[1] = 1.0f;
        deckVertex.normal[2] = 0.0f;
        deckVertex.texCoord[0] = side * 1.0f;
        deckVertex.texCoord[1] = t * length * 0.5f;
        vertices.push_back(deckVertex);
      }

      float archUnderHeight = bridge.start.y() + archHeight;
      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        float archWidth = halfWidth * 0.7f;
        QVector3D archPos = centerPos + perpendicular * (archWidth * sideSign);
        archPos.setY(archUnderHeight);

        Vertex archVertex;
        archVertex.position[0] = archPos.x();
        archVertex.position[1] = archPos.y();
        archVertex.position[2] = archPos.z();
        archVertex.normal[0] = perpendicular.x() * sideSign;
        archVertex.normal[1] = -0.5f;
        archVertex.normal[2] = perpendicular.z() * sideSign;
        archVertex.texCoord[0] = side * 1.0f;
        archVertex.texCoord[1] = t * length * 0.5f;
        vertices.push_back(archVertex);
      }

      float parapetHeight = deckHeight + 0.25f;
      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        QVector3D parapetPos =
            centerPos + perpendicular * (halfWidth * sideSign * 1.05f);
        parapetPos.setY(parapetHeight);

        Vertex parapetVertex;
        parapetVertex.position[0] = parapetPos.x();
        parapetVertex.position[1] = parapetPos.y();
        parapetVertex.position[2] = parapetPos.z();
        parapetVertex.normal[0] = perpendicular.x() * sideSign;
        parapetVertex.normal[1] = 0.0f;
        parapetVertex.normal[2] = perpendicular.z() * sideSign;
        parapetVertex.texCoord[0] = side * 1.0f;
        parapetVertex.texCoord[1] = t * length * 0.5f;
        vertices.push_back(parapetVertex);
      }

      if (i < lengthSegments) {
        unsigned int idx = i * 6;

        indices.push_back(idx + 0);
        indices.push_back(idx + 6);
        indices.push_back(idx + 1);
        indices.push_back(idx + 1);
        indices.push_back(idx + 6);
        indices.push_back(idx + 7);

        indices.push_back(idx + 2);
        indices.push_back(idx + 8);
        indices.push_back(idx + 0);
        indices.push_back(idx + 0);
        indices.push_back(idx + 8);
        indices.push_back(idx + 6);

        indices.push_back(idx + 1);
        indices.push_back(idx + 9);
        indices.push_back(idx + 3);
        indices.push_back(idx + 1);
        indices.push_back(idx + 7);
        indices.push_back(idx + 9);

        indices.push_back(idx + 0);
        indices.push_back(idx + 6);
        indices.push_back(idx + 4);
        indices.push_back(idx + 4);
        indices.push_back(idx + 6);
        indices.push_back(idx + 10);

        indices.push_back(idx + 1);
        indices.push_back(idx + 5);
        indices.push_back(idx + 7);
        indices.push_back(idx + 5);
        indices.push_back(idx + 11);
        indices.push_back(idx + 7);
      }
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

} 

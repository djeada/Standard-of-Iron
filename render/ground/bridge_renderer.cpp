#include "bridge_renderer.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "terrain_gpu.h"
#include "../../game/map/visibility_service.h"
#include <QDebug>
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
  qDebug() << "BridgeRenderer::configure() called with" << bridges.size()
           << "bridges, tileSize:" << tileSize;
  buildMeshes();
}

void BridgeRenderer::buildMeshes() {
  if (m_bridges.empty()) {
    qDebug() << "BridgeRenderer::buildMeshes() - No bridges to build";
    m_mesh.reset();
    return;
  }

  qDebug() << "BridgeRenderer::buildMeshes() - Building meshes for"
           << m_bridges.size() << "bridges";
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  for (const auto &bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float length = dir.length();
    if (length < 0.01f)
      continue;

    dir.normalize();
    QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
    float halfWidth = bridge.width * 0.5f;

    int lengthSegments = static_cast<int>(std::ceil(length / (m_tileSize * 0.3f)));
    lengthSegments = std::max(lengthSegments, 8);

    unsigned int baseIndex = static_cast<unsigned int>(vertices.size());

    // Medieval stone bridge with arched underside
    for (int i = 0; i <= lengthSegments; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(lengthSegments);
      QVector3D centerPos = bridge.start + dir * (length * t);

      // Arch curve (parabolic)
      float archCurve = 4.0f * t * (1.0f - t); // Peaks at t=0.5
      float archHeight = bridge.height * archCurve * 0.8f;

      // Bridge deck (top surface)
      float deckHeight = bridge.start.y() + bridge.height + archHeight * 0.3f;

      // Stone texture variation using position-based noise
      float stoneNoise = std::sin(centerPos.x() * 3.0f) * std::cos(centerPos.z() * 2.5f) * 0.02f;

      // Create bridge deck vertices
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

      // Create arch support vertices (underneath)
      float archUnderHeight = bridge.start.y() + archHeight;
      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        float archWidth = halfWidth * 0.7f; // Narrower arch
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

      // Stone parapet (railing) vertices
      float parapetHeight = deckHeight + 0.25f;
      for (int side = 0; side < 2; ++side) {
        float sideSign = (side == 0) ? -1.0f : 1.0f;
        QVector3D parapetPos = centerPos + perpendicular * (halfWidth * sideSign * 1.05f);
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

      // Create indices for bridge geometry
      if (i < lengthSegments) {
        unsigned int idx = baseIndex + i * 6;

        // Bridge deck (top)
        indices.push_back(idx + 0);
        indices.push_back(idx + 6);
        indices.push_back(idx + 1);
        indices.push_back(idx + 1);
        indices.push_back(idx + 6);
        indices.push_back(idx + 7);

        // Arch underside (left)
        indices.push_back(idx + 2);
        indices.push_back(idx + 8);
        indices.push_back(idx + 0);
        indices.push_back(idx + 0);
        indices.push_back(idx + 8);
        indices.push_back(idx + 6);

        // Arch underside (right)
        indices.push_back(idx + 1);
        indices.push_back(idx + 9);
        indices.push_back(idx + 3);
        indices.push_back(idx + 1);
        indices.push_back(idx + 7);
        indices.push_back(idx + 9);

        // Parapet walls (left)
        indices.push_back(idx + 0);
        indices.push_back(idx + 6);
        indices.push_back(idx + 4);
        indices.push_back(idx + 4);
        indices.push_back(idx + 6);
        indices.push_back(idx + 10);

        // Parapet walls (right)
        indices.push_back(idx + 1);
        indices.push_back(idx + 5);
        indices.push_back(idx + 7);
        indices.push_back(idx + 5);
        indices.push_back(idx + 11);
        indices.push_back(idx + 7);
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    qDebug() << "BridgeRenderer::buildMeshes() - No vertices/indices generated";
    m_mesh.reset();
    return;
  }

  qDebug() << "BridgeRenderer::buildMeshes() - Created mesh with"
           << vertices.size() << "vertices and" << indices.size() << "indices";
  m_mesh = std::make_unique<Mesh>(vertices, indices);
}

void BridgeRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (!m_mesh || m_bridges.empty()) {
    return;
  }

  Q_UNUSED(resources);

  // Check fog of war visibility - if any bridge is visible, render all bridges
  auto &visibility = Game::Map::VisibilityService::instance();
  const bool useVisibility = visibility.isInitialized();
  
  if (useVisibility) {
    bool anyVisible = false;
    
    // Check if any part of any bridge is visible
    for (const auto &bridge : m_bridges) {
      QVector3D dir = bridge.end - bridge.start;
      float length = dir.length();
      if (length < 0.01f)
        continue;
      
      dir.normalize();
      
      // Check start, middle, and end points
      for (int i = 0; i <= 2 && !anyVisible; ++i) {
        float t = i * 0.5f;
        QVector3D pos = bridge.start + dir * (length * t);
        
        if (visibility.isVisibleWorld(pos.x(), pos.z())) {
          anyVisible = true;
          break;
        }
      }
      
      if (anyVisible)
        break;
    }
    
    if (!anyVisible) {
      // No bridges visible - skip rendering entirely
      return;
    }
  }

  // Get the bridge shader
  auto shader = renderer.getShader("bridge");
  if (!shader) {
    qDebug() << "BridgeRenderer::submit() - Bridge shader not found! Falling back to basic shader";
    shader = renderer.getShader("basic");
    if (!shader) {
      qDebug() << "BridgeRenderer::submit() - Basic shader also not found!";
      return;
    }
  }

  // Set the shader as current
  renderer.setCurrentShader(shader);

  // Submit the bridge mesh
  QMatrix4x4 model;
  model.setToIdentity();
  
  // Medieval stone color (weathered gray stone)
  QVector3D stoneColor(0.55f, 0.52f, 0.48f);
  
  // Use mesh() which will use the bridge shader
  renderer.mesh(m_mesh.get(), model, stoneColor, nullptr, 1.0f);

  // Reset the current shader
  renderer.setCurrentShader(nullptr);
}

} // namespace Render::GL

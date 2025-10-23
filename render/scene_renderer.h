#pragma once

#include "draw_queue.h"
#include "gl/backend.h"
#include "gl/camera.h"
#include "gl/mesh.h"
#include "gl/resources.h"
#include "gl/texture.h"
#include "submitter.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Game {
namespace Systems {
class ArrowSystem;
}
} // namespace Game

namespace Render::GL {

class Backend;

class Renderer : public ISubmitter {
public:
  Renderer();
  ~Renderer();

  bool initialize();
  void shutdown();

  void beginFrame();
  void endFrame();
  void setViewport(int width, int height);

  void setCamera(Camera *camera);
  void setClearColor(float r, float g, float b, float a = 1.0f);

  void updateAnimationTime(float deltaTime) { m_accumulatedTime += deltaTime; }
  float getAnimationTime() const { return m_accumulatedTime; }

  ResourceManager *resources() const {
    return m_backend ? m_backend->resources() : nullptr;
  }
  void setHoveredEntityId(unsigned int id) { m_hoveredEntityId = id; }
  void setLocalOwnerId(int ownerId) { m_localOwnerId = ownerId; }

  void setSelectedEntities(const std::vector<unsigned int> &ids) {
    m_selectedIds.clear();
    m_selectedIds.insert(ids.begin(), ids.end());
  }

  Mesh *getMeshQuad() const {
    return m_backend && m_backend->resources() ? m_backend->resources()->quad()
                                               : nullptr;
  }
  Mesh *getMeshPlane() const {
    return m_backend && m_backend->resources()
               ? m_backend->resources()->ground()
               : nullptr;
  }
  Mesh *getMeshCube() const {
    return m_backend && m_backend->resources() ? m_backend->resources()->unit()
                                               : nullptr;
  }

  Texture *getWhiteTexture() const {
    return m_backend && m_backend->resources() ? m_backend->resources()->white()
                                               : nullptr;
  }

  Shader *getShader(const QString &name) const {
    return m_backend ? m_backend->shader(name) : nullptr;
  }
  Shader *loadShader(const QString &name, const QString &vertPath,
                     const QString &fragPath) {
    return m_backend ? m_backend->getOrLoadShader(name, vertPath, fragPath)
                     : nullptr;
  }

  void setCurrentShader(Shader *shader) { m_currentShader = shader; }
  Shader *getCurrentShader() const { return m_currentShader; }

  struct GridParams {
    float cellSize = 1.0f;
    float thickness = 0.06f;
    QVector3D gridColor{0.15f, 0.18f, 0.15f};
    float extent = 50.0f;
  };
  void setGridParams(const GridParams &gp) { m_gridParams = gp; }
  const GridParams &gridParams() const { return m_gridParams; }

  void pause() { m_paused = true; }
  void resume() { m_paused = false; }
  bool isPaused() const { return m_paused; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *texture = nullptr, float alpha = 1.0f) override;
  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0f) override;
  void selectionRing(const QMatrix4x4 &model, float alphaInner,
                     float alphaOuter, const QVector3D &color) override;

  void grid(const QMatrix4x4 &model, const QVector3D &color, float cellSize,
            float thickness, float extent) override;

  void selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                      float baseAlpha = 0.15f) override;
  void terrainChunk(Mesh *mesh, const QMatrix4x4 &model,
                    const TerrainChunkParams &params,
                    std::uint16_t sortKey = 0x8000u, bool depthWrite = true,
                    float depthBias = 0.0f);

  void renderWorld(Engine::Core::World *world);

  void lockWorldForModification() { m_worldMutex.lock(); }
  void unlockWorldForModification() { m_worldMutex.unlock(); }

  void fogBatch(const FogInstanceData *instances, std::size_t count);
  void grassBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                  const GrassBatchParams &params);
  void stoneBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                  const StoneBatchParams &params);
  void plantBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                  const PlantBatchParams &params);
  void pineBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                 const PineBatchParams &params);
  void firecampBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                     const FireCampBatchParams &params);

private:
  void enqueueSelectionRing(Engine::Core::Entity *entity,
                            Engine::Core::TransformComponent *transform,
                            Engine::Core::UnitComponent *unitComp,
                            bool selected, bool hovered);

  Camera *m_camera = nullptr;
  std::shared_ptr<Backend> m_backend;
  DrawQueue m_queues[2];
  DrawQueue *m_activeQueue = nullptr;
  int m_fillQueueIndex = 0;
  int m_renderQueueIndex = 1;

  std::unique_ptr<EntityRendererRegistry> m_entityRegistry;
  unsigned int m_hoveredEntityId = 0;
  std::unordered_set<unsigned int> m_selectedIds;

  int m_viewportWidth = 0;
  int m_viewportHeight = 0;
  GridParams m_gridParams;
  float m_accumulatedTime = 0.0f;
  std::atomic<bool> m_paused{false};

  std::mutex m_worldMutex;
  int m_localOwnerId = 1;

  QMatrix4x4 m_viewProj;
  Shader *m_currentShader = nullptr;
};

struct FrameScope {
  Renderer &r;
  FrameScope(Renderer &renderer) : r(renderer) { r.beginFrame(); }
  ~FrameScope() { r.endFrame(); }
};

} // namespace Render::GL

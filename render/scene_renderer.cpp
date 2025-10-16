#include "scene_renderer.h"
#include "../game/map/visibility_service.h"
#include "entity/registry.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "gl/backend.h"
#include "gl/camera.h"
#include "gl/primitives.h"
#include "gl/resources.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {
const QVector3D kAxisX(1.0f, 0.0f, 0.0f);
const QVector3D kAxisY(0.0f, 1.0f, 0.0f);
const QVector3D kAxisZ(0.0f, 0.0f, 1.0f);
} // namespace

Renderer::Renderer() { m_activeQueue = &m_queues[m_fillQueueIndex]; }

Renderer::~Renderer() { shutdown(); }

bool Renderer::initialize() {
  if (!m_backend)
    m_backend = std::make_shared<Backend>();
  m_backend->initialize();
  m_entityRegistry = std::make_unique<EntityRendererRegistry>();
  registerBuiltInEntityRenderers(*m_entityRegistry);
  return true;
}

void Renderer::shutdown() { m_backend.reset(); }

void Renderer::beginFrame() {
  m_activeQueue = &m_queues[m_fillQueueIndex];
  m_activeQueue->clear();

  if (m_camera) {
    m_viewProj = m_camera->getProjectionMatrix() * m_camera->getViewMatrix();
  }

  if (m_backend)
    m_backend->beginFrame();
}

void Renderer::endFrame() {
  if (m_paused.load())
    return;
  if (m_backend && m_camera) {
    std::swap(m_fillQueueIndex, m_renderQueueIndex);
    DrawQueue &renderQueue = m_queues[m_renderQueueIndex];
    renderQueue.sortForBatching();
    m_backend->execute(renderQueue, *m_camera);
  }
}

void Renderer::setCamera(Camera *camera) { m_camera = camera; }

void Renderer::setClearColor(float r, float g, float b, float a) {
  if (m_backend)
    m_backend->setClearColor(r, g, b, a);
}

void Renderer::setViewport(int width, int height) {
  m_viewportWidth = width;
  m_viewportHeight = height;
  if (m_backend)
    m_backend->setViewport(width, height);
  if (m_camera && height > 0) {
    float aspect = float(width) / float(height);
    m_camera->setPerspective(m_camera->getFOV(), aspect, m_camera->getNear(),
                             m_camera->getFar());
  }
}
void Renderer::mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *texture, float alpha) {
  if (!mesh)
    return;

  if (mesh == getUnitCylinder() && (!texture)) {
    QVector3D start, end;
    float radius = 0.0f;
    if (detail::decomposeUnitCylinder(model, start, end, radius)) {
      cylinder(start, end, radius, color, alpha);
      return;
    }
  }
  MeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.model = model;
  cmd.mvp = m_viewProj * model;
  cmd.color = color;
  cmd.alpha = alpha;
  if (m_activeQueue)
    m_activeQueue->submit(cmd);
}

void Renderer::cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color, float alpha) {
  CylinderCmd cmd;
  cmd.start = start;
  cmd.end = end;
  cmd.radius = radius;
  cmd.color = color;
  cmd.alpha = alpha;
  if (m_activeQueue)
    m_activeQueue->submit(cmd);
}

void Renderer::fogBatch(const FogInstanceData *instances, std::size_t count) {
  if (!instances || count == 0 || !m_activeQueue)
    return;
  FogBatchCmd cmd;
  cmd.instances = instances;
  cmd.count = count;
  m_activeQueue->submit(cmd);
}

void Renderer::grassBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                          const GrassBatchParams &params) {
  if (!instanceBuffer || instanceCount == 0 || !m_activeQueue)
    return;
  GrassBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instanceCount = instanceCount;
  cmd.params = params;
  cmd.params.time = m_accumulatedTime;
  m_activeQueue->submit(cmd);
}

void Renderer::stoneBatch(Buffer *instanceBuffer, std::size_t instanceCount,
                          const StoneBatchParams &params) {
  if (!instanceBuffer || instanceCount == 0 || !m_activeQueue)
    return;
  StoneBatchCmd cmd;
  cmd.instanceBuffer = instanceBuffer;
  cmd.instanceCount = instanceCount;
  cmd.params = params;
  m_activeQueue->submit(cmd);
}

void Renderer::terrainChunk(Mesh *mesh, const QMatrix4x4 &model,
                            const TerrainChunkParams &params,
                            std::uint16_t sortKey, bool depthWrite,
                            float depthBias) {
  if (!mesh || !m_activeQueue)
    return;
  TerrainChunkCmd cmd;
  cmd.mesh = mesh;
  cmd.model = model;
  cmd.params = params;
  cmd.sortKey = sortKey;
  cmd.depthWrite = depthWrite;
  cmd.depthBias = depthBias;
  m_activeQueue->submit(cmd);
}

void Renderer::selectionRing(const QMatrix4x4 &model, float alphaInner,
                             float alphaOuter, const QVector3D &color) {
  SelectionRingCmd cmd;
  cmd.model = model;
  cmd.mvp = m_viewProj * model;
  cmd.alphaInner = alphaInner;
  cmd.alphaOuter = alphaOuter;
  cmd.color = color;
  if (m_activeQueue)
    m_activeQueue->submit(cmd);
}

void Renderer::grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cellSize, float thickness, float extent) {
  GridCmd cmd;
  cmd.model = model;
  cmd.mvp = m_viewProj * model;
  cmd.color = color;
  cmd.cellSize = cellSize;
  cmd.thickness = thickness;
  cmd.extent = extent;
  if (m_activeQueue)
    m_activeQueue->submit(cmd);
}

void Renderer::selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                              float baseAlpha) {
  SelectionSmokeCmd cmd;
  cmd.model = model;
  cmd.mvp = m_viewProj * model;
  cmd.color = color;
  cmd.baseAlpha = baseAlpha;
  if (m_activeQueue)
    m_activeQueue->submit(cmd);
}

void Renderer::renderWorld(Engine::Core::World *world) {
  if (m_paused.load())
    return;
  if (!world)
    return;

  std::lock_guard<std::mutex> guard(m_worldMutex);

  auto &vis = Game::Map::VisibilityService::instance();
  const bool visibilityEnabled = vis.isInitialized();

  auto renderableEntities =
      world->getEntitiesWith<Engine::Core::RenderableComponent>();

  for (auto entity : renderableEntities) {
    auto renderable = entity->getComponent<Engine::Core::RenderableComponent>();
    auto transform = entity->getComponent<Engine::Core::TransformComponent>();

    if (!renderable->visible || !transform) {
      continue;
    }

    auto *unitComp = entity->getComponent<Engine::Core::UnitComponent>();
    if (unitComp && unitComp->ownerId != m_localOwnerId) {
      if (visibilityEnabled) {
        if (!vis.isVisibleWorld(transform->position.x, transform->position.z)) {
          continue;
        }
      }
    }

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(transform->position.x, transform->position.y,
                          transform->position.z);
    modelMatrix.rotate(transform->rotation.x, kAxisX);
    modelMatrix.rotate(transform->rotation.y, kAxisY);
    modelMatrix.rotate(transform->rotation.z, kAxisZ);
    modelMatrix.scale(transform->scale.x, transform->scale.y,
                      transform->scale.z);

    bool drawnByRegistry = false;
    if (unitComp && !unitComp->unitType.empty() && m_entityRegistry) {
      auto fn = m_entityRegistry->get(unitComp->unitType);
      if (fn) {
        DrawContext ctx{resources(), entity, world, modelMatrix};

        ctx.selected =
            (m_selectedIds.find(entity->getId()) != m_selectedIds.end());
        ctx.hovered = (entity->getId() == m_hoveredEntityId);
        ctx.animationTime = m_accumulatedTime;
        fn(ctx, *this);
        drawnByRegistry = true;
      }
    }
    if (drawnByRegistry)
      continue;

    Mesh *meshToDraw = nullptr;
    ResourceManager *res = resources();
    switch (renderable->mesh) {
    case Engine::Core::RenderableComponent::MeshKind::Quad:
      meshToDraw = res ? res->quad() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Plane:
      meshToDraw = res ? res->ground() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Cube:
      meshToDraw = res ? res->unit() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Capsule:
      meshToDraw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Ring:
      meshToDraw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::None:
    default:
      break;
    }
    if (!meshToDraw && res)
      meshToDraw = res->unit();
    if (!meshToDraw && res)
      meshToDraw = res->quad();
    QVector3D color = QVector3D(renderable->color[0], renderable->color[1],
                                renderable->color[2]);

    if (res) {
      Mesh *contactQuad = res->quad();
      Texture *white = res->white();
      if (contactQuad && white) {
        QMatrix4x4 contactBase;
        contactBase.translate(transform->position.x,
                              transform->position.y + 0.03f,
                              transform->position.z);
        contactBase.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
        float footprint =
            std::max({transform->scale.x, transform->scale.z, 0.6f});

        float sizeRatio = 1.0f;
        if (auto *unit = entity->getComponent<Engine::Core::UnitComponent>()) {
          int mh = std::max(1, unit->maxHealth);
          sizeRatio = std::clamp(unit->health / float(mh), 0.0f, 1.0f);
        }
        float eased = 0.25f + 0.75f * sizeRatio;

        float baseScaleX = footprint * 0.55f * eased;
        float baseScaleY = footprint * 0.35f * eased;

        QVector3D col(0.03f, 0.03f, 0.03f);
        float centerAlpha = 0.32f * eased;
        float midAlpha = 0.16f * eased;
        float outerAlpha = 0.07f * eased;

        QMatrix4x4 c0 = contactBase;
        c0.scale(baseScaleX * 0.60f, baseScaleY * 0.60f, 1.0f);
        mesh(contactQuad, c0, col, white, centerAlpha);

        QMatrix4x4 c1 = contactBase;
        c1.scale(baseScaleX * 0.95f, baseScaleY * 0.95f, 1.0f);
        mesh(contactQuad, c1, col, white, midAlpha);

        QMatrix4x4 c2 = contactBase;
        c2.scale(baseScaleX * 1.35f, baseScaleY * 1.35f, 1.0f);
        mesh(contactQuad, c2, col, white, outerAlpha);
      }
    }
    mesh(meshToDraw, modelMatrix, color, res ? res->white() : nullptr, 1.0f);
  }
}

} // namespace Render::GL

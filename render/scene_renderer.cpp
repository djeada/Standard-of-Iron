#include "scene_renderer.h"
#include "entity/registry.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "gl/backend.h"
#include "gl/camera.h"
#include "gl/resources.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace Render::GL {

Renderer::Renderer() {}

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
  if (m_backend)
    m_backend->beginFrame();
  m_queue.clear();
}

void Renderer::endFrame() {
  if (m_backend && m_camera) {
    m_queue.sortForBatching();
    m_backend->execute(m_queue, *m_camera);
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
  MeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.model = model;
  cmd.color = color;
  cmd.alpha = alpha;
  m_queue.submit(cmd);
}

void Renderer::selectionRing(const QMatrix4x4 &model, float alphaInner,
                             float alphaOuter, const QVector3D &color) {
  SelectionRingCmd cmd;
  cmd.model = model;
  cmd.alphaInner = alphaInner;
  cmd.alphaOuter = alphaOuter;
  cmd.color = color;
  m_queue.submit(cmd);
}

void Renderer::grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cellSize, float thickness, float extent) {
  GridCmd cmd;
  cmd.model = model;
  cmd.color = color;
  cmd.cellSize = cellSize;
  cmd.thickness = thickness;
  cmd.extent = extent;
  m_queue.submit(cmd);
}

void Renderer::selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                              float baseAlpha) {
  SelectionSmokeCmd cmd;
  cmd.model = model;
  cmd.color = color;
  cmd.baseAlpha = baseAlpha;
  m_queue.submit(cmd);
}

void Renderer::renderWorld(Engine::Core::World *world) {
  if (!world)
    return;

  auto renderableEntities =
      world->getEntitiesWith<Engine::Core::RenderableComponent>();

  for (auto entity : renderableEntities) {
    auto renderable = entity->getComponent<Engine::Core::RenderableComponent>();
    auto transform = entity->getComponent<Engine::Core::TransformComponent>();

    if (!renderable->visible || !transform) {
      continue;
    }

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(transform->position.x, transform->position.y,
                          transform->position.z);
    modelMatrix.rotate(transform->rotation.x, QVector3D(1, 0, 0));
    modelMatrix.rotate(transform->rotation.y, QVector3D(0, 1, 0));
    modelMatrix.rotate(transform->rotation.z, QVector3D(0, 0, 1));
    modelMatrix.scale(transform->scale.x, transform->scale.y,
                      transform->scale.z);

    bool drawnByRegistry = false;
    if (auto *unit = entity->getComponent<Engine::Core::UnitComponent>()) {
      if (!unit->unitType.empty() && m_entityRegistry) {
        auto fn = m_entityRegistry->get(unit->unitType);
        if (fn) {
          DrawContext ctx{resources(), entity, modelMatrix};

          ctx.selected =
              (m_selectedIds.find(entity->getId()) != m_selectedIds.end());
          ctx.hovered = (entity->getId() == m_hoveredBuildingId);
          fn(ctx, *this);
          drawnByRegistry = true;
        }
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
    mesh(meshToDraw, modelMatrix, color, res ? res->white() : nullptr, 1.0f);
  }
}

} // namespace Render::GL

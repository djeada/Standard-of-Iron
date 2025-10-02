#include "scene_renderer.h"
#include "gl/camera.h"
#include "gl/resources.h"
#include "gl/backend.h"
#include "game/core/world.h"
#include "game/core/component.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include "entity/registry.h"
#include "geom/selection_ring.h"

namespace Render::GL {

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize() {
    if (!m_backend) m_backend = std::make_shared<Backend>();
    if (!m_resources) m_resources = std::make_shared<ResourceManager>();
    m_backend->initialize();
    if (!m_resources->initialize()) {
        qWarning() << "Failed to initialize GL resources";
    }
    m_entityRegistry = std::make_unique<EntityRendererRegistry>();
    registerBuiltInEntityRenderers(*m_entityRegistry);
    return true;
}

void Renderer::shutdown() { m_resources.reset(); m_backend.reset(); }

void Renderer::beginFrame() {
    if (m_backend) m_backend->beginFrame();
    m_queue.clear();
}

void Renderer::endFrame() {
    if (m_backend && m_camera && m_resources) {
        m_backend->execute(m_queue, *m_camera, *m_resources);
    }
}

void Renderer::setCamera(Camera* camera) {
    m_camera = camera;
}

void Renderer::setClearColor(float r, float g, float b, float a) { if (m_backend) m_backend->setClearColor(r,g,b,a); }

void Renderer::setViewport(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
    if (m_backend) m_backend->setViewport(width, height);
    if (m_camera && height > 0) {
        float aspect = float(width) / float(height);
        m_camera->setPerspective(m_camera->getFOV(), aspect, m_camera->getNear(), m_camera->getFar());
    }
}
void Renderer::queueMeshColored(Mesh* mesh, const QMatrix4x4& modelMatrix, const QVector3D& color, Texture* texture) {
    if (!mesh) return;
    DrawItem item; item.mesh = mesh; item.texture = texture; item.model = modelMatrix; item.color = color;
    m_queue.submit(item);
}

void Renderer::submitRenderCommand(const RenderCommand& command) {
    queueMeshColored(command.mesh, command.modelMatrix, command.color, command.texture);
}

void Renderer::renderWorld(Engine::Core::World* world) {
    if (!world) {
        return;
    }
    // Ground drawing delegated to backend/higher-level code

    // Draw hover ring before entities so buildings naturally occlude it
    if (m_hoveredBuildingId) {
        if (auto* hovered = world->getEntity(m_hoveredBuildingId)) {
            if (hovered->hasComponent<Engine::Core::BuildingComponent>()) {
                if (auto* t = hovered->getComponent<Engine::Core::TransformComponent>()) {
                    Mesh* ring = Render::Geom::SelectionRing::get();
                    if (ring && m_camera) {
                        const float marginXZ = 1.25f;
                        const float pad = 1.06f;
                        float sx = std::max(0.6f, t->scale.x * marginXZ * pad * 1.5f);
                        float sz = std::max(0.6f, t->scale.z * marginXZ * pad * 1.5f);
                        QMatrix4x4 model;
                        model.translate(t->position.x, 0.01f, t->position.z);
                        model.scale(sx, 1.0f, sz);
                        // Shadow-like color (dark gray)
                        QVector3D c(0.0f, 0.0f, 0.0f);
                        QMatrix4x4 feather = model; feather.scale(1.08f, 1.0f, 1.08f);
                        queueMeshColored(ring, feather, c, m_resources ? m_resources->white() : nullptr);
                        queueMeshColored(ring, model, c, m_resources ? m_resources->white() : nullptr);
                    }
                }
            }
        }
    }

    // Get all entities with both transform and renderable components
    auto renderableEntities = world->getEntitiesWith<Engine::Core::RenderableComponent>();

    for (auto entity : renderableEntities) {
        auto renderable = entity->getComponent<Engine::Core::RenderableComponent>();
        auto transform = entity->getComponent<Engine::Core::TransformComponent>();

        if (!renderable->visible || !transform) {
            continue;
        }

        // Build model matrix from transform
        QMatrix4x4 modelMatrix;
        modelMatrix.translate(transform->position.x, transform->position.y, transform->position.z);
        modelMatrix.rotate(transform->rotation.x, QVector3D(1, 0, 0));
        modelMatrix.rotate(transform->rotation.y, QVector3D(0, 1, 0));
        modelMatrix.rotate(transform->rotation.z, QVector3D(0, 0, 1));
        modelMatrix.scale(transform->scale.x, transform->scale.y, transform->scale.z);

        // If entity has a unitType, try registry-based renderer first
        bool drawnByRegistry = false;
        if (auto* unit = entity->getComponent<Engine::Core::UnitComponent>()) {
            if (!unit->unitType.empty() && m_entityRegistry) {
                auto fn = m_entityRegistry->get(unit->unitType);
                if (fn) {
                    DrawParams params{this, m_resources.get(), entity, modelMatrix};
                    // Selection routed from app via setSelectedEntities to avoid mutating ECS flags for rendering
                    params.selected = (m_selectedIds.find(entity->getId()) != m_selectedIds.end());
                    fn(params);
                    drawnByRegistry = true;
                }
            }
        }
        if (drawnByRegistry) {
            // Draw rally flag marker if this is a barracks with a set rally
            if (auto* unit = entity->getComponent<Engine::Core::UnitComponent>()) {
                if (unit->unitType == "barracks") {
                    if (auto* prod = entity->getComponent<Engine::Core::ProductionComponent>()) {
                        if (prod->rallySet && m_resources) {
                            QMatrix4x4 flagModel;
                            flagModel.translate(prod->rallyX, 0.1f, prod->rallyZ);
                            flagModel.scale(0.2f, 0.2f, 0.2f);
                            queueMeshColored(m_resources->unit(), flagModel, QVector3D(1.0f, 0.9f, 0.2f), m_resources->white());
                        }
                    }
                }
            }
            continue;
        }

        // Else choose mesh based on RenderableComponent hint
        RenderCommand command;
        command.modelMatrix = modelMatrix;
        Mesh* meshToDraw = nullptr;
        switch (renderable->mesh) {
            case Engine::Core::RenderableComponent::MeshKind::Quad:    meshToDraw = m_resources? m_resources->quad() : nullptr; break;
            case Engine::Core::RenderableComponent::MeshKind::Plane:   meshToDraw = m_resources? m_resources->ground() : nullptr; break;
            case Engine::Core::RenderableComponent::MeshKind::Cube:    meshToDraw = m_resources? m_resources->unit() : nullptr; break;
            case Engine::Core::RenderableComponent::MeshKind::Capsule: meshToDraw = nullptr; break; // handled by specific renderer when available
            case Engine::Core::RenderableComponent::MeshKind::Ring:    meshToDraw = nullptr; break; // rings are drawn explicitly when needed
            case Engine::Core::RenderableComponent::MeshKind::None:    default: break;
        }
        if (!meshToDraw && m_resources) meshToDraw = m_resources->unit();
        if (!meshToDraw && m_resources) meshToDraw = m_resources->quad();
        command.mesh = meshToDraw;
        command.texture = (m_resources ? m_resources->white() : nullptr);
        // Use per-entity color if set, else a default
        command.color = QVector3D(renderable->color[0], renderable->color[1], renderable->color[2]);
    submitRenderCommand(command);

        // If this render path drew a barracks (no custom renderer used), also draw rally flag
        if (auto* unit = entity->getComponent<Engine::Core::UnitComponent>()) {
            if (unit->unitType == "barracks") {
                if (auto* prod = entity->getComponent<Engine::Core::ProductionComponent>()) {
                    if (prod->rallySet && m_resources) {
                        QMatrix4x4 flagModel;
                        flagModel.translate(prod->rallyX, 0.1f, prod->rallyZ);
                        flagModel.scale(0.2f, 0.2f, 0.2f);
                        queueMeshColored(m_resources->unit(), flagModel, QVector3D(1.0f, 0.9f, 0.2f), m_resources->white());
                    }
                }
            }
        }
        // Selection ring is drawn by entity-specific renderer if desired
    }
}

} // namespace Render::GL

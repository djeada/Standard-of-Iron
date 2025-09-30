#include "renderer.h"
#include "../../game/core/world.h"
#include "../../game/core/component.h"
#include <QDebug>
#include <QOpenGLContext>
#include <algorithm>
#include <cmath>
#include "../entity/registry.h"

namespace Render::GL {

Renderer::Renderer() {
    // Defer OpenGL function initialization until a valid context is current
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize() {
    // Ensure an OpenGL context is current before using any GL calls
    if (!QOpenGLContext::currentContext()) {
        qWarning() << "Renderer::initialize called without a current OpenGL context";
        return false;
    }

    initializeOpenGLFunctions();
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set default clear color with alpha 0 to allow QML overlay compositing
    setClearColor(0.2f, 0.3f, 0.3f, 0.0f);
    
    if (!loadShaders()) {
        return false;
    }
    // Initialize shared/default GL resources when not injected
    if (!m_resources) m_resources = std::make_shared<ResourceManager>();
    if (!m_resources->initialize()) {
        qWarning() << "Failed to initialize GL resources";
        // Non-fatal: renderer can still function in a limited capacity
    }
    // Set up entity renderer registry (built-ins)
    m_entityRegistry = std::make_unique<EntityRendererRegistry>();
    registerBuiltInEntityRenderers(*m_entityRegistry);
    
    return true;
}

void Renderer::shutdown() {
    m_basicShader.reset();
    m_lineShader.reset();
    m_gridShader.reset();
    m_resources.reset();
}

void Renderer::beginFrame() {
    if (m_viewportWidth > 0 && m_viewportHeight > 0) {
        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_renderQueue.clear();
}

void Renderer::endFrame() {
    flushBatch();
}
void Renderer::renderGridGround() {
    Mesh* groundMesh = (m_resources ? m_resources->ground() : nullptr);
    if (!groundMesh || !m_camera) return;
    QMatrix4x4 groundModel;
    groundModel.translate(0.0f, 0.0f, 0.0f);
    groundModel.scale(m_gridParams.extent, 1.0f, m_gridParams.extent);
    if (m_gridShader) {
        m_gridShader->use();
        m_gridShader->setUniform("u_model", groundModel);
        m_gridShader->setUniform("u_view", m_camera->getViewMatrix());
        m_gridShader->setUniform("u_projection", m_camera->getProjectionMatrix());
        m_gridShader->setUniform("u_gridColor", m_gridParams.gridColor);
        m_gridShader->setUniform("u_lineColor", m_gridParams.lineColor);
        m_gridShader->setUniform("u_cellSize", m_gridParams.cellSize);
        m_gridShader->setUniform("u_thickness", m_gridParams.thickness);
        groundMesh->draw();
        m_gridShader->release();
    } else {
        drawMeshColored(groundMesh, groundModel, m_gridParams.gridColor);
    }
}

void Renderer::setCamera(Camera* camera) {
    m_camera = camera;
}

void Renderer::setClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void Renderer::setViewport(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void Renderer::drawMesh(Mesh* mesh, const QMatrix4x4& modelMatrix, Texture* texture) {
    if (!mesh || !m_basicShader || !m_camera) {
        return;
    }
    
    m_basicShader->use();
    
    // Set matrices
    m_basicShader->setUniform("u_model", modelMatrix);
    m_basicShader->setUniform("u_view", m_camera->getViewMatrix());
    m_basicShader->setUniform("u_projection", m_camera->getProjectionMatrix());
    
    // Bind texture
    if (texture) {
        texture->bind(0);
        m_basicShader->setUniform("u_texture", 0);
        m_basicShader->setUniform("u_useTexture", true);
    } else {
        if (m_resources && m_resources->white()) {
            m_resources->white()->bind(0);
            m_basicShader->setUniform("u_texture", 0);
        }
        m_basicShader->setUniform("u_useTexture", false);
    }
    
    m_basicShader->setUniform("u_color", QVector3D(1.0f, 1.0f, 1.0f));
    
    mesh->draw();
    
    m_basicShader->release();
}

void Renderer::drawMeshColored(Mesh* mesh, const QMatrix4x4& modelMatrix, const QVector3D& color, Texture* texture) {
    if (!mesh || !m_basicShader || !m_camera) {
        return;
    }
    m_basicShader->use();
    m_basicShader->setUniform("u_model", modelMatrix);
    m_basicShader->setUniform("u_view", m_camera->getViewMatrix());
    m_basicShader->setUniform("u_projection", m_camera->getProjectionMatrix());
    if (texture) {
        texture->bind(0);
        m_basicShader->setUniform("u_texture", 0);
        m_basicShader->setUniform("u_useTexture", true);
    } else {
        if (m_resources && m_resources->white()) {
            m_resources->white()->bind(0);
            m_basicShader->setUniform("u_texture", 0);
        }
        m_basicShader->setUniform("u_useTexture", false);
    }
    m_basicShader->setUniform("u_color", color);
    mesh->draw();
    m_basicShader->release();
}

void Renderer::drawLine(const QVector3D& start, const QVector3D& end, const QVector3D& color) {
    // Simple line drawing implementation
    // In a full implementation, you'd want a proper line renderer
}

void Renderer::submitRenderCommand(const RenderCommand& command) {
    m_renderQueue.push_back(command);
}

void Renderer::flushBatch() {
    if (m_renderQueue.empty()) {
        return;
    }
    
    sortRenderQueue();
    
    for (const auto& command : m_renderQueue) {
        drawMeshColored(command.mesh, command.modelMatrix, command.color, command.texture);
    }
    
    m_renderQueue.clear();
}

void Renderer::renderWorld(Engine::Core::World* world) {
    if (!world) {
        return;
    }
    // Draw ground plane with grid using helper
    renderGridGround();
    
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
                    fn(params);
                    drawnByRegistry = true;
                }
            }
        }
        if (drawnByRegistry) continue;
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
        // Selection ring is drawn by entity-specific renderer if desired
    }
}

bool Renderer::loadShaders() {
    // Load shaders from assets
    const QString base = QStringLiteral("assets/shaders/");
    const QString basicVert = base + QStringLiteral("basic.vert");
    const QString basicFrag = base + QStringLiteral("basic.frag");
    const QString gridFrag  = base + QStringLiteral("grid.frag");

    m_basicShader = std::make_unique<Shader>();
    if (!m_basicShader->loadFromFiles(basicVert, basicFrag)) {
        qWarning() << "Failed to load basic shader from files" << basicVert << basicFrag;
        return false;
    }
    m_gridShader = std::make_unique<Shader>();
    // Reuse the same vertex shader for grid; load vertex from file and fragment from grid
    if (!m_gridShader->loadFromFiles(basicVert, gridFrag)) {
        qWarning() << "Failed to load grid shader from files" << basicVert << gridFrag;
        m_gridShader.reset();
    }
    return true;
}

void Renderer::sortRenderQueue() {
    // Simple sorting by texture to reduce state changes
    std::sort(m_renderQueue.begin(), m_renderQueue.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.texture < b.texture;
        });
}

} // namespace Render::GL
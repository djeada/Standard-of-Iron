#include "renderer.h"
#include "../../engine/core/world.h"
#include "../../engine/core/component.h"
#include <QDebug>
#include <QOpenGLContext>
#include <algorithm>
#include <cmath>

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
    
    createDefaultResources();
    
    return true;
}

void Renderer::shutdown() {
    m_basicShader.reset();
    m_lineShader.reset();
    m_gridShader.reset();
    m_quadMesh.reset();
    m_capsuleMesh.reset();
    m_ringMesh.reset();
    m_whiteTexture.reset();
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
        m_whiteTexture->bind(0);
        m_basicShader->setUniform("u_texture", 0);
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
        m_whiteTexture->bind(0);
        m_basicShader->setUniform("u_texture", 0);
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
    // Draw ground plane with grid
    if (m_groundMesh) {
        QMatrix4x4 groundModel;
        groundModel.translate(0.0f, 0.0f, 0.0f);
        groundModel.scale(50.0f, 1.0f, 50.0f);
        if (m_gridShader) {
            m_gridShader->use();
            m_gridShader->setUniform("u_model", groundModel);
            m_gridShader->setUniform("u_view", m_camera->getViewMatrix());
            m_gridShader->setUniform("u_projection", m_camera->getProjectionMatrix());
            m_gridShader->setUniform("u_gridColor", QVector3D(0.15f, 0.18f, 0.15f));
            m_gridShader->setUniform("u_lineColor", QVector3D(0.22f, 0.25f, 0.22f));
                m_gridShader->setUniform("u_cellSize", 1.0f);
                m_gridShader->setUniform("u_thickness", 0.06f);
            m_groundMesh->draw();
            m_gridShader->release();
        } else {
            drawMeshColored(m_groundMesh.get(), groundModel, QVector3D(0.15f, 0.2f, 0.15f));
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
        
        // Use capsule mesh for units
        RenderCommand command;
        command.modelMatrix = modelMatrix;
        command.mesh = m_capsuleMesh ? m_capsuleMesh.get() : m_quadMesh.get();
        command.texture = m_whiteTexture.get();
        command.color = QVector3D(0.8f, 0.9f, 1.0f);
        submitRenderCommand(command);

        // Draw selection ring if selected
        auto unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (unit && unit->selected) {
            QMatrix4x4 ringModel;
            ringModel.translate(transform->position.x, 0.01f, transform->position.z);
            ringModel.scale(0.5f, 1.0f, 0.5f);
            renderSelectionRing(ringModel, QVector3D(0.2f, 0.8f, 0.2f));
        }
    }
}

bool Renderer::loadShaders() {
    // Basic vertex shader
    QString basicVertexSource = R"(
        #version 330 core
        layout (location = 0) in vec3 a_position;
        layout (location = 1) in vec3 a_normal;
        layout (location = 2) in vec2 a_texCoord;
        
        uniform mat4 u_model;
        uniform mat4 u_view;
        uniform mat4 u_projection;
        
        out vec3 v_normal;
        out vec2 v_texCoord;
        out vec3 v_worldPos;
        
        void main() {
            v_normal = mat3(transpose(inverse(u_model))) * a_normal;
            v_texCoord = a_texCoord;
            v_worldPos = vec3(u_model * vec4(a_position, 1.0));
            
            gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
        }
    )";
    
    // Basic fragment shader
    QString basicFragmentSource = R"(
        #version 330 core
        in vec3 v_normal;
        in vec2 v_texCoord;
        in vec3 v_worldPos;
        
        uniform sampler2D u_texture;
        uniform vec3 u_color;
        uniform bool u_useTexture;
        
        out vec4 FragColor;
        
        void main() {
            vec3 color = u_color;
            
            if (u_useTexture) {
                color *= texture(u_texture, v_texCoord).rgb;
            }
            
            // Simple lighting
            vec3 normal = normalize(v_normal);
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            float diff = max(dot(normal, lightDir), 0.2); // Ambient + diffuse
            
            color *= diff;
            
            FragColor = vec4(color, 1.0);
        }
    )";
    
    m_basicShader = std::make_unique<Shader>();
    if (!m_basicShader->loadFromSource(basicVertexSource, basicFragmentSource)) {
        qWarning() << "Failed to load basic shader";
        return false;
    }
    // Grid shader
    QString gridVertex = basicVertexSource;
    QString gridFragment = R"(
        #version 330 core
        in vec3 v_normal;
        in vec2 v_texCoord;
        in vec3 v_worldPos;
        
        uniform vec3 u_gridColor;
        uniform vec3 u_lineColor;
        uniform float u_cellSize;
        uniform float u_thickness; // fraction of cell (0..0.5)
        
        out vec4 FragColor;
        
        void main() {
            vec2 coord = v_worldPos.xz / u_cellSize;
            vec2 g = abs(fract(coord) - 0.5);
            float lineX = step(0.5 - u_thickness, g.x);
            float lineY = step(0.5 - u_thickness, g.y);
            float lineMask = max(lineX, lineY);
            vec3 col = mix(u_gridColor, u_lineColor, lineMask);
            FragColor = vec4(col, 1.0);
        }
    )";
    m_gridShader = std::make_unique<Shader>();
    if (!m_gridShader->loadFromSource(gridVertex, gridFragment)) {
        qWarning() << "Failed to load grid shader";
        // Non-fatal
        m_gridShader.reset();
    }
    return true;
}

void Renderer::createDefaultResources() {
    m_quadMesh = std::unique_ptr<Mesh>(createQuadMesh());
    m_groundMesh = std::unique_ptr<Mesh>(createPlaneMesh(1.0f, 1.0f, 64));
    // Create a simple identifiable "archer": crossed body panels + small head disk + front bow arc
    {
        std::vector<Vertex> verts;
        std::vector<unsigned int> idx;
        auto addQuad = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d, const QVector3D& n){
            size_t base = verts.size();
            verts.push_back({{a.x(), a.y(), a.z()}, {n.x(), n.y(), n.z()}, {0,0}});
            verts.push_back({{b.x(), b.y(), b.z()}, {n.x(), n.y(), n.z()}, {1,0}});
            verts.push_back({{c.x(), c.y(), c.z()}, {n.x(), n.y(), n.z()}, {1,1}});
            verts.push_back({{d.x(), d.y(), d.z()}, {n.x(), n.y(), n.z()}, {0,1}});
            idx.push_back(base+0); idx.push_back(base+1); idx.push_back(base+2);
            idx.push_back(base+2); idx.push_back(base+3); idx.push_back(base+0);
        };
        float h = 1.6f; // height
        float r = 0.25f; // width
        // vertical crossed body
        addQuad({-r,0,-0.0f},{ r,0, 0.0f},{ r,h, 0.0f},{-r,h, 0.0f},{0,0,1});
        addQuad({0,-0.0f,-r},{0, 0.0f, r},{0, h, r},{0, h,-r},{1,0,0});
        // head as small flat disk on top
        {
            int seg = 16; float headY = h + 0.15f; float headR = 0.18f; QVector3D n(0,1,0);
            for (int i=0;i<seg;i++){
                float a0 = (i    / float(seg)) * 6.2831853f;
                float a1 = ((i+1)/ float(seg)) * 6.2831853f;
                QVector3D c0(headR*std::cos(a0), headY, headR*std::sin(a0));
                QVector3D c1(headR*std::cos(a1), headY, headR*std::sin(a1));
                QVector3D center(0, headY, 0);
                size_t base = verts.size();
                verts.push_back({{center.x(),center.y(),center.z()},{n.x(),n.y(),n.z()},{0,0}});
                verts.push_back({{c0.x(),c0.y(),c0.z()},{n.x(),n.y(),n.z()},{1,0}});
                verts.push_back({{c1.x(),c1.y(),c1.z()},{n.x(),n.y(),n.z()},{1,1}});
                idx.push_back(base+0); idx.push_back(base+1); idx.push_back(base+2);
            }
        }
        // simple bow arc in front (thin quad strip)
        {
            float by0 = 0.4f, by1 = 1.2f; float bx = 0.35f; QVector3D n(0,0,1);
            addQuad({bx,by0,-0.01f},{bx,by0,0.01f},{bx,by1,0.01f},{bx,by1,-0.01f}, n);
        }
        m_capsuleMesh = std::make_unique<Mesh>(verts, idx);
    }
    // Selection ring (flat torus approximated by triangle fan)
    {
        std::vector<Vertex> verts;
        std::vector<unsigned int> idx;
        int seg = 48;
        float inner = 0.8f;
        float outer = 1.0f;
        for (int i=0;i<seg;i++){
            float a0 = (i    / float(seg)) * 6.2831853f;
            float a1 = ((i+1)/ float(seg)) * 6.2831853f;
            QVector3D n(0,1,0);
            QVector3D v0i(inner*std::cos(a0), 0.0f, inner*std::sin(a0));
            QVector3D v0o(outer*std::cos(a0), 0.0f, outer*std::sin(a0));
            QVector3D v1o(outer*std::cos(a1), 0.0f, outer*std::sin(a1));
            QVector3D v1i(inner*std::cos(a1), 0.0f, inner*std::sin(a1));
            size_t base = verts.size();
            verts.push_back({{v0i.x(),0.0f,v0i.z()}, {n.x(),n.y(),n.z()}, {0,0}});
            verts.push_back({{v0o.x(),0.0f,v0o.z()}, {n.x(),n.y(),n.z()}, {1,0}});
            verts.push_back({{v1o.x(),0.0f,v1o.z()}, {n.x(),n.y(),n.z()}, {1,1}});
            verts.push_back({{v1i.x(),0.0f,v1i.z()}, {n.x(),n.y(),n.z()}, {0,1}});
            idx.push_back(base+0); idx.push_back(base+1); idx.push_back(base+2);
            idx.push_back(base+2); idx.push_back(base+3); idx.push_back(base+0);
        }
        m_ringMesh = std::make_unique<Mesh>(verts, idx);
    }

    m_whiteTexture = std::make_unique<Texture>();
    m_whiteTexture->createEmpty(1, 1, Texture::Format::RGBA);

    // Fill with white color
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    m_whiteTexture->bind();
    initializeOpenGLFunctions();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
}

void Renderer::sortRenderQueue() {
    // Simple sorting by texture to reduce state changes
    std::sort(m_renderQueue.begin(), m_renderQueue.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.texture < b.texture;
        });
}

void Renderer::renderSelectionRing(const QMatrix4x4& model, const QVector3D& color) {
    if (!m_ringMesh || !m_basicShader || !m_camera) return;
    m_basicShader->use();
    m_basicShader->setUniform("u_model", model);
    m_basicShader->setUniform("u_view", m_camera->getViewMatrix());
    m_basicShader->setUniform("u_projection", m_camera->getProjectionMatrix());
    m_basicShader->setUniform("u_useTexture", false);
    m_basicShader->setUniform("u_color", color);
    m_ringMesh->draw();
    m_basicShader->release();
}

} // namespace Render::GL
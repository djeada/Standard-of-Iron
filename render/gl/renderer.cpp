#include "renderer.h"
#include "../../engine/core/world.h"
#include "../../engine/core/component.h"
#include <QDebug>
#include <QOpenGLContext>
#include <algorithm>

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
    
    // Set default clear color
    setClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    
    if (!loadShaders()) {
        return false;
    }
    
    createDefaultResources();
    
    return true;
}

void Renderer::shutdown() {
    m_basicShader.reset();
    m_lineShader.reset();
    m_quadMesh.reset();
    m_whiteTexture.reset();
}

void Renderer::beginFrame() {
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
        drawMesh(command.mesh, command.modelMatrix, command.texture);
    }
    
    m_renderQueue.clear();
}

void Renderer::renderWorld(Engine::Core::World* world) {
    if (!world) {
        return;
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
        
        // Create render command
        RenderCommand command;
        command.modelMatrix = modelMatrix;
        // Note: In a full implementation, you'd load mesh and texture from paths
        command.mesh = m_quadMesh.get(); // Default mesh for now
        command.texture = m_whiteTexture.get();
        
        submitRenderCommand(command);
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
    
    return true;
}

void Renderer::createDefaultResources() {
    m_quadMesh = std::unique_ptr<Mesh>(createQuadMesh());

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

} // namespace Render::GL
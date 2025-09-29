#pragma once

#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "texture.h"
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
}

namespace Render::GL {

struct RenderCommand {
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    QMatrix4x4 modelMatrix;
    QVector3D color{1.0f, 1.0f, 1.0f};
};

class Renderer : protected QOpenGLFunctions_3_3_Core {
public:
    Renderer();
    ~Renderer();

    bool initialize();
    void shutdown();
    
    void beginFrame();
    void endFrame();
    void setViewport(int width, int height);
    
    void setCamera(Camera* camera);
    void setClearColor(float r, float g, float b, float a = 1.0f);
    
    // Immediate mode rendering
    void drawMesh(Mesh* mesh, const QMatrix4x4& modelMatrix, Texture* texture = nullptr);
    void drawMeshColored(Mesh* mesh, const QMatrix4x4& modelMatrix, const QVector3D& color, Texture* texture = nullptr);
    void drawLine(const QVector3D& start, const QVector3D& end, const QVector3D& color);
    
    // Batch rendering
    void submitRenderCommand(const RenderCommand& command);
    void flushBatch();
    
    // Render ECS entities
    void renderWorld(Engine::Core::World* world);
    
private:
    Camera* m_camera = nullptr;
    
    std::unique_ptr<Shader> m_basicShader;
    std::unique_ptr<Shader> m_lineShader;
    
    std::vector<RenderCommand> m_renderQueue;
    
    // Default resources
    std::unique_ptr<Mesh> m_quadMesh;
    std::unique_ptr<Mesh> m_groundMesh;
    std::unique_ptr<Texture> m_whiteTexture;

    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    
    bool loadShaders();
    void createDefaultResources();
    void sortRenderQueue();
};

} // namespace Render::GL
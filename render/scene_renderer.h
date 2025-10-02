#pragma once

#include "gl/camera.h"
#include "gl/mesh.h"
#include "gl/texture.h"
#include "gl/resources.h"
#include "draw_queue.h"
#include "submitter.h"
#include <memory>
#include <vector>
#include <optional>
#include <unordered_set>

namespace Engine::Core {
class World;
class Entity;
}

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Game { namespace Systems { class ArrowSystem; } }

namespace Render::GL {

class Backend;

struct RenderCommand { // legacy submission shape; mapped to DrawItem internally
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    QMatrix4x4 modelMatrix;
    QVector3D color{1.0f, 1.0f, 1.0f};
};

class Renderer : public ISubmitter {
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
    // Optional: inject an external ResourceManager owned by the app
    void setResources(const std::shared_ptr<ResourceManager>& resources) { m_resources = resources; }
    void setHoveredBuildingId(unsigned int id) { m_hoveredBuildingId = id; }
    // Selection information provided by the app before rendering
    void setSelectedEntities(const std::vector<unsigned int>& ids) {
        m_selectedIds.clear();
        m_selectedIds.insert(ids.begin(), ids.end());
    }

    // High-level helpers are provided in render/entity (see arrow)

    // Read-only access to default meshes/textures for app-side batching
    Mesh* getMeshQuad()    const { return m_resources ? m_resources->quad()    : nullptr; }
    Mesh* getMeshPlane()   const { return m_resources ? m_resources->ground()  : nullptr; }
    Mesh* getMeshCube()    const { return m_resources ? m_resources->unit()    : nullptr; }
    // Meshes now provided by entity-specific renderers or shared geom providers
    Texture* getWhiteTexture() const { return m_resources ? m_resources->white() : nullptr; }
    
    struct GridParams {
        float cellSize = 1.0f;
        float thickness = 0.06f; // fraction of cell (0..0.5)
        QVector3D gridColor{0.15f, 0.18f, 0.15f};
        QVector3D lineColor{0.22f, 0.25f, 0.22f};
        float extent = 50.0f; // half-size of plane scaling
    };
    void setGridParams(const GridParams& gp) { m_gridParams = gp; }
    const GridParams& gridParams() const { return m_gridParams; }
    
    // ISubmitter implementation (enqueue for backend)
    void mesh(Mesh* mesh, const QMatrix4x4& model, const QVector3D& color,
              Texture* texture = nullptr, float alpha = 1.0f) override;
    void selectionRing(const QMatrix4x4& model, float alphaInner, float alphaOuter,
                       const QVector3D& color) override;
    void submitRenderCommand(const RenderCommand& command);
    
    // Legacy: still available but apps are encouraged to issue draw calls explicitly
    void renderWorld(Engine::Core::World* world);
    
private:
    Camera* m_camera = nullptr;
    std::shared_ptr<Backend> m_backend;
    DrawQueue m_queue;
    
    // Default resources
    std::shared_ptr<ResourceManager> m_resources;
    std::unique_ptr<EntityRendererRegistry> m_entityRegistry;
    unsigned int m_hoveredBuildingId = 0;
    std::unordered_set<unsigned int> m_selectedIds; // for selection rings at render time

    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    GridParams m_gridParams;
};

} // namespace Render::GL

#pragma once

#include <memory>
#include <QOpenGLFunctions_3_3_Core>
#include "mesh.h"
#include "texture.h"

namespace Render::GL {

class ResourceManager : protected QOpenGLFunctions_3_3_Core {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    // Must be called with a current, valid GL context
    bool initialize();

    Mesh* quad() const { return m_quadMesh.get(); }
    Mesh* ground() const { return m_groundMesh.get(); }
    Mesh* unit() const { return m_unitMesh.get(); }
    Texture* white() const { return m_whiteTexture.get(); }

private:
    std::unique_ptr<Mesh> m_quadMesh;
    std::unique_ptr<Mesh> m_groundMesh;
    std::unique_ptr<Mesh> m_unitMesh;  // generic placeholder unit mesh
    // Capsule and selection ring meshes have been moved to entity/geom modules
    std::unique_ptr<Texture> m_whiteTexture;
};

} // namespace Render::GL

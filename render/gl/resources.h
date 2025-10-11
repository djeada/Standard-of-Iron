#pragma once

#include "mesh.h"
#include "texture.h"
#include <QOpenGLFunctions_3_3_Core>
#include <memory>

#include "../geom/arrow.h"

namespace Render::GL {

class ResourceManager : protected QOpenGLFunctions_3_3_Core {
public:
  ResourceManager() = default;
  ~ResourceManager() = default;

  bool initialize();

  Mesh *quad() const { return m_quadMesh.get(); }
  Mesh *ground() const { return m_groundMesh.get(); }
  Mesh *unit() const { return m_unitMesh.get(); }
  Mesh *arrow() const { return Render::Geom::Arrow::get(); }
  Texture *white() const { return m_whiteTexture.get(); }

private:
  std::unique_ptr<Mesh> m_quadMesh;
  std::unique_ptr<Mesh> m_groundMesh;
  std::unique_ptr<Mesh> m_unitMesh;

  std::unique_ptr<Texture> m_whiteTexture;
};

} // namespace Render::GL

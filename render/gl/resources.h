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
  ~ResourceManager() override = default;

  auto initialize() -> bool;

  [[nodiscard]] auto quad() const -> Mesh * { return m_quadMesh.get(); }
  [[nodiscard]] auto ground() const -> Mesh * { return m_groundMesh.get(); }
  [[nodiscard]] auto unit() const -> Mesh * { return m_unitMesh.get(); }
  [[nodiscard]] static auto arrow() -> Mesh * {
    return Render::Geom::Arrow::get();
  }
  [[nodiscard]] auto white() const -> Texture * { return m_whiteTexture.get(); }

private:
  std::unique_ptr<Mesh> m_quadMesh;
  std::unique_ptr<Mesh> m_groundMesh;
  std::unique_ptr<Mesh> m_unitMesh;

  std::unique_ptr<Texture> m_whiteTexture;
};

} 

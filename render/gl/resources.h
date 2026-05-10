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

  [[nodiscard]] auto quad() const -> Mesh * { return m_quad_mesh.get(); }
  [[nodiscard]] auto ground() const -> Mesh * { return m_ground_mesh.get(); }
  [[nodiscard]] auto unit() const -> Mesh * { return m_unit_mesh.get(); }
  [[nodiscard]] auto white() const -> Texture * {
    return m_white_texture.get();
  }

private:
  std::unique_ptr<Mesh> m_quad_mesh;
  std::unique_ptr<Mesh> m_ground_mesh;
  std::unique_ptr<Mesh> m_unit_mesh;

  std::unique_ptr<Texture> m_white_texture;
};

} // namespace Render::GL

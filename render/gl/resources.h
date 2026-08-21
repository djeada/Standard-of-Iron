#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <memory>

#include "mesh.h"
#include "render/geom/arrow.h"
#include "texture.h"

namespace Render::GL {

class ResourceManager : protected QOpenGLFunctions_3_3_Core {
public:
  ResourceManager() = default;
  ~ResourceManager() override = default;

  auto initialize() -> bool;

  [[nodiscard]] auto quad() const -> Mesh* { return m_quad_mesh.get(); }
  [[nodiscard]] auto ground() const -> Mesh* { return m_ground_mesh.get(); }
  [[nodiscard]] auto unit() const -> Mesh* { return m_unit_mesh.get(); }
  [[nodiscard]] auto white() const -> Texture* { return m_white_texture.get(); }

  [[nodiscard]] auto material_detail() const -> Texture* {
    return m_material_detail_texture.get();
  }

  [[nodiscard]] auto wear_volume() const -> unsigned int { return m_wear_volume; }

private:
  std::unique_ptr<Mesh> m_quad_mesh;
  std::unique_ptr<Mesh> m_ground_mesh;
  std::unique_ptr<Mesh> m_unit_mesh;

  std::unique_ptr<Texture> m_white_texture;
  std::unique_ptr<Texture> m_material_detail_texture;
  unsigned int m_wear_volume = 0U;
};

} // namespace Render::GL

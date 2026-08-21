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

  // Tileable packed detail bands shared by the generic material shaders, so
  // wood grain, cloth weave, leather blotching, metal noise and soot come from
  // one mip-filtered fetch instead of per-pixel value noise.
  [[nodiscard]] auto material_detail() const -> Texture* {
    return m_material_detail_texture.get();
  }

  // Tiling volume of blocky random bands. Sampled with nearest filtering it
  // reproduces the quantized position hashes the character wear patterns used
  // to rebuild per fragment, four decorrelated channels at a time.
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

#pragma once

namespace Render::GL {

class Mesh;
class Shader;
class Texture;

class DrawResources {
public:
  struct Parts {
    Mesh* quad = nullptr;
    Mesh* ground = nullptr;
    Mesh* unit = nullptr;
    Texture* white = nullptr;
    unsigned int wear_volume = 0;
    Mesh* banner_mesh = nullptr;
    Shader* banner_shader = nullptr;
    Shader* troop_shadow_shader = nullptr;
  };

  DrawResources() = default;
  explicit DrawResources(const Parts& parts) noexcept
      : m_parts(parts) {}

  [[nodiscard]] auto quad() const noexcept -> Mesh* { return m_parts.quad; }
  [[nodiscard]] auto ground() const noexcept -> Mesh* { return m_parts.ground; }
  [[nodiscard]] auto unit() const noexcept -> Mesh* { return m_parts.unit; }
  [[nodiscard]] auto white() const noexcept -> Texture* { return m_parts.white; }
  [[nodiscard]] auto wear_volume() const noexcept -> unsigned int {
    return m_parts.wear_volume;
  }
  [[nodiscard]] auto banner_mesh() const noexcept -> Mesh* {
    return m_parts.banner_mesh;
  }
  [[nodiscard]] auto banner_shader() const noexcept -> Shader* {
    return m_parts.banner_shader;
  }
  [[nodiscard]] auto troop_shadow_shader() const noexcept -> Shader* {
    return m_parts.troop_shadow_shader;
  }

private:
  Parts m_parts;
};

} // namespace Render::GL

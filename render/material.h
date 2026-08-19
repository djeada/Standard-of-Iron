#pragma once

#include <QVector3D>

#include <cstdint>

#include "i_render_backend.h"

namespace Render::GL {

class Shader;
class Texture;

struct Material {

  Shader* shader = nullptr;

  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;

  Texture* texture = nullptr;
  std::int32_t material_id = 0;

  [[nodiscard]] auto resolve(Render::ShaderQuality backend) const noexcept -> Shader* {
    return backend == Render::ShaderQuality::None ? nullptr : shader;
  }

  [[nodiscard]] auto is_flat_only() const noexcept -> bool { return shader == nullptr; }
};

class MaterialRegistry {
public:
  static auto instance() -> MaterialRegistry& {
    static MaterialRegistry s;
    return s;
  }

  void init(Shader* basic, Shader* shadow) {
    m_basic.shader = basic;
    m_character.shader = basic;
    m_shadow.shader = shadow;
    m_initialised = (basic != nullptr);
  }

  [[nodiscard]] auto is_initialised() const noexcept -> bool { return m_initialised; }

  [[nodiscard]] auto basic() noexcept -> Material* { return &m_basic; }
  [[nodiscard]] auto character() noexcept -> Material* { return &m_character; }
  [[nodiscard]] auto shadow() noexcept -> Material* { return &m_shadow; }

private:
  Material m_basic{};
  Material m_character{};
  Material m_shadow{};
  bool m_initialised = false;
};

} // namespace Render::GL

#pragma once

#include "i_render_backend.h"

#include <QVector3D>
#include <cstdint>

namespace Render::GL {

class Shader;
class Texture;

struct Material {
  Shader *shader_full = nullptr;
  Shader *shader_reduced = nullptr;
  Shader *shader_minimal = nullptr;

  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;

  Texture *texture = nullptr;
  std::int32_t material_id = 0;

  [[nodiscard]] auto
  resolve(Render::ShaderQuality q) const noexcept -> Shader * {
    using Render::ShaderQuality;
    if (q == ShaderQuality::None) {
      return nullptr;
    }

    switch (q) {
    case ShaderQuality::Full:
      if (shader_full != nullptr) {
        return shader_full;
      }
      if (shader_reduced != nullptr) {
        return shader_reduced;
      }
      return shader_minimal;
    case ShaderQuality::Reduced:
      if (shader_reduced != nullptr) {
        return shader_reduced;
      }
      if (shader_full != nullptr) {
        return shader_full;
      }
      return shader_minimal;
    case ShaderQuality::Minimal:
      if (shader_minimal != nullptr) {
        return shader_minimal;
      }
      if (shader_reduced != nullptr) {
        return shader_reduced;
      }
      return shader_full;
    case ShaderQuality::None:
      break;
    }
    return nullptr;
  }

  [[nodiscard]] auto is_flat_only() const noexcept -> bool {
    return shader_full == nullptr && shader_reduced == nullptr &&
           shader_minimal == nullptr;
  }
};

class MaterialRegistry {
public:
  static auto instance() -> MaterialRegistry & {
    static MaterialRegistry s;
    return s;
  }

  void init(Shader *basic, Shader *shadow) {
    m_basic.shader_full = basic;
    m_basic.shader_reduced = basic;
    m_basic.shader_minimal = basic;

    m_character.shader_full = basic;
    m_character.shader_reduced = basic;
    m_character.shader_minimal = basic;

    m_shadow.shader_full = shadow;
    m_shadow.shader_reduced = shadow;
    m_shadow.shader_minimal = shadow;
    m_initialised = (basic != nullptr);
  }

  [[nodiscard]] auto is_initialised() const noexcept -> bool {
    return m_initialised;
  }

  [[nodiscard]] auto basic() noexcept -> Material * { return &m_basic; }
  [[nodiscard]] auto character() noexcept -> Material * { return &m_character; }
  [[nodiscard]] auto shadow() noexcept -> Material * { return &m_shadow; }

private:
  Material m_basic{};
  Material m_character{};
  Material m_shadow{};
  bool m_initialised = false;
};

} // namespace Render::GL

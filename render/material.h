#pragma once

// Stage 5 seam — material + shader-quality plumbing.
//
// A Material describes what a DrawPartCmd *looks like* independent of how
// it is rendered. It carries up to three shader variants (Full / Reduced /
// Minimal) plus colour/texture. The None tier is intentionally implicit:
// if no shader is available (or ShaderQuality::None is requested), the
// material reports a nullptr shader and the backend routes the command
// through the flat-colour / software path.
//
// resolve() implements an upward fallback so that a pipeline which only
// shipped the "Full" shader still renders on a Reduced/Minimal request.
// This keeps the asset-author contract simple: provide at least one tier
// and the engine degrades gracefully.

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

  // Pick the shader that best satisfies `q`. Returns nullptr when either
  //   - `q == ShaderQuality::None`, or
  //   - no shader tier is populated at all.
  // Callers MUST treat nullptr as "use the flat-colour path".
  [[nodiscard]] auto resolve(Render::ShaderQuality q) const noexcept
      -> Shader * {
    using Render::ShaderQuality;
    if (q == ShaderQuality::None) {
      return nullptr;
    }
    // Try exact match first, then strictly stronger tiers, then strictly
    // weaker tiers. Stronger-first keeps visual fidelity as a tie-breaker
    // when the requested tier is missing; weaker-as-last-resort ensures we
    // never return nullptr if any shader exists.
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

  // True iff the material has no shader for any tier. Backends can use this
  // as a fast check for the flat-colour / software path.
  [[nodiscard]] auto is_flat_only() const noexcept -> bool {
    return shader_full == nullptr && shader_reduced == nullptr &&
           shader_minimal == nullptr;
  }
};

// Process-wide registry of built-in Materials. Populated once at backend
// initialisation after shaders are loaded. The registry owns the Material
// instances so every call site can take a stable non-owning pointer.
//
// Three presets today:
//   - basic     → falls back to the backend's "basic" shader, used for any
//                 legacy call that did not supply its own shader.
//   - character → shader picked per-quality; the default material used by
//                 humanoid/horse/elephant replays when no custom shader is
//                 bound. Today all three tiers share the basic shader; as
//                 higher-fidelity variants are authored they slot in
//                 without touching call sites.
//   - shadow    → quad-decal shadow shader, used by humanoid ground shadow.
//
// Thread-safety: init() must be called on the render thread before any
// concurrent access. Subsequent reads are lock-free.
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

    // Character material currently reuses `basic` across all tiers. The
    // tiered slots are live so GraphicsSettings::shader_quality changes
    // still propagate through resolve(); swapping in a reduced/minimal
    // shader later is a one-line edit here.
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
  [[nodiscard]] auto character() noexcept -> Material * {
    return &m_character;
  }
  [[nodiscard]] auto shadow() noexcept -> Material * { return &m_shadow; }

private:
  Material m_basic{};
  Material m_character{};
  Material m_shadow{};
  bool m_initialised = false;
};

} // namespace Render::GL

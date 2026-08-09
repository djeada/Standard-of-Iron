#pragma once

namespace Render::GL::ContextRequirements {

struct Version {
  int major;
  int minor;
};

// GLSL 330 and QOpenGLFunctions_3_3_Core remain the portable renderer floor.
inline constexpr Version required{3, 3};

// Prefer a context that also exposes compute/SSBO rendering (4.3), persistent
// buffer storage (4.4), and the complete modern desktop GL 4.5 feature tier.
inline constexpr Version preferred{4, 5};

// Apple deprecated OpenGL and never exposed a context newer than 4.1.
inline constexpr Version apple_maximum{4, 1};

[[nodiscard]] constexpr auto
at_least(int major, int minor, Version required_version) -> bool {
  return major > required_version.major ||
         (major == required_version.major && minor >= required_version.minor);
}

} // namespace Render::GL::ContextRequirements

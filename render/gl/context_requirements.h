#pragma once

namespace Render::GL::ContextRequirements {

struct Version {
  int major;
  int minor;
};

inline constexpr Version required{3, 3};

inline constexpr Version preferred{4, 5};

inline constexpr Version apple_maximum{4, 1};

[[nodiscard]] constexpr auto
at_least(int major, int minor, Version required_version) -> bool {
  return major > required_version.major ||
         (major == required_version.major && minor >= required_version.minor);
}

} // namespace Render::GL::ContextRequirements

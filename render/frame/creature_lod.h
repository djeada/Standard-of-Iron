#pragma once

#include <cstdint>

namespace Render::Creature {

enum class CreatureLOD : std::uint8_t {
  Full = 0,
  Minimal = 1,
  Culled = 2,
};

} // namespace Render::Creature

namespace Render::GL {

enum class FacialHairStyle {
  None,
  Stubble,
  ShortBeard,
  FullBeard,
  LongBeard,
  Goatee,
  Mustache,
  MustacheAndBeard
};

} // namespace Render::GL

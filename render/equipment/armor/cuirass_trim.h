#pragma once

#include "render/geom/garment_shell.h"

namespace Render::GL {

inline auto cuirass_shoulder_yoke() -> Mesh* {
  static const auto yoke = make_garment_shell(
      {
          {-0.534F, 0.55F, 0.54F},
          {-0.463F, 0.92F, 0.96F},
          {-0.29F, 1.15F, 1.18F},
          {-0.21F, 1.14F, 1.17F},
          {-0.195F, 1.105F, 1.13F},
      },
      32,
      3);
  return yoke.get();
}

inline auto cuirass_waist_binding() -> Mesh* {
  static const auto belt = make_garment_shell(
      {
          {0.335F, 0.957F, 0.93F},
          {0.35F, 1.02F, 0.986F},
          {0.425F, 1.025F, 0.99F},
          {0.44F, 0.95F, 0.925F},
      },
      32,
      2);
  return belt.get();
}

} // namespace Render::GL

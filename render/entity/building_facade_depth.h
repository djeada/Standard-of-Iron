#pragma once

#include <QVector3D>

#include "building_archetype_desc.h"

namespace Render::GL {

namespace BuildingDepth {

inline constexpr float k_wall_surface = 0.000F;

inline constexpr float k_recess = -0.030F;

inline constexpr float k_door = -0.014F;

inline constexpr float k_inlay = 0.012F;

inline constexpr float k_panel = 0.028F;

inline constexpr float k_trim = 0.046F;

inline constexpr float k_relief_base = 0.064F;

inline constexpr float k_relief_detail = 0.082F;

inline constexpr float k_ornament = 0.104F;

} // namespace BuildingDepth

enum class FacadeNormal : std::uint8_t {
  PlusX,
  MinusX,
  PlusY,
  MinusY,
  PlusZ,
  MinusZ,
};

struct FacadeBox {
  FacadeNormal normal{FacadeNormal::PlusZ};
  float surface{0.0F};
  float layer{BuildingDepth::k_panel};
  float thickness{0.04F};
  QVector3D center{0.0F, 0.0F, 0.0F};
  QVector3D half_size{0.1F, 0.1F, 0.1F};
};

namespace Detail {

inline auto facade_axis(FacadeNormal normal) -> int {
  switch (normal) {
  case FacadeNormal::PlusX:
  case FacadeNormal::MinusX:
    return 0;
  case FacadeNormal::PlusY:
  case FacadeNormal::MinusY:
    return 1;
  case FacadeNormal::PlusZ:
  case FacadeNormal::MinusZ:
  default:
    return 2;
  }
}

inline auto facade_sign(FacadeNormal normal) -> float {
  switch (normal) {
  case FacadeNormal::PlusX:
  case FacadeNormal::PlusY:
  case FacadeNormal::PlusZ:
    return 1.0F;
  case FacadeNormal::MinusX:
  case FacadeNormal::MinusY:
  case FacadeNormal::MinusZ:
  default:
    return -1.0F;
  }
}

inline auto facade_center(const FacadeBox& box) -> QVector3D {
  const int axis = facade_axis(box.normal);
  const float sign = facade_sign(box.normal);
  const float outer = box.surface + (sign * box.layer);
  QVector3D center = box.center;
  center[axis] = outer - (sign * box.thickness * 0.5F);
  return center;
}

inline auto facade_half_extent(const FacadeBox& box) -> QVector3D {
  const int axis = facade_axis(box.normal);
  QVector3D half = box.half_size;
  half[axis] = box.thickness * 0.5F;
  return half;
}

} // namespace Detail

inline auto facade_outer_face(const FacadeBox& box) -> float {
  return box.surface + (Detail::facade_sign(box.normal) * box.layer);
}

inline void add_facade_box(BuildingArchetypeDesc& desc,
                           const FacadeBox& box,
                           const QVector3D& color,
                           BuildingStateMask states = BuildingStateMask::All) {
  desc.add_box(
      Detail::facade_center(box), Detail::facade_half_extent(box), color, states);
}

inline void add_facade_palette_box(BuildingArchetypeDesc& desc,
                                   const FacadeBox& box,
                                   std::uint8_t palette_slot,
                                   BuildingStateMask states = BuildingStateMask::All) {
  desc.add_palette_box(Detail::facade_center(box),
                       Detail::facade_half_extent(box),
                       palette_slot,
                       states);
}

} // namespace Render::GL

#pragma once

#include <QVector3D>

#include <cstdint>

#include "render/entity/building_archetype_desc.h"

// The yard every nation's home is authored with, and the variant count the
// home archetypes are baked for. Nothing here draws.
namespace Render::GL {

inline constexpr int k_home_visual_variant_count = 6;
auto home_visual_variant(std::uint32_t seed) -> int;

struct HomeYardStyle {
  float plinth_half{1.18F};
  float wall_half{0.96F};
  float door_half_width{0.60F};
  QVector3D vent{0.0F, 1.66F, -0.42F};
  float chimney_base_y{1.30F};
  bool clay_oven_chimney{false};
  QVector3D stone{0.62F, 0.58F, 0.52F};
  QVector3D clay{0.66F, 0.40F, 0.24F};
  QVector3D clay_dark{0.46F, 0.25F, 0.15F};
  QVector3D chimney{0.58F, 0.54F, 0.49F};
  int seed{1};

  int variant{0};
  float wall_foot_y{0.16F};
  float wall_face{0.96F};
  float wall_foot_door_gap{0.30F};
  QVector3D wall_foot{0.45F, 0.40F, 0.33F};
};

void add_home_yard(BuildingArchetypeDesc& desc, const HomeYardStyle& style);

} // namespace Render::GL

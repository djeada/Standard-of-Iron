#pragma once

#include <QVector3D>

#include <string_view>

#include "render/entity/building_archetype_desc.h"

// Palisade and wall geometry, shared by every nation: the six junction
// variants, the wall walk, the ladder. Authored as BuildingArchetypeDescs and
// baked by the building archetype catalog; nothing here draws.
namespace Render::GL {

enum class WallVariant {
  Isolated,
  End,
  Straight,
  Corner,
  Tee,
  Cross,
};

struct WallPalette {
  QVector3D wood_light{0.67F, 0.50F, 0.31F};
  QVector3D wood_mid{0.54F, 0.39F, 0.23F};
  QVector3D wood_dark{0.36F, 0.25F, 0.14F};
  QVector3D rope{0.46F, 0.38F, 0.24F};
  QVector3D masonry_accent{0.67F, 0.50F, 0.31F};
  QVector3D earth_light{0.34F, 0.25F, 0.15F};
  QVector3D earth_dark{0.22F, 0.16F, 0.10F};
  QVector3D rubble{0.44F, 0.42F, 0.40F};
  QVector3D iron{0.25F, 0.24F, 0.23F};
  bool alternate_starts_light{true};
  bool horned_masonry{false};
};

struct WallGeometry {
  bool solid_masonry{false};
  bool earthwork_base{false};
  bool cross_braced{false};
  bool metal_bands{false};
  bool irregular_stakes{false};
  float open_span_length{1.0F};
  float stake_height{2.36F};
  float stake_radius{0.10F};
  float tip_height{0.36F};
  float post_radius{0.20F};
  float post_extra_height{0.12F};
  float lower_rail_y{0.74F};
  float upper_rail_y{1.52F};
  float rail_radius{0.058F};
  float berm_half_width{0.30F};
  float berm_height{0.18F};
  float masonry_half_width{0.28F};
  float masonry_height{1.34F};
};

[[nodiscard]] auto wall_binding_color(const WallPalette& palette,
                                      const WallGeometry& geometry) -> QVector3D;
[[nodiscard]] auto wall_binding_material(const WallGeometry& geometry) -> int;

auto build_wall_walk_span_desc(std::string_view name_prefix,
                               const WallPalette& palette) -> BuildingArchetypeDesc;
auto build_wall_walk_landing_desc(std::string_view name_prefix,
                                  const WallPalette& palette) -> BuildingArchetypeDesc;
auto build_wall_walk_stair_desc(std::string_view name_prefix,
                                const WallPalette& palette) -> BuildingArchetypeDesc;
auto build_wall_ladder_desc(std::string_view name_prefix,
                            const WallPalette& palette) -> BuildingArchetypeDesc;
auto build_wall_variant_desc(std::string_view name_prefix,
                             const WallPalette& palette,
                             const WallGeometry& geometry,
                             WallVariant variant) -> BuildingArchetypeDesc;

} // namespace Render::GL

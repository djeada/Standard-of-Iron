#pragma once

#include <QVector3D>

#include <array>
#include <string_view>
#include <utility>

#include "../render_archetype.h"
#include "../submitter.h"
#include "building_archetype_desc.h"
#include "building_render_common.h"

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

auto build_wall_archetype_set(std::string_view name_prefix,
                              const WallPalette& palette,
                              const WallGeometry& geometry)
    -> std::array<RenderArchetype, 6>;
auto wall_renderer_variants()
    -> const std::array<std::pair<std::string_view, WallVariant>, 7>&;
void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const std::array<RenderArchetype, 6>& archetypes,
                                 WallVariant variant);

} // namespace Render::GL

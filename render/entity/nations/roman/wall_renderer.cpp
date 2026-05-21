#include "wall_renderer.h"

#include <QVector3D>

#include <array>
#include <string>
#include <string_view>

#include "../../../../game/core/component.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../registry.h"

namespace Render::GL::Roman {
namespace {

constexpr std::uint8_t k_connection_north = 1U << 0U;
constexpr std::uint8_t k_connection_east = 1U << 1U;
constexpr std::uint8_t k_connection_south = 1U << 2U;
constexpr std::uint8_t k_connection_west = 1U << 3U;

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
  QVector3D shadow{0.24F, 0.17F, 0.09F};
};

constexpr float k_connected_span_length = 1.04F;
constexpr float k_open_span_length = 0.96F;
constexpr float k_stake_center_y = 1.18F;
constexpr float k_stake_half_height = 1.18F;
constexpr float k_tip_center_y = 2.48F;
constexpr float k_tip_half_height = 0.18F;
constexpr float k_lower_lashing_y = 0.74F;
constexpr float k_upper_lashing_y = 1.52F;

void add_x_span(BuildingArchetypeDesc& desc,
                const WallPalette& c,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const float center_x = direction * (length * 0.5F);

  desc.add_box(QVector3D(center_x, k_lower_lashing_y, 0.0F),
               QVector3D(length + 0.06F, 0.09F, 0.20F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, k_upper_lashing_y, 0.0F),
               QVector3D(length + 0.06F, 0.09F, 0.20F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, 0.30F, 0.0F),
               QVector3D(length + 0.08F, 0.07F, 0.24F),
               c.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float x = direction * (t * length);
    desc.add_box(QVector3D(x, k_stake_center_y, 0.0F),
                 QVector3D(0.15F, k_stake_half_height * 2.0F, 0.19F),
                 (i % 2 == 0) ? c.wood_light : c.wood_mid,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, k_tip_center_y, 0.0F),
                 QVector3D(0.08F, k_tip_half_height * 2.0F, 0.11F),
                 c.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  if (capped_outer_end) {
    desc.add_box(QVector3D(direction * (length - 0.02F), k_stake_center_y, 0.0F),
                 QVector3D(0.20F, k_stake_half_height * 2.0F, 0.24F),
                 c.shadow,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
}

void add_z_span(BuildingArchetypeDesc& desc,
                const WallPalette& c,
                float direction,
                float length,
                bool capped_outer_end) {
  constexpr int k_stakes = 5;
  const float center_z = direction * (length * 0.5F);

  desc.add_box(QVector3D(0.0F, k_lower_lashing_y, center_z),
               QVector3D(0.20F, 0.09F, length + 0.06F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, k_upper_lashing_y, center_z),
               QVector3D(0.20F, 0.09F, length + 0.06F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.30F, center_z),
               QVector3D(0.24F, 0.07F, length + 0.08F),
               c.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float z = direction * (t * length);
    desc.add_box(QVector3D(0.0F, k_stake_center_y, z),
                 QVector3D(0.19F, k_stake_half_height * 2.0F, 0.15F),
                 (i % 2 == 0) ? c.wood_light : c.wood_mid,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, k_tip_center_y, z),
                 QVector3D(0.11F, k_tip_half_height * 2.0F, 0.08F),
                 c.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  if (capped_outer_end) {
    desc.add_box(QVector3D(0.0F, k_stake_center_y, direction * (length - 0.02F)),
                 QVector3D(0.24F, k_stake_half_height * 2.0F, 0.20F),
                 c.shadow,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
}

void add_center_post(BuildingArchetypeDesc& desc, const WallPalette& c) {
  desc.add_box(QVector3D(0.0F, 0.18F, 0.0F),
               QVector3D(0.52F, 0.06F, 0.52F),
               c.wood_dark,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, k_stake_center_y, 0.0F),
               QVector3D(0.26F, k_stake_half_height * 2.0F, 0.26F),
               c.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, k_tip_center_y + 0.02F, 0.0F),
               QVector3D(0.11F, (k_tip_half_height + 0.02F) * 2.0F, 0.11F),
               c.wood_dark,
               BuildingStateMask::All,
               BuildingLODMask::Full);
}

auto variant_mask(WallVariant variant) -> std::uint8_t {
  switch (variant) {
  case WallVariant::End:
    return k_connection_east;
  case WallVariant::Straight:
    return k_connection_east | k_connection_west;
  case WallVariant::Corner:
    return k_connection_east | k_connection_north;
  case WallVariant::Tee:
    return k_connection_east | k_connection_north | k_connection_south;
  case WallVariant::Cross:
    return k_connection_north | k_connection_east | k_connection_south |
           k_connection_west;
  case WallVariant::Isolated:
  default:
    return 0;
  }
}

auto wall_archetype(WallVariant variant) -> const RenderArchetype& {
  static const std::array<RenderArchetype, 6> k_archetypes = [] {
    std::array<RenderArchetype, 6> out{};
    for (int i = 0; i < static_cast<int>(out.size()); ++i) {
      const auto v = static_cast<WallVariant>(i);
      const auto mask = variant_mask(v);
      const WallPalette c{};
      BuildingArchetypeDesc desc("roman_wall_variant_" + std::to_string(i));

      add_center_post(desc, c);

      if (v == WallVariant::Isolated) {
        add_x_span(desc, c, -1.0F, k_open_span_length, true);
        add_x_span(desc, c, 1.0F, k_open_span_length, true);
      } else if (v == WallVariant::End) {
        add_x_span(desc, c, 1.0F, k_connected_span_length, false);
        add_x_span(desc, c, -1.0F, k_open_span_length, true);
      } else {
        if ((mask & k_connection_east) != 0U) {
          add_x_span(desc, c, 1.0F, k_connected_span_length, false);
        }
        if ((mask & k_connection_west) != 0U) {
          add_x_span(desc, c, -1.0F, k_connected_span_length, false);
        }
        if ((mask & k_connection_north) != 0U) {
          add_z_span(desc, c, -1.0F, k_connected_span_length, false);
        }
        if ((mask & k_connection_south) != 0U) {
          add_z_span(desc, c, 1.0F, k_connected_span_length, false);
        }
      }

      out[static_cast<std::size_t>(i)] =
          build_building_archetype(desc, BuildingState::Normal);
    }
    return out;
  }();

  return k_archetypes[static_cast<std::size_t>(variant)];
}

void draw_wall_segment_variant(const DrawContext& p,
                               ISubmitter& out,
                               WallVariant variant) {
  submit_building_instance(out, p, wall_archetype(variant));
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  const std::array<std::pair<std::string_view, WallVariant>, 7> variants = {{
      {"wall_segment", WallVariant::Straight},
      {"wall_segment_isolated", WallVariant::Isolated},
      {"wall_segment_end", WallVariant::End},
      {"wall_segment_straight", WallVariant::Straight},
      {"wall_segment_corner", WallVariant::Corner},
      {"wall_segment_tee", WallVariant::Tee},
      {"wall_segment_cross", WallVariant::Cross},
  }};

  for (const auto& [name, variant] : variants) {
    register_building_renderer(
        registry, "roman", name, [variant](const DrawContext& p, ISubmitter& out) {
          draw_wall_segment_variant(p, out, variant);
        });
  }
}

} // namespace Render::GL::Roman

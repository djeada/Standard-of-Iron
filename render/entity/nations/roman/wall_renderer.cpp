#include "wall_renderer.h"

#include <QVector3D>

#include <array>
#include <string>
#include <string_view>

#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../registry.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_wall_team_slot = 0;
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
  QVector3D wood_light{0.72F, 0.56F, 0.38F};
  QVector3D wood_mid{0.60F, 0.46F, 0.30F};
  QVector3D wood_dark{0.45F, 0.33F, 0.20F};
  QVector3D iron_band{0.35F, 0.33F, 0.32F};
  QVector3D plank_highlight{0.80F, 0.64F, 0.44F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

auto make_palette(const QVector3D& team) -> WallPalette {
  WallPalette p;
  p.team = clamp_vec_01(team);
  return p;
}

void add_x_arm(BuildingArchetypeDesc& desc,
               const WallPalette& c,
               float direction,
               float length) {
  const float center_x = direction * (length * 0.5F);
  desc.add_box(QVector3D(center_x, 0.28F, 0.0F),
               QVector3D(length * 0.5F, 0.12F, 0.18F),
               c.wood_light);
  desc.add_box(QVector3D(center_x, 0.56F, 0.0F),
               QVector3D(length * 0.5F, 0.14F, 0.18F),
               c.wood_mid);
  desc.add_box(QVector3D(center_x, 0.84F, 0.0F),
               QVector3D(length * 0.5F, 0.12F, 0.18F),
               c.wood_light);
  desc.add_box(QVector3D(center_x, 0.42F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.018F, 0.20F),
               c.iron_band,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, 0.76F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.018F, 0.20F),
               c.iron_band,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  const float end_x = direction * length;
  desc.add_box(QVector3D(end_x, 0.60F, 0.0F),
               QVector3D(0.08F, 0.52F, 0.20F),
               c.wood_dark,
               BuildingStateMask::All,
               BuildingLODMask::Full);
}

void add_z_arm(BuildingArchetypeDesc& desc,
               const WallPalette& c,
               float direction,
               float length) {
  const float center_z = direction * (length * 0.5F);
  desc.add_box(QVector3D(0.0F, 0.28F, center_z),
               QVector3D(0.18F, 0.12F, length * 0.5F),
               c.wood_light);
  desc.add_box(QVector3D(0.0F, 0.56F, center_z),
               QVector3D(0.18F, 0.14F, length * 0.5F),
               c.wood_mid);
  desc.add_box(QVector3D(0.0F, 0.84F, center_z),
               QVector3D(0.18F, 0.12F, length * 0.5F),
               c.wood_light);
  desc.add_box(QVector3D(0.0F, 0.42F, center_z),
               QVector3D(0.20F, 0.018F, length * 0.5F + 0.02F),
               c.iron_band,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.76F, center_z),
               QVector3D(0.20F, 0.018F, length * 0.5F + 0.02F),
               c.iron_band,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  const float end_z = direction * length;
  desc.add_box(QVector3D(0.0F, 0.60F, end_z),
               QVector3D(0.20F, 0.52F, 0.08F),
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
      const WallPalette c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
      BuildingArchetypeDesc desc("roman_wall_variant_" + std::to_string(i));

      desc.add_box(QVector3D(0.0F, 0.58F, 0.0F),
                   QVector3D(0.16F, 0.54F, 0.16F),
                   c.wood_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(0.0F, 1.02F, 0.0F),
                   QVector3D(0.22F, 0.12F, 0.22F),
                   c.plank_highlight,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);

      if (v == WallVariant::Isolated) {
        add_x_arm(desc, c, -1.0F, 0.52F);
        add_x_arm(desc, c, 1.0F, 0.52F);
      } else {
        if ((mask & k_connection_east) != 0U) {
          add_x_arm(desc, c, 1.0F, 0.92F);
        }
        if ((mask & k_connection_west) != 0U) {
          add_x_arm(desc, c, -1.0F, 0.92F);
        }
        if ((mask & k_connection_north) != 0U) {
          add_z_arm(desc, c, -1.0F, 0.92F);
        }
        if ((mask & k_connection_south) != 0U) {
          add_z_arm(desc, c, 1.0F, 0.92F);
        }
      }

      desc.add_box(QVector3D(0.0F, 1.08F, 0.0F),
                   QVector3D(0.025F, 0.16F, 0.025F),
                   c.wood_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
      desc.add_palette_box(QVector3D(0.08F, 1.15F, 0.0F),
                           QVector3D(0.12F, 0.08F, 0.012F),
                           k_wall_team_slot,
                           BuildingStateMask::All,
                           BuildingLODMask::Full);
      out[static_cast<std::size_t>(i)] =
          build_building_archetype(desc, BuildingState::Normal);
    }
    return out;
  }();

  return k_archetypes[static_cast<std::size_t>(variant)];
}

auto wall_palette_slots(const WallPalette& palette) -> std::array<QVector3D, 1> {
  return {palette.team};
}

void draw_wall_segment_variant(const DrawContext& p,
                               ISubmitter& out,
                               WallVariant variant) {
  if (p.entity == nullptr) {
    return;
  }

  auto* r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  const WallPalette palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = wall_palette_slots(palette);
  submit_building_instance(out, p, wall_archetype(variant), palette_slots);
  draw_building_compact_health_bar(out, p, 1.35F);
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.4F, 2.4F});
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

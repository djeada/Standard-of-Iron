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

namespace Render::GL::Carthage {
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
  QVector3D wood_light{0.68F, 0.50F, 0.30F};
  QVector3D wood_mid{0.55F, 0.40F, 0.22F};
  QVector3D wood_dark{0.40F, 0.28F, 0.14F};
  QVector3D rope{0.58F, 0.48F, 0.30F};
  QVector3D plank_highlight{0.76F, 0.58F, 0.36F};
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
  desc.add_box(QVector3D(center_x, 0.26F, 0.0F),
               QVector3D(length * 0.5F, 0.11F, 0.18F),
               c.wood_light);
  desc.add_box(QVector3D(center_x, 0.52F, 0.0F),
               QVector3D(length * 0.5F, 0.13F, 0.18F),
               c.wood_mid);
  desc.add_box(QVector3D(center_x, 0.78F, 0.0F),
               QVector3D(length * 0.5F, 0.11F, 0.18F),
               c.wood_light);
  desc.add_box(QVector3D(center_x, 0.40F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.026F, 0.19F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, 0.68F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.026F, 0.19F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  const float end_x = direction * length;
  desc.add_box(QVector3D(end_x, 0.58F, 0.0F),
               QVector3D(0.09F, 0.50F, 0.20F),
               c.wood_dark,
               BuildingStateMask::All,
               BuildingLODMask::Full);
}

void add_z_arm(BuildingArchetypeDesc& desc,
               const WallPalette& c,
               float direction,
               float length) {
  const float center_z = direction * (length * 0.5F);
  desc.add_box(QVector3D(0.0F, 0.26F, center_z),
               QVector3D(0.18F, 0.11F, length * 0.5F),
               c.wood_light);
  desc.add_box(QVector3D(0.0F, 0.52F, center_z),
               QVector3D(0.18F, 0.13F, length * 0.5F),
               c.wood_mid);
  desc.add_box(QVector3D(0.0F, 0.78F, center_z),
               QVector3D(0.18F, 0.11F, length * 0.5F),
               c.wood_light);
  desc.add_box(QVector3D(0.0F, 0.40F, center_z),
               QVector3D(0.19F, 0.026F, length * 0.5F + 0.02F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.68F, center_z),
               QVector3D(0.19F, 0.026F, length * 0.5F + 0.02F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  const float end_z = direction * length;
  desc.add_box(QVector3D(0.0F, 0.58F, end_z),
               QVector3D(0.20F, 0.50F, 0.09F),
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
      BuildingArchetypeDesc desc("carthage_wall_variant_" + std::to_string(i));

      desc.add_box(QVector3D(0.0F, 0.56F, 0.0F),
                   QVector3D(0.18F, 0.52F, 0.18F),
                   c.wood_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(0.0F, 1.00F, 0.0F),
                   QVector3D(0.24F, 0.12F, 0.24F),
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
                           QVector3D(0.11F, 0.07F, 0.012F),
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
        registry, "carthage", name, [variant](const DrawContext& p, ISubmitter& out) {
          draw_wall_segment_variant(p, out, variant);
        });
  }
}

} // namespace Render::GL::Carthage

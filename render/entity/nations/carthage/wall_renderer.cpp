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

namespace Render::GL::Carthage {
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
  QVector3D wood_light{0.62F, 0.45F, 0.27F};
  QVector3D wood_mid{0.49F, 0.34F, 0.19F};
  QVector3D wood_dark{0.31F, 0.21F, 0.11F};
  QVector3D rope{0.52F, 0.40F, 0.22F};
  QVector3D shadow{0.22F, 0.15F, 0.08F};
};

void add_x_span(BuildingArchetypeDesc& desc,
                const WallPalette& c,
                float direction,
                float length) {
  constexpr int k_stakes = 6;
  const float center_x = direction * (length * 0.5F);

  desc.add_box(QVector3D(center_x, 0.32F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.05F, 0.10F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(center_x, 0.98F, 0.0F),
               QVector3D(length * 0.5F + 0.02F, 0.05F, 0.10F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float x = direction * (t * length);
    desc.add_box(QVector3D(x, 0.76F, 0.0F),
                 QVector3D(0.06F, 0.76F, 0.08F),
                 (i % 2 == 0) ? c.wood_mid : c.wood_light,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, 1.52F, 0.0F),
                 QVector3D(0.04F, 0.12F, 0.06F),
                 c.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  desc.add_box(QVector3D(direction * length, 0.82F, 0.0F),
               QVector3D(0.09F, 0.82F, 0.11F),
               c.shadow,
               BuildingStateMask::All,
               BuildingLODMask::Full);
}

void add_z_span(BuildingArchetypeDesc& desc,
                const WallPalette& c,
                float direction,
                float length) {
  constexpr int k_stakes = 6;
  const float center_z = direction * (length * 0.5F);

  desc.add_box(QVector3D(0.0F, 0.32F, center_z),
               QVector3D(0.10F, 0.05F, length * 0.5F + 0.02F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.98F, center_z),
               QVector3D(0.10F, 0.05F, length * 0.5F + 0.02F),
               c.rope,
               BuildingStateMask::All,
               BuildingLODMask::Full);

  for (int i = 0; i < k_stakes; ++i) {
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_stakes);
    const float z = direction * (t * length);
    desc.add_box(QVector3D(0.0F, 0.76F, z),
                 QVector3D(0.08F, 0.76F, 0.06F),
                 (i % 2 == 0) ? c.wood_mid : c.wood_light,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, 1.52F, z),
                 QVector3D(0.06F, 0.12F, 0.04F),
                 c.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  desc.add_box(QVector3D(0.0F, 0.82F, direction * length),
               QVector3D(0.11F, 0.82F, 0.09F),
               c.shadow,
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
      BuildingArchetypeDesc desc("carthage_wall_variant_" + std::to_string(i));

      desc.add_box(QVector3D(0.0F, 0.80F, 0.0F),
                   QVector3D(0.11F, 0.80F, 0.11F),
                   c.shadow,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(0.0F, 0.17F, 0.0F),
                   QVector3D(0.22F, 0.03F, 0.22F),
                   c.wood_dark,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);

      if (v == WallVariant::Isolated) {
        add_x_span(desc, c, -1.0F, 0.56F);
        add_x_span(desc, c, 1.0F, 0.56F);
      } else {
        if ((mask & k_connection_east) != 0U) {
          add_x_span(desc, c, 1.0F, 0.94F);
        }
        if ((mask & k_connection_west) != 0U) {
          add_x_span(desc, c, -1.0F, 0.94F);
        }
        if ((mask & k_connection_north) != 0U) {
          add_z_span(desc, c, -1.0F, 0.94F);
        }
        if ((mask & k_connection_south) != 0U) {
          add_z_span(desc, c, 1.0F, 0.94F);
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
        registry, "carthage", name, [variant](const DrawContext& p, ISubmitter& out) {
          draw_wall_segment_variant(p, out, variant);
        });
  }
}

} // namespace Render::GL::Carthage

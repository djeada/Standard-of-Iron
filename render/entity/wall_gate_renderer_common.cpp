#include "wall_gate_renderer_common.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <string>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../gl/resources.h"
#include "building_archetype_desc.h"
#include "building_render_common.h"

namespace Render::GL {
namespace {

constexpr float k_jamb_offset = 0.86F;
constexpr float k_jamb_depth = 0.24F;
constexpr float k_leaf_thickness = 0.065F;
constexpr float k_leaf_swing_degrees = 100.0F;
constexpr float k_leaf_height_ratio = 0.78F;
constexpr float k_detail_distance_sq = 900.0F;

auto leaf_height(const WallGeometry& geometry) -> float {
  return geometry.stake_height * k_leaf_height_ratio;
}

auto lintel_height(const WallGeometry& geometry) -> float {
  return leaf_height(geometry) + 0.24F;
}

void add_jamb(BuildingArchetypeDesc& desc,
              const WallPalette& palette,
              const WallGeometry& geometry,
              float x,
              float z) {
  const float radius = geometry.post_radius * 1.05F;
  const float top = geometry.stake_height + geometry.post_extra_height + 0.18F;

  desc.add_cylinder(
      QVector3D(x, 0.02F, z), QVector3D(x, top, z), radius, palette.wood_mid);
  desc.add_cone(QVector3D(x, top - 0.01F, z),
                QVector3D(x, top + geometry.tip_height, z),
                radius * 1.02F,
                palette.wood_dark);

  for (const float band : {geometry.lower_rail_y, geometry.upper_rail_y}) {
    desc.add_cylinder(QVector3D(x, band - 0.05F, z),
                      QVector3D(x, band + 0.05F, z),
                      radius * 1.12F,
                      geometry.metal_bands ? palette.masonry_accent : palette.rope,
                      BuildingStateMask::All,
                      BuildingLODMask::Full);
  }
}

} // namespace

auto build_wall_gate_archetype(std::string_view name_prefix,
                               const WallPalette& palette,
                               const WallGeometry& geometry) -> RenderArchetype {
  BuildingArchetypeDesc desc(std::string(name_prefix) + "_gate");

  if (geometry.earthwork_base) {
    constexpr float k_sink = 0.03F;
    const float half_height = (geometry.berm_height + k_sink) * 0.5F;
    const float center_y = (geometry.berm_height - k_sink) * 0.5F;
    for (const float side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(side * k_jamb_offset, center_y, 0.0F),
                   QVector3D(0.30F, half_height, 0.34F),
                   palette.earth_light);
    }
  }

  for (const float side : {-1.0F, 1.0F}) {
    for (const float depth : {-k_jamb_depth, k_jamb_depth}) {
      add_jamb(desc, palette, geometry, side * k_jamb_offset, depth);
    }
  }

  const float lintel_y = lintel_height(geometry);
  for (const float depth : {-k_jamb_depth, k_jamb_depth}) {
    desc.add_box(QVector3D(0.0F, lintel_y, depth),
                 QVector3D(k_jamb_offset + 0.14F, 0.085F, 0.07F),
                 palette.wood_dark);
  }

  desc.add_box(QVector3D(0.0F, lintel_y + 0.24F, 0.0F),
               QVector3D(k_jamb_offset + 0.20F, 0.075F, k_jamb_depth + 0.12F),
               palette.wood_light);

  for (int i = -1; i <= 1; ++i) {
    desc.add_box(QVector3D(static_cast<float>(i) * 0.52F, lintel_y + 0.38F, 0.0F),
                 QVector3D(0.20F, 0.055F, k_jamb_depth + 0.06F),
                 (i % 2 == 0) ? palette.wood_mid : palette.wood_light,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  return build_building_archetype(desc, BuildingState::Normal);
}

void submit_wall_gate(ISubmitter& out,
                      const DrawContext& ctx,
                      const RenderArchetype& frame,
                      const WallPalette& palette,
                      const WallGeometry& geometry) {
  submit_building_instance(out, ctx, frame);

  Mesh* mesh = (ctx.resources != nullptr) ? ctx.resources->unit() : nullptr;
  Texture* white = (ctx.resources != nullptr) ? ctx.resources->white() : nullptr;
  if (mesh != nullptr) {
    const auto* gate = (ctx.entity != nullptr)
                           ? ctx.entity->get_component<Engine::Core::GateComponent>()
                           : nullptr;
    const float open_amount =
        (gate != nullptr) ? std::clamp(gate->open_amount, 0.0F, 1.0F) : 0.0F;
    const float swing_degrees = open_amount * k_leaf_swing_degrees;

    const float height = leaf_height(geometry);
    const float length = k_jamb_offset;
    const bool detailed = ctx.distance_sq <= k_detail_distance_sq;

    for (const float side : {-1.0F, 1.0F}) {
      QMatrix4x4 hinge = ctx.model;
      hinge.translate(QVector3D(side * k_jamb_offset, 0.0F, 0.0F));
      hinge.rotate(-side * swing_degrees, 0.0F, 1.0F, 0.0F);

      const float center_x = -side * length * 0.5F;

      submit_building_box(out,
                          mesh,
                          white,
                          hinge,
                          QVector3D(center_x, (height * 0.5F) + 0.03F, 0.0F),
                          QVector3D(length * 0.5F, height * 0.5F, k_leaf_thickness),
                          palette.wood_mid);

      if (!detailed) {
        continue;
      }

      for (const float band_ratio : {0.28F, 0.72F}) {
        submit_building_box(out,
                            mesh,
                            white,
                            hinge,
                            QVector3D(center_x, (height * band_ratio) + 0.03F, 0.0F),
                            QVector3D(length * 0.5F, 0.055F, k_leaf_thickness + 0.022F),
                            geometry.metal_bands ? palette.masonry_accent
                                                 : palette.wood_dark);
      }

      submit_building_box(
          out,
          mesh,
          white,
          hinge,
          QVector3D(-side * (length - 0.10F), (height * 0.5F) + 0.03F, 0.0F),
          QVector3D(0.055F, height * 0.42F, k_leaf_thickness + 0.03F),
          palette.wood_dark);
    }
  }

  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL

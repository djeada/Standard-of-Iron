#include "wall_gate_renderer_common.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <string>

#include "building_archetype_desc.h"
#include "building_decay.h"
#include "building_render_common.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/gl/resources.h"

namespace Render::GL {
namespace {

constexpr float k_jamb_offset = Engine::Core::GateComponent::k_passage_half_width;
constexpr float k_structure_half_span =
    Engine::Core::GateComponent::k_structure_half_span;
constexpr float k_jamb_depth = 0.30F;
constexpr float k_leaf_thickness = 0.075F;
constexpr float k_leaf_swing_degrees = 100.0F;
constexpr float k_leaf_height_ratio = 0.82F;
constexpr float k_detail_distance_sq = 900.0F;

constexpr auto k_mask_intact = k_building_state_mask_intact;

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

  const float lean =
      (decay_hash(static_cast<int>((x * 31.0F) + (z * 7.0F))) - 0.5F) * 0.16F;

  desc.add_cylinder(QVector3D(x, 0.02F, z),
                    QVector3D(x, top, z),
                    radius,
                    palette.wood_mid,
                    BuildingStateMask::Normal);
  desc.add_cylinder(QVector3D(x, 0.02F, z),
                    QVector3D(x + lean, top * 0.94F, z + (lean * 0.5F)),
                    radius,
                    palette.wood_mid,
                    BuildingStateMask::Damaged);
  desc.add_cylinder(QVector3D(x, 0.02F, z),
                    QVector3D(x + (lean * 2.6F), top * 0.42F, z + (lean * 1.4F)),
                    radius * 0.96F,
                    palette.wood_dark,
                    BuildingStateMask::Destroyed);
  desc.add_cone(QVector3D(x, top - 0.01F, z),
                QVector3D(x, top + geometry.tip_height, z),
                radius * 1.02F,
                palette.wood_dark,
                BuildingStateMask::Normal);

  for (const float band : {geometry.lower_rail_y, geometry.upper_rail_y}) {
    desc.add_cylinder(QVector3D(x, band - 0.05F, z),
                      QVector3D(x, band + 0.05F, z),
                      radius * 1.12F,
                      geometry.metal_bands ? palette.masonry_accent : palette.rope,
                      band > geometry.lower_rail_y ? BuildingStateMask::Normal
                                                   : k_mask_intact,
                      BuildingLODMask::Full);
  }
}

void add_piers(BuildingArchetypeDesc& desc,
               const WallPalette& palette,
               const WallGeometry& geometry) {
  constexpr int k_stakes_per_pier = 5;
  const float pier_inner = k_jamb_offset + 0.16F;
  const float pier_outer = k_structure_half_span;
  const float span = pier_outer - pier_inner;
  const float spacing = span / static_cast<float>(k_stakes_per_pier);
  const float stake_radius = geometry.post_radius * 0.82F;

  for (const float side : {-1.0F, 1.0F}) {
    for (int i = 0; i < k_stakes_per_pier; ++i) {
      const float x = side * (pier_inner + (spacing * (static_cast<float>(i) + 0.5F)));
      const float height = geometry.stake_height * (i % 2 == 0 ? 1.0F : 0.94F);
      const float roll = decay_hash((i * 13) + static_cast<int>(side * 5.0F) + 2);
      const bool snaps = roll < 0.45F;
      const BuildingStateMask upright =
          snaps ? BuildingStateMask::Normal : k_mask_intact;

      desc.add_cylinder(QVector3D(x, 0.0F, 0.0F),
                        QVector3D(x, height, 0.0F),
                        stake_radius,
                        (i % 2 == 0) ? palette.wood_mid : palette.wood_light,
                        upright);
      desc.add_cone(QVector3D(x, height - 0.01F, 0.0F),
                    QVector3D(x, height + geometry.tip_height, 0.0F),
                    stake_radius * 1.02F,
                    palette.wood_dark,
                    upright,
                    BuildingLODMask::Full);
      if (snaps) {
        desc.add_cylinder(QVector3D(x, 0.0F, 0.0F),
                          QVector3D(x, height * (0.35F + (roll * 0.6F)), 0.0F),
                          stake_radius,
                          palette.wood_dark,
                          BuildingStateMask::Damaged);
      }
      if (roll > 0.72F) {
        desc.add_cylinder(QVector3D(x, 0.0F, 0.0F),
                          QVector3D(x, height * (0.2F + (roll * 0.25F)), 0.0F),
                          stake_radius * 0.95F,
                          palette.wood_dark,
                          BuildingStateMask::Destroyed);
      }
    }

    const float mid = side * (pier_inner + (span * 0.5F));
    for (const float rail : {geometry.lower_rail_y, geometry.upper_rail_y}) {
      desc.add_box(QVector3D(mid, rail, 0.0F),
                   QVector3D(span * 0.5F, 0.05F, stake_radius * 1.25F),
                   geometry.metal_bands ? palette.masonry_accent : palette.rope,
                   rail > geometry.lower_rail_y ? BuildingStateMask::Normal
                                                : k_mask_intact,
                   BuildingLODMask::Full);
    }

    if (geometry.earthwork_base) {
      desc.add_box(QVector3D(mid, geometry.berm_height * 0.5F, 0.0F),
                   QVector3D(span * 0.5F, geometry.berm_height * 0.5F, 0.34F),
                   palette.earth_light);
    }
  }
}

} // namespace

auto build_wall_gate_archetype(std::string_view name_prefix,
                               const WallPalette& palette,
                               const WallGeometry& geometry) -> BuildingArchetypeSet {
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
                 QVector3D(k_jamb_offset + 0.18F, 0.095F, 0.08F),
                 palette.wood_dark,
                 k_mask_intact);
  }

  desc.add_box(QVector3D(0.0F, lintel_y + 0.26F, 0.0F),
               QVector3D(k_jamb_offset + 0.26F, 0.085F, k_jamb_depth + 0.14F),
               palette.wood_light,
               BuildingStateMask::Normal);

  for (int i = -2; i <= 2; ++i) {
    desc.add_box(QVector3D(static_cast<float>(i) * 0.74F, lintel_y + 0.40F, 0.0F),
                 QVector3D(0.22F, 0.06F, k_jamb_depth + 0.07F),
                 (i % 2 == 0) ? palette.wood_mid : palette.wood_light,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  desc.add_rotated_box(QVector3D(0.35F, 0.10F, k_jamb_depth * 1.6F),
                       QVector3D(k_jamb_offset * 0.9F, 0.075F, 0.16F),
                       QVector3D(0.0F, 14.0F, 6.0F),
                       palette.wood_dark,
                       BuildingStateMask::Destroyed);

  add_piers(desc, palette, geometry);

  add_rubble_field(
      desc,
      RubbleField{
          .center = QVector3D(0.0F, 0.0F, 0.0F),
          .extent = QVector3D(k_structure_half_span * 0.85F, 0.0F, k_jamb_depth * 1.4F),
          .stone = palette.rubble,
          .stone_dark = palette.earth_dark,
          .count = 6,
          .seed = 71,
          .states = BuildingStateMask::Damaged});
  add_rubble_field(
      desc,
      RubbleField{.center = QVector3D(0.0F, 0.0F, 0.0F),
                  .extent = QVector3D(k_structure_half_span, 0.0F, k_jamb_depth * 1.9F),
                  .stone = palette.rubble,
                  .stone_dark = palette.earth_dark,
                  .chunk_scale = 1.2F,
                  .count = 13,
                  .seed = 311,
                  .states = BuildingStateMask::Destroyed});
  add_charred_beams(desc,
                    CharredBeams{.center = QVector3D(0.0F, 0.0F, 0.0F),
                                 .extent = QVector3D(k_jamb_offset, 0.0F, 0.35F),
                                 .timber = palette.wood_dark * 0.45F,
                                 .length = 0.8F,
                                 .count = 4,
                                 .seed = 209,
                                 .states = BuildingStateMask::Destroyed});
  add_scorch_patch(
      desc,
      ScorchPatch{.center = QVector3D(0.0F, 0.0F, 0.0F),
                  .radius = k_jamb_offset * 1.3F,
                  .count = 5,
                  .seed = 407,
                  .states = static_cast<BuildingStateMask>(
                      static_cast<std::uint8_t>(BuildingStateMask::Damaged) |
                      static_cast<std::uint8_t>(BuildingStateMask::Destroyed))});

  return build_stateful_building_archetype_set(
      [&](BuildingState state) { return build_building_archetype(desc, state); });
}

void submit_wall_gate(ISubmitter& out,
                      const DrawContext& ctx,
                      const BuildingArchetypeSet& frame,
                      const WallPalette& palette,
                      const WallGeometry& geometry) {
  const BuildingState state = resolve_building_state(ctx);
  submit_building_instance(out, ctx, frame.for_state(state));

  Mesh* mesh = (ctx.resources != nullptr) ? ctx.resources->unit() : nullptr;
  Texture* white = (ctx.resources != nullptr) ? ctx.resources->white() : nullptr;
  if (mesh != nullptr) {
    const auto* gate = (ctx.entity != nullptr)
                           ? ctx.entity->get_component<Engine::Core::GateComponent>()
                           : nullptr;
    const float open_amount =
        (gate != nullptr) ? std::clamp(gate->open_amount, 0.0F, 1.0F) : 0.0F;
    const float swing_degrees = open_amount * k_leaf_swing_degrees;

    const float leaf_scale = (state == BuildingState::Damaged)     ? 0.86F
                             : (state == BuildingState::Destroyed) ? 0.55F
                                                                   : 1.0F;
    const float height = leaf_height(geometry) * leaf_scale;
    const float length = k_jamb_offset;
    const bool detailed = ctx.distance_sq <= k_detail_distance_sq;

    for (const float side : {-1.0F, 1.0F}) {
      if (state == BuildingState::Destroyed && side < 0.0F) {
        continue;
      }

      QMatrix4x4 hinge = ctx.model;
      hinge.translate(QVector3D(side * k_jamb_offset, 0.0F, 0.0F));
      hinge.rotate(-side * swing_degrees, 0.0F, 1.0F, 0.0F);
      if (state == BuildingState::Destroyed) {
        hinge.rotate(-14.0F, 0.0F, 0.0F, 1.0F);
      }

      const float center_x = -side * length * 0.5F;

      submit_building_box(out,
                          mesh,
                          white,
                          hinge,
                          QVector3D(center_x, (height * 0.5F) + 0.03F, 0.0F),
                          QVector3D(length * 0.5F, height * 0.5F, k_leaf_thickness),
                          decayed_color(palette.wood_mid, state, 5));

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
                            decayed_color(geometry.metal_bands ? palette.masonry_accent
                                                               : palette.wood_dark,
                                          state,
                                          7));
      }

      submit_building_box(
          out,
          mesh,
          white,
          hinge,
          QVector3D(-side * (length - 0.10F), (height * 0.5F) + 0.03F, 0.0F),
          QVector3D(0.055F, height * 0.42F, k_leaf_thickness + 0.03F),
          decayed_color(palette.wood_dark, state, 9));
    }
  }

  draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.0F, 2.0F});
}

} // namespace Render::GL

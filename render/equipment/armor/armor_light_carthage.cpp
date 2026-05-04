#include "armor_light_carthage.h"
#include "../../geom/parts.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/mesh_helpers.h"
#include "../../humanoid/style_palette.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "torso_local_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

namespace Render::GL {

using Render::Geom::oriented_cylinder;
using Render::GL::Humanoid::saturate_color;

namespace {

enum ArmorLightCarthagePaletteSlot : std::uint8_t {
  k_carthage_light_shell_slot = 0U,
  k_carthage_light_strap_slot = 1U,
  k_carthage_light_front_slot = 2U,
  k_carthage_light_back_slot = 3U,
};

auto armor_light_carthage_archetype(
    const QMatrix4x4 &cuirass, const std::array<QMatrix4x4, 2> &straps,
    const QMatrix4x4 &front_panel,
    const QMatrix4x4 &back_panel) -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "carthage_light_armor_";
  append_quantized_key(key, cuirass);
  for (const auto &m : straps) {
    append_quantized_key(key, m);
  }
  append_quantized_key(key, front_panel);
  append_quantized_key(key, back_panel);

  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  if (torso_mesh == nullptr) {
    torso_mesh = get_unit_torso();
  }

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(torso_mesh, cuirass, k_carthage_light_shell_slot,
                           nullptr, 1.0F, 1);
  for (const auto &m : straps) {
    builder.add_palette_mesh(get_unit_cylinder(), m,
                             k_carthage_light_strap_slot, nullptr, 1.0F, 1);
  }
  builder.add_palette_mesh(torso_mesh, front_panel, k_carthage_light_front_slot,
                           nullptr, 1.0F, 1);
  builder.add_palette_mesh(torso_mesh, back_panel, k_carthage_light_back_slot,
                           nullptr, 1.0F, 1);

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

void ArmorLightCarthageRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void ArmorLightCarthageRenderer::submit(const ArmorLightCarthageConfig &,
                                        const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        EquipmentBatch &batch) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D leather_color = saturate_color(palette.leather);
  QVector3D leather_shadow =
      saturate_color(leather_color * QVector3D(0.90F, 0.90F, 0.90F));
  QVector3D leather_highlight =
      saturate_color(leather_color * QVector3D(1.08F, 1.05F, 1.02F));
  QVector3D metal_color =
      saturate_color(palette.metal * QVector3D(1.00F, 0.94F, 0.88F));
  QVector3D metal_core =
      saturate_color(metal_color * QVector3D(0.94F, 0.94F, 0.94F));
  QVector3D cloth_accent =
      saturate_color(palette.cloth * QVector3D(1.05F, 1.02F, 1.04F));

  QVector3D up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D right =
      safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;

  QVector3D head_up =
      (head.up.lengthSquared() > 1e-6F) ? head.up.normalized() : up;
  QVector3D waist_up =
      (waist.up.lengthSquared() > 1e-6F) ? waist.up.normalized() : up;

  QVector3D top = torso.origin + up * (torso_r * 0.50F);
  QVector3D head_guard =
      head.origin -
      head_up * ((head_r > 0.0F ? head_r : torso_r * 0.6F) * 1.45F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.24F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);

  float main_radius = torso_r * 1.36F;
  float const main_depth = torso_depth * 1.24F;

  QMatrix4x4 cuirass = oriented_cylinder(
      torso_local.point(top), torso_local.point(bottom),
      torso_local.direction(right), main_radius,
      main_radius * std::max(0.15F, main_depth / main_radius));
  align_torso_mesh_forward(cuirass);
  std::array<QMatrix4x4, 2> straps{};

  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.48F * side) - up * (torso_r * 0.12F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    return Render::Geom::cylinder_between(torso_local.point(shoulder_anchor),
                                          torso_local.point(chest_anchor),
                                          torso_r * 0.12F);
  };
  straps[0] = strap(1.0F);
  straps[1] = strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_depth * 0.35F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_depth * 0.38F) + up * (torso_r * 0.03F);
  float const front_radius = torso_r * 0.56F;
  QMatrix4x4 front_panel = oriented_cylinder(
      torso_local.point(front_panel_top), torso_local.point(front_panel_bottom),
      torso_local.direction(right), front_radius * 1.18F,
      front_radius *
          std::max(0.22F, (torso_depth * 0.76F) / (torso_r * 0.76F)));
  align_torso_mesh_forward(front_panel);

  QVector3D back_panel_top =
      top - forward * (torso_depth * 0.32F) - up * (torso_r * 0.05F);
  QVector3D back_panel_bottom =
      bottom - forward * (torso_depth * 0.34F) + up * (torso_r * 0.02F);
  float const back_radius = torso_r * 0.58F;
  QMatrix4x4 back_panel = oriented_cylinder(
      torso_local.point(back_panel_top), torso_local.point(back_panel_bottom),
      torso_local.direction(right), back_radius * 1.18F,
      back_radius * std::max(0.22F, (torso_depth * 0.74F) / (torso_r * 0.80F)));
  align_torso_mesh_forward(back_panel);

  std::array<QVector3D, 4> const palette_slots{
      metal_color, leather_highlight * 0.95F, cloth_accent, metal_core};
  append_equipment_archetype(
      batch,
      armor_light_carthage_archetype(cuirass, straps, front_panel, back_panel),
      torso_local.world, palette_slots);
}

auto armor_light_carthage_fill_role_colors(const HumanoidPalette &palette,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t {
  if (max < kArmorLightCarthageRoleCount) {
    return 0U;
  }
  using Render::GL::Humanoid::saturate_color;
  QVector3D const metal_color =
      saturate_color(palette.metal * QVector3D(1.00F, 0.94F, 0.88F));
  QVector3D const leather = saturate_color(palette.leather);
  QVector3D const leather_highlight =
      saturate_color(leather * QVector3D(1.08F, 1.05F, 1.02F));
  QVector3D const cloth_accent =
      saturate_color(palette.cloth * QVector3D(1.05F, 1.02F, 1.04F));
  out[0] = metal_color;
  out[1] = leather_highlight * 0.95F;
  out[2] = cloth_accent;
  out[3] = metal_color * 0.94F;
  return kArmorLightCarthageRoleCount;
}

auto armor_light_carthage_make_static_attachment(
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame &torso = bind_frames.torso;
  const AttachmentFrame &waist = bind_frames.waist;
  const AttachmentFrame &head = bind_frames.head;

  QVector3D up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D right =
      safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  TorsoLocalFrame const torso_local =
      make_torso_local_frame(QMatrix4x4{}, torso);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;
  QVector3D head_up =
      (head.up.lengthSquared() > 1e-6F) ? head.up.normalized() : up;
  QVector3D waist_up =
      (waist.up.lengthSquared() > 1e-6F) ? waist.up.normalized() : up;

  QVector3D top = torso.origin + up * (torso_r * 0.50F);
  QVector3D head_guard =
      head.origin -
      head_up * ((head_r > 0.0F ? head_r : torso_r * 0.6F) * 1.45F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }
  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.24F);
  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);

  float main_radius = torso_r * 1.36F;
  float const main_depth = torso_depth * 1.24F;
  QMatrix4x4 cuirass = oriented_cylinder(
      torso_local.point(top), torso_local.point(bottom),
      torso_local.direction(right), main_radius,
      main_radius * std::max(0.15F, main_depth / main_radius));
  align_torso_mesh_forward(cuirass);

  std::array<QMatrix4x4, 2> straps{};
  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.48F * side) - up * (torso_r * 0.12F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    return Render::Geom::cylinder_between(torso_local.point(shoulder_anchor),
                                          torso_local.point(chest_anchor),
                                          torso_r * 0.12F);
  };
  straps[0] = strap(1.0F);
  straps[1] = strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_depth * 0.35F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_depth * 0.38F) + up * (torso_r * 0.03F);
  float const front_radius = torso_r * 0.56F;
  QMatrix4x4 front_panel = oriented_cylinder(
      torso_local.point(front_panel_top), torso_local.point(front_panel_bottom),
      torso_local.direction(right), front_radius * 1.18F,
      front_radius *
          std::max(0.22F, (torso_depth * 0.76F) / (torso_r * 0.76F)));
  align_torso_mesh_forward(front_panel);

  QVector3D back_panel_top =
      top - forward * (torso_depth * 0.32F) - up * (torso_r * 0.05F);
  QVector3D back_panel_bottom =
      bottom - forward * (torso_depth * 0.34F) + up * (torso_r * 0.02F);
  float const back_radius = torso_r * 0.58F;
  QMatrix4x4 back_panel = oriented_cylinder(
      torso_local.point(back_panel_top), torso_local.point(back_panel_bottom),
      torso_local.direction(right), back_radius * 1.18F,
      back_radius * std::max(0.22F, (torso_depth * 0.74F) / (torso_r * 0.80F)));
  align_torso_mesh_forward(back_panel);

  return Render::Equipment::build_prepared_static_attachment({
      .attachment =
          {
              .archetype = &armor_light_carthage_archetype(
                  cuirass, straps, front_panel, back_panel),
              .socket_bone_index = torso_socket_bone_index,
              .unit_local_pose_at_bind = torso_local.world,
          },
      .palette_roles =
          Render::Equipment::sequential_palette_roles<4>(base_role_byte),
  });
}

} // namespace Render::GL

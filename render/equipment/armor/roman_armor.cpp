#include "roman_armor.h"
#include "../../geom/parts.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/mesh_helpers.h"
#include "../../humanoid/style_palette.h"
#include "../equipment_submit.h"
#include "torso_local_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
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

enum RomanHeavyArmorPaletteSlot : std::uint8_t {
  k_heavy_plate_slot = 0U,
  k_heavy_upper_guard_slot = 1U,
  k_heavy_lower_guard_slot = 2U,
  k_heavy_rivet_slot = 3U,
};

enum RomanLightArmorPaletteSlot : std::uint8_t {
  k_light_cuirass_slot = 0U,
  k_light_strap_slot = 1U,
  k_light_shadow_slot = 2U,
};

auto torso_mesh_for_armor() -> Mesh * {
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  return torso_mesh != nullptr ? torso_mesh : get_unit_torso();
}

auto roman_heavy_armor_archetype(
    const QMatrix4x4 &plates, const std::array<QMatrix4x4, 2> &upper_guards,
    const std::array<QMatrix4x4, 2> &lower_guards,
    const std::array<QMatrix4x4, 2> &rivets) -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "roman_heavy_armor_";
  append_quantized_key(key, plates);
  for (const auto &m : upper_guards) {
    append_quantized_key(key, m);
  }
  for (const auto &m : lower_guards) {
    append_quantized_key(key, m);
  }
  for (const auto &m : rivets) {
    append_quantized_key(key, m);
  }

  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(torso_mesh_for_armor(), plates, k_heavy_plate_slot,
                           nullptr, 1.0F, 1);
  for (const auto &m : upper_guards) {
    builder.add_palette_mesh(get_unit_sphere(), m, k_heavy_upper_guard_slot,
                             nullptr, 1.0F, 1);
  }
  for (const auto &m : lower_guards) {
    builder.add_palette_mesh(get_unit_sphere(), m, k_heavy_lower_guard_slot,
                             nullptr, 1.0F, 1);
  }
  for (const auto &m : rivets) {
    builder.add_palette_mesh(get_unit_sphere(), m, k_heavy_rivet_slot);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto roman_light_armor_archetype(
    const QMatrix4x4 &cuirass, const std::array<QMatrix4x4, 2> &straps,
    const QMatrix4x4 &front_panel,
    const QMatrix4x4 &back_panel) -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "roman_light_armor_";
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

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(torso_mesh_for_armor(), cuirass,
                           k_light_cuirass_slot, nullptr, 1.0F, 1);
  for (const auto &m : straps) {
    builder.add_palette_mesh(get_unit_cylinder(), m, k_light_strap_slot,
                             nullptr, 1.0F, 1);
  }
  builder.add_palette_mesh(torso_mesh_for_armor(), front_panel,
                           k_light_cuirass_slot, nullptr, 1.0F, 1);
  builder.add_palette_mesh(torso_mesh_for_armor(), back_panel,
                           k_light_shadow_slot, nullptr, 1.0F, 1);

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

void RomanHeavyArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanHeavyArmorRenderer::submit(const RomanHeavyArmorConfig &,
                                     const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.25F, 1.05F, 0.62F));

  QVector3D up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D right =
      safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D waist_up = safe_attachment_axis(waist.up, up);
  QVector3D head_up = safe_attachment_axis(head.up, up);
  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.88F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.58F;

  QVector3D top = torso.origin + up * (torso_r * 0.60F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.30F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.45F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);

  QMatrix4x4 plates =
      oriented_cylinder(torso_local.point(top), torso_local.point(bottom),
                        torso_local.direction(right), torso_r * 1.24F * 1.18F,
                        torso_r * 1.24F * depth_scale_for(1.10F));
  align_torso_mesh_forward(plates);
  std::array<QMatrix4x4, 2> upper_guards{};
  std::array<QMatrix4x4, 2> lower_guards{};
  std::array<QMatrix4x4, 2> rivets{};

  auto build_shoulder_guard = [&](const QVector3D &shoulder_pos,
                                  const QVector3D &outward, int index) {
    QVector3D upper_pos = shoulder_pos + outward * 0.03F + forward * 0.01F;
    upper_guards[static_cast<std::size_t>(index)] = local_scale_model(
        torso_local.point(upper_pos),
        QVector3D(HP::UPPER_ARM_R * 1.90F, HP::UPPER_ARM_R * 0.42F,
                  HP::UPPER_ARM_R * 1.65F));

    QVector3D lower_pos = upper_pos - up * 0.06F + outward * 0.02F;
    lower_guards[static_cast<std::size_t>(index)] = local_scale_model(
        torso_local.point(lower_pos),
        QVector3D(HP::UPPER_ARM_R * 1.68F, HP::UPPER_ARM_R * 0.38F,
                  HP::UPPER_ARM_R * 1.48F));

    rivets[static_cast<std::size_t>(index)] =
        local_scale_model(torso_local.point(upper_pos + forward * 0.04F),
                          QVector3D(0.012F, 0.012F, 0.012F));
  };

  build_shoulder_guard(frames.shoulder_l.origin, -right, 0);
  build_shoulder_guard(frames.shoulder_r.origin, right, 1);

  std::array<QVector3D, 4> const palette_slots{
      steel_color, steel_color * 0.98F, steel_color * 0.94F, brass_color};
  append_equipment_archetype(
      batch,
      roman_heavy_armor_archetype(plates, upper_guards, lower_guards, rivets),
      torso_local.world, palette_slots);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanLightArmorRenderer::submit(const RomanLightArmorConfig &,
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

  QVector3D leather_color = QVector3D(0.44F, 0.30F, 0.19F);
  QVector3D leather_shadow = leather_color * 0.90F;
  QVector3D leather_highlight = leather_color * 1.08F;

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

  float main_radius = torso_r * 1.26F;
  float const main_depth = torso_depth * 1.24F;

  QMatrix4x4 cuirass = oriented_cylinder(
      torso_local.point(top), torso_local.point(bottom),
      torso_local.direction(right), main_radius,
      main_radius * std::max(0.15F, main_depth / main_radius));
  align_torso_mesh_forward(cuirass);
  std::array<QMatrix4x4, 2> straps{};

  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.54F * side) - up * (torso_r * 0.04F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    return Render::Geom::cylinder_between(torso_local.point(shoulder_anchor),
                                          torso_local.point(chest_anchor),
                                          torso_r * 0.10F);
  };
  straps[0] = strap(1.0F);
  straps[1] = strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_depth * 0.35F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_depth * 0.38F) + up * (torso_r * 0.03F);
  float const front_radius = torso_r * 0.48F;
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
  float const back_radius = torso_r * 0.50F;
  QMatrix4x4 back_panel = oriented_cylinder(
      torso_local.point(back_panel_top), torso_local.point(back_panel_bottom),
      torso_local.direction(right), back_radius * 1.18F,
      back_radius * std::max(0.22F, (torso_depth * 0.74F) / (torso_r * 0.80F)));
  align_torso_mesh_forward(back_panel);

  std::array<QVector3D, 3> const palette_slots{
      leather_highlight, leather_highlight * 0.95F, leather_shadow};
  append_equipment_archetype(
      batch,
      roman_light_armor_archetype(cuirass, straps, front_panel, back_panel),
      torso_local.world, palette_slots);
}

} // namespace Render::GL

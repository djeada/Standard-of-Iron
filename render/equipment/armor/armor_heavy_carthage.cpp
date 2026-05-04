#include "armor_heavy_carthage.h"
#include "../../geom/parts.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/mesh_helpers.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "torso_local_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <string>

namespace Render::GL {

using Render::Geom::oriented_cylinder;

namespace {

enum ArmorHeavyCarthagePaletteSlot : std::uint8_t {
  k_carthage_heavy_chainmail_slot = 0U,
  k_carthage_heavy_bronze_slot = 1U,
  k_carthage_heavy_bronze_core_slot = 2U,
};

auto armor_heavy_carthage_archetype(const std::array<QMatrix4x4, 3> &torsos)
    -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "carthage_heavy_armor_";
  for (const auto &m : torsos) {
    append_quantized_key(key, m);
  }

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
  builder.add_palette_mesh(torso_mesh, torsos[0],
                           k_carthage_heavy_chainmail_slot, nullptr, 1.0F, 1);
  builder.add_palette_mesh(torso_mesh, torsos[1], k_carthage_heavy_bronze_slot,
                           nullptr, 1.0F, 1);
  builder.add_palette_mesh(torso_mesh, torsos[2],
                           k_carthage_heavy_bronze_core_slot, nullptr, 1.0F, 1);

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

void ArmorHeavyCarthageRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void ArmorHeavyCarthageRenderer::submit(const ArmorHeavyCarthageConfig &,
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
      (torso.depth > 0.0F) ? torso.depth : torso.radius * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.90F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.60F;

  QVector3D top = torso.origin + up * (torso_r * 0.64F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.35F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.06F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 1.60F);
  QVector3D chainmail_bottom = waist.origin - waist_up * (waist_r * 1.52F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);
  chainmail_bottom += forward * (torso_r * 0.010F);

  QVector3D bronze_color = QVector3D(0.72F, 0.53F, 0.28F);
  QVector3D bronze_core = bronze_color * 0.92F;
  QVector3D chainmail_color = QVector3D(0.50F, 0.52F, 0.58F);

  auto build_torso = [&](const QVector3D &a, const QVector3D &b, float radius,
                         float scale_x, float base_z) {
    QMatrix4x4 m =
        oriented_cylinder(torso_local.point(a), torso_local.point(b),
                          torso_local.direction(right), radius * scale_x,
                          radius * depth_scale_for(base_z));
    align_torso_mesh_forward(m);
    return m;
  };

  std::array<QMatrix4x4, 3> const torso_layers{
      build_torso(top, chainmail_bottom, torso_r * 1.10F, 1.07F, 1.04F),
      build_torso(top + forward * (torso_r * 0.02F),
                  bottom + forward * (torso_r * 0.02F), torso_r * 1.16F, 1.10F,
                  1.04F),
      build_torso(top, bottom, torso_r * 1.10F, 1.05F, 1.00F),
  };

  std::array<QVector3D, 3> const palette_slots{chainmail_color, bronze_color,
                                               bronze_core};
  append_equipment_archetype(batch,
                             armor_heavy_carthage_archetype(torso_layers),
                             torso_local.world, palette_slots);
}

auto armor_heavy_carthage_fill_role_colors(const HumanoidPalette &palette,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t {
  (void)palette;
  if (max < kArmorHeavyCarthageRoleCount) {
    return 0U;
  }
  out[0] = QVector3D(0.50F, 0.52F, 0.58F);
  out[1] = QVector3D(0.72F, 0.53F, 0.28F);
  out[2] = QVector3D(0.72F, 0.53F, 0.28F) * 0.92F;
  return kArmorHeavyCarthageRoleCount;
}

auto armor_heavy_carthage_make_static_attachment(
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
  QVector3D waist_up = safe_attachment_axis(waist.up, up);
  QVector3D head_up = safe_attachment_axis(head.up, up);
  TorsoLocalFrame const torso_local =
      make_torso_local_frame(QMatrix4x4{}, torso);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso.radius * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.90F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.60F;

  QVector3D top = torso.origin + up * (torso_r * 0.64F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.35F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.06F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 1.60F);
  QVector3D chainmail_bottom = waist.origin - waist_up * (waist_r * 1.52F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);
  chainmail_bottom += forward * (torso_r * 0.010F);

  auto build_torso = [&](const QVector3D &a, const QVector3D &b, float radius,
                         float scale_x, float base_z) {
    QMatrix4x4 m =
        oriented_cylinder(torso_local.point(a), torso_local.point(b),
                          torso_local.direction(right), radius * scale_x,
                          radius * depth_scale_for(base_z));
    align_torso_mesh_forward(m);
    return m;
  };

  std::array<QMatrix4x4, 3> const torso_layers{
      build_torso(top, chainmail_bottom, torso_r * 1.10F, 1.07F, 1.04F),
      build_torso(top + forward * (torso_r * 0.02F),
                  bottom + forward * (torso_r * 0.02F), torso_r * 1.16F, 1.10F,
                  1.04F),
      build_torso(top, bottom, torso_r * 1.10F, 1.05F, 1.00F),
  };

  return Render::Equipment::build_prepared_static_attachment({
      .attachment =
          {
              .archetype = &armor_heavy_carthage_archetype(torso_layers),
              .socket_bone_index = torso_socket_bone_index,
              .unit_local_pose_at_bind = torso_local.world,
          },
      .palette_roles =
          Render::Equipment::sequential_palette_roles<3>(base_role_byte),
  });
}

} // namespace Render::GL

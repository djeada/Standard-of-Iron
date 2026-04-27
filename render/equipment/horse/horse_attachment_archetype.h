#pragma once

#include "../equipment_submit.h"

#include "../../entity/registry.h"
#include "../../horse/attachment_frames.h"
#include "../../static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

auto horse_baseline_back_center_frame() noexcept -> HorseAttachmentFrame;

auto horse_baseline_barrel_frame() noexcept -> HorseAttachmentFrame;

auto horse_baseline_chest_frame() noexcept -> HorseAttachmentFrame;

auto horse_baseline_rump_frame() noexcept -> HorseAttachmentFrame;

auto horse_baseline_neck_base_frame() noexcept -> HorseAttachmentFrame;

auto horse_baseline_muzzle_frame() noexcept -> HorseAttachmentFrame;

inline void append_horse_attachment_archetype(
    EquipmentBatch &batch, const DrawContext &ctx,
    const HorseAttachmentFrame &frame, const RenderArchetype &archetype,
    std::span<const QVector3D> palette = {}, Texture *default_texture = nullptr,
    float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full) {
  append_equipment_archetype(
      batch, archetype,
      frame.make_local_transform(ctx.model, QVector3D(0.0F, 0.0F, 0.0F), 1.0F),
      palette, default_texture, alpha_multiplier, lod);
}

inline auto make_static_attachment_spec_from_horse_renderer(
    const RenderArchetype &archetype, std::uint16_t socket_bone_index,
    const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  Render::Creature::StaticAttachmentSpec spec;
  spec.archetype = &archetype;
  spec.socket_bone_index = socket_bone_index;
  spec.uniform_scale = 1.0F;

  QMatrix4x4 legacy_local;
  legacy_local.setColumn(0, QVector4D(bind_pose_frame.right, 0.0F));
  legacy_local.setColumn(1, QVector4D(bind_pose_frame.up, 0.0F));
  legacy_local.setColumn(2, QVector4D(bind_pose_frame.forward, 0.0F));
  legacy_local.setColumn(3, QVector4D(bind_pose_frame.origin, 1.0F));

  spec.local_offset = bind_palette_socket_bone.inverted() * legacy_local;
  return spec;
}

} // namespace Render::GL

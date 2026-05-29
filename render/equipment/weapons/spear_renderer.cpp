#include "spear_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "../../entity/renderer_constants.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/skeleton.h"
#include "../../humanoid/spear_pose_utils.h"
#include "../attachment_builder.h"
#include "../equipment_archetype_helpers.h"
#include "../equipment_submit.h"
#include "../generated_equipment.h"
#include "../oriented_archetype_utils.h"

namespace Render::GL {

namespace {

enum SpearPaletteSlot : std::uint8_t {
  k_shaft_slot = 0U,
  k_shaft_dark_slot = 1U,
  k_head_slot = 2U,
  k_grip_slot = 3U,
};

struct SpearArchetypeKey {
  int shaft_radius_key{0};
  int material_id{0};
};

auto operator==(const SpearArchetypeKey& lhs, const SpearArchetypeKey& rhs) -> bool {
  return lhs.shaft_radius_key == rhs.shaft_radius_key &&
         lhs.material_id == rhs.material_id;
}

template <std::uint8_t PaletteSlot, typename Tag>
auto cached_spear_part_archetype(const SpearRenderConfig& config,
                                 float radius,
                                 const char* debug_prefix,
                                 GeneratedEquipmentPrimitiveKind kind)
    -> const RenderArchetype& {
  SpearArchetypeKey const key{quantize_equipment_value(config.shaft_radius),
                              config.material_id};
  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      kind == GeneratedEquipmentPrimitiveKind::Cone
          ? generated_cone(QVector3D(0.0F, 0.0F, 0.0F),
                           QVector3D(0.0F, 1.0F, 0.0F),
                           radius,
                           PaletteSlot,
                           1.0F,
                           config.material_id)
          : generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                               QVector3D(0.0F, 1.0F, 0.0F),
                               radius,
                               PaletteSlot,
                               1.0F,
                               config.material_id),
  }};
  return cached_generated_archetype<Tag>(key,
                                         std::string(debug_prefix) + "_" +
                                             std::to_string(key.shaft_radius_key) +
                                             "_" + std::to_string(key.material_id),
                                         primitives);
}

struct SpearLowerShaftTag {};
struct SpearUpperShaftTag {};
struct SpearHeadTag {};
struct SpearGripTag {};

auto spear_lower_shaft_archetype(const SpearRenderConfig& config)
    -> const RenderArchetype& {
  return cached_spear_part_archetype<k_shaft_slot, SpearLowerShaftTag>(
      config,
      config.shaft_radius,
      "spear_lower_shaft",
      GeneratedEquipmentPrimitiveKind::Cylinder);
}

auto spear_upper_shaft_archetype(const SpearRenderConfig& config)
    -> const RenderArchetype& {
  return cached_spear_part_archetype<k_shaft_dark_slot, SpearUpperShaftTag>(
      config,
      config.shaft_radius * 0.95F,
      "spear_upper_shaft",
      GeneratedEquipmentPrimitiveKind::Cylinder);
}

auto spearhead_archetype(const SpearRenderConfig& config) -> const RenderArchetype& {
  return cached_spear_part_archetype<k_head_slot, SpearHeadTag>(
      config,
      config.shaft_radius * 1.8F,
      "spear_head",
      GeneratedEquipmentPrimitiveKind::Cone);
}

auto spear_grip_archetype(const SpearRenderConfig& config) -> const RenderArchetype& {
  return cached_spear_part_archetype<k_grip_slot, SpearGripTag>(
      config,
      config.shaft_radius * 1.5F,
      "spear_grip",
      GeneratedEquipmentPrimitiveKind::Cylinder);
}

auto spear_palette(const SpearRenderConfig& config,
                   const HumanoidPalette& palette) -> std::array<QVector3D, 4> {
  return {config.shaft_color,
          config.shaft_color * 0.98F,
          config.spearhead_color,
          palette.leather * 0.92F};
}

} // namespace

SpearRenderer::SpearRenderer(SpearRenderConfig config)
    : m_base(config) {
}

void SpearRenderer::render(const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void SpearRenderer::submit(const SpearRenderConfig& m_config,
                           const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  AttachmentFrame const grip =
      frames.grip_r.radius > 0.0F
          ? frames.grip_r
          : Render::Humanoid::socket_attachment_frame(
                frames.hand_r, Render::Humanoid::HumanoidSocket::GripR);
  QVector3D const grip_pos = grip.origin;

  QVector3D const spear_dir = compute_spear_direction(anim.inputs);

  QVector3D const shaft_base = grip_pos - spear_dir * 0.28F;
  QVector3D shaft_mid = grip_pos + spear_dir * (m_config.spear_length * 0.5F);
  QVector3D const shaft_tip = grip_pos + spear_dir * m_config.spear_length;

  shaft_mid.setY(shaft_mid.y() + 0.02F);
  auto const palette_slots = spear_palette(m_config, palette);
  QVector3D right_hint = grip.right;
  if (right_hint.lengthSquared() < 1e-6F) {
    right_hint = QVector3D(0.0F, 0.0F, 1.0F);
  }

  append_equipment_archetype(
      batch,
      spear_lower_shaft_archetype(m_config),
      oriented_segment_transform(
          ctx.model, shaft_base, shaft_mid - shaft_base, right_hint),
      palette_slots);

  append_equipment_archetype(
      batch,
      spear_upper_shaft_archetype(m_config),
      oriented_segment_transform(
          ctx.model, shaft_mid, shaft_tip - shaft_mid, right_hint),
      palette_slots);

  QVector3D const spearhead_base = shaft_tip;
  QVector3D const spearhead_tip = shaft_tip + spear_dir * m_config.spearhead_length;
  append_equipment_archetype(
      batch,
      spearhead_archetype(m_config),
      oriented_segment_transform(
          ctx.model, spearhead_base, spearhead_tip - spearhead_base, right_hint),
      palette_slots);

  QVector3D const grip_end = grip_pos + spear_dir * 0.10F;
  append_equipment_archetype(
      batch,
      spear_grip_archetype(m_config),
      oriented_segment_transform(ctx.model, grip_pos, grip_end - grip_pos, right_hint),
      palette_slots);
}

} // namespace Render::GL

auto Render::GL::spear_fill_role_colors(const HumanoidPalette& palette,
                                        const SpearRenderConfig& config,
                                        QVector3D* out,
                                        std::size_t max) -> std::uint32_t {
  return fill_role_colors(
      std::array<QVector3D, k_spear_role_count>{config.shaft_color,
                                                config.shaft_color * 0.98F,
                                                config.spearhead_color,
                                                palette.leather * 0.92F},
      out,
      max);
}

auto Render::GL::spear_make_static_attachments(const SpearRenderConfig& config,
                                               std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 4> {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const& bind_grip = Render::Humanoid::humanoid_bind_body_frames().grip_r;
  QMatrix4x4 bind_socket;
  bind_socket.setColumn(0, QVector4D(bind_grip.right, 0.0F));
  bind_socket.setColumn(1, QVector4D(bind_grip.up, 0.0F));
  bind_socket.setColumn(2, QVector4D(bind_grip.forward, 0.0F));
  bind_socket.setColumn(3, QVector4D(bind_grip.origin, 1.0F));
  bool bind_socket_invertible = false;
  QMatrix4x4 const bind_socket_inverse = bind_socket.inverted(&bind_socket_invertible);
  QVector3D const grip_pos = bind_socket.column(3).toVector3D();
  AnimationInputs const idle_inputs{};
  QVector3D const spear_dir = compute_spear_direction(idle_inputs);

  QVector3D const shaft_base = grip_pos - spear_dir * 0.28F;
  QVector3D shaft_mid = grip_pos + spear_dir * (config.spear_length * 0.5F);
  shaft_mid.setY(shaft_mid.y() + 0.02F);
  QVector3D const shaft_tip = grip_pos + spear_dir * config.spear_length;
  QVector3D const spearhead_tip = shaft_tip + spear_dir * config.spearhead_length;
  QVector3D const grip_end = grip_pos + spear_dir * 0.10F;

  QVector3D right_hint = bind_socket.column(0).toVector3D();
  if (right_hint.lengthSquared() < 1e-6F) {
    right_hint = QVector3D(0.0F, 0.0F, 1.0F);
  }

  const std::array<std::uint8_t, 4> remap{
      base_role_byte,
      static_cast<std::uint8_t>(base_role_byte + 1U),
      static_cast<std::uint8_t>(base_role_byte + 2U),
      static_cast<std::uint8_t>(base_role_byte + 3U),
  };

  auto make_spec = [&](const RenderArchetype& arch,
                       const QVector3D& from,
                       const QVector3D& to) {
    QMatrix4x4 const unit_pose =
        oriented_segment_transform(QMatrix4x4{}, from, to - from, right_hint);
    return Render::Equipment::build_socket_static_attachment({
        .archetype = &arch,
        .socket_bone_index = static_cast<std::uint16_t>(k_bone),
        .bind_bone_transform = bind_bone,
        .bind_socket_transform = bind_socket,
        .mesh_from_socket =
            bind_socket_invertible ? bind_socket_inverse * unit_pose : unit_pose,
        .palette_role_remap = std::span<const std::uint8_t>(remap.data(), remap.size()),
    });
  };

  return {
      make_spec(spear_lower_shaft_archetype(config), shaft_base, shaft_mid),
      make_spec(spear_upper_shaft_archetype(config), shaft_mid, shaft_tip),
      make_spec(spearhead_archetype(config), shaft_tip, spearhead_tip),
      make_spec(spear_grip_archetype(config), grip_pos, grip_end),
  };
}

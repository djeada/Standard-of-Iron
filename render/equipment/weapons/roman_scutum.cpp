#include "roman_scutum.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/skeleton.h"
#include "../../humanoid/style_palette.h"
#include "../../render_archetype.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

namespace {

constexpr std::uint8_t k_red_even = 0;
constexpr std::uint8_t k_red_odd = 1;
constexpr std::uint8_t k_bronze_ridge = 2;
constexpr std::uint8_t k_bronze_ring = 3;
constexpr std::uint8_t k_bronze_boss = 4;
constexpr std::uint8_t k_bronze_rim = 5;
constexpr std::uint8_t k_bronze_rivet = 6;
constexpr QVector3D k_shield_center{0.0F, 0.0F, 0.15F};

auto roman_scutum_archetype() -> const RenderArchetype & {
  static const RenderArchetype kArchetype = []() {
    constexpr float shield_height = 1.2F;
    constexpr float shield_width = 0.65F;
    constexpr float shield_curve = 0.19F;
    constexpr float body_half_depth = 0.040F;
    constexpr float face_half_depth = 0.024F;
    constexpr float frame_half_depth = 0.034F;
    constexpr float frame_half_width = 0.028F;
    constexpr float ridge_half_width = 0.045F;
    constexpr float boss_radius = 0.12F;
    constexpr float boss_depth = 0.080F;

    constexpr QVector3D shield_forward{0.0F, 0.0F, 1.0F};
    constexpr QVector3D shield_up{0.0F, 1.0F, 0.0F};
    constexpr QVector3D shield_right{1.0F, 0.0F, 0.0F};

    RenderArchetypeBuilder builder{"roman_scutum"};

    auto curved_box = [&](const QVector3D &offset, float half_width,
                          float half_height, float half_depth,
                          std::uint8_t slot, float yaw_scale_degrees = 0.0F) {
      float const half_span = shield_width * 0.5F;
      float const normalized_x =
          (half_span > 1e-4F) ? (offset.x() / half_span) : 0.0F;

      QMatrix4x4 m;
      m.translate(k_shield_center + offset + shield_forward * shield_curve);
      m.rotate(-normalized_x * yaw_scale_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(half_width, half_height, half_depth);
      builder.add_palette_mesh(get_unit_cube(), m, slot);
    };

    curved_box(QVector3D(0.0F, 0.0F, 0.0F), shield_width * 0.48F,
               shield_height * 0.49F, body_half_depth, k_red_even);
    curved_box(QVector3D(0.0F, 0.0F, 0.028F), shield_width * 0.36F,
               shield_height * 0.43F, face_half_depth, k_red_odd);

    curved_box(QVector3D(0.0F, shield_height * 0.48F, 0.010F),
               shield_width * 0.45F, frame_half_width, frame_half_depth,
               k_bronze_rim);
    curved_box(QVector3D(0.0F, -shield_height * 0.48F, 0.010F),
               shield_width * 0.45F, frame_half_width, frame_half_depth,
               k_bronze_rim);
    curved_box(QVector3D(shield_width * 0.45F, 0.0F, 0.010F), frame_half_width,
               shield_height * 0.44F, frame_half_depth, k_bronze_rim);
    curved_box(QVector3D(-shield_width * 0.45F, 0.0F, 0.010F), frame_half_width,
               shield_height * 0.44F, frame_half_depth, k_bronze_rim);

    curved_box(QVector3D(0.0F, 0.0F, 0.040F), ridge_half_width,
               shield_height * 0.40F, frame_half_depth, k_bronze_ridge);
    curved_box(QVector3D(0.0F, 0.0F, 0.070F), boss_radius * 0.95F,
               boss_radius * 0.85F, frame_half_depth, k_bronze_ring);

    QVector3D const boss_center =
        k_shield_center + shield_forward * (shield_curve + boss_depth * 0.75F);
    QMatrix4x4 boss_m;
    boss_m.translate(boss_center);
    boss_m.rotate(90.0F, 1.0F, 0.0F, 0.0F);
    boss_m.scale(boss_radius * 0.72F, boss_depth, boss_radius * 0.72F);
    builder.add_palette_mesh(get_unit_cylinder(), boss_m, k_bronze_boss);

    std::array<QVector3D, 4> const rivet_offsets{
        QVector3D(-shield_width * 0.28F, shield_height * 0.30F, 0.060F),
        QVector3D(shield_width * 0.28F, shield_height * 0.30F, 0.060F),
        QVector3D(-shield_width * 0.28F, -shield_height * 0.30F, 0.060F),
        QVector3D(shield_width * 0.28F, -shield_height * 0.30F, 0.060F),
    };
    for (const QVector3D &offset : rivet_offsets) {
      curved_box(offset, 0.018F, 0.018F, 0.020F, k_bronze_rivet, 0.0F);
    }

    return std::move(builder).build();
  }();
  return kArchetype;
}

auto hand_l_basis_transform(const QMatrix4x4 &parent,
                            const AttachmentFrame &hand_l) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(hand_l.right, 0.0F));
  local.setColumn(1, QVector4D(hand_l.up, 0.0F));
  local.setColumn(2, QVector4D(hand_l.forward, 0.0F));
  local.setColumn(3, QVector4D(hand_l.origin, 1.0F));
  return parent * local;
}

auto scutum_local_pose() -> QMatrix4x4 {
  QMatrix4x4 pose;
  pose.translate(0.16F, -0.04F, 0.02F);
  pose.translate(k_shield_center);
  pose.rotate(90.0F, 0.0F, 1.0F, 0.0F);
  pose.translate(-k_shield_center);
  return pose;
}

} // namespace

void RomanScutumRenderer::render(const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanScutumRenderer::submit(const RomanScutumConfig &,
                                 const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &hand_l = frames.hand_l;
  if (hand_l.radius <= 0.0F) {
    return;
  }
  AttachmentFrame const grip =
      frames.grip_l.radius > 0.0F
          ? frames.grip_l
          : Render::Humanoid::socket_attachment_frame(
                frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);

  QVector3D const shield_red =
      saturate_color(palette.cloth * QVector3D(1.5F, 0.3F, 0.3F));
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.0F, 0.5F));

  std::array<QVector3D, kRomanScutumRoleCount> const palette_slots{
      shield_red * 0.975F,  shield_red * 1.025F, bronze_color * 0.9F,
      bronze_color,         bronze_color * 1.1F, bronze_color * 0.95F,
      bronze_color * 1.15F,
  };

  append_equipment_archetype(batch, roman_scutum_archetype(),
                             hand_l_basis_transform(ctx.model, grip) *
                                 scutum_local_pose(),
                             palette_slots);
}

auto roman_scutum_fill_role_colors(const HumanoidPalette &palette,
                                   QVector3D *out,
                                   std::size_t max) -> std::uint32_t {
  if (max < kRomanScutumRoleCount) {
    return 0;
  }
  QVector3D const shield_red =
      saturate_color(palette.cloth * QVector3D(1.5F, 0.3F, 0.3F));
  QVector3D const bronze_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.0F, 0.5F));
  out[k_red_even] = shield_red * 0.975F;
  out[k_red_odd] = shield_red * 1.025F;
  out[k_bronze_ridge] = bronze_color * 0.9F;
  out[k_bronze_ring] = bronze_color;
  out[k_bronze_boss] = bronze_color * 1.1F;
  out[k_bronze_rim] = bronze_color * 0.95F;
  out[k_bronze_rivet] = bronze_color * 1.15F;
  return kRomanScutumRoleCount;
}

auto roman_scutum_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          k_bone)];
  auto const &bind_grip = Render::Humanoid::humanoid_bind_body_frames().grip_l;
  QMatrix4x4 bind_socket;
  bind_socket.setColumn(0, QVector4D(bind_grip.right, 0.0F));
  bind_socket.setColumn(1, QVector4D(bind_grip.up, 0.0F));
  bind_socket.setColumn(2, QVector4D(bind_grip.forward, 0.0F));
  bind_socket.setColumn(3, QVector4D(bind_grip.origin, 1.0F));
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &roman_scutum_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = scutum_local_pose(),
  });
  spec.palette_role_remap[k_red_even] = base_role_byte;
  spec.palette_role_remap[k_red_odd] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_bronze_ridge] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_bronze_ring] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  spec.palette_role_remap[k_bronze_boss] =
      static_cast<std::uint8_t>(base_role_byte + 4U);
  spec.palette_role_remap[k_bronze_rim] =
      static_cast<std::uint8_t>(base_role_byte + 5U);
  spec.palette_role_remap[k_bronze_rivet] =
      static_cast<std::uint8_t>(base_role_byte + 6U);
  return spec;
}

} // namespace Render::GL

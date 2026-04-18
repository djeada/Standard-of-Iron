#include "chainmail_armor.h"
#include "../../geom/parts.h"
#include "../../geom/transforms.h"
#include "../../gl/backend.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../equipment_submit.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::oriented_cylinder;

auto ChainmailArmorRenderer::calculate_ring_color(float x, float y,
                                                  float z) const -> QVector3D {

  float rust_noise =
      std::sin(x * 127.3F) * std::cos(y * 97.1F) * std::sin(z * 83.7F);
  rust_noise = (rust_noise + 1.0F) * 0.5F;

  float gravity_rust = std::clamp(1.0F - y * 0.8F, 0.0F, 1.0F);
  float total_rust =
      (rust_noise * 0.6F + gravity_rust * 0.4F) * m_config.rust_amount;

  return m_config.metal_color * (1.0F - total_rust) +
         m_config.rust_tint * total_rust;
}

void ChainmailArmorRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &anim,
                                    EquipmentBatch &batch) {
  (void)anim;
  (void)palette;

  renderTorsoMail(ctx, frames, batch);

  if (m_config.has_shoulder_guards) {
    renderShoulderGuards(ctx, frames, batch);
  }

  if (m_config.has_arm_coverage) {
    renderArmMail(ctx, frames, batch);
  }
}

void ChainmailArmorRenderer::renderTorsoMail(const DrawContext &ctx,
                                             const BodyFrames &frames,
                                             EquipmentBatch &batch) {
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso.radius * 0.75F;

  QVector3D top = torso.origin + torso.up * (torso_r * 0.20F);
  QVector3D bottom = waist.origin - waist.up * (torso_r * 0.35F);

  QVector3D right_dir = torso.right.normalized();
  QMatrix4x4 mail_transform = oriented_cylinder(
      ctx.model, top, bottom, right_dir, torso_r * 1.12F,
      std::max(0.08F, torso_depth * 1.10F));
  align_torso_mesh_forward(mail_transform);

  QVector3D steel_color = QVector3D(0.65F, 0.67F, 0.70F);

  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  batch.meshes.push_back({torso_mesh != nullptr ? torso_mesh : get_unit_torso(), nullptr,
                 mail_transform, steel_color, nullptr, 1.0F});
}

void ChainmailArmorRenderer::renderShoulderGuards(const DrawContext &ctx,
                                                  const BodyFrames &frames,
                                                  EquipmentBatch &batch) {
  const AttachmentFrame &shoulder_l = frames.shoulder_l;
  const AttachmentFrame &shoulder_r = frames.shoulder_r;
  const AttachmentFrame &torso = frames.torso;

  QVector3D left_base = shoulder_l.origin;
  QVector3D left_tip = left_base + torso.up * 0.08F + torso.right * (-0.05F);

  float const shoulder_radius = 0.08F;

  QVector3D left_color =
      calculate_ring_color(left_base.x(), left_base.y(), left_base.z());
  batch.meshes.push_back({
      get_unit_cylinder(), nullptr,
      cylinder_between(ctx.model, left_base, left_tip, shoulder_radius),
      left_color, nullptr, 0.8F});

  QVector3D right_base = shoulder_r.origin;
  QVector3D right_tip = right_base + torso.up * 0.08F + torso.right * 0.05F;

  QVector3D right_color =
      calculate_ring_color(right_base.x(), right_base.y(), right_base.z());
  batch.meshes.push_back({
      get_unit_cylinder(), nullptr,
      cylinder_between(ctx.model, right_base, right_tip, shoulder_radius),
      right_color, nullptr, 0.8F});

  if (m_config.detail_level >= 1) {
    for (int layer = 0; layer < 3; ++layer) {
      float layer_offset = static_cast<float>(layer) * 0.025F;

      QVector3D left_layer = left_base + torso.up * (-layer_offset);
      QVector3D right_layer = right_base + torso.up * (-layer_offset);

      QMatrix4x4 left_m = ctx.model;
      left_m.translate(left_layer);
      left_m.scale(shoulder_radius * 1.3F);
      batch.meshes.push_back({get_unit_sphere(), nullptr, left_m,
                     left_color * (1.0F - layer_offset), nullptr, 0.75F});

      QMatrix4x4 right_m = ctx.model;
      right_m.translate(right_layer);
      right_m.scale(shoulder_radius * 1.3F);
      batch.meshes.push_back({get_unit_sphere(), nullptr, right_m,
                     right_color * (1.0F - layer_offset), nullptr, 0.75F});
    }
  }
}

void ChainmailArmorRenderer::renderArmMail(const DrawContext &ctx,
                                           const BodyFrames &frames,
                                           EquipmentBatch &batch) {

  const AttachmentFrame &shoulder_l = frames.shoulder_l;
  const AttachmentFrame &shoulder_r = frames.shoulder_r;
  const AttachmentFrame &hand_l = frames.hand_l;
  const AttachmentFrame &hand_r = frames.hand_r;

  QVector3D left_shoulder = shoulder_l.origin;
  QVector3D left_elbow = (left_shoulder + hand_l.origin) * 0.5F;

  int const arm_segments = m_config.detail_level >= 2 ? 6 : 3;

  for (int i = 0; i < arm_segments; ++i) {
    float t0 = static_cast<float>(i) / static_cast<float>(arm_segments);
    float t1 = static_cast<float>(i + 1) / static_cast<float>(arm_segments);

    QVector3D pos0 = left_shoulder * (1.0F - t0) + left_elbow * t0;
    QVector3D pos1 = left_shoulder * (1.0F - t1) + left_elbow * t1;

    float radius = 0.05F * (1.0F - t0 * 0.2F);

    QVector3D color = calculate_ring_color(pos0.x(), pos0.y(), pos0.z());
    batch.meshes.push_back({get_unit_cylinder(), nullptr,
                   cylinder_between(ctx.model, pos0, pos1, radius), color,
                   nullptr, 0.75F});
  }

  QVector3D right_shoulder = shoulder_r.origin;
  QVector3D right_elbow = (right_shoulder + hand_r.origin) * 0.5F;

  for (int i = 0; i < arm_segments; ++i) {
    float t0 = static_cast<float>(i) / static_cast<float>(arm_segments);
    float t1 = static_cast<float>(i + 1) / static_cast<float>(arm_segments);

    QVector3D pos0 = right_shoulder * (1.0F - t0) + right_elbow * t0;
    QVector3D pos1 = right_shoulder * (1.0F - t1) + right_elbow * t1;

    float radius = 0.05F * (1.0F - t0 * 0.2F);

    QVector3D color = calculate_ring_color(pos0.x(), pos0.y(), pos0.z());
    batch.meshes.push_back({get_unit_cylinder(), nullptr,
                   cylinder_between(ctx.model, pos0, pos1, radius), color,
                   nullptr, 0.75F});
  }
}

void ChainmailArmorRenderer::renderRingDetails(
    const DrawContext &ctx, const QVector3D &center, float radius, float height,
    const QVector3D &up, const QVector3D &right, EquipmentBatch &batch) {

  int const rings_around = 24;
  int const rings_vertical = 4;

  for (int row = 0; row < rings_vertical; ++row) {
    float y =
        (static_cast<float>(row) / static_cast<float>(rings_vertical)) * height;

    float row_offset = (row % 2) * 0.5F;

    for (int col = 0; col < rings_around; ++col) {
      float angle = ((static_cast<float>(col) + row_offset) /
                     static_cast<float>(rings_around)) *
                    2.0F * std::numbers::pi_v<float>;

      float x = std::cos(angle) * radius;
      float z = std::sin(angle) * radius;

      QVector3D ring_pos = center + up * y + right * x +
                           QVector3D::crossProduct(up, right).normalized() * z;

      QMatrix4x4 ring_m = ctx.model;
      ring_m.translate(ring_pos);
      ring_m.scale(m_config.ring_size);

      QVector3D color =
          calculate_ring_color(ring_pos.x(), ring_pos.y(), ring_pos.z());
      batch.meshes.push_back({get_unit_sphere(), nullptr, ring_m, color, nullptr, 0.85F});
    }
  }
}

} // namespace Render::GL

#include "tunic_renderer.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

TunicRenderer::TunicRenderer(const TunicConfig &config) : m_config(config) {}

void TunicRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           ISubmitter &submitter) {
  // Animation context currently unused - armor is rigid and follows torso frame
  // Future enhancement: could add breathing effects or battle damage
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.96F, 1.0F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));

  using HP = HumanProportions;
  float const y_top = HP::SHOULDER_Y + 0.02F;

  renderTorsoArmor(ctx, torso, steel_color, brass_color, submitter);

  if (m_config.include_pauldrons) {
    renderPauldrons(ctx, frames, steel_color, brass_color, submitter);
  }

  if (m_config.include_gorget) {
    renderGorget(ctx, torso, y_top, steel_color, brass_color, submitter);
  }

  if (m_config.include_belt) {
    renderBelt(ctx, waist, steel_color, brass_color, submitter);
  }
}

void TunicRenderer::renderTorsoArmor(const DrawContext &ctx,
                                     const AttachmentFrame &torso,
                                     const QVector3D &steel_color,
                                     const QVector3D &brass_color,
                                     ISubmitter &submitter) {
  using HP = HumanProportions;

  const QVector3D &origin = torso.origin;
  const QVector3D &right = torso.right;
  const QVector3D &up = torso.up;
  const QVector3D &forward = torso.forward;
  float const torso_r = torso.radius * m_config.torso_scale;

  float const y_top = HP::SHOULDER_Y + 0.02F;
  float const y_mid_chest = (HP::SHOULDER_Y + HP::CHEST_Y) * 0.5F;
  float const y_bottom_chest = HP::CHEST_Y;
  float const y_waist = HP::WAIST_Y + 0.06F;

  float const shoulder_width = torso_r * m_config.shoulder_width_scale;
  float const chest_width = torso_r * 1.15F;
  float const waist_width = torso_r * m_config.waist_taper;

  float const chest_depth_front = torso_r * 1.1F;
  float const chest_depth_back = torso_r * m_config.chest_depth_scale;

  constexpr int segments = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  auto createTorsoSegment = [&](float y_pos, float width_scale,
                                float depth_front, float depth_back,
                                const QVector3D &color) {
    for (int i = 0; i < segments; ++i) {
      float const angle1 = (static_cast<float>(i) / segments) * 2.0F * pi;
      float const angle2 =
          (static_cast<float>(i + 1) / segments) * 2.0F * pi;

      float const sin1 = std::sin(angle1);
      float const cos1 = std::cos(angle1);
      float const sin2 = std::sin(angle2);
      float const cos2 = std::cos(angle2);

      auto getRadiusAtAngle = [&](float angle_rad) -> float {
        float const cos_a = std::cos(angle_rad);
        float const abs_cos = std::abs(cos_a);

        // Select depth based on front (chest) vs back
        float depth = (cos_a > 0.0F) ? depth_front : depth_back;

        // Create broader shoulders: base scale (1.0) + variation (0.15) at shoulder points
        constexpr float BASE_SHOULDER_SCALE = 1.0F;
        constexpr float SHOULDER_VARIATION_FACTOR = 0.15F;
        float const shoulder_bias =
            BASE_SHOULDER_SCALE +
            SHOULDER_VARIATION_FACTOR * std::abs(std::sin(angle_rad));

        // Blend between circular (abs_cos * 0.3) and depth-based (0.7 * depth)
        return width_scale * shoulder_bias * (abs_cos * 0.3F + 0.7F * depth);
      };

      float const r1 = getRadiusAtAngle(angle1);
      float const r2 = getRadiusAtAngle(angle2);

      QVector3D const p1 =
          origin + right * (r1 * sin1) + forward * (r1 * cos1) + up * (y_pos - origin.y());
      QVector3D const p2 =
          origin + right * (r2 * sin2) + forward * (r2 * cos2) + up * (y_pos - origin.y());

      float const seg_r = (r1 + r2) * 0.5F * 0.08F;
      submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, p1, p2, seg_r),
                     color, nullptr, 1.0F);
    }
  };

  createTorsoSegment(y_top, shoulder_width, chest_depth_front,
                     chest_depth_back, steel_color);
  createTorsoSegment(y_mid_chest, chest_width, chest_depth_front,
                     chest_depth_back, steel_color * 0.99F);
  createTorsoSegment(y_bottom_chest, chest_width * 0.98F, chest_depth_front * 0.95F,
                     chest_depth_back * 0.95F, steel_color * 0.98F);
  createTorsoSegment(y_waist, waist_width, chest_depth_front * 0.90F,
                     chest_depth_back * 0.90F, steel_color * 0.97F);

  auto connectSegments = [&](float y1, float y2, float width1, float width2) {
    for (int i = 0; i < segments / 2; ++i) {
      float const angle = (static_cast<float>(i) / (segments / 2)) * 2.0F * pi;
      float const sin_a = std::sin(angle);
      float const cos_a = std::cos(angle);

      float const depth1 = (cos_a > 0.0F) ? chest_depth_front : chest_depth_back;
      float const depth2 =
          (cos_a > 0.0F) ? chest_depth_front * 0.95F : chest_depth_back * 0.95F;

      float const r1 = width1 * depth1;
      float const r2 = width2 * depth2;

      QVector3D const top =
          origin + right * (r1 * sin_a) + forward * (r1 * cos_a) + up * (y1 - origin.y());
      QVector3D const bot =
          origin + right * (r2 * sin_a) + forward * (r2 * cos_a) + up * (y2 - origin.y());

      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, top, bot, torso_r * 0.06F),
                     steel_color * 0.96F, nullptr, 1.0F);
    }
  };

  connectSegments(y_top, y_mid_chest, shoulder_width, chest_width);
  connectSegments(y_mid_chest, y_bottom_chest, chest_width, chest_width * 0.98F);
  connectSegments(y_bottom_chest, y_waist, chest_width * 0.98F, waist_width);

  // Add decorative rivets around the chest
  auto draw_rivet = [&](const QVector3D &pos) {
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.012F);
    submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
  };

  // Position rivets in a ring around the chest at mid-height
  constexpr float RIVET_POSITION_SCALE =
      0.92F; // Slightly inset from armor edge
  for (int i = 0; i < 8; ++i) {
    float const angle = (static_cast<float>(i) / 8.0F) * 2.0F * pi;
    float const x =
        chest_width * std::sin(angle) * chest_depth_front * RIVET_POSITION_SCALE;
    float const z =
        chest_width * std::cos(angle) * chest_depth_front * RIVET_POSITION_SCALE;
    draw_rivet(
        origin + right * x + forward * z + up * (y_mid_chest + 0.08F - origin.y()));
  }
}

void TunicRenderer::renderPauldrons(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const QVector3D &steel_color,
                                    const QVector3D &brass_color,
                                    ISubmitter &submitter) {
  using HP = HumanProportions;

  auto draw_pauldron = [&](const QVector3D &shoulder,
                           const QVector3D &outward) {
    float const upper_arm_r = HP::UPPER_ARM_R;
    for (int i = 0; i < 4; ++i) {
      float const seg_y = shoulder.y() + 0.04F - static_cast<float>(i) * 0.045F;
      float const seg_r = upper_arm_r * (2.5F - static_cast<float>(i) * 0.12F);
      QVector3D seg_pos =
          shoulder + outward * (0.02F + static_cast<float>(i) * 0.008F);
      seg_pos.setY(seg_y);

      submitter.mesh(getUnitSphere(), sphereAt(ctx.model, seg_pos, seg_r),
                     i == 0 ? steel_color * 1.05F
                            : steel_color * (1.0F - static_cast<float>(i) * 0.03F),
                     nullptr, 1.0F);

      if (i < 3) {
        QMatrix4x4 m = ctx.model;
        m.translate(seg_pos + QVector3D(0, 0.015F, 0.03F));
        m.scale(0.012F);
        submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
      }
    }
  };

  QVector3D const shoulder_right = frames.shoulderR.origin;
  QVector3D const shoulder_left = frames.shoulderL.origin;
  QVector3D const right_axis = frames.torso.right;

  draw_pauldron(shoulder_left, -right_axis);
  draw_pauldron(shoulder_right, right_axis);
}

void TunicRenderer::renderGorget(const DrawContext &ctx,
                                 const AttachmentFrame &torso, float y_top,
                                 const QVector3D &steel_color,
                                 const QVector3D &brass_color,
                                 ISubmitter &submitter) {
  using HP = HumanProportions;

  QVector3D const gorget_top(torso.origin.x(), y_top + 0.025F, torso.origin.z());
  QVector3D const gorget_bot(torso.origin.x(), y_top - 0.012F, torso.origin.z());

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, gorget_bot, gorget_top,
                                 HP::NECK_RADIUS * 2.6F),
                 steel_color * 1.08F, nullptr, 1.0F);

  QVector3D const a = gorget_top + QVector3D(0, 0.005F, 0);
  QVector3D const b = gorget_top - QVector3D(0, 0.005F, 0);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, a, b, HP::NECK_RADIUS * 2.62F),
                 brass_color, nullptr, 1.0F);
}

void TunicRenderer::renderBelt(const DrawContext &ctx,
                               const AttachmentFrame &waist,
                               const QVector3D &steel_color,
                               const QVector3D &brass_color,
                               ISubmitter &submitter) {
  using HP = HumanProportions;

  float const waist_r = waist.radius * m_config.waist_taper;

  for (int i = 0; i < 4; ++i) {
    float const y0 = HP::WAIST_Y + 0.04F - static_cast<float>(i) * 0.038F;
    float const y1 = y0 - 0.032F;
    float const r0 = waist_r * (1.06F + static_cast<float>(i) * 0.025F);

    submitter.mesh(getUnitCone(),
                   coneFromTo(ctx.model, QVector3D(waist.origin.x(), y0, waist.origin.z()),
                              QVector3D(waist.origin.x(), y1, waist.origin.z()), r0),
                   steel_color * (0.96F - static_cast<float>(i) * 0.02F), nullptr,
                   1.0F);

    if (i < 3) {
      QMatrix4x4 m = ctx.model;
      m.translate(QVector3D(r0 * 0.90F, y0 - 0.016F, 0));
      m.scale(0.012F);
      submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
    }
  }
}

} // namespace Render::GL

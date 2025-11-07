#include "roman_armor.h"
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

void RomanHeavyArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.92F, 0.94F, 0.98F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.2F, 1.0F, 0.6F));
  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.6F, 0.4F, 0.3F));

  const QVector3D &origin = torso.origin;
  const QVector3D &right = torso.right;
  const QVector3D &up = torso.up;
  const QVector3D &forward = torso.forward;

  float const torso_r = torso.radius * 1.08F;
  float const shoulder_width = torso_r * 1.18F;
  float const chest_depth_front = torso_r * 1.15F;
  float const chest_depth_back = torso_r * 0.82F;

  constexpr int num_bands = 8;
  float const y_top = HP::SHOULDER_Y;
  float const y_bottom = HP::WAIST_Y + 0.05F;
  float const band_height = (y_top - y_bottom) / static_cast<float>(num_bands);

  constexpr int segments = 20;
  constexpr float pi = std::numbers::pi_v<float>;

  for (int band = 0; band < num_bands; ++band) {
    float const y_band_top = y_top - static_cast<float>(band) * band_height;
    float const y_band_bottom = y_band_top - band_height * 0.92F;

    float const t =
        static_cast<float>(band) / static_cast<float>(num_bands - 1);
    float const width_scale = shoulder_width * (1.0F - t * 0.18F);

    QVector3D band_color =
        steel_color * (1.0F - static_cast<float>(band % 2) * 0.05F);

    auto getRadius = [&](float angle) -> float {
      float const cos_a = std::cos(angle);
      float depth = (cos_a > 0.0F) ? chest_depth_front : chest_depth_back;
      return width_scale * depth * (std::abs(cos_a) * 0.25F + 0.75F);
    };

    for (int i = 0; i < segments; ++i) {
      float const angle1 = (static_cast<float>(i) / segments) * 2.0F * pi;
      float const angle2 = (static_cast<float>(i + 1) / segments) * 2.0F * pi;

      float const cos1 = std::cos(angle1);
      float const sin1 = std::sin(angle1);
      float const cos2 = std::cos(angle2);
      float const sin2 = std::sin(angle2);

      float const r1 = getRadius(angle1);
      float const r2 = getRadius(angle2);

      QVector3D const p1_top = origin + right * (r1 * sin1) +
                               forward * (r1 * cos1) +
                               up * (y_band_top - origin.y());
      QVector3D const p2_top = origin + right * (r2 * sin2) +
                               forward * (r2 * cos2) +
                               up * (y_band_top - origin.y());

      QVector3D const p1_bot = origin + right * (r1 * sin1) +
                               forward * (r1 * cos1) +
                               up * (y_band_bottom - origin.y());
      QVector3D const p2_bot = origin + right * (r2 * sin2) +
                               forward * (r2 * cos2) +
                               up * (y_band_bottom - origin.y());

      float const seg_r = (r1 + r2) * 0.5F * 0.04F;
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p1_top, p1_bot, seg_r),
                     band_color, nullptr, 1.0F);
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p2_top, p2_bot, seg_r),
                     band_color, nullptr, 1.0F);
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p1_top, p2_top, seg_r),
                     band_color, nullptr, 1.0F);
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p1_bot, p2_bot, seg_r),
                     band_color, nullptr, 1.0F);
    }

    if (band % 2 == 0) {
      for (int i = 0; i < 6; ++i) {
        float const angle = (static_cast<float>(i) / 6.0F) * 2.0F * pi;
        float const r = getRadius(angle) * 0.95F;
        float const y_rivet = (y_band_top + y_band_bottom) * 0.5F;
        QVector3D rivet_pos = origin + right * (r * std::sin(angle)) +
                              forward * (r * std::cos(angle)) +
                              up * (y_rivet - origin.y());

        QMatrix4x4 m = ctx.model;
        m.translate(rivet_pos);
        m.scale(0.01F);
        submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
      }
    }
  }

  auto renderShoulderGuard = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    constexpr int shoulder_segments = 4;
    float const upper_arm_r = HP::UPPER_ARM_R;

    for (int i = 0; i < shoulder_segments; ++i) {
      float const seg_y = shoulder_pos.y() - static_cast<float>(i) * 0.04F;
      float const seg_r = upper_arm_r * (2.3F - static_cast<float>(i) * 0.15F);
      QVector3D seg_pos =
          shoulder_pos + outward * (0.03F + static_cast<float>(i) * 0.01F);
      seg_pos.setY(seg_y);

      submitter.mesh(getUnitSphere(), sphereAt(ctx.model, seg_pos, seg_r),
                     steel_color * (1.0F - static_cast<float>(i) * 0.04F),
                     nullptr, 1.0F);

      if (i < 3) {
        QMatrix4x4 m = ctx.model;
        m.translate(seg_pos + QVector3D(0, 0.02F, 0.04F));
        m.scale(0.008F);
        submitter.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
      }
    }
  };

  renderShoulderGuard(frames.shoulderL.origin, -right);
  renderShoulderGuard(frames.shoulderR.origin, right);

  const AttachmentFrame &waist = frames.waist;
  auto safeDir = [](const QVector3D &axis, const QVector3D &fallback) {
    if (axis.lengthSquared() > 1e-6F) {
      return axis.normalized();
    }
    QVector3D fb = fallback;
    if (fb.lengthSquared() < 1e-6F) {
      fb = QVector3D(0.0F, 1.0F, 0.0F);
    }
    return fb.normalized();
  };

  QVector3D const waist_center =
      (waist.radius > 0.0F) ? waist.origin
                            : QVector3D(origin.x(), HP::WAIST_Y, origin.z());
  QVector3D const waist_up = safeDir(waist.up, up);
  float const belt_height =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 0.24F;
  QVector3D const belt_top = waist_center + waist_up * (0.5F * belt_height);
  QVector3D const belt_bot = waist_center - waist_up * (0.5F * belt_height);
  float const belt_radius =
      (waist.radius > 0.0F ? waist.radius : torso.radius * 0.95F) * 1.12F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, belt_bot, belt_top, belt_radius),
                 leather_color * 0.92F, nullptr, 1.0F);

  QVector3D const trim_top = belt_top + waist_up * 0.012F;
  QVector3D const trim_bot = belt_bot - waist_up * 0.012F;
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, trim_bot, trim_top, belt_radius * 1.03F),
      brass_color * 0.95F, nullptr, 1.0F);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.9F, 0.93F, 0.97F));
  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.55F, 0.38F, 0.28F));

  const QVector3D &origin = torso.origin;
  const QVector3D &right = torso.right;
  const QVector3D &up = torso.up;
  const QVector3D &forward = torso.forward;

  float const torso_r = torso.radius * 1.04F;
  float const shoulder_width = torso_r * 1.12F;
  float const chest_depth_front = torso_r * 1.10F;
  float const chest_depth_back = torso_r * 0.86F;

  constexpr int num_bands = 4;
  float const y_top = HP::SHOULDER_Y - 0.02F;
  float const y_bottom = HP::CHEST_Y;
  float const band_height = (y_top - y_bottom) / static_cast<float>(num_bands);

  constexpr int segments = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  for (int band = 0; band < num_bands; ++band) {
    float const y_band_top = y_top - static_cast<float>(band) * band_height;
    float const y_band_bottom = y_band_top - band_height * 0.90F;

    float const t =
        static_cast<float>(band) / static_cast<float>(num_bands - 1);
    float const width_scale = shoulder_width * (1.0F - t * 0.12F);

    QVector3D band_color =
        steel_color * (1.0F - static_cast<float>(band % 2) * 0.04F);

    for (int i = 0; i < segments; ++i) {
      float const angle1 = (static_cast<float>(i) / segments) * 2.0F * pi;
      float const angle2 = (static_cast<float>(i + 1) / segments) * 2.0F * pi;

      auto getRadius = [&](float angle) -> float {
        float const cos_a = std::cos(angle);
        float depth = (cos_a > 0.0F) ? chest_depth_front : chest_depth_back;
        return width_scale * depth * (std::abs(cos_a) * 0.3F + 0.7F);
      };

      float const r1 = getRadius(angle1);
      float const r2 = getRadius(angle2);

      QVector3D const p1_top = origin + right * (r1 * std::sin(angle1)) +
                               forward * (r1 * std::cos(angle1)) +
                               up * (y_band_top - origin.y());
      QVector3D const p1_bot = origin + right * (r1 * std::sin(angle1)) +
                               forward * (r1 * std::cos(angle1)) +
                               up * (y_band_bottom - origin.y());

      float const seg_r = r1 * 0.035F;
      submitter.mesh(getUnitCylinder(),
                     cylinderBetween(ctx.model, p1_top, p1_bot, seg_r),
                     band_color, nullptr, 1.0F);
    }
  }

  const AttachmentFrame &waist = frames.waist;
  auto safeDir = [](const QVector3D &axis, const QVector3D &fallback) {
    if (axis.lengthSquared() > 1e-6F) {
      return axis.normalized();
    }
    QVector3D fb = fallback;
    if (fb.lengthSquared() < 1e-6F) {
      fb = QVector3D(0.0F, 1.0F, 0.0F);
    }
    return fb.normalized();
  };

  QVector3D const waist_center =
      (waist.radius > 0.0F)
          ? waist.origin
          : QVector3D(origin.x(), HP::WAIST_Y + 0.02F, origin.z());
  QVector3D const waist_up = safeDir(waist.up, up);
  float const belt_height =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 0.18F;
  QVector3D const belt_top = waist_center + waist_up * (0.5F * belt_height);
  QVector3D const belt_bot = waist_center - waist_up * (0.5F * belt_height);
  float const belt_r =
      (waist.radius > 0.0F ? waist.radius : torso.radius) * 1.02F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, belt_top, belt_bot, belt_r),
                 leather_color, nullptr, 1.0F);
}

} // namespace Render::GL

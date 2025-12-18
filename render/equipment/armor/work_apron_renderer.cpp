#include "work_apron_renderer.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

WorkApronRenderer::WorkApronRenderer(const WorkApronConfig &config)
    : m_config(config) {}

void WorkApronRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &,
                               const HumanoidAnimationContext &,
                               ISubmitter &submitter) {
  renderApronBody(ctx, frames.torso, frames.waist, submitter);

  if (m_config.include_straps) {
    renderStraps(ctx, frames.torso, frames, submitter);
  }

  if (m_config.include_pockets) {
    renderPockets(ctx, frames.waist, submitter);
  }
}

void WorkApronRenderer::renderApronBody(const DrawContext &ctx,
                                        const AttachmentFrame &torso,
                                        const AttachmentFrame &waist,
                                        ISubmitter &submitter) {
  using HP = HumanProportions;

  if (torso.radius <= 0.0F || waist.radius <= 0.0F) {
    return;
  }

  const QVector3D &origin = waist.origin;
  const QVector3D &right = waist.right;
  const QVector3D &up = waist.up;
  const QVector3D &forward = waist.forward;

  float const waist_r = waist.radius * m_config.apron_width;
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.85F : waist.radius * 0.75F;

  float const y_top = origin.y() + 0.05F;
  float const y_bottom = origin.y() - m_config.apron_length;

  constexpr int segs = 14;
  constexpr float pi = std::numbers::pi_v<float>;

  QVector3D const apron_color = m_config.leather_color;
  QVector3D const apron_dark = apron_color * 0.80F;

  for (int ring = 0; ring < 6; ++ring) {
    float const t = float(ring) / 5.0F;
    float const y = y_top - t * (y_top - y_bottom);
    float const flare = 1.0F + t * 0.15F;
    float const w = waist_r * flare;
    float const d = waist_d * flare;
    float const thickness = 0.018F + t * 0.004F;

    QVector3D const color = apron_color * (1.0F - t * 0.12F);

    for (int i = 0; i < segs; ++i) {
      float const angle_start = (float(i) / segs - 0.25F) * pi;
      float const angle_end = (float(i + 1) / segs - 0.25F) * pi;

      if (angle_start < -pi * 0.5F || angle_start > pi * 0.5F) {
        continue;
      }

      QVector3D const p1 = origin + right * (w * std::sin(angle_start)) +
                           forward * (d * std::cos(angle_start)) +
                           up * (y - origin.y());
      QVector3D const p2 = origin + right * (w * std::sin(angle_end)) +
                           forward * (d * std::cos(angle_end)) +
                           up * (y - origin.y());

      submitter.mesh(get_unit_cylinder(),
                     cylinder_between(ctx.model, p1, p2, thickness), color,
                     nullptr, 1.0F);
    }
  }

  for (int edge = 0; edge < 2; ++edge) {
    float const side_angle = (edge == 0) ? -pi * 0.25F : pi * 0.25F;

    for (int i = 0; i < 6; ++i) {
      float const t = float(i) / 5.0F;
      float const y = y_top - t * (y_top - y_bottom);
      float const flare = 1.0F + t * 0.15F;
      float const w = waist_r * flare;
      float const d = waist_d * flare;

      QVector3D const pos = origin + right * (w * std::sin(side_angle)) +
                            forward * (d * std::cos(side_angle)) +
                            up * (y - origin.y());

      submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, 0.020F),
                     apron_dark, nullptr, 1.0F);
    }
  }
}

void WorkApronRenderer::renderStraps(const DrawContext &ctx,
                                     const AttachmentFrame &torso,
                                     const BodyFrames &frames,
                                     ISubmitter &submitter) {
  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D const strap_color = m_config.strap_color;

  QVector3D const shoulder_l = frames.shoulder_l.origin;
  QVector3D const shoulder_r = frames.shoulder_r.origin;

  QVector3D const torso_front =
      torso.origin + torso.forward * torso.radius * 0.80F;
  QVector3D const torso_back =
      torso.origin - torso.forward * torso.radius * 0.65F;

  QVector3D const chest_l =
      torso_front + torso.right * torso.radius * 0.30F + torso.up * 0.08F;
  QVector3D const chest_r =
      torso_front - torso.right * torso.radius * 0.30F + torso.up * 0.08F;

  QVector3D const back_l =
      torso_back + torso.right * torso.radius * 0.20F - torso.up * 0.02F;
  QVector3D const back_r =
      torso_back - torso.right * torso.radius * 0.20F - torso.up * 0.02F;

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, chest_l, back_l, 0.020F),
                 strap_color, nullptr, 1.0F);
  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, chest_r, back_r, 0.020F),
                 strap_color, nullptr, 1.0F);

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, back_l, back_r, 0.018F),
                 strap_color, nullptr, 1.0F);
}

void WorkApronRenderer::renderPockets(const DrawContext &ctx,
                                      const AttachmentFrame &waist,
                                      ISubmitter &submitter) {
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const pocket_color = m_config.leather_color * 0.88F;

  constexpr float pi = std::numbers::pi_v<float>;

  for (int side = -1; side <= 1; side += 2) {
    float const pocket_angle = float(side) * 0.12F * pi;
    float const pocket_x = waist.radius * 0.55F * std::sin(pocket_angle);
    float const pocket_z = waist.radius * 0.45F * std::cos(pocket_angle);

    QVector3D const pocket_center = waist.origin + waist.right * pocket_x +
                                    waist.forward * pocket_z - waist.up * 0.12F;

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        float const x_off = (float(i) - 1.0F) * 0.018F;
        float const y_off = (float(j) - 1.0F) * 0.022F;

        QVector3D const pos = pocket_center +
                              waist.right * x_off * float(side) +
                              waist.up * y_off;

        float const r = 0.012F - float(i + j) * 0.0005F;
        submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r),
                       pocket_color, nullptr, 1.0F);
      }
    }
  }
}

} // namespace Render::GL

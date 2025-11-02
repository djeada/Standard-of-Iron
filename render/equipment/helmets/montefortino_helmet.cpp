#include "montefortino_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

void MontefortinoHelmetRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim; // Unused

  const AttachmentFrame &head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto headPoint = [&](const QVector3D &norm) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, norm);
  };

  auto headTransform = [&](const QVector3D &norm, float scale) -> QMatrix4x4 {
    return HumanoidRendererBase::makeFrameLocalTransform(ctx.model, head, norm,
                                                         scale);
  };

  QVector3D const base_metal = palette.metal;
  QVector3D bronze =
      saturate_color(base_metal * QVector3D(1.22F, 1.04F, 0.70F));
  QVector3D patina = saturate_color(bronze * QVector3D(0.88F, 0.96F, 0.92F));
  QVector3D tinned_highlight =
      saturate_color(bronze * QVector3D(1.12F, 1.08F, 1.04F));
  QVector3D leather_band =
      saturate_color(palette.leatherDark * QVector3D(1.10F, 0.96F, 0.80F));

  // Draw leather cap
  auto draw_leather_cap = [&]() {
    QVector3D leather_brown = saturate_color(palette.leatherDark *
                                             QVector3D(1.15F, 0.95F, 0.78F));
    QVector3D leather_dark =
        saturate_color(leather_brown * QVector3D(0.85F, 0.88F, 0.92F));
    QVector3D bronze_stud =
        saturate_color(palette.metal * QVector3D(1.20F, 1.02F, 0.70F));

    QMatrix4x4 cap_transform =
        headTransform(QVector3D(0.0F, 0.70F, 0.0F), 1.0F);
    cap_transform.scale(0.92F, 0.55F, 0.88F);
    submitter.mesh(getUnitSphere(), cap_transform, leather_brown, nullptr,
                   1.0F);

    QVector3D const band_top = headPoint(QVector3D(0.0F, 0.20F, 0.0F));
    QVector3D const band_bot = headPoint(QVector3D(0.0F, 0.15F, 0.0F));

    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, band_bot, band_top,
                                   head_r * 1.02F),
                   leather_dark, nullptr, 1.0F);

    auto draw_stud = [&](float angle) {
      QVector3D const stud_pos = headPoint(
          QVector3D(std::sin(angle) * 1.03F, 0.175F, std::cos(angle) * 1.03F));
      submitter.mesh(getUnitSphere(),
                     sphereAt(ctx.model, stud_pos, head_r * 0.012F),
                     bronze_stud, nullptr, 1.0F);
    };

    for (int i = 0; i < 4; ++i) {
      float const angle = (i / 4.0F) * 2.0F * std::numbers::pi_v<float>;
      draw_stud(angle);
    }
  };

  draw_leather_cap();

  // Top knob
  QMatrix4x4 top_knob = headTransform(QVector3D(0.0F, 0.88F, 0.0F), 0.18F);
  submitter.mesh(getUnitSphere(), top_knob, tinned_highlight, nullptr, 1.0F);

  // Brow band
  QVector3D const brow_top = headPoint(QVector3D(0.0F, 0.55F, 0.0F));
  QVector3D const brow_bot = headPoint(QVector3D(0.0F, 0.42F, 0.0F));
  QMatrix4x4 brow =
      cylinderBetween(ctx.model, brow_bot, brow_top, head_r * 1.20F);
  brow.scale(1.04F, 1.0F, 0.86F);
  submitter.mesh(getUnitCylinder(), brow, leather_band, nullptr, 1.0F);

  // Rim
  QVector3D const rim_upper = headPoint(QVector3D(0.0F, 0.40F, 0.0F));
  QVector3D const rim_lower = headPoint(QVector3D(0.0F, 0.30F, 0.0F));
  QMatrix4x4 rim =
      cylinderBetween(ctx.model, rim_lower, rim_upper, head_r * 1.30F);
  rim.scale(1.06F, 1.0F, 0.90F);
  submitter.mesh(getUnitCylinder(), rim,
                 bronze * QVector3D(0.94F, 0.92F, 0.88F), nullptr, 1.0F);

  // Crest
  QVector3D const crest_front = headPoint(QVector3D(0.0F, 0.92F, 0.82F));
  QVector3D const crest_back = headPoint(QVector3D(0.0F, 0.92F, -0.90F));
  QMatrix4x4 crest =
      cylinderBetween(ctx.model, crest_back, crest_front, head_r * 0.14F);
  crest.scale(0.54F, 1.0F, 1.0F);
  submitter.mesh(getUnitCylinder(), crest,
                 tinned_highlight * QVector3D(0.94F, 0.96F, 1.02F), nullptr,
                 1.0F);
}

} // namespace Render::GL

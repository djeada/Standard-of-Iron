#include "roman_light_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

void RomanLightHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      ISubmitter &submitter) {
  (void)anim;

  AttachmentFrame head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frame_local_position(head, normalized);
  };

  QVector3D const helmet_color =
      saturate_color(palette.metal * QVector3D(1.15F, 0.92F, 0.68F));
  QVector3D const helmet_accent = helmet_color * 1.14F;

  QVector3D const helmet_top = headPoint(QVector3D(0.0F, 1.28F, 0.0F));
  QVector3D const helmet_bot = headPoint(QVector3D(0.0F, 0.08F, 0.0F));
  float const helmet_r = head_r * 1.08F;

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, helmet_bot, helmet_top, helmet_r),
                 helmet_color, nullptr, 1.0F, 2);

  QVector3D const apex_pos = headPoint(QVector3D(0.0F, 1.48F, 0.0F));
  submitter.mesh(get_unit_cone(),
                 cone_from_to(ctx.model, helmet_top, apex_pos, helmet_r * 0.97F),
                 helmet_accent, nullptr, 1.0F, 2);

  auto ring = [&](float y_offset, float r_scale, const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    float const h = head_r * 0.018F;
    QVector3D const a = center + head.up * (h * 0.5F);
    QVector3D const b = center - head.up * (h * 0.5F);
    submitter.mesh(get_unit_cylinder(),
                   cylinder_between(ctx.model, a, b, helmet_r * r_scale), col,
                   nullptr, 1.0F, 2);
  };

  ring(0.35F, 1.06F, helmet_accent);
  ring(0.95F, 1.02F, helmet_color * 1.04F);

  QVector3D const neck_guard_top = headPoint(QVector3D(0.0F, 0.03F, -0.85F));
  QVector3D const neck_guard_bot = headPoint(QVector3D(0.0F, -0.32F, -0.92F));
  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, neck_guard_bot, neck_guard_top,
                                 helmet_r * 0.86F),
                 helmet_color * 0.90F, nullptr, 1.0F, 2);

  QVector3D const crest_base = apex_pos;
  QVector3D const crest_mid = crest_base + head.up * 0.09F;
  QVector3D const crest_top = crest_mid + head.up * 0.12F;

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, crest_base, crest_mid, 0.018F),
                 helmet_accent, nullptr, 1.0F, 2);

  submitter.mesh(get_unit_cone(),
                 cone_from_to(ctx.model, crest_mid, crest_top, 0.042F),
                 QVector3D(0.88F, 0.18F, 0.18F), nullptr, 1.0F, 0);

  submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, crest_top, 0.020F),
                 helmet_accent, nullptr, 1.0F, 2);
}

} // namespace Render::GL

#include "arm_guards_renderer.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

ArmGuardsRenderer::ArmGuardsRenderer(const ArmGuardsConfig &config)
    : m_config(config) {}

void ArmGuardsRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &,
                               const HumanoidAnimationContext &,
                               ISubmitter &submitter) {
  QVector3D elbow_l = frames.shoulder_l.origin +
                      (frames.hand_l - frames.shoulder_l.origin) * 0.55F;
  QVector3D elbow_r = frames.shoulder_r.origin +
                      (frames.hand_r - frames.shoulder_r.origin) * 0.55F;

  renderArmGuard(ctx, elbow_l, frames.hand_l, submitter);
  renderArmGuard(ctx, elbow_r, frames.hand_r, submitter);
}

void ArmGuardsRenderer::renderArmGuard(const DrawContext &ctx,
                                       const QVector3D &elbow,
                                       const QVector3D &wrist,
                                       ISubmitter &submitter) {
  QVector3D const guard_color = m_config.leather_color;
  QVector3D const strap_color = m_config.strap_color;

  QVector3D const arm_dir = (wrist - elbow).normalized();
  float const arm_length = (wrist - elbow).length();

  if (arm_length < 0.01F) {
    return;
  }

  float const guard_start = 0.15F;
  float const guard_end = 0.15F + m_config.guard_length;

  QVector3D const guard_top = elbow + arm_dir * (arm_length * guard_start);
  QVector3D const guard_bot =
      elbow + arm_dir * std::min(arm_length * guard_end, arm_length * 0.85F);

  constexpr int segments = 5;
  for (int i = 0; i < segments; ++i) {
    float const t = float(i) / float(segments - 1);
    QVector3D const pos = guard_top * (1.0F - t) + guard_bot * t;

    float const r = 0.026F - t * 0.004F;

    submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r),
                   guard_color * (1.0F - t * 0.08F), nullptr, 1.0F);
  }

  if (m_config.include_straps) {
    QVector3D const strap_1 = guard_top + arm_dir * 0.02F;
    QVector3D const strap_2 =
        guard_top + arm_dir * (guard_end - guard_start) * arm_length * 0.5F;
    QVector3D const strap_3 = guard_bot - arm_dir * 0.02F;

    for (const auto &strap_pos : {strap_1, strap_2, strap_3}) {
      submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, strap_pos, 0.010F),
                     strap_color, nullptr, 1.0F);
    }
  }
}

} // namespace Render::GL

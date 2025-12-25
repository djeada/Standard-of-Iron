#include "carthage_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace {
auto mix_color(const QVector3D &a, const QVector3D &b, float t) -> QVector3D {
  return a * (1.0F - t) + b * t;
}
} 

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

void CarthageHeavyHelmetRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) {
    return;
  }

  const float R = head.radius;
  const float lift = R * 0.03f;
  const float helmet_scale = 1.08f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled = n * helmet_scale;
    return HumanoidRendererBase::frame_local_position(head, scaled) +
           head.up * lift;
  };

  QVector3D const base_color = m_config.bronze_color;
  QVector3D const accent =
      mix_color(m_config.bronze_color, m_config.glow_color, 0.32F);

  float base_r = R * 1.04f;
  QVector3D cone_base = head_point(QVector3D(0.0f, 0.58f, 0.0f));
  QVector3D cone_tip = head_point(QVector3D(0.0f, 1.46f, 0.0f));
  submitter.mesh(get_unit_cone(),
                 cone_from_to(ctx.model, cone_base, cone_tip, base_r),
                 base_color, nullptr, 1.0f, 2);

  QVector3D tip_base = head_point(QVector3D(0.0f, 1.12f, 0.0f));
  QVector3D tip_apex = head_point(QVector3D(0.0f, 1.70f, 0.0f));
  submitter.mesh(get_unit_cone(),
                 cone_from_to(ctx.model, tip_base, tip_apex,
                              std::max(0.05f, base_r * 0.28f)),
                 accent, nullptr, 1.0f, 2);

  QMatrix4x4 tip_cap =
      sphere_at(ctx.model, tip_apex + head.up * (R * 0.015f), R * 0.06f);
  submitter.mesh(get_unit_sphere(), tip_cap,
                 mix_color(accent, m_config.glow_color, 0.48F), nullptr, 1.0f,
                 2);
}

} 

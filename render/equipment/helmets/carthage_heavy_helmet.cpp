#include "carthage_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::sphereAt;

void CarthageHeavyHelmetRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) return;

  render_bowl(ctx, head, submitter);
  
  if (m_config.has_cheek_guards) {
    render_cheek_guards(ctx, head, submitter);
  }
}

void CarthageHeavyHelmetRenderer::render_bowl(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  // Simple bronze bowl helmet for heavy infantry
  QVector3D c0 = headPoint(QVector3D(0.0f, 0.75f, 0.0f));
  QMatrix4x4 m0 = ctx.model;
  m0.translate(c0);
  m0.scale(R * 1.15f, R * 0.75f, R * 1.12f);

  // BRIGHT bronze color - very visible, distinct from leather
  submitter.mesh(getUnitSphere(), m0, QVector3D(0.85f, 0.55f, 0.25f), nullptr, 0.3f);
}

void CarthageHeavyHelmetRenderer::render_cheek_guards(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  // Simple cheek plates
  QVector3D left_cheek = headPoint(QVector3D(-0.65f, 0.2f, 0.3f));
  QVector3D right_cheek = headPoint(QVector3D(0.65f, 0.2f, 0.3f));

  QMatrix4x4 m_left = ctx.model;
  m_left.translate(left_cheek);
  m_left.scale(R * 0.35f, R * 0.55f, R * 0.15f);

  QMatrix4x4 m_right = ctx.model;
  m_right.translate(right_cheek);
  m_right.scale(R * 0.35f, R * 0.55f, R * 0.15f);

  submitter.mesh(getUnitSphere(), m_left, m_config.bronze_color, nullptr, 0.6f);
  submitter.mesh(getUnitSphere(), m_right, m_config.bronze_color, nullptr, 0.6f);
}

} // namespace Render::GL

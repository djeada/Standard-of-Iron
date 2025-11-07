#include "carthage_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace {
auto mixColor(const QVector3D &a, const QVector3D &b, float t) -> QVector3D {
  return a * (1.0F - t) + b * t;
}
} // namespace

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

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

  render_bowl(ctx, head, submitter);

  if (m_config.has_neck_guard) {
    render_neck_guard(ctx, head, submitter);
  }

  if (m_config.has_face_plate) {
    render_face_plate(ctx, head, submitter);
  }

  if (m_config.has_hair_crest) {
    render_crest(ctx, head, submitter);
  }

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

  QVector3D c0 = headPoint(QVector3D(0.0f, 0.92f, 0.0f));
  QMatrix4x4 m0 = ctx.model;
  m0.translate(c0);
  m0.scale(R * 1.12f, R * 0.68f, R * 1.08f);

  QVector3D luminousBronze =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.35F);
  submitter.mesh(getUnitSphere(), m0, luminousBronze, nullptr, 0.3f);

  QVector3D rimCenter = headPoint(QVector3D(0.0f, 0.62f, 0.0f));
  QMatrix4x4 rim = ctx.model;
  rim.translate(rimCenter);
  rim.scale(R * 1.28f, R * 0.16f, R * 1.25f);
  submitter.mesh(getUnitSphere(), rim, m_config.glow_color, nullptr, 0.16f);
}

void CarthageHeavyHelmetRenderer::render_cheek_guards(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  QVector3D left_cheek = headPoint(QVector3D(-0.58f, 0.18f, 0.42f));
  QVector3D right_cheek = headPoint(QVector3D(0.58f, 0.18f, 0.42f));

  QMatrix4x4 m_left = ctx.model;
  m_left.translate(left_cheek);
  m_left.scale(R * 0.32f, R * 0.48f, R * 0.18f);
  m_left.rotate(-6.0f, QVector3D(0.0f, 0.0f, 1.0f));

  QMatrix4x4 m_right = ctx.model;
  m_right.translate(right_cheek);
  m_right.scale(R * 0.32f, R * 0.48f, R * 0.18f);
  m_right.rotate(6.0f, QVector3D(0.0f, 0.0f, 1.0f));

  submitter.mesh(getUnitSphere(), m_left, m_config.bronze_color, nullptr, 0.6f);
  submitter.mesh(getUnitSphere(), m_right, m_config.bronze_color, nullptr,
                 0.6f);
}

void CarthageHeavyHelmetRenderer::render_face_plate(const DrawContext &ctx,
                                                    const AttachmentFrame &head,
                                                    ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  QVector3D brow = headPoint(QVector3D(0.0f, 0.70f, 0.60f));
  QVector3D chin = headPoint(QVector3D(0.0f, -0.08f, 0.34f));
  QMatrix4x4 mask =
      cylinderBetween(ctx.model, chin, brow, std::max(0.10f, R * 0.26f));
  QVector3D plateColor =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.25F);
  submitter.mesh(getUnitCylinder(), mask, plateColor, nullptr, 0.45f);

  QVector3D noseTop = headPoint(QVector3D(0.0f, 0.58f, 0.70f));
  QVector3D noseBottom = headPoint(QVector3D(0.0f, -0.02f, 0.46f));
  QMatrix4x4 nose = cylinderBetween(ctx.model, noseBottom, noseTop,
                                    std::max(0.05f, R * 0.12f));
  submitter.mesh(getUnitCylinder(), nose, m_config.glow_color, nullptr, 0.65f);

  render_brow_arch(ctx, head, submitter);
}

void CarthageHeavyHelmetRenderer::render_neck_guard(const DrawContext &ctx,
                                                    const AttachmentFrame &head,
                                                    ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  QVector3D guardCenter = headPoint(QVector3D(0.0f, 0.15f, -0.65f));
  QMatrix4x4 guard = ctx.model;
  guard.translate(guardCenter);
  guard.scale(R * 1.25f, R * 0.52f, R * 0.58f);
  QVector3D guardColor =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.15F);
  submitter.mesh(getUnitSphere(), guard, guardColor, nullptr, 0.28f);
}

void CarthageHeavyHelmetRenderer::render_brow_arch(const DrawContext &ctx,
                                                   const AttachmentFrame &head,
                                                   ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  QVector3D left = headPoint(QVector3D(-0.62f, 0.66f, 0.60f));
  QVector3D right = headPoint(QVector3D(0.62f, 0.66f, 0.60f));
  float archRadius = std::max(0.04f, R * 0.10f);
  QMatrix4x4 arch = cylinderBetween(ctx.model, left, right, archRadius);
  QVector3D archColor =
      mixColor(m_config.glow_color, m_config.bronze_color, 0.5F);
  submitter.mesh(getUnitCylinder(), arch, archColor, nullptr, 0.52f);

  QVector3D ridgeTop = headPoint(QVector3D(0.0f, 0.82f, 0.58f));
  QMatrix4x4 ridge = ctx.model;
  ridge.translate(ridgeTop);
  ridge.scale(R * 0.22f, R * 0.10f, R * 0.26f);
  submitter.mesh(getUnitSphere(), ridge, m_config.glow_color, nullptr, 0.58f);
}

void CarthageHeavyHelmetRenderer::render_crest(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n);
  };

  QVector3D crestBack = headPoint(QVector3D(0.0f, 1.18f, -0.28f));
  QVector3D crestFront = headPoint(QVector3D(0.0f, 1.18f, 0.28f));
  float crestRadius = std::max(0.06f, R * 0.26f);
  QMatrix4x4 crestBridge =
      cylinderBetween(ctx.model, crestBack, crestFront, crestRadius);
  submitter.mesh(getUnitCylinder(), crestBridge, m_config.crest_color, nullptr,
                 0.52f);

  QVector3D plumeTop = headPoint(QVector3D(0.0f, 1.70f, 0.0f));
  QVector3D plumeBase = headPoint(QVector3D(0.0f, 1.08f, 0.0f));
  float plumeRadius = std::max(0.05f, R * 0.18f);
  QMatrix4x4 plume =
      cylinderBetween(ctx.model, plumeBase, plumeTop, plumeRadius);
  QVector3D plumeColor =
      mixColor(m_config.crest_color, m_config.glow_color, 0.40F);
  submitter.mesh(getUnitCylinder(), plume, plumeColor, nullptr, 0.70f);
}

} // namespace Render::GL

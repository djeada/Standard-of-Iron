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
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D bowl_center = head_point(QVector3D(0.0f, 1.47f, 0.0f));
  QMatrix4x4 bowl = ctx.model;
  bowl.translate(bowl_center);
  bowl.scale(R * 1.12f, R * 0.68f, R * 1.08f);

  QVector3D luminous_bronze =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.35F);
  // Material ID: 2 = helmet
  submitter.mesh(getUnitSphere(), bowl, luminous_bronze, nullptr, 0.3f, 2);

  QVector3D rim_center = head_point(QVector3D(0.0f, 1.17f, 0.0f));
  QMatrix4x4 rim = ctx.model;
  rim.translate(rim_center);
  rim.scale(R * 1.28f, R * 0.16f, R * 1.25f);
  submitter.mesh(getUnitSphere(), rim, m_config.glow_color, nullptr, 0.16f, 2);
}

void CarthageHeavyHelmetRenderer::render_cheek_guards(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  const float R = head.radius;
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D left_cheek = head_point(QVector3D(-0.58f, 0.73f, 0.42f));
  QVector3D right_cheek = head_point(QVector3D(0.58f, 0.73f, 0.42f));

  QMatrix4x4 left_guard = ctx.model;
  left_guard.translate(left_cheek);
  left_guard.scale(R * 0.32f, R * 0.48f, R * 0.18f);
  left_guard.rotate(-6.0f, QVector3D(0.0f, 0.0f, 1.0f));

  QMatrix4x4 right_guard = ctx.model;
  right_guard.translate(right_cheek);
  right_guard.scale(R * 0.32f, R * 0.48f, R * 0.18f);
  right_guard.rotate(6.0f, QVector3D(0.0f, 0.0f, 1.0f));

  submitter.mesh(getUnitSphere(), left_guard, m_config.bronze_color, nullptr, 0.6f, 2);
  submitter.mesh(getUnitSphere(), right_guard, m_config.bronze_color, nullptr,
                 0.6f, 2);
}

void CarthageHeavyHelmetRenderer::render_face_plate(const DrawContext &ctx,
                                                    const AttachmentFrame &head,
                                                    ISubmitter &submitter) {
  const float R = head.radius;
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D brow = head_point(QVector3D(0.0f, 1.25f, 0.60f));
  QVector3D chin = head_point(QVector3D(0.0f, 0.47f, 0.34f));
  QMatrix4x4 mask =
      cylinderBetween(ctx.model, chin, brow, std::max(0.10f, R * 0.26f));
  QVector3D plate_color =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.25F);
  submitter.mesh(getUnitCylinder(), mask, plate_color, nullptr, 0.45f, 2);

  QVector3D nose_top = head_point(QVector3D(0.0f, 1.13f, 0.70f));
  QVector3D nose_bottom = head_point(QVector3D(0.0f, 0.53f, 0.46f));
  QMatrix4x4 nose = cylinderBetween(ctx.model, nose_bottom, nose_top,
                                    std::max(0.05f, R * 0.12f));
  submitter.mesh(getUnitCylinder(), nose, m_config.glow_color, nullptr, 0.65f, 2);

  render_brow_arch(ctx, head, submitter);
}

void CarthageHeavyHelmetRenderer::render_neck_guard(const DrawContext &ctx,
                                                    const AttachmentFrame &head,
                                                    ISubmitter &submitter) {
  const float R = head.radius;
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D guard_center = head_point(QVector3D(0.0f, 0.70f, -0.65f));
  QMatrix4x4 guard = ctx.model;
  guard.translate(guard_center);
  guard.scale(R * 1.25f, R * 0.52f, R * 0.58f);
  QVector3D guard_color =
      mixColor(m_config.bronze_color, m_config.glow_color, 0.15F);
  submitter.mesh(getUnitSphere(), guard, guard_color, nullptr, 0.28f, 2);
}

void CarthageHeavyHelmetRenderer::render_brow_arch(const DrawContext &ctx,
                                                   const AttachmentFrame &head,
                                                   ISubmitter &submitter) {
  const float R = head.radius;
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D left = head_point(QVector3D(-0.62f, 1.21f, 0.60f));
  QVector3D right = head_point(QVector3D(0.62f, 1.21f, 0.60f));
  float arch_radius = std::max(0.04f, R * 0.10f);
  QMatrix4x4 arch = cylinderBetween(ctx.model, left, right, arch_radius);
  QVector3D arch_color =
      mixColor(m_config.glow_color, m_config.bronze_color, 0.5F);
  submitter.mesh(getUnitCylinder(), arch, arch_color, nullptr, 0.52f, 2);

  QVector3D ridge_top = head_point(QVector3D(0.0f, 1.37f, 0.58f));
  QMatrix4x4 ridge = ctx.model;
  ridge.translate(ridge_top);
  ridge.scale(R * 0.22f, R * 0.10f, R * 0.26f);
  submitter.mesh(getUnitSphere(), ridge, m_config.glow_color, nullptr, 0.58f, 2);
}

void CarthageHeavyHelmetRenderer::render_crest(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  const float helm_scale = 1.2f;
  const float helmet_y_offset = R * 0.1f;
  auto head_point = [&](const QVector3D &n) {
    QVector3D scaled_n = n;

    if (scaled_n.z() > 0.2f) {
      scaled_n.setZ(std::max(scaled_n.z(), 1.05f / helm_scale));
    }

    scaled_n = scaled_n * helm_scale;

    QVector3D p = HumanoidRendererBase::frameLocalPosition(head, scaled_n);
    return p + head.up * helmet_y_offset;
  };

  QVector3D crest_back = head_point(QVector3D(0.0f, 1.73f, -0.28f));
  QVector3D crest_front = head_point(QVector3D(0.0f, 1.73f, 0.28f));
  float crest_radius = std::max(0.06f, R * 0.26f);
  QMatrix4x4 crest_bridge =
      cylinderBetween(ctx.model, crest_back, crest_front, crest_radius);
  submitter.mesh(getUnitCylinder(), crest_bridge, m_config.crest_color, nullptr,
                 0.52f, 2);

  QVector3D plume_top = head_point(QVector3D(0.0f, 2.25f, 0.0f));
  QVector3D plume_base = head_point(QVector3D(0.0f, 1.63f, 0.0f));
  float plume_radius = std::max(0.05f, R * 0.18f);
  QMatrix4x4 plume =
      cylinderBetween(ctx.model, plume_base, plume_top, plume_radius);
  QVector3D plume_color =
      mixColor(m_config.crest_color, m_config.glow_color, 0.40F);
  submitter.mesh(getUnitCylinder(), plume, plume_color, nullptr, 0.70f, 2);
}

} // namespace Render::GL

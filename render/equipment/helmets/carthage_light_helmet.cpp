#include "carthage_light_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/backend.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

static constexpr float kHelmetVerticalLift = 0.14F;

static inline auto helmetLiftVector(const AttachmentFrame &head) -> QVector3D {
  QVector3D up = head.up;
  if (up.lengthSquared() < 1e-6F) {
    up = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    up.normalize();
  }
  return up * (head.radius * kHelmetVerticalLift);
}

static inline void submit_disk(ISubmitter &submitter, const DrawContext &ctx,
                               const QVector3D &center,
                               const QVector3D &normal_dir, float radius,
                               float thickness, const QVector3D &color,
                               float roughness, int material_id = 2) {
  QVector3D n = normal_dir;
  if (n.lengthSquared() < 1e-5f) {
    n = QVector3D(0, 1, 0);
  }
  n.normalize();
  QVector3D a = center - 0.5f * thickness * n;
  QVector3D b = center + 0.5f * thickness * n;
  // Material ID: 2 = helmet
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, radius),
                 color, nullptr, roughness, material_id);
}

static inline void submit_spike(ISubmitter &submitter, const DrawContext &ctx,
                                const QVector3D &base, const QVector3D &dir,
                                float length, float base_radius,
                                const QVector3D &color, float roughness,
                                int material_id = 2) {
  QVector3D d = dir;
  if (d.lengthSquared() < 1e-5f) {
    d = QVector3D(0, 1, 0);
  }
  d.normalize();
  QVector3D tip = base + d * length;
  // Material ID: 2 = helmet
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, base, tip, base_radius), color,
                 nullptr, roughness, material_id);
  QMatrix4x4 m;
  m = ctx.model;
  m.translate(tip);
  m.scale(base_radius * 1.1f);
  submitter.mesh(getUnitSphere(), m, color * 1.05f, nullptr, roughness, material_id);
}

void CarthageLightHelmetRenderer::render(const DrawContext &ctx,
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
}

void CarthageLightHelmetRenderer::render_bowl(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  const float baseR = head.radius;
  const float helmetScale = 1.26F;
  const float R = baseR * helmetScale;

  QVector3D up = head.up;
  if (up.lengthSquared() < 1e-6F) {
    up = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    up.normalize();
  }
  QVector3D right = head.right;
  if (right.lengthSquared() < 1e-6F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }
  QVector3D forward = head.forward;
  if (forward.lengthSquared() < 1e-6F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    forward.normalize();
  }
  QVector3D const helmet_offset = helmetLiftVector(head);
  QVector3D const helmet_origin = head.origin + helmet_offset;
  auto headPoint = [&](const QVector3D &n) {
    QVector3D p = n * helmetScale;
    return HumanoidRendererBase::frameLocalPosition(head, p) + helmet_offset;
  };

  QVector3D cap_center = helmet_origin + up * (R * 0.62F);
  QMatrix4x4 bowl = ctx.model;
  bowl.translate(cap_center);
  bowl.scale(R * 0.88F, R * 0.82F, R * 0.88F);
  submitter.mesh(getUnitSphere(), bowl, m_config.leather_color * 0.94F, nullptr,
                 0.9F, 2);

  QVector3D taper_top = helmet_origin + up * (R * 0.48F);
  QVector3D taper_bot = helmet_origin + up * (R * 0.26F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, taper_top, taper_bot, R * 0.78F),
                 m_config.leather_color * 0.86F, nullptr, 0.92F, 2);

  QVector3D band_top = helmet_origin + up * (R * 0.24F);
  QVector3D band_bot = helmet_origin + up * (R * 0.10F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, band_top, band_bot, R * 0.92F),
                 m_config.leather_color * 0.72F, nullptr, 0.95F, 2);

  QVector3D crest_base = helmet_origin + up * (R * 0.82F);
  QVector3D crest_tip = crest_base + up * (R * 0.55F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, crest_base, crest_tip, R * 0.35F),
                 m_config.bronze_color * 0.78F, nullptr, 0.92F, 2);
  QMatrix4x4 crest_cap = ctx.model;
  crest_cap.translate(crest_tip);
  crest_cap.scale(R * 0.42F, R * 0.32F, R * 0.42F);
  submitter.mesh(getUnitSphere(), crest_cap, m_config.bronze_color * 0.88F,
                 nullptr, 0.93F, 2);

  QVector3D strap_front_top =
      helmet_origin + up * (R * 0.44F) + forward * (R * 0.60F);
  QVector3D strap_front_bot =
      helmet_origin + up * (R * 0.20F) + forward * (R * 0.70F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, strap_front_top, strap_front_bot, R * 0.20F),
      m_config.bronze_color * 0.85F, nullptr, 0.92F, 2);

  QVector3D strap_left_top =
      helmet_origin + up * (R * 0.38F) - right * (R * 0.66F);
  QVector3D strap_left_bot =
      helmet_origin + up * (R * 0.16F) - right * (R * 0.72F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, strap_left_top, strap_left_bot, R * 0.16F),
      m_config.bronze_color * 0.90F, nullptr, 0.95F, 2);

  QVector3D strap_right_top =
      helmet_origin + up * (R * 0.38F) + right * (R * 0.66F);
  QVector3D strap_right_bot =
      helmet_origin + up * (R * 0.16F) + right * (R * 0.72F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, strap_right_top, strap_right_bot, R * 0.16F),
      m_config.bronze_color * 0.90F, nullptr, 0.95F, 2);
}

void CarthageLightHelmetRenderer::render_brim(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  const float baseR = head.radius;
  const float helmetScale = 1.26F;
  const float R = baseR * helmetScale;
  QVector3D const helmet_offset = helmetLiftVector(head);
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n) + helmet_offset;
  };

  auto blade = [&](float sx) {
    QVector3D center = headPoint(QVector3D(0.42f * sx, 0.58f, 0.83f));
    QVector3D nrm =
        (head.forward * 0.90f - head.right * sx * 0.25f - head.up * 0.15f)
            .normalized();
    submit_disk(submitter, ctx, center, nrm, R * 0.56f, R * 0.08f,
                m_config.bronze_color * 1.02f, 0.92f);
  };
  blade(-1.0f);
  blade(+1.0f);

  auto connect_brow = [&](const QVector3D &a, const QVector3D &b,
                          float radius_scale) {
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, R * radius_scale),
                   m_config.bronze_color * 1.08f, nullptr, 0.95f, 2);
  };

  QVector3D brow_left = headPoint(QVector3D(-0.54f, 0.86f, 0.64f));
  QVector3D brow_mid = headPoint(QVector3D(0.0f, 0.94f, 0.70f));
  QVector3D brow_right = headPoint(QVector3D(0.54f, 0.86f, 0.64f));
  connect_brow(brow_left, brow_mid, 0.07f);
  connect_brow(brow_mid, brow_right, 0.07f);
}

void CarthageLightHelmetRenderer::render_cheek_guards(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  const float baseR = head.radius;
  const float helmetScale = 1.26F;
  const float R = baseR * helmetScale;
  QVector3D const helmet_offset = helmetLiftVector(head);
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n) + helmet_offset;
  };

  auto side = [&](float sx) {
    for (int j = 0; j < 4; ++j) {
      float v = (float)j / 3.0f;
      QVector3D center = headPoint(
          QVector3D(0.90f * sx, 0.54f - 0.24f * v, 0.22f + 0.18f * v));
      QVector3D n =
          (head.right * sx * 0.95f + head.forward * 0.25f - head.up * 0.08f)
              .normalized();
      float r_plate = R * (0.38f - 0.05f * v);
      float thick = R * 0.055f;
      submit_disk(submitter, ctx, center, n, r_plate, thick,
                  m_config.bronze_color * 0.95f, 0.86f);

      if (m_config.detail_level >= 1) {

        QMatrix4x4 riv_m = ctx.model;
        riv_m.translate(center + n * (thick * 0.55f));
        riv_m.scale(R * 0.06f);
        submitter.mesh(getUnitSphere(), riv_m, m_config.bronze_color * 1.28f,
                       nullptr, 1.0f, 2);
      }
    }

    QVector3D fang_base = headPoint(QVector3D(0.78f * sx, 0.20f, 0.36f));
    submit_spike(submitter, ctx, fang_base,
                 -head.up * 0.8f + head.forward * 0.2f, R * 0.22f, R * 0.05f,
                 m_config.bronze_color * 1.1f, 0.95f);
  };

  side(-1.0f);
  side(+1.0f);
}

void CarthageLightHelmetRenderer::render_nasal_guard(
    const DrawContext &ctx, const AttachmentFrame &head,
    ISubmitter &submitter) {
  const float R = head.radius;
  QVector3D const helmet_offset = helmetLiftVector(head);
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n) + helmet_offset;
  };

  QVector3D top = headPoint(QVector3D(0.0f, 0.70f, 0.80f));
  QVector3D bot = headPoint(QVector3D(0.0f, -0.04f, 0.95f));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, bot, top, R * 0.14f),
                 m_config.bronze_color * 0.98f, nullptr, 0.9f, 2);

  QVector3D left = top + head.right * (R * 0.30f);
  QVector3D right = top - head.right * (R * 0.30f);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, left, right, R * 0.07f),
                 m_config.bronze_color * 1.06f, nullptr, 0.93f, 2);

  for (int i = 0; i < 3; ++i) {
    float yy = -0.02f - 0.08f * i;
    QVector3D gl = headPoint(QVector3D(-0.32f, yy, 0.96f));
    QVector3D gr = headPoint(QVector3D(0.32f, yy, 0.96f));
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, gl, gr, R * 0.045f),
                   m_config.bronze_color * 1.02f, nullptr, 0.9f, 2);
  }
}

void CarthageLightHelmetRenderer::render_crest(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  QVector3D const helmet_offset = helmetLiftVector(head);
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n) + helmet_offset;
  };

  QVector3D left = headPoint(QVector3D(-0.95f, 1.02f, 0.02f));
  QVector3D right = headPoint(QVector3D(0.95f, 1.02f, 0.02f));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, left, right, R * 0.12f),
                 m_config.bronze_color * 1.12f, nullptr, 0.96f, 2);

  QVector3D crest_color(0.85f, 0.15f, 0.18f);
  int strands = (m_config.detail_level >= 2) ? 24 : 14;
  for (int i = 0; i < strands; ++i) {
    float t = (float)i / std::max(1, strands - 1);
    QVector3D base = left * (1.0f - t) + right * t;
    base += head.up * (R * 0.06f);
    float bow = std::sin(t * std::numbers::pi_v<float>) * 0.35f;
    QVector3D tip = base +
                    head.up * (R * (0.55f - 0.15f * std::abs(0.5f - t))) +
                    head.forward * (R * (0.30f + bow));
    float spread = (float((i % 5) - 2)) * R * 0.06f;
    tip += head.forward * 0.0f + head.right * 0.0f;
    QVector3D col = crest_color * (0.9f + 0.18f * ((i % 2) ? 1.0f : 0.0f));
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, base, tip, R * 0.04f), col,
                   nullptr, 0.62f, 2);
  }
}

void CarthageLightHelmetRenderer::render_rivets(const DrawContext &ctx,
                                                const AttachmentFrame &head,
                                                ISubmitter &submitter) {
  const float R = head.radius;
  QVector3D const helmet_offset = helmetLiftVector(head);
  auto headPoint = [&](const QVector3D &n) {
    return HumanoidRendererBase::frameLocalPosition(head, n) + helmet_offset;
  };
  QVector3D col = m_config.bronze_color * 1.25f;

  int n1 = 16;
  for (int i = 0; i < n1; ++i) {
    float a = (float)i / n1 * 2.0f * std::numbers::pi_v<float>;
    float ca = std::cos(a), sa = std::sin(a);
    QVector3D p1 = headPoint(QVector3D(0.98f * ca, 0.48f, 0.98f * sa));
    QMatrix4x4 m = ctx.model;
    m.translate(p1);
    m.scale(R * 0.058f);
    submitter.mesh(getUnitSphere(), m, col, nullptr, 1.0f, 2);
  }
  int n2 = 12;
  for (int i = 0; i < n2; ++i) {
    float a = (float)i / n2 * 2.0f * std::numbers::pi_v<float>;
    float ca = std::cos(a), sa = std::sin(a);
    QVector3D p2 = headPoint(QVector3D(0.82f * ca, 0.86f, 0.82f * sa - 0.03f));
    QMatrix4x4 m = ctx.model;
    m.translate(p2);
    m.scale(R * 0.05f);
    submitter.mesh(getUnitSphere(), m, col * 0.98f, nullptr, 1.0f, 2);
  }

  int sp = 7;
  for (int i = 0; i < sp; ++i) {
    float t = (float)i / (sp - 1);
    QVector3D base = headPoint(
        QVector3D(0.0f, 0.95f + 0.1f * (t - 0.5f), 0.50f - 1.05f * t));
    QVector3D dir = (head.up * 0.85f - head.forward * 0.15f);
    submit_spike(submitter, ctx, base, dir,
                 R * (0.22f + 0.06f * std::sin(t * std::numbers::pi_v<float>)),
                 R * 0.045f, m_config.bronze_color * 1.12f, 0.96f);
  }
}

} // namespace Render::GL

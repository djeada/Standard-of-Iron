#include "carthage_light_helmet.h"
#include "../../geom/transforms.h"
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

// --- Helper ---------------------------------------------------------------
static inline void submit_disk(ISubmitter &submitter,
                               const DrawContext &ctx,
                               const QVector3D &center,
                               const QVector3D &normal_dir,
                               float radius,
                               float thickness,
                               const QVector3D &color,
                               float roughness) {
  QVector3D n = normal_dir;
  if (n.lengthSquared() < 1e-5f) n = QVector3D(0, 1, 0);
  n.normalize();
  QVector3D a = center - 0.5f * thickness * n;
  QVector3D b = center + 0.5f * thickness * n;
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, radius),
                 color, nullptr, roughness);
}

void CarthageLightHelmetRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) {
    return;
  }

  render_shell(ctx, head, submitter);        // taller dome + knob
  render_brow_band(ctx, head, submitter);    // reinforced brow
  render_neck_guard(ctx, head, submitter);   // flared back skirt
  render_cheek_plates(ctx, head, submitter); // contoured guards

  if (m_config.has_nasal_guard) {
    render_nasal(ctx, head, submitter);
  }

  if (m_config.has_crest) {
    render_crest_mount(ctx, head, submitter);
  }

  if (m_config.detail_level >= 2) {
    render_rivets(ctx, head, submitter);
  }
}

// -------------------------------------------------------------------------
// SHELL: taller, more conical bowl with subtle ridge and top knob
void CarthageLightHelmetRenderer::render_shell(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  // Primary bowl – pushed higher and slightly conical so it reads as a helmet, not a cap
  QVector3D bowl_center = headPoint(QVector3D(0.0f, 0.68f, 0.02f));
  QMatrix4x4 m_bowl = ctx.model;
  m_bowl.translate(bowl_center);
  // X,Z broader; Y taller for a conical/oval bowl
  m_bowl.scale(R * 1.12f, R * 1.05f, R * 1.08f);
  submitter.mesh(getUnitSphere(), m_bowl, m_config.bronze_color, nullptr, 0.92f);

  // Subtle longitudinal ridge (front-to-back) to break the cap silhouette
  QVector3D crest_front = headPoint(QVector3D(0.0f, 1.0f, 0.45f));
  QVector3D crest_back  = headPoint(QVector3D(0.0f, 0.98f, -0.55f));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, crest_front, crest_back, R * 0.07f),
                 m_config.bronze_color * 1.08f, nullptr, 0.95f);

  // Top knob (Montefortino-style)
  QVector3D knob_pos = headPoint(QVector3D(0.0f, 1.1f, -0.02f));
  QMatrix4x4 m_knob = ctx.model;
  m_knob.translate(knob_pos);
  m_knob.scale(R * 0.16f);
  submitter.mesh(getUnitSphere(), m_knob, m_config.bronze_color * 1.15f, nullptr, 0.95f);
}

// -------------------------------------------------------------------------
// BROW BAND: continuous reinforced band around the rim (no visor/brim)
void CarthageLightHelmetRenderer::render_brow_band(const DrawContext &ctx,
                                                   const AttachmentFrame &head,
                                                   ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  QVector3D ring_center = headPoint(QVector3D(0.0f, 0.42f, 0.0f));
  // A very short, very wide "coin" that intersects the bowl – visually reads as a band
  submit_disk(submitter, ctx, ring_center, head.up, R * 1.22f, R * 0.07f,
              m_config.bronze_color * 1.05f, 0.95f);

  if (m_config.detail_level >= 1) {
    // Small front peak integrated into the band (not a separate cap-like brim)
    QVector3D front_center = headPoint(QVector3D(0.0f, 0.48f, 0.73f));
    QVector3D n = (head.forward * 0.9f + head.up * 0.15f).normalized();
    submit_disk(submitter, ctx, front_center, n, R * 0.48f, R * 0.05f,
                m_config.bronze_color * 1.0f, 0.9f);
  }
}

// -------------------------------------------------------------------------
// NECK GUARD: flared skirt made of overlapping plates along the back half
void CarthageLightHelmetRenderer::render_neck_guard(const DrawContext &ctx,
                                                    const AttachmentFrame &head,
                                                    ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  // Arc over the back ~160° span
  const int plates = (m_config.detail_level >= 2) ? 14 : 9;
  const float a0 = 140.0f * std::numbers::pi_v<float> / 180.0f;
  const float a1 = 220.0f * std::numbers::pi_v<float> / 180.0f;
  for (int i = 0; i < plates; ++i) {
    float t = (plates == 1) ? 0.5f : (float)i / (plates - 1);
    float a = a0 + (a1 - a0) * t;
    float ca = std::cos(a), sa = std::sin(a);

    // Center of each plate, slightly below the band, more drop near the very back
    float drop = 0.10f + 0.10f * std::sin(t * std::numbers::pi_v<float>);
    QVector3D c = headPoint(QVector3D(0.85f * ca, 0.32f - drop, 0.85f * sa - 0.02f));

    // Plate normal: outward radial with downward tilt for flare
    QVector3D radial = (head.right * ca + head.forward * sa).normalized();
    QVector3D n = (radial * 0.7f + head.up * -0.6f).normalized();

    // Plate size subtly larger near the middle back
    float r_plate = R * (0.46f + 0.10f * std::sin(t * std::numbers::pi_v<float>));
    float thick   = R * 0.06f;

    submit_disk(submitter, ctx, c, n, r_plate, thick, m_config.bronze_color * 1.02f, 0.9f);
  }
}

// -------------------------------------------------------------------------
// CHEEK PLATES: articulated, curved plates following the jawline
void CarthageLightHelmetRenderer::render_cheek_plates(const DrawContext &ctx,
                                                      const AttachmentFrame &head,
                                                      ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  auto side = [&](float sx) {
    // Three overlapping plates per side descending and slightly wrapping forward
    int segments = 3;
    for (int j = 0; j < segments; ++j) {
      float v = (float)j / (segments - 1);
      QVector3D center = headPoint(QVector3D(0.86f * sx, 0.50f - 0.22f * v, 0.28f + 0.12f * v));
      QVector3D n = (head.right * sx * 0.9f + head.forward * 0.18f - head.up * 0.05f).normalized();
      float r_plate = R * (0.34f - 0.03f * v);
      float thick   = R * 0.05f;
      submit_disk(submitter, ctx, center, n, r_plate, thick, m_config.bronze_color * 0.96f, 0.85f);

      if (m_config.detail_level >= 1) {
        // Hinge/rivet near the top of each plate
        QMatrix4x4 riv_m = ctx.model;
        riv_m.translate(center + n * (thick * 0.55f));
        riv_m.scale(R * 0.055f);
        submitter.mesh(getUnitSphere(), riv_m, m_config.bronze_color * 1.25f, nullptr, 1.0f);
      }
    }
  };

  side(-1.0f);
  side(+1.0f);
}

// -------------------------------------------------------------------------
// NASAL GUARD: straight bar from brow to mid-nose
void CarthageLightHelmetRenderer::render_nasal(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  QVector3D top = headPoint(QVector3D(0.0f, 0.60f, 0.80f));
  QVector3D bot = headPoint(QVector3D(0.0f, 0.05f, 0.92f));
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, bot, top, R * 0.11f),
                 m_config.bronze_color * 0.98f, nullptr, 0.9f);

  if (m_config.detail_level >= 2) {
    // Small T-joint at the top
    QVector3D left = top + head.right * (R * 0.18f);
    QVector3D right = top - head.right * (R * 0.18f);
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, left, right, R * 0.055f),
                   m_config.bronze_color * 1.05f, nullptr, 0.92f);
  }
}

// -------------------------------------------------------------------------
// CREST MOUNT: low bronze saddle + optional plume strands
void CarthageLightHelmetRenderer::render_crest_mount(const DrawContext &ctx,
                                                     const AttachmentFrame &head,
                                                     ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  QVector3D base_front = headPoint(QVector3D(0.0f, 0.95f, 0.48f));
  QVector3D base_back  = headPoint(QVector3D(0.0f, 0.98f, -0.50f));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, base_front, base_back, R * 0.09f),
                 m_config.bronze_color * 1.12f, nullptr, 0.95f);

  // Optional plume (kept subtle so it doesn't read as a cap)
  QVector3D crest_color(0.7f, 0.12f, 0.16f);
  int strands = (m_config.detail_level >= 2) ? 10 : 6;
  for (int i = 0; i < strands; ++i) {
    float t = (float)i / std::max(1, strands - 1);
    QVector3D base = base_front * (1.0f - t) + base_back * t;
    base += head.up * (R * 0.05f);
    float curve = std::sin(t * std::numbers::pi_v<float>) * 0.28f;
    QVector3D tip = base + head.up * (R * (0.22f - 0.10f * t)) + head.forward * (R * (-0.42f - curve));
    float spread = (float((i % 3) - 1)) * R * 0.07f;
    tip += head.right * spread;
    QVector3D col = crest_color * (0.9f + (i % 2) * 0.18f);
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, base, tip, R * 0.032f),
                   col, nullptr, 0.65f);
  }
}

// -------------------------------------------------------------------------
// RIVETS: around brow and along the ridge
void CarthageLightHelmetRenderer::render_rivets(const DrawContext &ctx,
                                                const AttachmentFrame &head,
                                                ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  QVector3D col = m_config.bronze_color * 1.25f;

  // Around the brow band
  int n = 14;
  for (int i = 0; i < n; ++i) {
    float a = (float)i / n * 2.0f * std::numbers::pi_v<float>;
    float ca = std::cos(a), sa = std::sin(a);
    QVector3D pos = headPoint(QVector3D(0.95f * ca, 0.49f, 0.95f * sa));
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(R * 0.055f);
    submitter.mesh(getUnitSphere(), m, col, nullptr, 1.0f);
  }

  // Along the longitudinal ridge
  int r = 6;
  for (int i = 0; i < r; ++i) {
    float t = (float)i / (r - 1);
    QVector3D p = headPoint(QVector3D(0.0f, 1.02f - 0.02f * t, 0.46f - 1.0f * t));
    QMatrix4x4 m = ctx.model;
    m.translate(p);
    m.scale(R * 0.045f);
    submitter.mesh(getUnitSphere(), m, col, nullptr, 1.0f);
  }
}

} // namespace Render::GL

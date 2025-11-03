#include "carthage_light_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../gl/backend.h"
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

// --- Small helpers ---------------------------------------------------------
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

static inline void submit_spike(ISubmitter &submitter,
                                const DrawContext &ctx,
                                const QVector3D &base,
                                const QVector3D &dir,
                                float length,
                                float base_radius,
                                const QVector3D &color,
                                float roughness) {
  QVector3D d = dir;
  if (d.lengthSquared() < 1e-5f) d = QVector3D(0, 1, 0);
  d.normalize();
  QVector3D tip = base + d * length;
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, base, tip, base_radius),
                 color, nullptr, roughness);
  QMatrix4x4 m; m = ctx.model; m.translate(tip); m.scale(base_radius * 1.1f);
  submitter.mesh(getUnitSphere(), m, color * 1.05f, nullptr, roughness);
}

void CarthageLightHelmetRenderer::render(
    const DrawContext &ctx, const BodyFrames &frames,
    const HumanoidPalette &palette, const HumanoidAnimationContext &anim,
    ISubmitter &submitter) {
  (void)anim; (void)palette;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) return;

  // EXTREME silhouette
  render_bowl(ctx, head, submitter);         // taller, ribbed cone + spike
  render_brim(ctx, head, submitter);         // two-piece visor blades
  render_cheek_guards(ctx, head, submitter); // winged cheek fins + fangs

  if (m_config.has_nasal_guard) {
    render_nasal_guard(ctx, head, submitter); // heavy T-nasal + grille
  }
  if (m_config.has_crest) {
    render_crest(ctx, head, submitter);       // massive transverse plume
  }
  if (m_config.detail_level >= 2) {
    render_rivets(ctx, head, submitter);      // bands + dorsal spines
  }
}

// -------------------------------------------------------------------------
// BOWL: Simple bronze sphere - shader creates hammered detail, rivets, patina
void CarthageLightHelmetRenderer::render_bowl(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  // Single helmet sphere - shader adds all detail
  QVector3D c0 = headPoint(QVector3D(0.0f, 0.74f, 0.02f));
  QMatrix4x4 m0 = ctx.model; 
  m0.translate(c0); 
  m0.scale(R * 1.18f, R * 1.22f, R * 1.14f);
  
  // Bronze color that shader detects (isBronze = true)
  submitter.mesh(getUnitSphere(), m0, m_config.bronze_color, nullptr, 0.9f);
}

// -------------------------------------------------------------------------
// BRIM: split visor blades (left+right) that project forward (not a brim)
void CarthageLightHelmetRenderer::render_brim(const DrawContext &ctx,
                                              const AttachmentFrame &head,
                                              ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  auto blade = [&](float sx) {
    QVector3D center = headPoint(QVector3D(0.42f * sx, 0.58f, 0.83f));
    QVector3D nrm = (head.forward * 0.90f - head.right * sx * 0.25f - head.up * 0.15f).normalized();
    submit_disk(submitter, ctx, center, nrm, R * 0.56f, R * 0.08f,
                m_config.bronze_color * 1.02f, 0.92f);
  };
  blade(-1.0f);
  blade(+1.0f);

  // Bridge above nasal connecting blades (reinforced look)
  QVector3D L = headPoint(QVector3D(-0.48f, 0.66f, 0.83f));
  QVector3D Rg = headPoint(QVector3D( 0.48f, 0.66f, 0.83f));
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, L, Rg, R * 0.09f),
                 m_config.bronze_color * 1.08f, nullptr, 0.95f);
}

// -------------------------------------------------------------------------
// CHEEK GUARDS: winged fins + small downward spikes near the jaw
void CarthageLightHelmetRenderer::render_cheek_guards(const DrawContext &ctx,
                                                      const AttachmentFrame &head,
                                                      ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  auto side = [&](float sx) {
    // Four overlapping fins wrapping forward
    for (int j = 0; j < 4; ++j) {
      float v = (float)j / 3.0f;
      QVector3D center = headPoint(QVector3D(0.90f * sx, 0.54f - 0.24f * v, 0.22f + 0.18f * v));
      QVector3D n = (head.right * sx * 0.95f + head.forward * 0.25f - head.up * 0.08f).normalized();
      float r_plate = R * (0.38f - 0.05f * v);
      float thick   = R * 0.055f;
      submit_disk(submitter, ctx, center, n, r_plate, thick, m_config.bronze_color * 0.95f, 0.86f);

      if (m_config.detail_level >= 1) {
        // Rivet/hinge
        QMatrix4x4 riv_m = ctx.model; riv_m.translate(center + n * (thick * 0.55f)); riv_m.scale(R * 0.06f);
        submitter.mesh(getUnitSphere(), riv_m, m_config.bronze_color * 1.28f, nullptr, 1.0f);
      }
    }

    // Jaw "fang" spike
    QVector3D fang_base = headPoint(QVector3D(0.78f * sx, 0.20f, 0.36f));
    submit_spike(submitter, ctx, fang_base, -head.up * 0.8f + head.forward * 0.2f, R * 0.22f, R * 0.05f,
                 m_config.bronze_color * 1.1f, 0.95f);
  };

  side(-1.0f);
  side(+1.0f);
}

// -------------------------------------------------------------------------
// NASAL GUARD: heavy vertical with T-bar and a lower mouth grille
void CarthageLightHelmetRenderer::render_nasal_guard(const DrawContext &ctx,
                                                     const AttachmentFrame &head,
                                                     ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  QVector3D top = headPoint(QVector3D(0.0f, 0.70f, 0.80f));
  QVector3D bot = headPoint(QVector3D(0.0f, -0.04f, 0.95f));
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, bot, top, R * 0.14f),
                 m_config.bronze_color * 0.98f, nullptr, 0.9f);

  // T-bar
  QVector3D left = top + head.right * (R * 0.30f);
  QVector3D right = top - head.right * (R * 0.30f);
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, left, right, R * 0.07f),
                 m_config.bronze_color * 1.06f, nullptr, 0.93f);

  // Lower grille (3 bars)
  for (int i = 0; i < 3; ++i) {
    float yy = -0.02f - 0.08f * i;
    QVector3D gl = headPoint(QVector3D(-0.32f, yy, 0.96f));
    QVector3D gr = headPoint(QVector3D( 0.32f, yy, 0.96f));
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, gl, gr, R * 0.045f),
                   m_config.bronze_color * 1.02f, nullptr, 0.9f);
  }
}

// -------------------------------------------------------------------------
// CREST: transverse bronze saddle + massive arched plume (left-right)
void CarthageLightHelmetRenderer::render_crest(const DrawContext &ctx,
                                               const AttachmentFrame &head,
                                               ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };

  // Bronze saddle across left-right
  QVector3D left = headPoint(QVector3D(-0.95f, 1.02f, 0.02f));
  QVector3D right = headPoint(QVector3D( 0.95f, 1.02f, 0.02f));
  submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, left, right, R * 0.12f),
                 m_config.bronze_color * 1.12f, nullptr, 0.96f);

  // Colossal plume arcing upward+forward
  QVector3D crest_color(0.85f, 0.15f, 0.18f);
  int strands = (m_config.detail_level >= 2) ? 24 : 14;
  for (int i = 0; i < strands; ++i) {
    float t = (float)i / std::max(1, strands - 1);
    QVector3D base = left * (1.0f - t) + right * t;
    base += head.up * (R * 0.06f);
    float bow = std::sin(t * std::numbers::pi_v<float>) * 0.35f;
    QVector3D tip = base + head.up * (R * (0.55f - 0.15f * std::abs(0.5f - t))) + head.forward * (R * (0.30f + bow));
    float spread = (float((i % 5) - 2)) * R * 0.06f;
    tip += head.forward * 0.0f + head.right * 0.0f; // centered; forward already added
    QVector3D col = crest_color * (0.9f + 0.18f * ((i % 2) ? 1.0f : 0.0f));
    submitter.mesh(getUnitCylinder(), cylinderBetween(ctx.model, base, tip, R * 0.04f),
                   col, nullptr, 0.62f);
  }
}

// -------------------------------------------------------------------------
// DETAILS: rivet rings + dorsal spines for extra menace
void CarthageLightHelmetRenderer::render_rivets(const DrawContext &ctx,
                                                const AttachmentFrame &head,
                                                ISubmitter &submitter) {
  const float R = head.radius;
  auto headPoint = [&](const QVector3D &n) { return HumanoidRendererBase::frameLocalPosition(head, n); };
  QVector3D col = m_config.bronze_color * 1.25f;

  // Two rivet rings (rim and mid)
  int n1 = 16;
  for (int i = 0; i < n1; ++i) {
    float a = (float)i / n1 * 2.0f * std::numbers::pi_v<float>;
    float ca = std::cos(a), sa = std::sin(a);
    QVector3D p1 = headPoint(QVector3D(0.98f * ca, 0.48f, 0.98f * sa));
    QMatrix4x4 m = ctx.model; m.translate(p1); m.scale(R * 0.058f);
    submitter.mesh(getUnitSphere(), m, col, nullptr, 1.0f);
  }
  int n2 = 12;
  for (int i = 0; i < n2; ++i) {
    float a = (float)i / n2 * 2.0f * std::numbers::pi_v<float>;
    float ca = std::cos(a), sa = std::sin(a);
    QVector3D p2 = headPoint(QVector3D(0.82f * ca, 0.86f, 0.82f * sa - 0.03f));
    QMatrix4x4 m = ctx.model; m.translate(p2); m.scale(R * 0.05f);
    submitter.mesh(getUnitSphere(), m, col * 0.98f, nullptr, 1.0f);
  }

  // Dorsal spines from brow to back (really sells the "extreme")
  int sp = 7;
  for (int i = 0; i < sp; ++i) {
    float t = (float)i / (sp - 1);
    QVector3D base = headPoint(QVector3D(0.0f, 0.95f + 0.1f * (t - 0.5f), 0.50f - 1.05f * t));
    QVector3D dir  = (head.up * 0.85f - head.forward * 0.15f);
    submit_spike(submitter, ctx, base, dir, R * (0.22f + 0.06f * std::sin(t * std::numbers::pi_v<float>)), R * 0.045f,
                 m_config.bronze_color * 1.12f, 0.96f);
  }
}

} // namespace Render::GL

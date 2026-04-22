#include "carthage_heavy_helmet.h"

#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/style_palette.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

// Montefortino-style Punic heavy helmet.
//
// Visual design (learned from earlier flat-ring failure):
//   - Main bowl is a SPHERE (not cylinder) — naturally dome-shaped from all angles
//   - Narrow brim cylinder at the base (not a wide flat disc ring)
//   - Prominent thick red horsehair crest — the primary RTS silhouette cue
//   - Cheek guards, neck guard, nasal guard
//   - Montefortino knob on top
//
// Key proportions:
//   helm_r = head_r × 1.14  (wider than head)
//   dome sphere centre at +0.54 × head_r → encapsulates head fully
//   bronze multiplier (1.40, 1.15, 0.65) → bright gold, distinct from skin/leather

namespace {
// Dome sphere — radius = kHelmScale × head_r
// Roman helmet uses 1.18; Carthage heavy pushed to 1.65 so the dome
// is clearly visible as a distinct bronze shape from the RTS camera.
constexpr float kHelmScale     = 1.65F;
// Dome sphere centre: raised so the helmet sits above the skull.
// Dome top Y = kDomeCenterY + kHelmScale = 0.60 + 1.65 = 2.25 × head_r
constexpr float kDomeCenterY   = 0.60F;
// Narrow brim skirt at dome base (not a flat disc)
constexpr float kBrimBottomY   = -0.44F;
constexpr float kBrimTopY      = -0.06F;
constexpr float kBrimScale     = 1.08F;
// Neck guard
constexpr float kNeckTopY      = -0.14F;
constexpr float kNeckTopZ      = -1.06F;
constexpr float kNeckBottomY   = -0.36F;
constexpr float kNeckBottomZ   = -1.00F;
constexpr float kNeckScale     =  0.86F;
// Montefortino knob — sits just above dome top (dome top ≈ 2.25 × head_r)
constexpr float kKnobBaseY     =  2.28F;
constexpr float kKnobTopY      =  2.72F;
constexpr float kKnobR         =  0.08F;
constexpr float kKnobCapR      =  0.12F;
// Cheek guards — X positions scaled so they emerge from the dome edge,
// not from inside the sphere (dome edge at equator ≈ helm_r = 1.65 × R)
constexpr float kCheekTopX     =  1.50F;
constexpr float kCheekTopY     =  0.06F;
constexpr float kCheekTopZ     =  0.20F;
constexpr float kCheekBotX     =  1.22F;
constexpr float kCheekBotY     = -0.46F;
constexpr float kCheekBotZ     =  0.28F;
constexpr float kHelmetYOffset =  0.05F;
// Bronze: warm gold, clearly distinct from Carthage skin/leather
constexpr float kBronzeR       =  1.40F;
constexpr float kBronzeG       =  1.15F;
constexpr float kBronzeB       =  0.65F;
} // namespace

void CarthageHeavyHelmetRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void CarthageHeavyHelmetRenderer::submit(
    const CarthageHeavyHelmetConfig &config, const DrawContext &ctx,
    const BodyFrames &frames, const HumanoidPalette &palette,
    const HumanoidAnimationContext &anim, EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) {
    return;
  }

  auto head_point = [&](const QVector3D &n) -> QVector3D {
    QVector3D p = HumanoidRendererBase::frame_local_position(head, n);
    return p + head.up * kHelmetYOffset;
  };

  QVector3D const bronze = saturate_color(
      palette.metal * QVector3D(kBronzeR, kBronzeG, kBronzeB));
  QVector3D const bronze_dark  = bronze * 0.78F;
  QVector3D const bronze_light = saturate_color(bronze * 1.08F);

  QVector3D const crest_col = config.crest_color.lengthSquared() > 0.01F
                                  ? config.crest_color
                                  : QVector3D(0.80F, 0.08F, 0.08F);

  const float R      = head.radius;
  const float helm_r = R * kHelmScale;

  // ── Main dome — SPHERE, not cylinder ────────────────────────────────────
  // A sphere centred above the head origin produces a natural rounded
  // silhouette from every camera angle instead of a flat cylinder ring.
  QVector3D const dome_c = head_point({0.0F, kDomeCenterY, 0.0F});
  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, dome_c, helm_r),
       bronze, nullptr, 1.0F, 2});

  // ── Narrow brim ring at the base of the dome ─────────────────────────────
  // This is a low-profile ring, NOT a wide flat flying-saucer disc.
  QVector3D const brim_bot = head_point({0.0F, kBrimBottomY, 0.0F});
  QVector3D const brim_top = head_point({0.0F, kBrimTopY,    0.0F});
  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, brim_bot, brim_top, helm_r * kBrimScale),
       bronze_dark, nullptr, 1.0F, 2});

  // ── Neck guard ───────────────────────────────────────────────────────────
  if (config.has_neck_guard) {
    QVector3D const neck_top = head_point({0.0F, kNeckTopY,    kNeckTopZ});
    QVector3D const neck_bot = head_point({0.0F, kNeckBottomY, kNeckBottomZ});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, neck_bot, neck_top, helm_r * kNeckScale),
         bronze_dark * 0.92F, nullptr, 1.0F, 2});
  }

  // ── Montefortino knob ────────────────────────────────────────────────────
  QVector3D const knob_base = head_point({0.0F, kKnobBaseY, 0.0F});
  QVector3D const knob_top  = head_point({0.0F, kKnobTopY,  0.0F});
  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, knob_base, knob_top, R * kKnobR),
       bronze_light, nullptr, 1.0F, 2});
  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, knob_top, R * kKnobCapR),
       bronze_light, nullptr, 1.0F, 2});

  // ── Cheek guards ─────────────────────────────────────────────────────────
  if (config.has_cheek_guards) {
    for (float side : {-1.0F, 1.0F}) {
      QVector3D const g_top = head_point({kCheekTopX * side, kCheekTopY, kCheekTopZ});
      QVector3D const g_bot = head_point({kCheekBotX * side, kCheekBotY, kCheekBotZ});
      batch.meshes.push_back(
          {get_unit_cylinder(), nullptr,
           cylinder_between(ctx.model, g_top, g_bot, R * 0.22F),
           bronze_dark, nullptr, 1.0F, 2});
    }
  }

  // ── Nasal guard ───────────────────────────────────────────────────────────
  if (config.has_face_plate) {
    QVector3D const n_top = head_point({0.0F,  0.58F, 0.70F});
    QVector3D const n_bot = head_point({0.0F, -0.06F, 0.90F});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, n_bot, n_top, R * 0.10F),
         bronze, nullptr, 1.0F, 2});
  }

  // ── Prominent horsehair crest ────────────────────────────────────────────
  // This is the primary visual signature of the Montefortino from RTS view.
  // Thick strands (0.052 × R) and bright crimson make it pop over the bronze dome.
  if (config.has_hair_crest) {
    QVector3D const crest_front = head_point({0.0F, 2.64F,  0.28F});
    QVector3D const crest_back  = head_point({0.0F, 2.04F, -0.58F});
    // Crest rail along the dome ridge
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, crest_front, crest_back, R * 0.044F),
         bronze_light, nullptr, 1.0F, 2});

    // Thick hair strands — clearly visible at RTS camera distance
    const int strands = 12;
    for (int i = 0; i < strands; ++i) {
      float const t   = static_cast<float>(i) / static_cast<float>(strands - 1);
      float const lat = (static_cast<float>(i % 3) - 1.0F) * R * 0.036F;
      QVector3D const base = crest_front * (1.0F - t) + crest_back * t;
      float const lift  = 0.70F - 0.34F * std::abs(t - 0.42F);
      float const droop = 0.16F * t;
      QVector3D const tip = base
          + head.up      * (R * lift)
          - head.forward * (R * droop)
          + head.right   * lat;
      batch.meshes.push_back(
          {get_unit_cylinder(), nullptr,
           cylinder_between(ctx.model, base, tip, R * 0.052F),
           crest_col * (0.88F + 0.12F * static_cast<float>(i % 2)),
           nullptr, 0.92F, 0});
    }
  }

  // ── Decorative rivets ────────────────────────────────────────────────────
  if (config.detail_level > 0) {
    for (float ax : {-0.52F, -0.26F, 0.0F, 0.26F, 0.52F}) {
      batch.meshes.push_back(
          {get_unit_sphere(), nullptr,
           sphere_at(ctx.model, head_point({ax, 0.28F, 0.64F}), R * 0.05F),
           bronze_light, nullptr, 1.0F, 2});
    }
  }
}

} // namespace Render::GL

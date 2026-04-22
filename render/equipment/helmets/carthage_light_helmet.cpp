#include "carthage_light_helmet.h"

#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/style_palette.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

// Phrygian / Chalcidian-style Punic light helmet.
//
// Visual design (rebuilt after the flat-ring failure):
//   - Main bowl is a SPHERE — dome shape from all camera angles
//   - Low-profile narrow brim ring (not a wide flat disc)
//   - Tall Phrygian cone (1.05 × helm_r high) with wide base and forward lean
//     — the primary visual identity cue from RTS camera
//   - Nasal guard, cheek guards, neck guard, hair crest
//
// Key proportions:
//   helm_r = head_r × 1.10
//   dome sphere centre at +0.50 × head_r → encapsulates head
//   Phrygian cone height = 1.05 × helm_r, base radius = 0.30 × helm_r

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

namespace {
// Dome sphere — radius = kHelmScale × head_r
// Pushed to 1.55 so the helmet is clearly visible from the RTS camera.
constexpr float kHelmScale    = 1.55F;
constexpr float kDomeCenterY  = 0.58F;
// Narrow brim
constexpr float kBrimBottomY  = -0.38F;
constexpr float kBrimTopY     = -0.05F;
constexpr float kBrimScale    = 1.06F;
// Phrygian cone — tall and wide (mounted above dome top)
// kDomeTopY = kDomeCenterY + kHelmScale ≈ 2.13 × head_r (constexpr below)
constexpr float kConeHeight   = 1.05F;   // mult of helm_r above dome top
constexpr float kConeFwdLean  = 0.30F;   // forward tilt (× helm_r)
constexpr float kConeBaseR    = 0.30F;   // cone base radius (× helm_r)
constexpr float kConeTipR     = 0.10F;   // sphere at cone tip (× helm_r)
// Dome top Y in head_r units (used for cone base placement)
constexpr float kDomeTopY     = kDomeCenterY + kHelmScale; // ≈ 2.13
constexpr float kYOffset      = 0.05F;
// Bronze colour
constexpr float kBzR = 1.32F, kBzG = 1.08F, kBzB = 0.62F;
} // namespace

void CarthageLightHelmetRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void CarthageLightHelmetRenderer::submit(
    const CarthageLightHelmetConfig &config, const DrawContext &ctx,
    const BodyFrames &frames, const HumanoidPalette &palette,
    const HumanoidAnimationContext &anim, EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &head = frames.head;
  if (head.radius <= 0.0F) {
    return;
  }

  auto head_point = [&](const QVector3D &n) -> QVector3D {
    return HumanoidRendererBase::frame_local_position(head, n)
           + head.up * kYOffset;
  };

  QVector3D const bronze = saturate_color(
      palette.metal * QVector3D(kBzR, kBzG, kBzB));
  QVector3D const bronze_dark  = bronze * 0.78F;
  QVector3D const bronze_light = saturate_color(bronze * 1.08F);
  QVector3D const leather_col  = saturate_color(
      palette.leather_dark * QVector3D(0.96F, 0.88F, 0.78F));

  const float R       = head.radius;
  const float helm_r  = R * kHelmScale;

  // ── Main dome — SPHERE ────────────────────────────────────────────────
  QVector3D const dome_c = head_point({0.0F, kDomeCenterY, 0.0F});
  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, dome_c, helm_r),
       bronze, nullptr, 1.0F, 2});

  // ── Narrow brim ring ─────────────────────────────────────────────────
  QVector3D const brim_bot = head_point({0.0F, kBrimBottomY, 0.0F});
  QVector3D const brim_top = head_point({0.0F, kBrimTopY,    0.0F});
  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, brim_bot, brim_top, helm_r * kBrimScale),
       bronze_dark, nullptr, 1.0F, 2});

  // ── Leather liner band ────────────────────────────────────────────────
  QVector3D const liner_bot = head_point({0.0F, -0.20F, 0.0F});
  QVector3D const liner_top = head_point({0.0F, -0.02F, 0.0F});
  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, liner_bot, liner_top, helm_r * 0.75F),
       leather_col, nullptr, 1.0F, 2});

  // ── Phrygian cone ─────────────────────────────────────────────────────
  // TALL (1.05 × helm_r above the dome top) and wide base (0.30 × helm_r).
  // This is the main visual identifier of a Phrygian helmet from above.
  QVector3D const cone_base = head_point({0.0F, kDomeTopY,   0.0F});
  QVector3D const cone_tip  = cone_base
      + head.up      * (helm_r * kConeHeight)
      + head.forward * (helm_r * kConeFwdLean);
  batch.meshes.push_back(
      {get_unit_cone(), nullptr,
       cone_from_to(ctx.model, cone_base, cone_tip, helm_r * kConeBaseR),
       bronze_light, nullptr, 1.0F, 2});
  // Sphere cap at tip
  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, cone_tip, helm_r * kConeTipR),
       bronze_light, nullptr, 1.0F, 2});

  // ── Neck guard ────────────────────────────────────────────────────────
  QVector3D const neck_top = head_point({0.0F, -0.12F, -1.00F});
  QVector3D const neck_bot = head_point({0.0F, -0.30F, -0.96F});
  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, neck_bot, neck_top, helm_r * 0.88F),
       bronze_dark * 0.96F, nullptr, 1.0F, 2});

  // ── Nasal guard ───────────────────────────────────────────────────────
  if (config.has_nasal_guard) {
    QVector3D const n_top = head_point({0.0F,  0.56F, 0.72F});
    QVector3D const n_bot = head_point({0.0F, -0.08F, 0.90F});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, n_bot, n_top, R * 0.09F),
         bronze, nullptr, 1.0F, 2});
  }

  // ── Cheek guards ─────────────────────────────────────────────────────
  for (float side : {-1.0F, 1.0F}) {
    QVector3D const g_top = head_point({1.44F * side,  0.06F, 0.20F});
    QVector3D const g_mid = head_point({1.48F * side, -0.18F, 0.28F});
    QVector3D const g_bot = head_point({1.28F * side, -0.38F, 0.36F});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, g_top, g_mid, R * 0.19F),
         bronze_dark, nullptr, 1.0F, 2});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, g_mid, g_bot, R * 0.16F),
         bronze_dark * 0.94F, nullptr, 1.0F, 2});
  }

  // ── Horsehair crest ───────────────────────────────────────────────────
  if (config.has_crest) {
    QVector3D const crest_col(0.78F, 0.08F, 0.08F);
    QVector3D const crest_front = head_point({0.0F, 2.22F,  0.08F});
    QVector3D const crest_back  = head_point({0.0F, 2.06F, -0.24F});
    batch.meshes.push_back(
        {get_unit_cylinder(), nullptr,
         cylinder_between(ctx.model, crest_front, crest_back, R * 0.044F),
         bronze_light, nullptr, 1.0F, 2});
    const int strands = (config.detail_level >= 2) ? 14 : 8;
    for (int i = 0; i < strands; ++i) {
      float const t = (strands > 1)
          ? static_cast<float>(i) / static_cast<float>(strands - 1)
          : 0.0F;
      QVector3D const base = crest_front * (1.0F - t) + crest_back * t
                             + head.up * (R * 0.03F);
      float const lat = (static_cast<float>(i % 3) - 1.0F) * R * 0.022F;
      QVector3D const tip = base
          + head.up      * (R * (0.44F - 0.12F * std::abs(t - 0.5F)))
          - head.forward * (R * (0.10F + 0.20F * t))
          + head.right   * lat;
      batch.meshes.push_back(
          {get_unit_cylinder(), nullptr,
           cylinder_between(ctx.model, base, tip, R * 0.048F),
           crest_col * (0.90F + 0.10F * static_cast<float>(i % 2)),
           nullptr, 0.90F, 0});
    }
  }

  // ── Rivets ────────────────────────────────────────────────────────────
  if (config.detail_level > 0) {
    for (float ax : {-0.50F, -0.25F, 0.0F, 0.25F, 0.50F}) {
      batch.meshes.push_back(
          {get_unit_sphere(), nullptr,
           sphere_at(ctx.model, head_point({ax, 0.46F, 0.64F}), R * 0.05F),
           bronze_light, nullptr, 1.0F, 2});
    }
  }
}

// ── Sub-method stubs (kept for ABI / test compatibility) ─────────────────────
void CarthageLightHelmetRenderer::render_bowl(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}
void CarthageLightHelmetRenderer::render_brim(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}
void CarthageLightHelmetRenderer::render_cheek_guards(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}
void CarthageLightHelmetRenderer::render_nasal_guard(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}
void CarthageLightHelmetRenderer::render_crest(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}
void CarthageLightHelmetRenderer::render_rivets(
    const CarthageLightHelmetConfig &, const DrawContext &,
    const AttachmentFrame &, EquipmentBatch &) {}

} // namespace Render::GL

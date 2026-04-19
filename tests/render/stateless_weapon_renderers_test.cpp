// Phase D — stateless weapon renderer guarantees.
//
// Verifies that the four refactored weapon renderers (SwordRenderer,
// ShieldRenderer, SpearRenderer, BowRenderer) behave as pure mesh
// providers: a single shared instance, invoked through the static
// `submit(config, ...)` API, must produce different draw batches for
// different configs in the same frame, must not be perturbed by
// prior `submit()` calls, and must produce results identical to the
// equivalent legacy `render()` virtual when the legacy entry point is
// fed the same base config.

#include "render/equipment/weapons/bow_renderer.h"
#include "render/equipment/weapons/shield_renderer.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"

#include "render/entity/registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/palette.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

using Render::GL::BodyFrames;
using Render::GL::BowRenderConfig;
using Render::GL::BowRenderer;
using Render::GL::DrawContext;
using Render::GL::EquipmentBatch;
using Render::GL::EquipmentMeshPrim;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidPalette;
using Render::GL::ShieldRenderConfig;
using Render::GL::ShieldRenderer;
using Render::GL::SpearRenderConfig;
using Render::GL::SpearRenderer;
using Render::GL::SwordRenderConfig;
using Render::GL::SwordRenderer;

auto make_frames() -> BodyFrames {
  BodyFrames f{};
  f.hand_r.origin = {0.30F, 1.10F, 0.20F};
  f.hand_r.right = {1.0F, 0.0F, 0.0F};
  f.hand_r.up = {0.0F, 1.0F, 0.0F};
  f.hand_r.forward = {0.0F, 0.0F, 1.0F};
  f.hand_l.origin = {-0.30F, 1.10F, 0.20F};
  f.hand_l.right = {-1.0F, 0.0F, 0.0F};
  f.hand_l.up = {0.0F, 1.0F, 0.0F};
  f.hand_l.forward = {0.0F, 0.0F, 1.0F};
  return f;
}

auto make_anim() -> HumanoidAnimationContext {
  HumanoidAnimationContext anim{};
  anim.inputs.time = 0.0F;
  anim.inputs.is_attacking = false;
  anim.inputs.is_melee = false;
  return anim;
}

auto make_palette() -> HumanoidPalette {
  HumanoidPalette p;
  return p;
}

auto make_ctx() -> DrawContext {
  DrawContext ctx{};
  ctx.model.setToIdentity();
  return ctx;
}

// Concatenate every batch primitive into a deterministic byte
// signature. Two batches with the same colours / transforms / radii /
// material ids hash to the same signature.
auto hash_batch(const EquipmentBatch &b) -> std::size_t {
  std::size_t h = 0;
  auto mix = [&h](std::size_t v) {
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6U) + (h >> 2U);
  };
  auto mix_f = [&mix](float v) {
    std::uint32_t u{};
    std::memcpy(&u, &v, sizeof(u));
    mix(u);
  };
  auto mix_v = [&mix_f](const QVector3D &v) {
    mix_f(v.x());
    mix_f(v.y());
    mix_f(v.z());
  };
  auto mix_m = [&mix_f](const QMatrix4x4 &m) {
    for (int i = 0; i < 16; ++i) {
      mix_f(m.constData()[i]);
    }
  };
  mix(b.meshes.size());
  for (const auto &p : b.meshes) {
    mix(reinterpret_cast<std::uintptr_t>(p.mesh));
    mix_m(p.model);
    mix_v(p.color);
    mix_f(p.alpha);
    mix(static_cast<std::size_t>(p.material_id));
  }
  mix(b.cylinders.size());
  for (const auto &c : b.cylinders) {
    mix_v(c.start);
    mix_v(c.end);
    mix_f(c.radius);
    mix_v(c.color);
    mix_f(c.alpha);
  }
  return h;
}

template <class Renderer, class Cfg>
void run_stateless_battery(Renderer &renderer, const Cfg &cfg_a,
                           const Cfg &cfg_b) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  // 1. Same instance, two different configs ⇒ two different batches.
  EquipmentBatch a;
  EquipmentBatch b;
  Renderer::submit(cfg_a, ctx, frames, palette, anim, a);
  Renderer::submit(cfg_b, ctx, frames, palette, anim, b);
  EXPECT_FALSE(a.meshes.empty());
  EXPECT_FALSE(b.meshes.empty());
  EXPECT_NE(hash_batch(a), hash_batch(b))
      << "two configs should yield distinct draw batches";

  // 2. Re-submitting cfg_a after cfg_b reproduces the original a hash
  //    bit-for-bit ⇒ no leftover instance state from the cfg_b call.
  EquipmentBatch a2;
  Renderer::submit(cfg_a, ctx, frames, palette, anim, a2);
  EXPECT_EQ(hash_batch(a), hash_batch(a2))
      << "submit() must not retain any per-call state";

  // 3. Legacy render() fed the same config (via base) matches submit().
  Renderer legacy{cfg_a};
  EquipmentBatch legacy_batch;
  legacy.render(ctx, frames, palette, anim, legacy_batch);
  EXPECT_EQ(hash_batch(a), hash_batch(legacy_batch))
      << "legacy render() must forward to submit() without divergence";
}

} // namespace

TEST(StatelessWeaponRenderers, SwordSubmitIsStateless) {
  SwordRenderer renderer{};
  SwordRenderConfig a;
  a.metal_color = {0.7F, 0.7F, 0.8F};
  a.sword_length = 0.80F;
  SwordRenderConfig b;
  b.metal_color = {0.3F, 0.6F, 0.4F};
  b.sword_length = 1.10F;
  b.guard_half_width = 0.18F;
  run_stateless_battery<SwordRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, ShieldSubmitIsStateless) {
  ShieldRenderer renderer{};
  ShieldRenderConfig a;
  a.shield_color = {0.7F, 0.3F, 0.2F};
  a.shield_radius = 0.18F;
  ShieldRenderConfig b;
  b.shield_color = {0.2F, 0.4F, 0.6F};
  b.shield_radius = 0.24F;
  b.shield_aspect = 1.4F;
  b.has_cross_decal = true;
  run_stateless_battery<ShieldRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, SpearSubmitIsStateless) {
  SpearRenderer renderer{};
  SpearRenderConfig a;
  a.spear_length = 1.10F;
  a.shaft_radius = 0.020F;
  SpearRenderConfig b;
  b.spear_length = 1.45F;
  b.shaft_radius = 0.024F;
  b.spearhead_color = {0.9F, 0.85F, 0.7F};
  run_stateless_battery<SpearRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, BowSubmitIsStateless) {
  BowRenderer renderer{};
  BowRenderConfig a;
  a.bow_top_y = 1.4F;
  a.bow_bot_y = 0.6F;
  a.bow_depth = 0.22F;
  BowRenderConfig b;
  b.bow_top_y = 1.6F;
  b.bow_bot_y = 0.4F;
  b.bow_depth = 0.30F;
  b.bow_curve_factor = 1.3F;
  b.fletching_color = {0.9F, 0.2F, 0.1F};
  run_stateless_battery<BowRenderer>(renderer, a, b);
}

// Visual-parity regression: a ShieldRenderer instance carrying a
// non-default base_config must produce the exact same batch when
// invoked via either submit(base_config, ...) or render(...).
TEST(StatelessWeaponRenderers, BaseConfigPreservedAcrossSubmitPaths) {
  ShieldRenderConfig base;
  base.shield_color = {0.4F, 0.5F, 0.7F};
  base.shield_radius = 0.21F;
  base.has_cross_decal = true;
  ShieldRenderer renderer{base};

  EXPECT_EQ(renderer.base_config().shield_radius, base.shield_radius);

  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  ShieldRenderer::submit(renderer.base_config(), ctx, frames, palette, anim,
                         via_submit);
  renderer.render(ctx, frames, palette, anim, via_render);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

// Visual-parity regression: BowRenderer's default-bow-vs-Roman-bow
// difference must survive the stateless API. When two distinct
// instances carry distinct defaults, their static submit() calls
// using each instance's base_config() must produce the historical
// legacy outputs.
TEST(StatelessWeaponRenderers, BowDefaultsHonouredViaBaseConfig) {
  BowRenderConfig roman_defaults;
  roman_defaults.bow_depth = 0.22F;
  roman_defaults.bow_curve_factor = 1.0F;
  roman_defaults.bow_height_scale = 1.0F;
  BowRenderer roman{roman_defaults};

  BowRenderConfig carthage_defaults;
  carthage_defaults.bow_depth = 0.28F;
  carthage_defaults.bow_curve_factor = 1.2F;
  carthage_defaults.bow_height_scale = 0.95F;
  BowRenderer carthage{carthage_defaults};

  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch r_legacy;
  EquipmentBatch c_legacy;
  roman.render(ctx, frames, palette, anim, r_legacy);
  carthage.render(ctx, frames, palette, anim, c_legacy);

  EquipmentBatch r_submit;
  EquipmentBatch c_submit;
  BowRenderer::submit(roman.base_config(), ctx, frames, palette, anim,
                      r_submit);
  BowRenderer::submit(carthage.base_config(), ctx, frames, palette, anim,
                      c_submit);

  EXPECT_EQ(hash_batch(r_legacy), hash_batch(r_submit));
  EXPECT_EQ(hash_batch(c_legacy), hash_batch(c_submit));
  EXPECT_NE(hash_batch(r_submit), hash_batch(c_submit))
      << "different ctor defaults must yield different draw batches";
}

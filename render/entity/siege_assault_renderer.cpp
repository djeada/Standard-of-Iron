#include "siege_assault_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <unordered_map>

#include "../entity_appearance.h"
#include "game/core/component_combat.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "math/math_utils.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"
#include "siege_renderer_common.h"

namespace Render::GL {
namespace {

constexpr float k_ram_wreck_sink_depth = 1.4F;
constexpr float k_tower_wreck_sink_depth = 2.6F;
constexpr float k_assault_crew_sink_depth = 0.6F;

using Render::Geom::clamp_vec_01;
using Render::Geom::cylinder_between;

constexpr float k_pi = std::numbers::pi_v<float>;

struct Palette {
  QVector3D wood_frame{0.45F, 0.32F, 0.18F};
  QVector3D wood_dark{0.30F, 0.21F, 0.12F};
  QVector3D wood_light{0.58F, 0.43F, 0.27F};
  QVector3D iron{0.30F, 0.29F, 0.28F};
  QVector3D bronze{0.66F, 0.48F, 0.24F};
  QVector3D hide{0.58F, 0.44F, 0.30F};
  QVector3D hide_wet{0.43F, 0.31F, 0.21F};
  QVector3D rope{0.52F, 0.43F, 0.28F};
  QVector3D wicker{0.55F, 0.45F, 0.28F};
  QVector3D team{0.8F, 0.2F, 0.2F};
};

auto palette_for(bool carthage, const QVector3D& team) -> Palette {
  Palette p;
  if (carthage) {
    p.wood_frame = {0.38F, 0.26F, 0.17F};
    p.wood_dark = {0.25F, 0.17F, 0.11F};
    p.wood_light = {0.52F, 0.37F, 0.24F};
    p.bronze = {0.70F, 0.50F, 0.22F};
    p.hide = {0.60F, 0.44F, 0.33F};
    p.hide_wet = {0.45F, 0.31F, 0.24F};
    p.wicker = {0.60F, 0.47F, 0.30F};
  }
  p.team = clamp_vec_01(team);
  return p;
}

struct Painter {
  ISubmitter& out;
  Mesh* cube;
  Texture* white;
  const QMatrix4x4& model;

  void box(const QVector3D& pos, const QVector3D& size, const QVector3D& color) const {
    QMatrix4x4 m = model;
    m.translate(pos);
    m.scale(size);
    out.mesh(cube, m, color, white, 1.0F);
  }
  void tilted_box(const QVector3D& pos,
                  const QVector3D& size,
                  const QVector3D& axis,
                  float degrees,
                  const QVector3D& color) const {
    QMatrix4x4 m = model;
    m.translate(pos);
    m.rotate(degrees, axis);
    m.scale(size);
    out.mesh(cube, m, color, white, 1.0F);
  }
  void
  cyl(const QVector3D& a, const QVector3D& b, float r, const QVector3D& color) const {
    out.mesh(
        get_unit_cylinder(), model * cylinder_between(a, b, r), color, white, 1.0F);
  }
  void cone(const QVector3D& base,
            const QVector3D& tip,
            float r,
            const QVector3D& color) const {
    out.mesh(
        get_unit_cone(), model * cylinder_between(base, tip, r), color, white, 1.0F);
  }
};

auto hide_tone(const Palette& c, int index) -> QVector3D {
  static constexpr std::array<float, 5> k_shade{0.0F, 0.55F, 0.22F, 0.85F, 0.38F};
  float const t = k_shade[static_cast<std::size_t>(index) % k_shade.size()];
  return c.hide * (1.0F - t) + c.hide_wet * t;
}

auto in_melee(const DrawContext& ctx) -> bool {
  if (ctx.entity == nullptr || ctx.world == nullptr) {
    return false;
  }
  const auto* attack =
      ctx.world->try_get<Engine::Core::AttackComponent>(ctx.entity->get_id());
  return attack != nullptr && attack->in_melee_lock;
}

constexpr float k_ram_stroke_seconds = 1.0F;

auto ram_stroke(float phase) -> float {
  constexpr float k_draw_end = 0.62F;
  constexpr float k_strike_end = 0.76F;
  if (phase < k_draw_end) {
    float const t = phase / k_draw_end;
    return -0.34F * t * t * (3.0F - 2.0F * t);
  }
  if (phase < k_strike_end) {
    float const t = (phase - k_draw_end) / (k_strike_end - k_draw_end);
    return -0.34F + 0.58F * t * t;
  }
  float const t = (phase - k_strike_end) / (1.0F - k_strike_end);
  return 0.24F * (1.0F - t * t * (3.0F - 2.0F * t));
}

void draw_wheels(const DrawContext& p,
                 ISubmitter& out,
                 Texture* white,
                 const Palette& c,
                 const SiegeMotion& motion,
                 float half_track,
                 float radius,
                 std::initializer_list<float> axles) {
  for (float z : axles) {
    draw_siege_wheel(out,
                     white,
                     p.model,
                     {-half_track, radius, z},
                     radius,
                     -motion.right_roll,
                     c.wood_light,
                     c.iron,
                     c.bronze);
    draw_siege_wheel(out,
                     white,
                     p.model,
                     {half_track, radius, z},
                     radius,
                     -motion.left_roll,
                     c.wood_light,
                     c.iron,
                     c.bronze);
    out.mesh(get_unit_cylinder(),
             p.model * cylinder_between(
                           {-half_track, radius, z}, {half_track, radius, z}, 0.035F),
             c.iron,
             white,
             1.0F);
  }
}

void draw_pennant(const Painter& g,
                  const Palette& c,
                  const QVector3D& foot,
                  float height,
                  float time,
                  float seed) {
  QVector3D const top = foot + QVector3D(0.0F, height, 0.0F);
  g.cyl(foot, top, 0.014F, c.wood_dark);
  g.cone(top, top + QVector3D(0.0F, 0.06F, 0.0F), 0.02F, c.bronze);
  for (int strip = 0; strip < 6; ++strip) {
    float const along = 0.035F + static_cast<float>(strip) * 0.05F;
    float const wave = std::sin(time * 3.1F + seed - along * 14.0F) * along * 0.22F;
    float const droop = along * 0.10F;
    g.box(top + QVector3D(wave, -0.07F - droop, -along),
          {0.006F, 0.055F - along * 0.06F, 0.027F},
          strip % 2 == 0 ? c.team : c.team * 0.86F);
  }
}

constexpr float k_ram_half_length = 1.05F;
constexpr float k_ram_half_width = 0.46F;
constexpr float k_ram_eave = 0.78F;
constexpr float k_ram_ridge = 1.30F;
constexpr float k_ram_beam_y = 0.56F;

void draw_ram_body(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const Palette& c,
                   const SiegeMotion& motion,
                   float stroke) {
  DrawContext ctx = p;
  ctx.model = siege_body_model(p, motion);
  const Painter g{out, unit, white, ctx.model};

  constexpr float L = k_ram_half_length;
  constexpr float W = k_ram_half_width;
  constexpr std::array<float, 4> k_bays{-0.95F, -0.32F, 0.32F, 0.95F};

  for (float side : {-1.0F, 1.0F}) {
    g.box({side * W, 0.30F, 0.0F}, {0.05F, 0.05F, L}, c.wood_frame);
    g.box({side * W, k_ram_eave, 0.0F}, {0.04F, 0.04F, L}, c.wood_dark);
    for (float z : k_bays) {
      g.box({side * W, (0.30F + k_ram_eave) * 0.5F, z},
            {0.038F, (k_ram_eave - 0.30F) * 0.5F, 0.038F},
            c.wood_dark);
      g.cyl({side * W, k_ram_eave, z}, {0.0F, k_ram_ridge, z}, 0.026F, c.wood_frame);
    }
    g.cyl({side * W, 0.36F, -0.95F},
          {side * W, k_ram_eave - 0.04F, -0.32F},
          0.018F,
          c.wood_frame);
  }
  for (float z : {-0.95F, 0.0F, 0.95F}) {
    g.box({0.0F, 0.28F, z}, {W, 0.035F, 0.04F}, c.wood_frame);
  }
  g.cyl({0.0F, k_ram_ridge, -L - 0.06F},
        {0.0F, k_ram_ridge, L + 0.06F},
        0.04F,
        c.wood_dark);

  float const slope_run = W + 0.06F;
  float const slope_rise = k_ram_ridge - k_ram_eave + 0.04F;
  float const slope_len = std::hypot(slope_run, slope_rise);
  float const slope_deg = std::atan2(slope_rise, slope_run) * 180.0F / k_pi;
  constexpr int k_strips = 5;
  for (float side : {-1.0F, 1.0F}) {
    for (int i = 0; i < k_strips; ++i) {
      float const z0 =
          -L - 0.08F +
          (2.0F * L + 0.16F) * static_cast<float>(i) / static_cast<float>(k_strips);
      float const zc = z0 + (L + 0.08F) / static_cast<float>(k_strips);
      g.tilted_box({side * slope_run * 0.5F,
                    (k_ram_eave - 0.04F + k_ram_ridge) * 0.5F + 0.03F,
                    zc},
                   {slope_len * 0.5F, 0.018F, (L + 0.08F) / k_strips + 0.02F},
                   {0.0F, 0.0F, 1.0F},
                   -side * slope_deg,
                   hide_tone(c, i + (side > 0.0F ? 2 : 0)));
    }
    for (float z : {-0.62F, 0.05F, 0.70F}) {
      g.tilted_box({side * slope_run * 0.5F,
                    (k_ram_eave - 0.04F + k_ram_ridge) * 0.5F + 0.055F,
                    z},
                   {slope_len * 0.5F + 0.01F, 0.008F, 0.012F},
                   {0.0F, 0.0F, 1.0F},
                   -side * slope_deg,
                   c.rope);
    }

    for (int bay = 0; bay < 3; ++bay) {
      float const za = k_bays[static_cast<std::size_t>(bay)];
      float const zb = k_bays[static_cast<std::size_t>(bay + 1)];
      g.box({side * (W + 0.045F), k_ram_eave - 0.16F, (za + zb) * 0.5F},
            {0.012F, 0.16F, (zb - za) * 0.5F - 0.03F},
            hide_tone(c, bay + 1));
    }
  }

  float const z_shift = stroke;
  QVector3D const tail{0.0F, k_ram_beam_y, -0.92F + z_shift};
  QVector3D const neck{0.0F, k_ram_beam_y, 1.10F + z_shift};
  g.cyl(tail, neck, 0.085F, c.wood_light);
  for (float z : {-0.70F, -0.05F, 0.60F}) {
    g.cyl({0.0F, k_ram_beam_y, z + z_shift - 0.025F},
          {0.0F, k_ram_beam_y, z + z_shift + 0.025F},
          0.094F,
          c.iron);
  }
  QVector3D const cap_end = neck + QVector3D(0.0F, 0.0F, 0.17F);
  g.cyl(neck, cap_end, 0.112F, c.iron);
  g.cone(cap_end, cap_end + QVector3D(0.0F, 0.0F, 0.11F), 0.112F, c.iron * 1.2F);
  for (float side : {-1.0F, 1.0F}) {
    g.cone(neck + QVector3D(side * 0.08F, 0.07F, 0.05F),
           neck + QVector3D(side * 0.17F, 0.02F, -0.10F),
           0.035F,
           c.iron * 0.9F);
  }
  for (float z : {-0.45F, 0.48F}) {
    for (float side : {-1.0F, 1.0F}) {
      g.cyl({side * 0.12F, k_ram_ridge - 0.04F, z},
            {0.0F, k_ram_beam_y + 0.08F, z + z_shift * 0.55F},
            0.011F,
            c.iron);
    }
  }

  draw_wheels(ctx, out, white, c, motion, W + 0.08F, 0.20F, {-0.62F, 0.62F});
  float const seed =
      p.entity != nullptr ? static_cast<float>(p.entity->get_id() % 29U) : 0.0F;
  draw_pennant(
      g, c, {-W + 0.04F, k_ram_eave, -L + 0.05F}, 0.80F, p.animation_time, seed);
}

constexpr float k_tower_base_half = 0.52F;
constexpr float k_tower_top_half = 0.44F;
constexpr float k_tower_chassis = 0.32F;
constexpr float k_tower_floor_one = 1.05F;
constexpr float k_tower_bridge_floor = 1.94F;
constexpr float k_tower_top = 2.62F;
constexpr float k_tower_parapet = 2.95F;
constexpr float k_tower_door_half = 0.32F;
constexpr float k_tower_bridge_length = 0.86F;

auto tower_half(float y) -> float {
  float const t =
      std::clamp((y - k_tower_chassis) / (k_tower_top - k_tower_chassis), 0.0F, 1.0F);
  return k_tower_base_half + (k_tower_top_half - k_tower_base_half) * t;
}

void clad_band(const Painter& g,
               const Palette& c,
               int face,
               float y0,
               float y1,
               float gap,
               int tone) {
  float const ym = (y0 + y1) * 0.5F;
  float const half = tower_half(ym) - 0.02F;
  float const out = tower_half(ym) + 0.012F;
  float const hh = (y1 - y0) * 0.5F;
  auto place = [&](float along_center, float along_half, int shade) {
    QVector3D pos;
    QVector3D size;
    switch (face) {
    case 0:
      pos = {along_center, ym, out};
      size = {along_half, hh, 0.012F};
      break;
    case 1:
      pos = {along_center, ym, -out};
      size = {along_half, hh, 0.012F};
      break;
    case 2:
      pos = {-out, ym, along_center};
      size = {0.012F, hh, along_half};
      break;
    default:
      pos = {out, ym, along_center};
      size = {0.012F, hh, along_half};
      break;
    }
    g.box(pos, size, hide_tone(c, shade));
  };
  if (gap <= 0.0F) {
    place(-half * 0.5F, half * 0.5F - 0.005F, tone);
    place(half * 0.5F, half * 0.5F - 0.005F, tone + 1);
  } else {
    float const side_half = (half - gap) * 0.5F;
    place(-gap - side_half, side_half, tone);
    place(gap + side_half, side_half, tone + 1);
  }

  for (float a : {-half * 0.5F, 0.0F, half * 0.5F}) {
    if (gap > 0.0F && std::abs(a) < gap) {
      continue;
    }
    QVector3D pos;
    QVector3D size;
    switch (face) {
    case 0:
      pos = {a, ym, out + 0.006F};
      size = {0.006F, hh, 0.006F};
      break;
    case 1:
      pos = {a, ym, -out - 0.006F};
      size = {0.006F, hh, 0.006F};
      break;
    case 2:
      pos = {-out - 0.006F, ym, a};
      size = {0.006F, hh, 0.006F};
      break;
    default:
      pos = {out + 0.006F, ym, a};
      size = {0.006F, hh, 0.006F};
      break;
    }
    g.box(pos, size, c.hide_wet * 0.7F);
  }
}

void draw_tower_body(const DrawContext& p,
                     ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const Palette& c,
                     const SiegeMotion& motion,
                     float ramp) {
  DrawContext ctx = p;
  ctx.model = siege_body_model(p, motion);
  const Painter g{out, unit, white, ctx.model};

  constexpr float B = k_tower_base_half;

  for (float side : {-1.0F, 1.0F}) {
    g.box({side * B, 0.28F, 0.0F}, {0.06F, 0.06F, B + 0.06F}, c.wood_frame);
    g.box({side * (B + 0.075F), 0.30F, 0.0F}, {0.012F, 0.07F, B + 0.04F}, c.wood_dark);
  }
  for (float z : {-B, 0.0F, B}) {
    g.box({0.0F, 0.28F, z}, {B, 0.045F, 0.045F}, c.wood_frame);
  }
  draw_wheels(ctx, out, white, c, motion, B + 0.11F, 0.22F, {-0.36F, 0.0F, 0.36F});

  for (float sx : {-1.0F, 1.0F}) {
    for (float sz : {-1.0F, 1.0F}) {
      g.cyl({sx * B, k_tower_chassis - 0.02F, sz * B},
            {sx * k_tower_top_half, k_tower_parapet, sz * k_tower_top_half},
            0.048F,
            c.wood_dark);
    }
  }
  for (float y : {k_tower_chassis + 0.02F,
                  k_tower_floor_one,
                  k_tower_bridge_floor,
                  k_tower_top}) {
    float const h = tower_half(y);
    for (float s : {-1.0F, 1.0F}) {
      g.box({0.0F, y, s * h}, {h, 0.032F, 0.032F}, c.wood_frame);
      g.box({s * h, y, 0.0F}, {0.032F, 0.032F, h}, c.wood_frame);
    }
    g.box({0.0F, y - 0.02F, 0.0F}, {h - 0.03F, 0.015F, h - 0.03F}, c.wood_light * 0.9F);
  }

  clad_band(g, c, 0, k_tower_chassis + 0.05F, k_tower_floor_one, 0.0F, 0);
  clad_band(g, c, 0, k_tower_floor_one, k_tower_bridge_floor, 0.0F, 2);
  clad_band(g, c, 0, k_tower_bridge_floor, k_tower_top, k_tower_door_half, 4);
  for (int face : {2, 3}) {
    clad_band(g, c, face, k_tower_chassis + 0.05F, k_tower_floor_one, 0.0F, 1 + face);
    clad_band(g, c, face, k_tower_floor_one, k_tower_bridge_floor, 0.0F, 3 + face);
    clad_band(g, c, face, k_tower_bridge_floor, k_tower_top, 0.0F, face);
  }
  clad_band(g, c, 1, k_tower_floor_one, k_tower_bridge_floor, 0.0F, 1);
  clad_band(g, c, 1, k_tower_bridge_floor, k_tower_top, 0.0F, 3);

  float const ladder_z = -B + 0.10F;
  for (float x : {0.12F, 0.32F}) {
    g.cyl({x, k_tower_chassis, ladder_z},
          {x, k_tower_floor_one, ladder_z + 0.08F},
          0.016F,
          c.wood_light);
  }
  for (int rung = 1; rung < 6; ++rung) {
    float const t = static_cast<float>(rung) / 6.0F;
    float const y = k_tower_chassis + (k_tower_floor_one - k_tower_chassis) * t;
    g.cyl({0.12F, y, ladder_z + 0.08F * t},
          {0.32F, y, ladder_z + 0.08F * t},
          0.011F,
          c.wood_light);
  }

  float const th = tower_half(k_tower_top) + 0.04F;
  g.box({0.0F, k_tower_top + 0.01F, 0.0F}, {th, 0.025F, th}, c.wood_frame);
  for (int side = 0; side < 4; ++side) {
    for (int m = 0; m < 3; ++m) {
      float const a = -th + th * (2.0F * static_cast<float>(m) + 1.0F) / 3.0F;
      float const height = (k_tower_parapet - k_tower_top) * 0.5F;
      QVector3D pos;
      QVector3D size;
      switch (side) {
      case 0:
        pos = {a, k_tower_top + height, th};
        size = {th / 3.0F - 0.035F, height, 0.022F};
        break;
      case 1:
        pos = {a, k_tower_top + height, -th};
        size = {th / 3.0F - 0.035F, height, 0.022F};
        break;
      case 2:
        pos = {-th, k_tower_top + height, a};
        size = {0.022F, height, th / 3.0F - 0.035F};
        break;
      default:
        pos = {th, k_tower_top + height, a};
        size = {0.022F, height, th / 3.0F - 0.035F};
        break;
      }
      g.box(pos, size, (m + side) % 2 == 0 ? c.wicker : c.wicker * 0.86F);
    }
    float const rail = k_tower_top + 0.13F;
    if (side < 2) {
      g.box({0.0F, rail, side == 0 ? th : -th}, {th, 0.018F, 0.026F}, c.wood_dark);
    } else {
      g.box({side == 2 ? -th : th, rail, 0.0F}, {0.026F, 0.018F, th}, c.wood_dark);
    }
  }
  float const seed =
      p.entity != nullptr ? static_cast<float>(p.entity->get_id() % 31U) : 0.0F;
  draw_pennant(g,
               c,
               {-th + 0.03F, k_tower_parapet - 0.05F, -th + 0.03F},
               0.95F,
               p.animation_time,
               seed);

  float const hinge_z = tower_half(k_tower_bridge_floor) + 0.02F;
  float const angle = -90.0F * (1.0F - std::clamp(ramp, 0.0F, 1.0F));
  QMatrix4x4 bridge = ctx.model;
  bridge.translate(0.0F, k_tower_bridge_floor, hinge_z);
  bridge.rotate(angle, 1.0F, 0.0F, 0.0F);
  const Painter b{out, unit, white, bridge};
  constexpr float half_len = k_tower_bridge_length * 0.5F;
  b.box({0.0F, 0.0F, half_len},
        {k_tower_door_half - 0.02F, 0.024F, half_len},
        c.wood_light);
  for (float x : {-0.20F, -0.07F, 0.07F, 0.20F}) {
    b.box(
        {x, 0.026F, half_len}, {0.004F, 0.004F, half_len - 0.01F}, c.wood_dark * 0.9F);
  }
  for (float z : {0.12F, 0.45F, 0.78F}) {
    b.box({0.0F, -0.03F, z}, {k_tower_door_half - 0.03F, 0.02F, 0.03F}, c.wood_dark);
  }
  for (float x : {-0.18F, 0.18F}) {
    b.cone({x, -0.02F, k_tower_bridge_length - 0.04F},
           {x, -0.16F, k_tower_bridge_length - 0.02F},
           0.025F,
           c.iron);
  }
  QVector3D const tip_local{0.0F, 0.0F, k_tower_bridge_length - 0.05F};
  for (float x : {-(k_tower_door_half - 0.04F), k_tower_door_half - 0.04F}) {
    QVector3D const tip = bridge.map(tip_local + QVector3D(x, 0.0F, 0.0F));
    QVector3D const anchor = ctx.model.map(
        QVector3D(x, k_tower_top - 0.06F, tower_half(k_tower_top) + 0.02F));
    out.mesh(get_unit_cylinder(),
             cylinder_between(anchor, tip, 0.012F),
             c.iron,
             white,
             1.0F);
  }

  for (float x : {-k_tower_door_half, k_tower_door_half}) {
    g.box({x, (k_tower_bridge_floor + k_tower_top) * 0.5F, hinge_z},
          {0.025F, (k_tower_top - k_tower_bridge_floor) * 0.5F, 0.025F},
          c.wood_dark);
  }
}

enum class AssaultKind : std::uint8_t {
  Ram,
  Tower,
};

struct Variant {
  std::string key;
  AssaultKind kind;
  bool carthage;
};

void register_variant(EntityRendererRegistry& registry, const Variant& v) {
  struct History {
    std::unordered_map<std::uint64_t, SiegeTravelState> travel;
    float last_sweep{0.0F};
  };
  registry.register_renderer(
      v.key,
      [v, history = std::make_shared<History>()](const DrawContext& ctx,
                                                 ISubmitter& out) {
        Mesh* unit = get_unit_cube();
        Texture* white = nullptr;
        if (ctx.resources != nullptr) {
          unit = ctx.resources->unit();
          white = ctx.resources->white();
        }
        if (auto* scene_renderer = dynamic_cast<Renderer*>(out.unwrap_submitter())) {
          unit = scene_renderer->get_mesh_cube();
          white = scene_renderer->get_white_texture();
        }
        if (unit == nullptr) {
          return;
        }
        QVector3D team_color(0.8F, 0.2F, 0.2F);
        if (ctx.entity != nullptr &&
            ctx.entity->get_component<Engine::Core::RenderableComponent>() != nullptr) {
          team_color = Render::entity_color(*ctx.entity);
        }
        if (ctx.animation_time < history->last_sweep) {
          history->travel.clear();
          history->last_sweep = ctx.animation_time;
        }
        if (ctx.animation_time - history->last_sweep > 10.0F) {
          std::erase_if(history->travel, [&](const auto& item) {
            return ctx.animation_time - item.second.time > 30.0F;
          });
          history->last_sweep = ctx.animation_time;
        }
        SiegeTravelState transient;
        auto& state = ctx.entity != nullptr && should_persist_animation_state(ctx)
                          ? history->travel[ctx.entity->get_id()]
                          : transient;
        bool const tower = v.kind == AssaultKind::Tower;
        const float wheel = tower ? 0.22F : 0.20F;
        const float track =
            tower ? k_tower_base_half + 0.11F : k_ram_half_width + 0.08F;
        const auto motion = siege_motion(ctx, state, wheel, track);
        auto const wreck = resolve_siege_wreck(ctx,
                                               tower ? Animation::SiegeWreckKind::Tower
                                                     : Animation::SiegeWreckKind::Ram);
        DrawContext presentation = ctx;
        apply_siege_wreck(presentation.model,
                          wreck,
                          track,
                          tower ? k_tower_wreck_sink_depth : k_ram_wreck_sink_depth);
        Palette palette = palette_for(v.carthage, team_color);
        for (QVector3D* tone : {&palette.wood_frame,
                                &palette.wood_dark,
                                &palette.wood_light,
                                &palette.iron,
                                &palette.bronze,
                                &palette.hide,
                                &palette.hide_wet,
                                &palette.rope,
                                &palette.wicker,
                                &palette.team}) {
          *tone = siege_charred(*tone, wreck);
        }

        SiegeCrewFrame crew{};
        crew.kind = tower ? SiegeCrewKind::Tower : SiegeCrewKind::Ram;
        crew.engine_scale = ctx.model.column(0).toVector3D().length();
        crew.travelled = state.travelled;
        crew.movement = motion.movement;
        crew.destroyed = wreck.destroyed;
        crew.destroyed_elapsed = wreck.elapsed;
        crew.sink_offset = wreck.sink * k_assault_crew_sink_depth;

        if (tower) {
          float ramp = 0.0F;
          if (ctx.entity != nullptr && ctx.world != nullptr) {
            if (const auto* component =
                    ctx.world->try_get<Engine::Core::SiegeTowerComponent>(
                        ctx.entity->get_id())) {
              ramp = component->ramp;
            }
          }
          draw_tower_body(presentation, out, unit, white, palette, motion, ramp);
        } else {
          bool const striking = !wreck.destroyed && in_melee(ctx);
          float const phase = std::fmod(ctx.animation_time, k_ram_stroke_seconds) /
                              k_ram_stroke_seconds;
          float const stroke = striking ? ram_stroke(phase) : -0.06F * motion.jolt;
          crew.loading = striking;
          crew.loading_time = ctx.animation_time;
          crew.loading_progress = phase;
          draw_ram_body(presentation, out, unit, white, palette, motion, stroke);
        }
        advance_siege_crew(state.crew, crew, ctx.animation_time);
        submit_siege_crew(ctx, out, state.crew, crew, v.carthage);
      });
}

} // namespace

void register_siege_assault_renderers(EntityRendererRegistry& registry) {
  for (const char* nation : {"roman", "carthage"}) {
    const bool carthage = std::string_view(nation) == "carthage";
    register_variant(
        registry,
        {std::string("troops/") + nation + "/ram", AssaultKind::Ram, carthage});
    register_variant(registry,
                     {std::string("troops/") + nation + "/siege_tower",
                      AssaultKind::Tower,
                      carthage});
  }
}

} // namespace Render::GL

#include "siege_assault_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

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
#include "game/visuals/team_colors.h"
#include "math/math_utils.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"
#include "siege_renderer_common.h"

namespace Render::GL {
namespace {

using Render::Geom::clamp_vec_01;
using Render::Geom::cylinder_between;

struct Palette {
  QVector3D wood_frame{0.45F, 0.32F, 0.18F};
  QVector3D wood_dark{0.32F, 0.22F, 0.12F};
  QVector3D wood_light{0.55F, 0.40F, 0.25F};
  QVector3D iron{0.38F, 0.36F, 0.34F};
  QVector3D accent{0.72F, 0.52F, 0.30F};
  QVector3D hide{0.50F, 0.40F, 0.30F};
  QVector3D team{0.8F, 0.2F, 0.2F};
};

auto palette_for(bool carthage, const QVector3D& team) -> Palette {
  Palette p;
  if (carthage) {
    p.wood_frame = {0.36F, 0.25F, 0.17F};
    p.wood_dark = {0.26F, 0.18F, 0.12F};
    p.accent = {0.62F, 0.34F, 0.52F};
    p.hide = {0.42F, 0.30F, 0.26F};
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
  void
  cyl(const QVector3D& a, const QVector3D& b, float r, const QVector3D& color) const {
    out.mesh(
        get_unit_cylinder(), model * cylinder_between(a, b, r), color, white, 1.0F);
  }
};

auto in_melee(const DrawContext& ctx) -> bool {
  if (ctx.entity == nullptr || ctx.world == nullptr) {
    return false;
  }
  const auto* attack =
      ctx.world->try_get<Engine::Core::AttackComponent>(ctx.entity->get_id());
  return attack != nullptr && attack->in_melee_lock;
}

void draw_wheel_pair(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const Palette& c,
                     const SiegeMotion& motion,
                     float half_track,
                     float radius,
                     const std::initializer_list<float>& axles) {
  for (float z : axles) {
    draw_siege_wheel(out,
                     white,
                     p.model,
                     {-half_track, radius, z},
                     radius,
                     -motion.right_roll,
                     c.wood_light,
                     c.iron,
                     c.accent);
    draw_siege_wheel(out,
                     white,
                     p.model,
                     {half_track, radius, z},
                     radius,
                     -motion.left_roll,
                     c.wood_light,
                     c.iron,
                     c.accent);
    out.mesh(get_unit_cylinder(),
             p.model * cylinder_between(
                           {-half_track, radius, z}, {half_track, radius, z}, 0.03F),
             c.iron,
             white,
             1.0F);
  }
}

void draw_ram_body(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const QVector3D& team_color,
                   const SiegeMotion& motion) {
  const Palette c = palette_for(false, team_color);
  DrawContext ctx = p;
  ctx.model = siege_body_model(p, motion);

  const Painter g{out, unit, white, ctx.model};

  // Chassis: two runners and cross-beams.
  for (float side : {-1.0F, 1.0F}) {
    g.box({side * 0.30F, 0.26F, 0.0F}, {0.04F, 0.04F, 0.72F}, c.wood_frame);
    // Four posts.
    for (float z : {-0.60F, 0.60F}) {
      g.box({side * 0.30F, 0.56F, z}, {0.035F, 0.30F, 0.035F}, c.wood_dark);
    }
    g.box({side * 0.30F, 0.86F, 0.0F}, {0.035F, 0.035F, 0.70F}, c.wood_dark);
  }
  for (float z : {-0.60F, 0.0F, 0.60F}) {
    g.box({0.0F, 0.26F, z}, {0.30F, 0.035F, 0.035F}, c.wood_frame);
  }

  // Pitched hide roof, two sloping panels meeting at a ridge.
  for (float side : {-1.0F, 1.0F}) {
    QMatrix4x4 m = ctx.model;
    m.translate(side * 0.19F, 1.00F, 0.0F);
    m.rotate(side * 28.0F, 0.0F, 0.0F, 1.0F);
    m.scale(0.22F, 0.025F, 0.74F);
    out.mesh(unit, m, c.hide, white, 1.0F);
  }
  g.box({0.0F, 1.10F, 0.0F}, {0.03F, 0.03F, 0.76F}, c.wood_dark);
  // Team-coloured pennant stripe along the roof ridge side.
  for (float side : {-1.0F, 1.0F}) {
    g.box({side * 0.34F, 0.92F, 0.0F}, {0.006F, 0.05F, 0.46F}, c.team * 0.8F);
  }

  // Suspended beam. It rocks while rolling and thrusts when locked on a target.
  const bool striking = in_melee(p);
  float swing = 0.0F;
  if (striking) {
    swing = 0.5F + 0.5F * std::sin(p.animation_time * 2.5F * std::numbers::pi_v<float>);
    swing = swing * swing;
  } else {
    swing = 0.5F * motion.jolt;
  }
  const float draw_back = -0.18F + 0.55F * swing;
  const QVector3D tail{0.0F, 0.52F, -0.78F + draw_back};
  const QVector3D head{0.0F, 0.52F, 0.88F + draw_back};
  g.cyl(tail, head, 0.075F, c.wood_light);
  g.cyl(head, head + QVector3D(0.0F, 0.0F, 0.12F), 0.10F, c.iron);
  g.box(head + QVector3D(0.0F, 0.0F, 0.14F), {0.07F, 0.07F, 0.03F}, c.iron * 1.15F);
  // Chains hanging the beam from the ridge.
  for (float z : {-0.30F, 0.30F}) {
    g.cyl({0.0F, 1.08F, z}, {0.0F, 0.58F, z + draw_back * 0.35F}, 0.012F, c.iron);
  }

  draw_wheel_pair(p, out, white, c, motion, 0.36F, 0.17F, {-0.45F, 0.45F});
}

void draw_tower_body(const DrawContext& p,
                     ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const QVector3D& team_color,
                     const SiegeMotion& motion) {
  const Palette c = palette_for(false, team_color);
  DrawContext ctx = p;
  ctx.model = siege_body_model(p, motion);
  const Painter g{out, unit, white, ctx.model};

  constexpr float half = 0.42F;
  constexpr float top = 2.05F;
  // Corner posts and rails on four levels.
  for (float x : {-half, half}) {
    for (float z : {-half, half}) {
      g.box({x, top * 0.5F + 0.18F, z}, {0.045F, top * 0.5F, 0.045F}, c.wood_dark);
    }
  }
  for (float y : {0.40F, 0.95F, 1.50F, top + 0.12F}) {
    for (float s : {-1.0F, 1.0F}) {
      g.box({0.0F, y, s * half}, {half, 0.035F, 0.035F}, c.wood_frame);
      g.box({s * half, y, 0.0F}, {0.035F, 0.035F, half}, c.wood_frame);
    }
  }
  // Hide-clad walls on both sides and the back; the front stays open to the ramp.
  for (float s : {-1.0F, 1.0F}) {
    g.box({s * (half + 0.01F), 1.05F, 0.0F}, {0.012F, 0.95F, half - 0.03F}, c.hide);
  }
  g.box({0.0F, 1.05F, -half - 0.01F}, {half - 0.03F, 0.95F, 0.012F}, c.hide);
  // Diagonal braces.
  g.cyl({-half, 0.4F, half}, {half, 1.5F, half}, 0.02F, c.wood_dark);
  g.cyl({half, 0.4F, half}, {-half, 1.5F, half}, 0.02F, c.wood_dark);
  // Fighting platform and parapet at the top.
  g.box({0.0F, top + 0.18F, 0.0F}, {half + 0.04F, 0.02F, half + 0.04F}, c.wood_frame);
  for (float s : {-1.0F, 1.0F}) {
    g.box(
        {s * (half + 0.04F), top + 0.30F, 0.0F}, {0.02F, 0.12F, half + 0.04F}, c.hide);
    g.box(
        {0.0F, top + 0.30F, -s * (half + 0.04F)}, {half + 0.04F, 0.12F, 0.02F}, c.hide);
  }
  // Boarding ramp folded against the front face; it drops when the tower is docked.
  float ramp = 0.0F;
  if (p.entity != nullptr && p.world != nullptr) {
    if (const auto* tower =
            p.world->try_get<Engine::Core::SiegeTowerComponent>(p.entity->get_id())) {
      ramp = tower->ramp;
    }
  }
  {
    QMatrix4x4 m = ctx.model;
    m.translate(0.0F, top + 0.20F, half + 0.04F);
    m.rotate(-90.0F * (1.0F - ramp), 1.0F, 0.0F, 0.0F);
    m.translate(0.0F, 0.0F, 0.33F);
    m.scale(0.34F, 0.018F, 0.33F);
    out.mesh(unit, m, c.wood_light, white, 1.0F);
  }
  // Banner.
  g.cyl({-half, top + 0.18F, -half}, {-half, top + 0.78F, -half}, 0.012F, c.accent);
  g.box({-half, top + 0.66F, -half + 0.09F}, {0.006F, 0.08F, 0.09F}, c.team);

  draw_wheel_pair(p, out, white, c, motion, half + 0.06F, 0.20F, {-0.30F, 0.30F});
}

struct Variant {
  std::string key;
  bool tower;
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
        DrawContext presentation = ctx;
        const float wheel = v.tower ? 0.20F : 0.17F;
        const float track = v.tower ? 0.48F : 0.36F;
        const auto motion = siege_motion(presentation, state, wheel, track);
        if (v.tower) {
          draw_tower_body(presentation, out, unit, white, team_color, motion);
        } else {
          draw_ram_body(presentation, out, unit, white, team_color, motion);
        }
      });
}

} // namespace

void register_siege_assault_renderers(EntityRendererRegistry& registry) {
  for (const char* nation : {"roman", "carthage"}) {
    const bool carthage = std::string_view(nation) == "carthage";
    register_variant(registry,
                     {std::string("troops/") + nation + "/ram", false, carthage});
    register_variant(
        registry, {std::string("troops/") + nation + "/siege_tower", true, carthage});
  }
}

} // namespace Render::GL

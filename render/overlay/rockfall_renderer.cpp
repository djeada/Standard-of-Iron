#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "render/entity/barracks_flag_renderer.h"
#include "render/frame/draw_context.h"
#include "render/frame/draw_resources.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/submission_visibility.h"
#include "render/geom/arrow.h"
#include "render/geom/primitives.h"
#include "render/geom/stone.h"
#include "render/geom/transforms.h"
#include "render/overlay/projectile_renderer.h"
#include "render/overlay/projectile_renderer_internal.h"
#include "render/overlay/raft_renderer.h"

namespace Render::GL {

using namespace projectile_renderer_detail;

namespace {

const QVector3D k_granite(0.88F, 0.85F, 0.79F);
const QVector3D k_dark_granite(0.68F, 0.65F, 0.60F);
const QVector3D k_lichen(0.56F, 0.60F, 0.44F);
const QVector3D k_timber(0.38F, 0.27F, 0.16F);
const QVector3D k_timber_light(0.52F, 0.39F, 0.24F);
const QVector3D k_unclaimed_cloth(0.62F, 0.58F, 0.50F);
const QVector3D k_banner_metal(0.55F, 0.52F, 0.46F);
const QVector3D k_rockfall_dust_color(0.74F, 0.68F, 0.58F);
const QVector3D k_threat_color(1.0F, 0.34F, 0.14F);
constexpr float k_rockfall_dust_intensity = 1.7F;
constexpr float k_rad_to_deg_f = 180.0F / std::numbers::pi_v<float>;

[[nodiscard]] auto rock_hash(std::uint32_t seed, std::uint32_t salt) -> float {
  std::uint32_t value = seed * 0x9e3779b9U ^ (salt * 0x85ebca6bU);
  value ^= value >> 15U;
  value *= 0x2c1b3c6dU;
  value ^= value >> 12U;
  return static_cast<float>(value & 0xffffU) / 65535.0F;
}

[[nodiscard]] auto rock_color(std::uint32_t seed) -> QVector3D {
  float const shade = rock_hash(seed, 1U);
  QVector3D color = k_dark_granite + (k_granite - k_dark_granite) * shade;
  if (rock_hash(seed, 2U) > 0.72F) {
    color = color * 0.75F + k_lichen * 0.25F;
  }
  return color;
}

[[nodiscard]] auto rock_shape(std::uint32_t seed) -> QVector3D {
  return {0.92F + 0.20F * rock_hash(seed, 3U),
          0.78F + 0.18F * rock_hash(seed, 4U),
          0.90F + 0.18F * rock_hash(seed, 5U)};
}

void draw_rock(FrameSubmitter* renderer,
               Mesh* mesh,
               const QMatrix4x4& placement,
               float radius,
               std::uint32_t seed) {
  QMatrix4x4 model = placement;
  QVector3D const shape = rock_shape(seed) * (radius / Geom::Stone::k_mean_radius);
  model.scale(shape);
  renderer->mesh(mesh, model, rock_color(seed), nullptr, 1.0F);
}

void draw_log(FrameSubmitter* renderer,
              Mesh* cylinder,
              const QVector3D& a,
              const QVector3D& b,
              float radius,
              const QVector3D& color) {
  renderer->mesh(cylinder, Geom::cylinder_between(a, b, radius), color, nullptr, 1.0F);
}

void draw_chevron(FrameSubmitter* renderer,
                  Mesh* cube,
                  const QVector3D& tip,
                  const QVector3D& downhill,
                  const QVector3D& color,
                  float size,
                  float alpha) {

  float const yaw = std::atan2(downhill.x(), downhill.z()) * k_rad_to_deg_f;
  float const arm = 0.62F * size;
  for (float side : {-1.0F, 1.0F}) {
    QMatrix4x4 model;
    model.translate(tip);
    model.rotate(yaw + side * 135.0F, 0.0F, 1.0F, 0.0F);
    model.translate(0.0F, 0.0F, arm * 0.5F);
    model.scale(0.065F * size, 0.015F, arm * 0.5F);
    renderer->mesh(cube, model, color, nullptr, alpha);
  }
}

void draw_cache_banner(FrameSubmitter* renderer,
                       const QVector3D& pole_base,
                       float yaw,
                       const QVector3D& cloth) {
  auto* resources = renderer->resources();
  Mesh* unit = resources != nullptr ? resources->unit() : nullptr;
  if (unit == nullptr) {
    unit = get_unit_cube();
  }
  Texture* white = resources != nullptr ? resources->white() : nullptr;

  DrawContext context;
  context.resources = resources;
  context.animation_time = renderer->get_animation_time();

  context.model.translate(pole_base);
  context.model.rotate(yaw + 180.0F, 0.0F, 1.0F, 0.0F);

  BarracksFlagRenderer::ClothBannerResources cloth_resources;
  if (context.resources != nullptr) {
    cloth_resources.cloth_mesh = context.resources->banner_mesh();
    cloth_resources.banner_shader = context.resources->banner_shader();
  }
  QVector3D const trim = cloth * 0.5F + k_banner_metal * 0.5F;
  BarracksFlagRenderer::draw_hanging_banner(
      context,
      *renderer,
      unit,
      white,
      cloth,
      trim,
      {.pole_base = QVector3D(0.0F, 0.0F, 0.0F),
       .pole_height = 2.7F,
       .pole_radius = 0.045F,
       .banner_width = 0.8F,
       .banner_height = 0.55F,
       .pole_color = k_timber,
       .beam_color = k_timber_light,
       .connector_color = k_banner_metal,
       .ornament_offset = QVector3D(0.22F, 2.85F, 0.03F),
       .ornament_size = QVector3D(0.3F, 0.03F, 0.015F),
       .ornament_color = k_banner_metal,
       .ring_count = 3,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.55F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = k_banner_metal},
      &cloth_resources);
}

void draw_cache(FrameSubmitter* renderer,
                const Game::Systems::RockfallCacheView& cache,
                const ProjectileViewContext* view) {
  auto* stone = Geom::Stone::get();
  auto* cylinder = get_unit_cylinder();
  auto* cube = get_unit_cube();
  if (stone == nullptr || cylinder == nullptr || cube == nullptr) {
    return;
  }
  float const time = renderer->get_animation_time();
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const forward = cache.downhill;
  QVector3D const lateral(-forward.z(), 0.0F, forward.x());
  float const r = cache.boulder_radius;
  float const yaw = std::atan2(forward.x(), forward.z()) * k_rad_to_deg_f;
  float const push = std::max(cache.push_progress, 0.0F);

  QMatrix4x4 pile;
  QVector3D const pivot = cache.position + forward * (r * 1.1F);
  pile.translate(pivot);
  if (cache.push_progress >= 0.0F) {
    float const tip = push * push * 14.0F + std::sin(time * 26.0F) * 2.2F * push;
    pile.rotate(tip, lateral);
  }
  pile.translate(-pivot);

  int const left = std::max(0, cache.boulders_left);
  int const bottom = std::min(left, 3);
  int const top = std::max(0, left - bottom);
  for (int i = 0; i < left; ++i) {
    bool const upper = i >= bottom;
    int const row_count = upper ? top : bottom;
    int const slot = upper ? i - bottom : i;
    float const across =
        (static_cast<float>(slot) - 0.5F * static_cast<float>(row_count - 1)) * r *
        1.9F;
    std::uint32_t const seed = cache.seed + static_cast<std::uint32_t>(i) * 7919U;
    QVector3D centre = cache.position + lateral * across +
                       forward * ((upper ? -0.35F : 0.0F) * r) +
                       up * (upper ? r * 2.3F : r * 0.85F);
    QMatrix4x4 placement = pile;
    placement.translate(centre);
    placement.rotate(360.0F * rock_hash(seed, 6U), up);
    placement.rotate(30.0F * rock_hash(seed, 7U), QVector3D(1.0F, 0.0F, 0.3F));
    draw_rock(
        renderer, stone, placement, r * (0.85F + 0.25F * rock_hash(seed, 8U)), seed);
  }

  float const half_width = r * 2.6F;
  float const lean = push * 0.55F * r;
  for (float side : {-1.0F, 1.0F}) {
    QVector3D const foot =
        cache.position + forward * (r * 1.25F) + lateral * (side * half_width);
    QVector3D const head = foot + up * (r * 2.6F) + forward * lean;
    draw_log(renderer, cylinder, foot - up * 0.2F, head, 0.07F, k_timber);
  }
  QVector3D const log_a =
      cache.position + forward * (r * 1.3F) - lateral * half_width + up * (r * 1.0F);
  QVector3D const log_b =
      cache.position + forward * (r * 1.3F) + lateral * half_width + up * (r * 1.0F);
  draw_log(renderer,
           cylinder,
           log_a + forward * (lean * 0.4F),
           log_b + forward * (lean * 0.4F),
           0.09F,
           k_timber_light);

  bool const claimed = cache.owner_id >= 0;
  QVector3D const cloth =
      claimed ? Render::team_color(cache.owner_id) : k_unclaimed_cloth;
  draw_cache_banner(renderer,
                    cache.position - forward * (r * 2.4F) +
                        lateral * (half_width + 0.4F),
                    yaw,
                    cloth);

  bool const own = view != nullptr && claimed && cache.owner_id == view->local_owner_id;
  if (!own || !cache.armed || cache.push_progress >= 0.0F) {
    return;
  }
  QVector3D const start = cache.position + forward * (r * 2.6F);
  QVector3D const end = cache.zone_target + forward * (cache.zone_radius * 0.5F);
  constexpr int k_chevrons = 4;
  float const pulse = 0.5F + 0.5F * std::sin(time * 6.0F);
  for (int i = 0; i < k_chevrons; ++i) {
    float const t = (static_cast<float>(i) + 0.5F) / static_cast<float>(k_chevrons);
    QVector3D tip = start + (end - start) * t;
    tip.setY(tip.y() + 0.12F);
    QVector3D color = cloth;
    float alpha = 0.35F;
    if (cache.threatened) {
      float const wave =
          0.5F + 0.5F * std::sin(time * 8.0F - static_cast<float>(i) * 1.2F);
      color = k_threat_color;
      alpha = 0.45F + 0.45F * wave * (0.6F + 0.4F * pulse);
    }
    draw_chevron(renderer, cube, tip, forward, color, 1.1F, alpha);
  }
}

} // namespace

void render_rockfall(FrameSubmitter* renderer,
                     const Game::Systems::RenderEffectsFrame& effects,
                     const ProjectileViewContext* view) {
  if (renderer == nullptr) {
    return;
  }

  for (auto const& cache : effects.rockfall_caches) {
    draw_cache(renderer, cache, view);
  }

  if (!effects.rockfall_boulders.empty()) {
    if (auto* mesh = Geom::Stone::get(); mesh != nullptr) {
      for (auto const& boulder : effects.rockfall_boulders) {
        QMatrix4x4 placement;
        placement.translate(boulder.position - QVector3D(0.0F, boulder.sink, 0.0F));
        placement.rotate(boulder.orientation);
        draw_rock(renderer, mesh, placement, boulder.radius, boulder.seed);
      }
    }
  }

  for (auto const& dust : effects.rockfall_dust) {
    renderer->stone_impact(dust.position,
                           k_rockfall_dust_color,
                           dust.scale,
                           k_rockfall_dust_intensity,
                           dust.age);
  }
}

void render_rafts(FrameSubmitter* renderer,
                  const Game::Systems::RenderEffectsFrame& effects) {
  if (effects.rafts.empty()) {
    return;
  }
  std::vector<RaftDrawable> drawables;
  drawables.reserve(effects.rafts.size());
  for (auto const& raft : effects.rafts) {
    drawables.push_back({.position = raft.position,
                         .across = raft.across,
                         .docks = raft.docks,
                         .half_length = raft.half_length,
                         .half_width = raft.half_width,
                         .moving = raft.moving,
                         .seed = raft.seed});
  }
  draw_rafts(renderer, drawables);
}

} // namespace Render::GL

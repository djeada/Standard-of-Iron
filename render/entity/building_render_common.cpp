#include "building_render_common.h"

#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <string>

namespace Render::GL {
namespace {

using Render::Geom::clamp_vec_01;

auto building_unit(const DrawContext &ctx) -> Engine::Core::UnitComponent * {
  return (ctx.entity != nullptr)
             ? ctx.entity->get_component<Engine::Core::UnitComponent>()
             : nullptr;
}

auto building_capture(const DrawContext &ctx)
    -> Engine::Core::CaptureComponent * {
  return (ctx.entity != nullptr)
             ? ctx.entity->get_component<Engine::Core::CaptureComponent>()
             : nullptr;
}

auto building_box_mesh(const DrawContext &ctx) -> Mesh * {
  return (ctx.resources != nullptr) ? ctx.resources->unit() : nullptr;
}

auto building_white_texture(const DrawContext &ctx) -> Texture * {
  return (ctx.resources != nullptr) ? ctx.resources->white() : nullptr;
}

void submit_box(ISubmitter &out, const DrawContext &ctx, const QVector3D &pos,
                const QVector3D &size, const QVector3D &color) {
  Mesh *mesh = building_box_mesh(ctx);
  if (mesh == nullptr) {
    return;
  }

  QMatrix4x4 model = ctx.model;
  model.translate(pos);
  model.scale(size);
  out.mesh(mesh, model, color, building_white_texture(ctx), 1.0F);
}

auto resolve_bar_colors(float ratio) -> std::pair<QVector3D, QVector3D> {
  if (ratio >= HEALTH_THRESHOLD_NORMAL) {
    return {HealthBarColors::NORMAL_BRIGHT, HealthBarColors::NORMAL_DARK};
  }
  if (ratio >= HEALTH_THRESHOLD_DAMAGED) {
    float const t = (ratio - HEALTH_THRESHOLD_DAMAGED) /
                    (HEALTH_THRESHOLD_NORMAL - HEALTH_THRESHOLD_DAMAGED);
    return {HealthBarColors::NORMAL_BRIGHT * t +
                HealthBarColors::DAMAGED_BRIGHT * (1.0F - t),
            HealthBarColors::NORMAL_DARK * t +
                HealthBarColors::DAMAGED_DARK * (1.0F - t)};
  }

  float const t = ratio / HEALTH_THRESHOLD_DAMAGED;
  return {HealthBarColors::DAMAGED_BRIGHT * t +
              HealthBarColors::CRITICAL_BRIGHT * (1.0F - t),
          HealthBarColors::DAMAGED_DARK * t +
              HealthBarColors::CRITICAL_DARK * (1.0F - t)};
}

} // namespace

auto resolve_building_health_ratio(const DrawContext &ctx) -> float {
  auto *unit = building_unit(ctx);
  if (unit == nullptr) {
    return 0.0F;
  }

  return std::clamp(unit->health / float(std::max(1, unit->max_health)), 0.0F,
                    1.0F);
}

auto resolve_building_state(const DrawContext &ctx) -> BuildingState {
  auto *unit = building_unit(ctx);
  if (unit == nullptr) {
    return BuildingState::Normal;
  }
  return get_building_state(resolve_building_health_ratio(ctx));
}

auto building_renderer_key(std::string_view nation_slug,
                           std::string_view building_type) -> std::string {
  return "troops/" + std::string(nation_slug) + "/" +
         std::string(building_type);
}

auto building_renderer_key(Game::Systems::NationID nation_id,
                           std::string_view building_type) -> std::string {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
    return building_renderer_key("carthage", building_type);
  case Game::Systems::NationID::RomanRepublic:
  default:
    return building_renderer_key("roman", building_type);
  }
}

auto canonicalize_building_renderer_key(std::string_view renderer_key)
    -> std::string_view {
  if (renderer_key == "barracks_roman") {
    return "troops/roman/barracks";
  }
  if (renderer_key == "barracks_carthage") {
    return "troops/carthage/barracks";
  }
  return renderer_key;
}

auto resolve_building_renderer_key(
    std::string_view renderer_key, std::string_view building_type,
    Game::Systems::NationID nation_id) -> std::string {
  if (renderer_key.empty() || renderer_key == building_type) {
    return building_renderer_key(nation_id, building_type);
  }
  return std::string(canonicalize_building_renderer_key(renderer_key));
}

void submit_building_instance(ISubmitter &out, const DrawContext &ctx,
                              const RenderArchetype &archetype,
                              std::span<const QVector3D> palette) {
  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world = ctx.model;
  instance.palette = palette;
  instance.default_texture = building_white_texture(ctx);
  instance.lod =
      select_render_archetype_lod(archetype, std::sqrt(ctx.distance_sq));
  submit_render_instance(out, instance);
}

void submit_building_box(ISubmitter &out, Mesh *mesh, Texture *texture,
                         const QMatrix4x4 &model, const QVector3D &pos,
                         const QVector3D &size, const QVector3D &color,
                         float alpha) {
  if (mesh == nullptr) {
    return;
  }

  QMatrix4x4 local = model;
  local.translate(pos);
  local.scale(size);
  out.mesh(mesh, local, color, texture, alpha);
}

void submit_building_cylinder(ISubmitter &out, const QMatrix4x4 &model,
                              const QVector3D &start, const QVector3D &end,
                              float radius, const QVector3D &color,
                              Texture *texture, float alpha) {
  out.mesh(get_unit_cylinder(),
           model * Render::Geom::cylinder_between(start, end, radius), color,
           texture, alpha);
}

void draw_building_health_bar(ISubmitter &out, const DrawContext &ctx,
                              const BuildingHealthBarStyle &style) {
  if (building_box_mesh(ctx) == nullptr) {
    return;
  }

  auto *unit = building_unit(ctx);
  if (unit == nullptr) {
    return;
  }

  float const ratio = resolve_building_health_ratio(ctx);
  if (ratio <= 0.0F) {
    return;
  }

  auto *capture = building_capture(ctx);
  bool const under_attack = (capture != nullptr) && capture->is_being_captured;
  if (!under_attack && unit->health >= unit->max_health) {
    return;
  }

  float const bar_width = style.width;
  float const bar_height = style.height;
  float const bar_y = style.y;
  constexpr float k_border_thickness = 0.012F;

  if (under_attack) {
    float const pulse =
        HEALTHBAR_PULSE_MIN +
        HEALTHBAR_PULSE_AMPLITUDE *
            std::sin(ctx.animation_time * HEALTHBAR_PULSE_SPEED);
    submit_box(out, ctx, QVector3D(0.0F, bar_y, 0.0F),
               QVector3D(bar_width * 0.5F + k_border_thickness * 3.0F,
                         bar_height * 0.5F + k_border_thickness * 3.0F, 0.095F),
               HealthBarColors::GLOW_ATTACK * pulse * 0.6F);
  }

  submit_box(out, ctx, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + k_border_thickness,
                       bar_height * 0.5F + k_border_thickness, 0.09F),
             HealthBarColors::BORDER);
  submit_box(out, ctx, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + k_border_thickness * 0.5F,
                       bar_height * 0.5F + k_border_thickness * 0.5F, 0.088F),
             HealthBarColors::INNER_BORDER);
  submit_box(out, ctx, QVector3D(0.0F, bar_y + 0.003F, 0.0F),
             QVector3D(bar_width * 0.5F, bar_height * 0.5F, 0.085F),
             HealthBarColors::BACKGROUND);

  auto [fg_color, fg_dark] = resolve_bar_colors(ratio);
  submit_box(
      out, ctx,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.005F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.48F, 0.08F), fg_dark);
  submit_box(
      out, ctx,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.008F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.40F, 0.078F),
      fg_color);

  QVector3D const highlight = clamp_vec_01(fg_color * 1.6F);
  submit_box(out, ctx,
             QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                       bar_y + bar_height * 0.35F, 0.0F),
             QVector3D(bar_width * ratio * 0.5F, bar_height * 0.20F, 0.075F),
             highlight);
  submit_box(out, ctx,
             QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                       bar_y + bar_height * 0.48F, 0.0F),
             QVector3D(bar_width * ratio * 0.5F, bar_height * 0.08F, 0.073F),
             HealthBarColors::SHINE * 0.8F);

  float const marker_70_x = bar_width * 0.5F * (HEALTH_THRESHOLD_NORMAL - 0.5F);
  submit_box(out, ctx, QVector3D(marker_70_x, bar_y, 0.0F),
             QVector3D(0.015F, bar_height * 0.55F, 0.09F),
             HealthBarColors::SEGMENT);
  if (style.draw_segment_highlights) {
    submit_box(
        out, ctx,
        QVector3D(marker_70_x - 0.003F, bar_y + bar_height * 0.40F, 0.0F),
        QVector3D(0.008F, bar_height * 0.15F, 0.091F),
        HealthBarColors::SEGMENT_HIGHLIGHT);
  }

  float const marker_30_x =
      bar_width * 0.5F * (HEALTH_THRESHOLD_DAMAGED - 0.5F);
  submit_box(out, ctx, QVector3D(marker_30_x, bar_y, 0.0F),
             QVector3D(0.015F, bar_height * 0.55F, 0.09F),
             HealthBarColors::SEGMENT);
  if (style.draw_segment_highlights) {
    submit_box(
        out, ctx,
        QVector3D(marker_30_x - 0.003F, bar_y + bar_height * 0.40F, 0.0F),
        QVector3D(0.008F, bar_height * 0.15F, 0.091F),
        HealthBarColors::SEGMENT_HIGHLIGHT);
  }
}

void draw_building_compact_health_bar(ISubmitter &out, const DrawContext &ctx,
                                      float y) {
  if (building_box_mesh(ctx) == nullptr) {
    return;
  }

  float const ratio = resolve_building_health_ratio(ctx);
  if (ratio <= 0.0F) {
    return;
  }

  submit_box(out, ctx, QVector3D(0.0F, y, 0.0F), QVector3D(0.6F, 0.03F, 0.05F),
             QVector3D(0.06F, 0.06F, 0.06F));

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  submit_box(out, ctx, QVector3D(-0.3F * (1.0F - ratio), y + 0.01F, 0.0F),
             QVector3D(0.3F * ratio, 0.025F, 0.045F), fg);
}

void draw_building_selection_overlay(ISubmitter &out, const DrawContext &ctx,
                                     const BuildingSelectionStyle &style) {
  QMatrix4x4 model;
  QVector3D const pos = ctx.model.column(3).toVector3D();
  model.translate(pos.x(), 0.0F, pos.z());
  model.scale(style.scale_x, 1.0F, style.scale_z);

  if (ctx.selected) {
    out.selection_smoke(model, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (ctx.hovered) {
    out.selection_smoke(model, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

auto select_nation_variant_renderer_key(
    std::string_view roman_key, std::string_view carthage_key,
    Game::Systems::NationID nation_id) -> std::string_view {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
    return carthage_key;
  case Game::Systems::NationID::RomanRepublic:
  default:
    return roman_key;
  }
}

void register_nation_variant_renderer(EntityRendererRegistry &registry,
                                      const std::string &public_key,
                                      std::string roman_key,
                                      std::string carthage_key) {
  registry.register_renderer(
      public_key, [&registry, roman_key = std::move(roman_key),
                   carthage_key = std::move(carthage_key)](
                      const DrawContext &ctx, ISubmitter &out) {
        auto *unit = building_unit(ctx);
        if (unit == nullptr) {
          return;
        }

        std::string const renderer_key(select_nation_variant_renderer_key(
            roman_key, carthage_key, unit->nation_id));
        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(ctx, out);
        }
      });
}

void register_building_renderer(EntityRendererRegistry &registry,
                                std::string_view nation_slug,
                                std::string_view building_type,
                                RenderFunc func) {
  const std::string canonical_key =
      building_renderer_key(nation_slug, building_type);
  registry.register_renderer(canonical_key, func);
}

} // namespace Render::GL

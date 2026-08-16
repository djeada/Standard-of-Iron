#include "projectile_renderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "arrow.h"
#include "game/systems/arrow_projectile.h"
#include "game/systems/projectile_system.h"
#include "game/systems/stone_projectile.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "stone.h"

namespace Render::GL {

auto classify_projectile_relation(int local_owner_id,
                                  int attacker_owner_id,
                                  int target_owner_id) -> ProjectileRelation {
  if (local_owner_id <= 0) {
    return ProjectileRelation::Neutral;
  }
  if (attacker_owner_id == local_owner_id) {
    return ProjectileRelation::Outgoing;
  }
  if (target_owner_id == local_owner_id) {
    return ProjectileRelation::Incoming;
  }
  return ProjectileRelation::Neutral;
}

auto ProjectileViewContext::relation_for(
    std::uint64_t attacker_id, std::uint64_t target_id) const -> ProjectileRelation {
  if (!owner_of || local_owner_id <= 0) {
    return ProjectileRelation::Neutral;
  }
  const int attacker_owner = attacker_id != 0U ? owner_of(attacker_id) : 0;
  const int target_owner = target_id != 0U ? owner_of(target_id) : 0;
  return classify_projectile_relation(local_owner_id, attacker_owner, target_owner);
}

namespace {

const QVector3D k_incoming_glow(1.0F, 0.34F, 0.22F);
const QVector3D k_outgoing_glow(1.0F, 0.92F, 0.70F);

[[nodiscard]] auto relation_glow(const QVector3D& glow,
                                 ProjectileRelation relation) -> QVector3D {
  switch (relation) {
  case ProjectileRelation::Incoming:
    return glow * 0.35F + k_incoming_glow * 0.65F;
  case ProjectileRelation::Outgoing:
    return glow * 0.55F + k_outgoing_glow * 0.45F;
  case ProjectileRelation::Neutral:
    break;
  }
  return glow;
}

[[nodiscard]] auto relation_brightness(ProjectileRelation relation) -> float {
  switch (relation) {
  case ProjectileRelation::Incoming:
    return 1.30F;
  case ProjectileRelation::Outgoing:
    return 1.15F;
  case ProjectileRelation::Neutral:
    break;
  }
  return 0.90F;
}

[[nodiscard]] auto relation_trail_boost(ProjectileRelation relation) -> float {
  switch (relation) {
  case ProjectileRelation::Incoming:
    return 1.6F;
  case ProjectileRelation::Outgoing:
    return 1.25F;
  case ProjectileRelation::Neutral:
    break;
  }
  return 1.0F;
}

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
constexpr float k_streak_xy_scale = 0.62F;
constexpr float k_bolt_z_scale = Geom::Arrow::k_arrow_z_scale * 2.05F;
constexpr float k_bolt_xy_scale = Geom::Arrow::k_arrow_xy_scale * 1.55F;

[[nodiscard]] auto scaled_color(const QVector3D& color, float scale) -> QVector3D {
  return {std::clamp(color.x() * scale, 0.0F, 1.0F),
          std::clamp(color.y() * scale, 0.0F, 1.0F),
          std::clamp(color.z() * scale, 0.0F, 1.0F)};
}

void draw_arrow_glow(Renderer* renderer,
                     GL::Mesh* shaft,
                     GL::Mesh* tip,
                     const QMatrix4x4& model,
                     const QVector3D& glow,
                     float alpha_scale) {
  using Geom::Arrow;

  QMatrix4x4 sheath = model;
  sheath.scale(Arrow::k_shaft_glow_xy_scale, Arrow::k_shaft_glow_xy_scale, 1.0F);
  renderer->mesh(shaft, sheath, glow, nullptr, Arrow::k_shaft_glow_alpha * alpha_scale);

  QMatrix4x4 head = model;
  head.translate(0.0F, 0.0F, Arrow::k_head_center_z);
  head.scale(Arrow::k_head_glow_xy_scale,
             Arrow::k_head_glow_xy_scale,
             Arrow::k_head_glow_z_scale);
  head.translate(0.0F, 0.0F, -Arrow::k_head_center_z);
  renderer->mesh(tip, head, glow, nullptr, Arrow::k_head_glow_alpha * alpha_scale);
}

[[nodiscard]] auto projectile_model_at(const QVector3D& position,
                                       const QVector3D& direction) -> QMatrix4x4 {
  QVector3D dir = direction;
  if (dir.lengthSquared() <= 1.0e-6F) {
    dir = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    dir.normalize();
  }
  QMatrix4x4 model;
  model.translate(position);
  model.rotate(std::atan2(dir.x(), dir.z()) * k_rad_to_deg, QVector3D(0, 1, 0));
  float const horizontal = std::hypot(dir.x(), dir.z());
  model.rotate(-std::atan2(dir.y(), horizontal) * k_rad_to_deg, QVector3D(1, 0, 0));
  return model;
}

void render_aimed_arrow_impact(Renderer* renderer,
                               const Game::Systems::ProjectileImpactEvent& impact,
                               float progress,
                               float fade) {

  float const flash = std::clamp(1.0F - (progress * 4.5F), 0.0F, 1.0F);
  float const burst = 1.0F - std::pow(1.0F - progress, 2.6F);
  float const scale = std::max(0.55F, impact.scale);
  float const time = renderer->get_animation_time();

  QVector3D const incoming = impact.incoming_direction;
  QVector3D lateral = QVector3D::crossProduct(incoming, QVector3D(0.0F, 1.0F, 0.0F));
  if (lateral.lengthSquared() < 1.0e-5F) {
    lateral = QVector3D(1.0F, 0.0F, 0.0F);
  }
  lateral.normalize();
  QVector3D const vertical =
      QVector3D::crossProduct(lateral, incoming).normalized() * 1.0F;

  constexpr int k_spark_count = 14;
  for (int spark = 0; spark < k_spark_count; ++spark) {
    float const seed = static_cast<float>(spark) * 2.399963F;
    float const cone = 0.32F + (0.55F * std::abs(std::sin(seed * 1.7F)));

    QVector3D const spray =
        ((-incoming) + (lateral * std::cos(seed) * cone) +
         (vertical * std::sin(seed) * cone) + QVector3D(0.0F, 0.22F, 0.0F))
            .normalized();
    float const reach = (0.30F + (0.85F * std::abs(std::cos(seed * 2.3F)))) * scale;
    renderer->metal_spark(impact.position + (spray * reach * burst),
                          impact.hit_target ? QVector3D(0.86F, 0.16F, 0.11F)
                                            : QVector3D(1.0F, 0.86F, 0.55F),
                          (0.052F + (0.028F * std::sin(seed))) * scale,
                          1.45F * fade * fade,
                          impact.age + (seed * 0.03F),
                          spray);
  }

  if (impact.hit_target) {

    renderer->combat_dust(impact.position - (incoming * 0.12F * scale),
                          QVector3D(0.44F, 0.07F, 0.05F),
                          (0.24F + (0.42F * burst)) * scale,
                          1.30F * fade * fade,
                          time + impact.age);
    renderer->combat_dust(impact.position + QVector3D(0.0F, 0.16F * burst, 0.0F) -
                              (incoming * 0.30F * scale),
                          QVector3D(0.52F, 0.10F, 0.08F),
                          (0.14F + (0.34F * burst)) * scale,
                          0.85F * fade,
                          time * 1.21F + impact.age);
  } else {
    renderer->combat_dust(impact.position,
                          QVector3D(0.62F, 0.56F, 0.46F),
                          (0.18F + (0.36F * burst)) * scale,
                          0.95F * fade,
                          time + impact.age);
  }

  if (flash > 0.0F) {

    float const flash_radius =
        impact.hit_target ? (0.045F + (0.040F * flash)) : (0.085F + (0.085F * flash));
    renderer->fireball(impact.position,
                       impact.hit_target ? QVector3D(1.0F, 0.46F, 0.36F)
                                         : QVector3D(1.0F, 0.94F, 0.76F),
                       flash_radius * scale,
                       (impact.hit_target ? 1.5F : 2.2F) * flash * flash,
                       time * 1.7F);

    Render::LocalLight spark_light;
    spark_light.position = impact.position + QVector3D(0.0F, 0.12F, 0.0F);
    spark_light.color = impact.hit_target ? QVector3D(1.0F, 0.44F, 0.30F)
                                          : QVector3D(1.0F, 0.90F, 0.68F);
    spark_light.radius = 2.6F * scale;
    spark_light.intensity = (impact.hit_target ? 0.85F : 1.35F) * flash;
    renderer->local_light(spark_light);
  }
}

void render_plain_arrow_impact(Renderer* renderer,
                               const Game::Systems::ProjectileImpactEvent& impact,
                               ProjectileRelation relation,
                               bool reduced_effects,
                               float progress,
                               float fade) {
  float const time = renderer->get_animation_time();
  float const scale = std::max(0.5F, impact.scale);
  bool const connected = impact.hit_target && impact.damage_applied;

  if (connected) {
    float const flash = std::clamp(1.0F - (progress * 5.0F), 0.0F, 1.0F);
    QVector3D const color = relation == ProjectileRelation::Incoming
                                ? QVector3D(1.0F, 0.30F, 0.20F)
                                : QVector3D(1.0F, 0.62F, 0.34F);
    if (flash > 0.0F) {
      renderer->fireball(impact.position,
                         color,
                         (0.05F + 0.06F * flash) * scale,
                         (reduced_effects ? 1.0F : 1.6F) * flash,
                         time * 1.5F);
    }
    if (!reduced_effects) {
      QVector3D const incoming = impact.incoming_direction;
      QVector3D lateral =
          QVector3D::crossProduct(incoming, QVector3D(0.0F, 1.0F, 0.0F));
      if (lateral.lengthSquared() < 1.0e-5F) {
        lateral = QVector3D(1.0F, 0.0F, 0.0F);
      }
      lateral.normalize();
      float const burst = 1.0F - std::pow(1.0F - progress, 2.2F);
      constexpr int k_spark_count = 5;
      for (int spark = 0; spark < k_spark_count; ++spark) {
        float const seed = static_cast<float>(spark) * 2.399963F;
        QVector3D const spray = ((-incoming) + lateral * std::cos(seed) * 0.5F +
                                 QVector3D(0.0F, 0.35F + 0.25F * std::sin(seed), 0.0F))
                                    .normalized();
        renderer->metal_spark(impact.position + spray * (0.25F + 0.45F * burst) * scale,
                              color,
                              0.04F * scale,
                              1.1F * fade * fade,
                              impact.age + seed * 0.03F,
                              spray);
      }
    }
    renderer->combat_dust(impact.position - impact.incoming_direction * 0.10F * scale,
                          QVector3D(0.46F, 0.09F, 0.06F),
                          (0.14F + 0.22F * progress) * scale,
                          0.75F * fade,
                          time + impact.age);
    return;
  }

  if (impact.hit_target || reduced_effects) {
    return;
  }

  renderer->combat_dust(impact.position,
                        QVector3D(0.60F, 0.55F, 0.45F),
                        (0.12F + 0.26F * progress) * scale,
                        0.70F * fade,
                        time + impact.age);
}

void render_projectile_impact(Renderer* renderer,
                              const Game::Systems::ProjectileImpactEvent& impact,
                              ProjectileRelation relation = ProjectileRelation::Neutral,
                              bool reduced_effects = false) {
  float const progress =
      std::clamp(impact.age / std::max(impact.lifetime, 1.0e-4F), 0.0F, 1.0F);
  float const fade = 1.0F - progress;

  if (impact.kind == Game::Systems::ProjectileKind::Stone) {
    renderer->stone_impact(impact.position + QVector3D(0.0F, 0.08F, 0.0F),
                           QVector3D(0.72F, 0.60F, 0.42F),
                           0.82F * impact.scale,
                           1.65F,
                           impact.age);
    return;
  }

  if (impact.kind == Game::Systems::ProjectileKind::FlamingStone) {
    renderer->stone_impact(impact.position + QVector3D(0.0F, 0.08F, 0.0F),
                           QVector3D(0.78F, 0.52F, 0.30F),
                           0.86F * impact.scale,
                           1.80F,
                           impact.age);
    float const burst = 0.22F + 0.85F * std::sin(progress * std::numbers::pi_v<float>);
    renderer->fireball(impact.position + QVector3D(0.0F, 0.18F, 0.0F),
                       QVector3D(1.0F, 0.32F, 0.05F),
                       burst * impact.scale * 0.6F,
                       1.45F * fade,
                       renderer->get_animation_time() + impact.age * 2.1F);
    renderer->fireball(impact.position + QVector3D(0.0F, 0.34F, 0.0F),
                       QVector3D(1.0F, 0.70F, 0.22F),
                       burst * impact.scale * 0.28F,
                       1.70F * fade,
                       renderer->get_animation_time() * 1.31F);
    return;
  }

  if (impact.kind == Game::Systems::ProjectileKind::Fireball) {

    float const growth = 1.0F - std::pow(1.0F - progress, 2.4F);
    float const cooling = 1.0F - progress;
    float const flash_life = std::clamp(1.0F - progress * 3.2F, 0.0F, 1.0F);
    float const scale = std::max(0.35F, impact.scale);
    float const time = renderer->get_animation_time();

    float const outer_radius = (0.22F + 0.82F * growth) * scale;

    renderer->fireball(impact.position + QVector3D(0.0F, 0.26F * growth, 0.0F),
                       QVector3D(0.34F, 0.085F, 0.02F),
                       outer_radius * 1.12F,
                       (0.40F + 1.05F * cooling * cooling) * 1.25F,
                       time + impact.age * 1.9F);

    renderer->fireball(impact.position + QVector3D(0.0F, 0.22F * growth, 0.0F),
                       QVector3D(0.92F, 0.30F, 0.05F),
                       outer_radius,
                       (0.45F + 1.25F * cooling * cooling) * 1.35F,
                       time * 1.13F + impact.age * 2.3F);

    renderer->fireball(impact.position + QVector3D(0.0F, 0.14F * growth, 0.0F),
                       QVector3D(1.0F, 0.52F, 0.10F),
                       outer_radius * 0.58F,
                       (0.35F + 1.55F * cooling) * 1.25F,
                       time * 1.37F + impact.age);

    if (flash_life > 0.0F) {

      QVector3D const heart_offset(0.10F * scale * std::sin(impact.age * 9.0F),
                                   0.08F + 0.16F * growth * scale,
                                   0.10F * scale * std::cos(impact.age * 7.0F));
      renderer->fireball(impact.position + heart_offset,
                         QVector3D(1.0F, 0.82F, 0.44F),
                         outer_radius * (0.14F + 0.14F * flash_life),
                         2.1F * flash_life * flash_life,
                         time * 1.71F);
    }

    constexpr int k_ember_count = 9;
    for (int ember = 0; ember < k_ember_count; ++ember) {
      float const seed = static_cast<float>(ember) * 2.399963F;
      float const spread = (0.55F + 0.75F * std::sin(seed * 3.1F)) * scale;
      float const travel = growth * (0.9F + 0.5F * std::cos(seed * 1.7F));
      QVector3D const offset(std::cos(seed) * spread * travel,
                             (0.40F + 0.60F * std::sin(seed * 2.3F)) * travel * scale,
                             std::sin(seed) * spread * travel);
      renderer->metal_spark(impact.position + offset,
                            QVector3D(1.0F, 0.42F, 0.09F),
                            0.085F * scale,
                            1.15F * cooling,
                            impact.age + seed * 0.05F,
                            offset);
    }

    float const smoke = std::clamp((progress - 0.12F) * 1.7F, 0.0F, 1.0F);
    if (smoke > 0.0F) {
      renderer->combat_dust(
          impact.position + QVector3D(0.0F, (0.30F + 0.95F * progress) * scale, 0.0F),
          QVector3D(0.30F, 0.26F, 0.24F),
          (0.70F + 1.25F * progress) * scale,
          1.25F * smoke * cooling,
          impact.age);
      renderer->combat_dust(impact.position +
                                QVector3D(0.28F * scale,
                                          (0.55F + 1.20F * progress) * scale,
                                          -0.20F * scale),
                            QVector3D(0.24F, 0.21F, 0.20F),
                            (0.45F + 0.95F * progress) * scale,
                            0.85F * smoke * cooling,
                            impact.age * 1.3F + 0.7F);
    }

    Render::LocalLight blast;
    blast.position = impact.position + QVector3D(0.0F, 0.35F * scale, 0.0F);
    blast.color = QVector3D(1.0F, 0.42F, 0.13F);
    blast.radius = std::clamp((3.2F + 2.4F * growth) * scale, 3.0F, 7.5F);
    blast.intensity = (0.35F + 1.25F * flash_life) * cooling;
    renderer->local_light(blast);
    return;
  }

  if (impact.aimed_shot) {
    render_aimed_arrow_impact(renderer, impact, progress, fade);
    return;
  }

  if (!impact.ballista_bolt) {
    if (relation != ProjectileRelation::Neutral) {
      render_plain_arrow_impact(
          renderer, impact, relation, reduced_effects, progress, fade);
    }
    return;
  }

  renderer->combat_dust(impact.position,
                        QVector3D(0.58F, 0.52F, 0.42F),
                        0.30F * impact.scale,
                        0.62F * fade,
                        renderer->get_animation_time() + impact.age);
}

void render_spent_projectile(Renderer* renderer,
                             const Game::Systems::SpentProjectile& spent) {
  float const alpha = Game::Systems::spent_projectile_alpha(spent);
  if (alpha <= 0.01F) {
    return;
  }

  auto* shaft = Geom::Arrow::get_shaft();
  auto* tip = Geom::Arrow::get_tip();
  auto* fletching = Geom::Arrow::get_fletching();
  if (shaft == nullptr || tip == nullptr || fletching == nullptr) {
    return;
  }

  float const z_scale =
      (spent.ballista_bolt ? k_bolt_z_scale : Geom::Arrow::k_arrow_z_scale) *
      spent.scale;
  float const xy_scale =
      (spent.ballista_bolt ? k_bolt_xy_scale : Geom::Arrow::k_arrow_xy_scale) *
      spent.scale;

  float const above_ground =
      (1.0F - spent.embed) * Geom::Arrow::k_total_length * z_scale;
  QMatrix4x4 model = projectile_model_at(
      spent.position - spent.direction * above_ground, spent.direction);
  model.rotate(spent.roll_deg, QVector3D(0, 0, 1));
  model.scale(xy_scale, xy_scale, z_scale);

  bool const cursed = spent.kind == Game::Systems::ProjectileKind::CursedArrow;
  QVector3D const shaft_color =
      cursed ? QVector3D(0.34F, 0.15F, 0.47F)
             : scaled_color(Geom::Arrow::shaft_color(spent.color), 0.86F);
  QVector3D const fletch_color =
      cursed ? QVector3D(0.48F, 0.19F, 0.68F)
             : scaled_color(Geom::Arrow::fletch_color(spent.color), 0.80F);

  renderer->mesh(shaft, model, shaft_color, nullptr, alpha);
  renderer->mesh(fletching, model, fletch_color, nullptr, alpha);

  constexpr float k_head_fraction =
      Geom::Arrow::k_tip_length / Geom::Arrow::k_total_length;
  if (spent.embed < k_head_fraction) {
    renderer->mesh(tip, model, Geom::Arrow::tip_color(0.82F), nullptr, alpha);
  }
}

} // namespace

void render_arrow_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::ArrowProjectile& arrow,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model,
                             ProjectileRelation relation) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  auto* arrow_shaft_mesh = Geom::Arrow::get_shaft();
  auto* arrow_tip_mesh = Geom::Arrow::get_tip();
  auto* arrow_fletching_mesh = Geom::Arrow::get_fletching();
  if ((arrow_shaft_mesh == nullptr) || (arrow_tip_mesh == nullptr) ||
      (arrow_fletching_mesh == nullptr)) {
    return;
  }

  const QVector3D delta = arrow.get_end() - arrow.get_start();
  const float dist = std::max(0.001F, delta.length());

  QMatrix4x4 model = base_model;

  constexpr float k_arc_height_multiplier = 8.0F;
  constexpr float k_arc_center_offset = 0.5F;
  float const vy = (arrow.get_end().y() - arrow.get_start().y()) / dist;
  float const pitch_deg =
      -std::atan2(vy - (k_arc_height_multiplier * arrow.get_arc_height() *
                        (arrow.get_progress() - k_arc_center_offset) / dist),
                  1.0F) *
      (180.0F / std::numbers::pi_v<float>);
  model.rotate(pitch_deg, QVector3D(1, 0, 0));

  if (arrow.get_kind() == Game::Systems::ProjectileKind::Fireball) {
    auto* fireball_mesh = get_unit_sphere();
    if (fireball_mesh == nullptr) {
      return;
    }

    float const animation_time = renderer->get_animation_time();
    float const pulse =
        0.94F + 0.08F * std::sin(animation_time * 12.0F + arrow.get_progress() * 32.0F);
    float const spell_phase = animation_time + arrow.get_progress() * 3.7F;

    renderer->fireball(
        pos, QVector3D(0.34F, 0.09F, 0.02F), 0.205F * pulse, 0.72F, spell_phase);
    renderer->fireball(pos,
                       QVector3D(1.0F, 0.42F, 0.07F),
                       0.128F * pulse,
                       1.25F,
                       spell_phase * 1.37F + 1.9F);
    renderer->fireball(pos,
                       QVector3D(1.0F, 0.88F, 0.55F),
                       0.058F * pulse,
                       2.15F,
                       spell_phase * 1.83F + 4.4F);

    constexpr int k_trail_segments = 18;
    for (int trail_idx = 1; trail_idx <= k_trail_segments; ++trail_idx) {
      float const trail_t =
          arrow.get_progress() - static_cast<float>(trail_idx) * 0.013F;
      if (trail_t < 0.0F) {
        continue;
      }

      QVector3D trail_pos = arrow.get_start() + delta * trail_t;
      float const trail_h = arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t);
      float const age =
          static_cast<float>(trail_idx) / static_cast<float>(k_trail_segments);
      trail_pos.setY(trail_pos.y() + trail_h + age * 0.055F);

      float const heat = (1.0F - age) * (1.0F - age);
      QVector3D const trail_color = QVector3D(0.95F, 0.30F, 0.05F) * heat +
                                    QVector3D(0.075F, 0.065F, 0.060F) * (1.0F - heat);
      renderer->fireball(trail_pos,
                         trail_color,
                         (0.105F + 0.155F * age) * pulse,
                         std::max(0.09F, 0.85F * heat),
                         spell_phase - static_cast<float>(trail_idx) * 0.13F);
    }

    QMatrix4x4 core_model = model;
    core_model.scale(0.026F * pulse, 0.026F * pulse, 0.026F * pulse);
    renderer->mesh(
        fireball_mesh, core_model, QVector3D(1.0F, 0.96F, 0.80F), nullptr, 1.0F);

    for (int spark_idx = 0; spark_idx < 3; ++spark_idx) {
      float const seed = spell_phase * (5.2F + static_cast<float>(spark_idx) * 1.7F) +
                         static_cast<float>(spark_idx) * 2.1F;
      QVector3D const orbit(std::sin(seed) * 0.070F,
                            std::cos(seed * 0.83F) * 0.042F + 0.02F,
                            std::cos(seed * 1.13F) * 0.070F);
      renderer->metal_spark(
          pos + orbit,
          QVector3D(1.0F, 0.50F, 0.12F),
          0.045F,
          0.9F,
          std::fmod(spell_phase * 0.7F + static_cast<float>(spark_idx), 0.28F));
    }

    Render::LocalLight spell_light;
    spell_light.position = pos;
    spell_light.color = QVector3D(1.0F, 0.46F, 0.15F);
    spell_light.radius = 3.4F;
    spell_light.intensity = 0.85F * pulse;
    renderer->local_light(spell_light);
    return;
  }

  if (arrow.is_ballista_bolt()) {

    float const spin_speed = 190.0F;
    float const spin_angle = arrow.get_progress() * spin_speed;
    model.rotate(spin_angle, QVector3D(0, 0, 1));

    QMatrix4x4 bolt_model = model;
    bolt_model.translate(
        0.0F, 0.0F, -k_bolt_z_scale * Geom::Arrow::k_arrow_z_translate_factor);
    bolt_model.scale(k_bolt_xy_scale, k_bolt_xy_scale, k_bolt_z_scale);

    QVector3D const base_color = arrow.get_color();
    QVector3D const wood_color =
        scaled_color(Geom::Arrow::shaft_color(base_color), 0.84F);
    QVector3D const iron_color = Geom::Arrow::tip_color(0.94F);
    QVector3D const vane_color =
        scaled_color(Geom::Arrow::fletch_color(base_color), 0.86F);

    renderer->mesh(arrow_shaft_mesh, bolt_model, wood_color, nullptr, 1.0F);
    renderer->mesh(arrow_tip_mesh, bolt_model, iron_color, nullptr, 1.0F);
    renderer->mesh(arrow_fletching_mesh, bolt_model, vane_color, nullptr, 1.0F);
    draw_arrow_glow(renderer,
                    arrow_shaft_mesh,
                    arrow_tip_mesh,
                    bolt_model,
                    Geom::Arrow::glow_color(base_color),
                    1.15F);

    for (int trail_idx = 1; trail_idx <= 2; trail_idx++) {
      float const trail_t = arrow.get_progress() - (trail_idx * 0.055F);
      if (trail_t <= 0.0F) {
        continue;
      }
      QVector3D trail_pos = arrow.get_start() + delta * trail_t;
      trail_pos.setY(trail_pos.y() +
                     arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t));

      QVector3D tangent = delta;
      tangent.setY(tangent.y() +
                   arrow.get_arc_height() * 4.0F * (1.0F - 2.0F * trail_t));
      QMatrix4x4 trail_model = projectile_model_at(trail_pos, tangent);
      trail_model.rotate(trail_t * spin_speed, QVector3D(0, 0, 1));
      trail_model.translate(
          0.0F, 0.0F, -k_bolt_z_scale * Geom::Arrow::k_arrow_z_translate_factor);
      trail_model.scale(k_bolt_xy_scale * k_streak_xy_scale,
                        k_bolt_xy_scale * k_streak_xy_scale,
                        k_bolt_z_scale);
      renderer->mesh(arrow_shaft_mesh,
                     trail_model,
                     wood_color,
                     nullptr,
                     0.34F - (0.12F * static_cast<float>(trail_idx)));
    }
  } else {
    bool const aimed = arrow.visual_style() == Game::Systems::ArrowVisualStyle::Aimed;
    int const trail_segments = aimed ? 6 : 2;
    float const trail_step = aimed ? 0.17F : 0.38F;
    if (arrow.trail_alpha() > 0.001F && arrow.trail_length() > 0.0F) {
      for (int segment = 1; segment <= trail_segments; ++segment) {
        float const trail_t =
            arrow.get_progress() -
            arrow.trail_length() * (0.55F + trail_step * static_cast<float>(segment));
        if (trail_t <= 0.0F) {
          continue;
        }
        QVector3D trail_pos = arrow.get_start() + delta * trail_t;
        trail_pos.setY(trail_pos.y() +
                       arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t));
        QVector3D tangent = delta;
        tangent.setY(tangent.y() +
                     arrow.get_arc_height() * 4.0F * (1.0F - 2.0F * trail_t));
        QMatrix4x4 trail_model = projectile_model_at(trail_pos, tangent);
        trail_model.rotate(arrow.roll_deg() + trail_t * arrow.spin_rate_deg(),
                           QVector3D(0, 0, 1));
        float const trail_scale = arrow.get_scale() * (1.0F - 0.06F * segment);
        trail_model.translate(0.0F,
                              0.0F,
                              -Geom::Arrow::k_arrow_z_scale * trail_scale *
                                  Geom::Arrow::k_arrow_z_translate_factor);
        float const streak_xy = aimed ? k_streak_xy_scale * 0.72F : k_streak_xy_scale;
        trail_model.scale(Geom::Arrow::k_arrow_xy_scale * trail_scale * streak_xy,
                          Geom::Arrow::k_arrow_xy_scale * trail_scale * streak_xy,
                          Geom::Arrow::k_arrow_z_scale * trail_scale *
                              arrow.length_scale() * (aimed ? 1.35F : 1.0F));
        float const falloff = 1.0F - (static_cast<float>(segment) /
                                      static_cast<float>(trail_segments + 1));
        float const alpha =
            (aimed ? arrow.trail_alpha() * falloff * falloff
                   : arrow.trail_alpha() *
                         (0.55F - 0.16F * static_cast<float>(segment))) *
            relation_trail_boost(relation);
        renderer->mesh(arrow_shaft_mesh,
                       trail_model,
                       scaled_color(Geom::Arrow::shaft_color(arrow.get_color()),
                                    aimed ? 1.10F : 0.88F),
                       nullptr,
                       alpha);
      }
    }

    if (aimed) {

      float const flight = std::clamp(arrow.get_progress(), 0.0F, 1.0F);
      float const settle = std::clamp(flight * 6.0F, 0.0F, 1.0F);
      QVector3D const glow_color = Geom::Arrow::glow_color(arrow.get_color());

      renderer->metal_spark(pos,
                            QVector3D(1.0F, 0.92F, 0.72F),
                            0.055F * arrow.get_scale(),
                            1.35F * settle,
                            arrow.get_progress() * 0.24F,
                            delta.normalized());

      Render::LocalLight flight_light;
      flight_light.position = pos;
      flight_light.color =
          (glow_color * 0.45F) + (QVector3D(1.0F, 0.90F, 0.66F) * 0.55F);
      flight_light.radius = 2.6F;
      flight_light.intensity = 0.65F * settle * arrow.brightness();
      renderer->local_light(flight_light);
    }
    model.rotate(arrow.roll_deg() + arrow.get_progress() * arrow.spin_rate_deg(),
                 QVector3D(0, 0, 1));
    constexpr float arrow_z_scale = Geom::Arrow::k_arrow_z_scale;
    constexpr float arrow_xy_scale = Geom::Arrow::k_arrow_xy_scale;
    constexpr float arrow_z_translate_factor = Geom::Arrow::k_arrow_z_translate_factor;
    model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
    model.scale(arrow_xy_scale * arrow.get_scale(),
                arrow_xy_scale * arrow.get_scale(),
                arrow_z_scale * arrow.get_scale() * arrow.length_scale());

    QVector3D const team_color = arrow.get_color();
    float const brightness = arrow.brightness() * relation_brightness(relation);
    QVector3D shaft_color =
        scaled_color(Geom::Arrow::shaft_color(team_color), brightness);
    QVector3D tip_color = scaled_color(Geom::Arrow::tip_color(), brightness);
    QVector3D fletch_color =
        scaled_color(Geom::Arrow::fletch_color(team_color), brightness);
    if (arrow.get_kind() == Game::Systems::ProjectileKind::CursedArrow) {
      shaft_color = QVector3D(0.42F, 0.18F, 0.58F);
      tip_color = QVector3D(0.72F, 0.32F, 0.92F);
      fletch_color = QVector3D(0.58F, 0.22F, 0.82F);
    }
    renderer->mesh(arrow_shaft_mesh, model, shaft_color, nullptr, 1.0F);
    renderer->mesh(arrow_tip_mesh, model, tip_color, nullptr, 1.0F);

    QVector3D const glow =
        arrow.get_kind() == Game::Systems::ProjectileKind::CursedArrow
            ? QVector3D(0.78F, 0.35F, 1.0F)
            : relation_glow(Geom::Arrow::glow_color(team_color), relation);
    draw_arrow_glow(renderer,
                    arrow_shaft_mesh,
                    arrow_tip_mesh,
                    model,
                    aimed ? (glow * 0.55F) + (QVector3D(1.0F, 0.94F, 0.74F) * 0.45F)
                          : glow,
                    brightness * (aimed ? 1.6F : 1.0F));

    QMatrix4x4 fletch_model = model;
    fletch_model.translate(
        0.0F, 0.0F, -arrow_z_scale * Geom::Arrow::k_fletch_z_offset_factor);
    fletch_model.scale(Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_z_scale);
    renderer->mesh(arrow_fletching_mesh, fletch_model, fletch_color, nullptr, 1.0F);
  }
}

void render_stone_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::StoneProjectile& stone,
                             const QVector3D& position,
                             const QMatrix4x4& base_model) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  auto* stone_mesh = Geom::Stone::get();
  if (stone_mesh == nullptr) {
    return;
  }

  QMatrix4x4 model = base_model;

  float const tumble_speed = 720.0F;
  float const tumble_angle = stone.get_progress() * tumble_speed;
  model.rotate(tumble_angle, QVector3D(1, 0.5F, 0.3F).normalized());

  float const stone_scale = stone.get_scale();
  float const mesh_scale = stone_scale * Geom::Stone::k_projectile_radius;
  model.scale(mesh_scale, mesh_scale, mesh_scale);

  bool const flaming = stone.get_kind() == Game::Systems::ProjectileKind::FlamingStone;
  QVector3D const stone_color =
      flaming ? QVector3D(0.32F, 0.24F, 0.20F) : QVector3D(0.45F, 0.42F, 0.38F);
  renderer->mesh(stone_mesh, model, stone_color, nullptr, 1.0F);

  if (!flaming) {
    return;
  }

  float const animation_time = renderer->get_animation_time();
  float const pulse =
      0.92F + 0.10F * std::sin(animation_time * 11.0F + stone.get_progress() * 27.0F);
  float const core_radius = 0.20F * stone_scale * pulse;
  renderer->fireball(position,
                     QVector3D(0.95F, 0.26F, 0.04F),
                     core_radius,
                     0.92F,
                     animation_time + stone.get_progress() * 3.1F);
  renderer->fireball(position,
                     QVector3D(1.0F, 0.66F, 0.18F),
                     core_radius * 0.55F,
                     1.28F,
                     animation_time * 1.29F + 1.4F);

  const QVector3D delta = stone.get_end() - stone.get_start();
  constexpr int k_trail_segments = 4;
  for (int trail_idx = 1; trail_idx <= k_trail_segments; ++trail_idx) {
    float const trail_t = stone.get_progress() - static_cast<float>(trail_idx) * 0.045F;
    if (trail_t < 0.0F) {
      continue;
    }

    QVector3D trail_pos = stone.get_start() + delta * trail_t;
    trail_pos.setY(trail_pos.y() +
                   stone.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t));

    float const falloff =
        1.0F - static_cast<float>(trail_idx) / static_cast<float>(k_trail_segments);
    renderer->fireball(trail_pos,
                       QVector3D(0.86F, 0.20F, 0.03F) * (0.35F + 0.65F * falloff),
                       core_radius * (0.30F + 0.42F * falloff),
                       0.72F * falloff,
                       animation_time * 1.13F + static_cast<float>(trail_idx));
  }
}

void render_projectiles(Renderer* renderer,
                        ResourceManager* resources,
                        const Game::Systems::ProjectileSystem& projectile_system,
                        const ProjectileViewContext* view) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }
  auto relation_of = [&](std::uint64_t attacker, std::uint64_t target) {
    return view != nullptr ? view->relation_for(attacker, target)
                           : ProjectileRelation::Neutral;
  };
  bool const reduced_effects = view != nullptr && view->reduced_effects;

  const auto& projectiles = projectile_system.projectiles();

  for (const auto& projectile : projectiles) {
    if (!projectile->is_active() || projectile->get_progress() < 0.0F) {
      continue;
    }

    const QVector3D delta = projectile->get_end() - projectile->get_start();
    const float dist = std::max(0.001F, delta.length());
    QVector3D pos = projectile->get_start() + delta * projectile->get_progress();

    float const h = projectile->get_arc_height() * 4.0F * projectile->get_progress() *
                    (1.0F - projectile->get_progress());
    pos.setY(pos.y() + h);

    QMatrix4x4 model;
    model.translate(pos.x(), pos.y(), pos.z());

    QVector3D const dir = delta.normalized();
    float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
    model.rotate(yaw_deg, QVector3D(0, 1, 0));

    if (const auto* arrow =
            dynamic_cast<const Game::Systems::ArrowProjectile*>(projectile.get())) {
      render_arrow_projectile(
          renderer,
          resources,
          *arrow,
          pos,
          model,
          relation_of(arrow->get_attacker_id(), arrow->get_target_id()));
    } else if (const auto* stone = dynamic_cast<const Game::Systems::StoneProjectile*>(
                   projectile.get())) {
      render_stone_projectile(renderer, resources, *stone, pos, model);
    }
  }

  for (auto const& spent : projectile_system.spent_projectiles()) {
    render_spent_projectile(renderer, spent);
  }

  int plain_impact_budget = k_projectile_impact_effect_budget;
  for (auto const& impact : projectile_system.impacts()) {
    ProjectileRelation relation = ProjectileRelation::Neutral;
    if (!impact.aimed_shot && !impact.ballista_bolt &&
        (impact.kind == Game::Systems::ProjectileKind::Arrow ||
         impact.kind == Game::Systems::ProjectileKind::CursedArrow)) {
      if (plain_impact_budget <= 0) {
        continue;
      }
      relation = relation_of(impact.attacker_id, impact.target_id);
      if (relation != ProjectileRelation::Neutral) {
        --plain_impact_budget;
      }
    }
    render_projectile_impact(renderer, impact, relation, reduced_effects);
  }
}

} // namespace Render::GL

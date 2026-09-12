#include "commander_aura_renderer.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "render/draw_commands.h"
#include "render/scene_renderer.h"
#include "render/selection_ring_layout.h"

namespace Render::GL {

namespace {

auto get_commander_aura_color(Game::Systems::NationID nation_id) -> QVector3D {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return {1.0F, 0.85F, 0.3F};
  case Game::Systems::NationID::Carthage:
    return {0.9F, 0.4F, 1.0F};
  default:
    return {1.0F, 0.7F, 0.2F};
  }
}

constexpr float k_aura_ring_lift = 0.06F;
constexpr float k_aura_boundary_thickness = 0.22F;
constexpr float k_aura_boundary_alpha = 0.62F;
constexpr float k_aura_boundary_pulse = 0.14F;
constexpr float k_aura_pulse_seconds = 0.9F;
constexpr float k_aura_pulse_thickness = 0.9F;
constexpr float k_aura_pulse_alpha = 0.95F;
constexpr float k_buff_ring_thickness = 0.10F;

} // namespace

void render_commander_auras(Renderer* renderer,
                            ResourceManager*,
                            Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();

  auto commanders = world->collect_entities_with<Engine::Core::CommanderComponent>();

  for (auto* entity : commanders) {
    Engine::Core::EntityID const id = entity->get_id();
    if (world->has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }

    auto* commander = world->try_get<Engine::Core::CommanderComponent>(id);
    auto* transform = world->try_get<Engine::Core::TransformComponent>(id);
    auto* unit_comp = world->try_get<Engine::Core::UnitComponent>(id);

    if (commander == nullptr || transform == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    QVector3D const banner_color = unit_comp != nullptr
                                       ? get_commander_aura_color(unit_comp->nation_id)
                                       : QVector3D(1.0F, 0.7F, 0.2F);

    float const readiness =
        commander->signature_cooldown > 0.0F
            ? std::clamp(1.0F - commander->signature_cooldown_remaining /
                                    commander->signature_cooldown,
                         0.0F,
                         1.0F)
            : 1.0F;
    float const gathering = std::clamp((readiness - 0.72F) / 0.28F, 0.0F, 1.0F);
    if (gathering > 0.01F || commander->signature_strike_active) {
      float const footprint = std::max(transform->scale.x, transform->scale.z);
      float const base_radius = std::clamp(0.80F + footprint * 0.35F, 0.92F, 1.20F);
      float const pulse = 0.5F + 0.5F * std::sin(animation_time * 4.2F +
                                                 static_cast<float>(entity->get_id()));
      float const glow_intensity =
          commander->signature_strike_active
              ? 0.11F
              : gathering * gathering * (0.030F + 0.026F * pulse);
      float const glow_radius =
          commander->signature_strike_active ? base_radius * 1.35F : base_radius;
      renderer->healer_aura(QVector3D(transform->position.x,
                                      transform->position.y + 0.04F,
                                      transform->position.z),
                            banner_color,
                            glow_radius,
                            glow_intensity,
                            animation_time);
    }

    if (!commander->aura_ability_active) {
      continue;
    }

    QVector3D const position(
        transform->position.x, transform->position.y + 0.1F, transform->position.z);
    float const radius = commander->aura_radius;
    float const elapsed = std::max(
        0.0F, commander->aura_ability_duration - commander->aura_ability_remaining);

    renderer->healer_aura(position, banner_color, radius, 1.0F, animation_time);

    GroundMarkerCmd boundary;
    boundary.center = QVector3D(transform->position.x,
                                transform->position.y + k_aura_ring_lift,
                                transform->position.z);
    boundary.outer_radius = radius;
    boundary.thickness = k_aura_boundary_thickness;
    boundary.color = banner_color;
    boundary.alpha =
        k_aura_boundary_alpha + k_aura_boundary_pulse * std::sin(animation_time * 3.0F);
    renderer->ground_marker(boundary);

    if (elapsed < k_aura_pulse_seconds) {
      float const t = elapsed / k_aura_pulse_seconds;
      float const eased = 1.0F - (1.0F - t) * (1.0F - t);
      GroundMarkerCmd pulse;
      pulse.center = boundary.center;
      pulse.outer_radius = std::max(0.4F, radius * eased);
      pulse.thickness = k_aura_pulse_thickness * (1.0F - 0.5F * t);
      pulse.color = banner_color;
      pulse.alpha = k_aura_pulse_alpha * (1.0F - t);
      pulse.focused = true;
      renderer->ground_marker(pulse);
    }
  }

  for (auto* entity :
       world->collect_entities_with<Engine::Core::CommanderAuraBuffComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto const* buff =
        entity->get_component<Engine::Core::CommanderAuraBuffComponent>();
    auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (buff == nullptr || !buff->active || transform == nullptr || unit == nullptr ||
        unit->health <= 0) {
      continue;
    }

    Game::Systems::NationID glow_nation = unit->nation_id;
    if (auto* source = world->get_entity(buff->source_commander_id)) {
      if (auto const* source_unit =
              source->get_component<Engine::Core::UnitComponent>()) {
        glow_nation = source_unit->nation_id;
      }
    }

    const float pulse = 0.72F + 0.18F * std::sin(animation_time * 5.5F +
                                                 static_cast<float>(entity->get_id()));
    std::span<const Engine::Core::FormationSoldierPresentation> soldiers;
    if (auto const* formation =
            entity->get_component<Engine::Core::FormationPresentationComponent>()) {
      soldiers = formation->soldiers;
    }
    const float footprint = std::max(transform->scale.x, transform->scale.z);
    const float glow_radius = std::clamp(0.72F + footprint * 0.35F, 0.78F, 1.15F);
    auto const root = Render::Entity::resolve_formation_root(entity, *transform);
    const auto placements = build_selection_ring_layout({.soldiers = soldiers,
                                                         .ring_size = glow_radius,
                                                         .position = root.position,
                                                         .yaw_degrees = root.yaw});
    for (auto const& placement : placements) {
      GroundMarkerCmd ring;
      ring.center = QVector3D(placement.world_x,
                              transform->position.y + k_aura_ring_lift,
                              placement.world_z);
      ring.outer_radius = placement.ring_size;
      ring.thickness = k_buff_ring_thickness;
      ring.color = get_commander_aura_color(glow_nation);
      ring.alpha = pulse;
      renderer->ground_marker(ring);
    }
  }
}

} // namespace Render::GL

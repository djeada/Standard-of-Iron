#include "commander_aura_renderer.h"

#include <algorithm>
#include <cmath>

#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/nation_id.h"
#include "../scene_renderer.h"
#include "../selection_ring_layout.h"

namespace Render::GL {

namespace {

auto get_commander_aura_color(Game::Systems::NationID nation_id) -> QVector3D {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return QVector3D(1.0F, 0.85F, 0.3F);
  case Game::Systems::NationID::Carthage:
    return QVector3D(0.9F, 0.4F, 1.0F);
  default:
    return QVector3D(1.0F, 0.7F, 0.2F);
  }
}

} // namespace

void render_commander_auras(Renderer* renderer,
                            ResourceManager*,
                            Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();

  auto commanders = world->get_entities_with<Engine::Core::CommanderComponent>();

  for (auto* entity : commanders) {
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* commander = entity->get_component<Engine::Core::CommanderComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();

    if (commander == nullptr || transform == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!commander->aura_ability_active) {
      continue;
    }

    QVector3D const position(
        transform->position.x, transform->position.y + 0.1F, transform->position.z);
    float const radius = commander->aura_radius;
    float const intensity = 0.10F;

    QVector3D const color = unit_comp != nullptr
                                ? get_commander_aura_color(unit_comp->nation_id)
                                : QVector3D(1.0F, 0.7F, 0.2F);

    renderer->healer_aura(position, color, radius, intensity, animation_time);
  }

  for (auto* entity :
       world->get_entities_with<Engine::Core::CommanderAuraBuffComponent>()) {
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

    const float pulse = 0.26F + 0.08F * std::sin(animation_time * 5.5F +
                                                 static_cast<float>(entity->get_id()));
    std::span<const Engine::Core::FormationSoldierPresentation> soldiers;
    if (auto const* formation =
            entity->get_component<Engine::Core::FormationPresentationComponent>()) {
      soldiers = formation->soldiers;
    }
    const float footprint = std::max(transform->scale.x, transform->scale.z);
    const float glow_radius = std::clamp(0.72F + footprint * 0.35F, 0.78F, 1.15F);
    const auto placements = build_selection_ring_layout(
        {.soldiers = soldiers,
         .ring_size = glow_radius,
         .position = QVector3D(
             transform->position.x, transform->position.y, transform->position.z),
         .yaw_degrees = transform->rotation.y});
    for (auto const& placement : placements) {
      renderer->healer_aura(QVector3D(placement.world_x,
                                      transform->position.y + 0.35F,
                                      placement.world_z),
                            get_commander_aura_color(glow_nation),
                            placement.ring_size,
                            pulse,
                            animation_time);
    }
  }
}

} // namespace Render::GL

#include "commander_aura_renderer.h"

#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/nation_id.h"
#include "../scene_renderer.h"

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
    float const intensity = 1.0F;

    QVector3D const color =
        unit_comp != nullptr
            ? get_commander_aura_color(unit_comp->nation_id)
            : QVector3D(1.0F, 0.7F, 0.2F);

    renderer->healer_aura(position, color, radius, intensity, animation_time);
  }
}

} // namespace Render::GL

#include "healer_aura_renderer.h"

#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/healing_colors.h"
#include "../../game/systems/nation_id.h"
#include "../scene_renderer.h"

namespace Render::GL {

void render_healer_auras(Renderer* renderer,
                         ResourceManager*,
                         Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();

  auto healers = world->get_entities_with<Engine::Core::HealerComponent>();

  for (auto* healer : healers) {
    if (healer->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = healer->get_component<Engine::Core::TransformComponent>();
    auto* healer_comp = healer->get_component<Engine::Core::HealerComponent>();
    auto* unit_comp = healer->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || healer_comp == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!healer_comp->is_healing_active) {
      continue;
    }

    if (unit_comp != nullptr &&
        unit_comp->nation_id == Game::Systems::NationID::RomanRepublic) {
      continue;
    }

    QVector3D const position(
        transform->position.x, transform->position.y + 0.1F, transform->position.z);
    float const radius = healer_comp->healing_range;

    float const intensity = 1.0F;

    QVector3D const color = unit_comp != nullptr
                                ? Game::Systems::get_healing_color(unit_comp->nation_id)
                                : Game::Systems::get_carthage_healing_color();

    renderer->healer_aura(position, color, radius, intensity, animation_time);
  }
}

} // namespace Render::GL

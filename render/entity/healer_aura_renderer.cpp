#include "healer_aura_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../scene_renderer.h"

namespace Render::GL {

void render_healer_auras(Renderer *renderer, ResourceManager *,
                         Engine::Core::World *world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float animation_time = renderer->get_animation_time();

  auto healers = world->get_entities_with<Engine::Core::HealerComponent>();

  for (auto *healer : healers) {
    if (healer->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *transform = healer->get_component<Engine::Core::TransformComponent>();
    auto *healer_comp = healer->get_component<Engine::Core::HealerComponent>();
    auto *unit_comp = healer->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || healer_comp == nullptr) {
      continue;
    }

    // Skip dead healers
    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    QVector3D position(transform->position.x, transform->position.y,
                       transform->position.z);
    float radius = healer_comp->healing_range;

    // Intensity based on whether actively healing
    // Always show aura even when not healing (0.5 base, 1.0 when active)
    float intensity = healer_comp->is_healing_active ? 1.0F : 0.5F;

    // Golden-green healing color
    QVector3D color(0.4F, 1.0F, 0.5F);

    renderer->healer_aura(position, color, radius, intensity, animation_time);
  }
}

} // namespace Render::GL

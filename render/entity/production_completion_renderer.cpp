#include "production_completion_renderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component_presentation.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "render/scene_renderer.h"

namespace Render::GL {

void render_production_completions(Renderer* renderer,
                                   Engine::Core::World* world,
                                   int local_owner_id,
                                   bool reduced_motion) {
  if (renderer == nullptr || world == nullptr ||
      Game::Core::is_neutral_owner(local_owner_id)) {
    return;
  }

  for (auto [entity, effect, transform, unit] :
       world->entity_view<Engine::Core::ProductionCompletionComponent,
                          Engine::Core::TransformComponent,
                          Engine::Core::UnitComponent>()) {
    if (unit.owner_id != local_owner_id || unit.health <= 0 ||
        entity.has_component<Engine::Core::PendingRemovalComponent>() ||
        effect.remaining <= 0.0F) {
      continue;
    }

    const float age =
        Engine::Core::ProductionCompletionComponent::k_duration - effect.remaining;
    const float progress = std::clamp(
        age / Engine::Core::ProductionCompletionComponent::k_duration, 0.0F, 1.0F);
    const float fade_in = std::clamp(age / 0.18F, 0.0F, 1.0F);
    const float fade_out = 1.0F - progress * progress * (3.0F - 2.0F * progress);
    const float intensity = fade_in * fade_out;
    const QVector3D position(
        transform.position.x, transform.position.y + 0.08F, transform.position.z);
    const QVector3D gold(1.0F, 0.76F, 0.28F);
    const float time = reduced_motion ? 0.0F : age;
    const float radius =
        effect.radius * (reduced_motion ? 1.0F : 1.0F + 0.18F * progress);

    renderer->healer_aura(position, gold, radius, intensity, time);
    renderer->healer_aura(position,
                          QVector3D(1.0F, 0.94F, 0.70F),
                          radius * 0.78F,
                          intensity * 0.65F,
                          time);
    if (reduced_motion) {
      continue;
    }

    constexpr int k_glint_count = 10;
    for (int i = 0; i < k_glint_count; ++i) {
      const float phase = static_cast<float>(i) / k_glint_count;
      const float angle = phase * 2.0F * std::numbers::pi_v<float> + age * 0.65F;
      const float orbit = radius * (0.65F + 0.12F * std::sin(phase * 19.0F));
      const float height = effect.radius * (0.15F + progress * 1.5F + phase * 0.35F);
      const QVector3D glint =
          position +
          QVector3D(std::cos(angle) * orbit, height, std::sin(angle) * orbit);
      const float shimmer = 0.65F + 0.35F * std::sin(age * 7.0F + phase * 23.0F);
      renderer->metal_spark(glint,
                            gold,
                            0.12F,
                            intensity * shimmer * 0.7F,
                            0.0F,
                            QVector3D(0.0F, 1.0F, 0.0F));
    }
  }
}

} // namespace Render::GL

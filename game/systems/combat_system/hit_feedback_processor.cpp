#include "hit_feedback_processor.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../camera_visibility_service.h"
#include "combat_types.h"

#include <cmath>

namespace Game::Systems::Combat {

void process_hit_feedback(Engine::Core::World *world, float delta_time) {
  auto units = world->get_entities_with<Engine::Core::HitFeedbackComponent>();
  auto &visibility = CameraVisibilityService::instance();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *feedback = unit->get_component<Engine::Core::HitFeedbackComponent>();
    if (feedback == nullptr || !feedback->is_reacting) {
      continue;
    }

    feedback->reaction_time += delta_time;
    float const progress =
        feedback->reaction_time /
        Engine::Core::HitFeedbackComponent::kReactionDuration;

    if (progress >= 1.0F) {
      feedback->is_reacting = false;
      feedback->reaction_time = 0.0F;
      feedback->reaction_intensity = 0.0F;
      feedback->knockback_x = 0.0F;
      feedback->knockback_z = 0.0F;
    } else {
      auto *transform = unit->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        if (!visibility.should_process_detailed_effects(
                transform->position.x, transform->position.y,
                transform->position.z)) {
          continue;
        }

        float const fade = 1.0F - progress;
        float const dx = feedback->knockback_x * fade * delta_time;
        float const dz = feedback->knockback_z * fade * delta_time;
        float const displacement = std::sqrt(dx * dx + dz * dz);
        float const scale =
            (displacement > Constants::kMaxDisplacementPerFrame &&
             displacement > 0.0001F)
                ? Constants::kMaxDisplacementPerFrame / displacement
                : 1.0F;
        transform->position.x += dx * scale;
        transform->position.z += dz * scale;
      }
    }
  }
}

} 

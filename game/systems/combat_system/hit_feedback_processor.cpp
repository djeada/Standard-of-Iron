#include "hit_feedback_processor.h"

#include "../../core/component.h"
#include "../../core/world.h"

namespace Game::Systems::Combat {

void process_hit_feedback(Engine::Core::World* world, float delta_time) {
  auto units = world->get_entities_with<Engine::Core::HitFeedbackComponent>();

  for (auto* unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* feedback = unit->get_component<Engine::Core::HitFeedbackComponent>();
    if (feedback == nullptr || !feedback->is_reacting) {
      continue;
    }

    feedback->reaction_time += delta_time;
    float const progress = feedback->reaction_time /
                           Engine::Core::HitFeedbackComponent::k_reaction_duration;

    if (progress >= 1.0F) {
      feedback->is_reacting = false;
      feedback->reaction_time = 0.0F;
      feedback->reaction_intensity = 0.0F;
      feedback->knockback_x = 0.0F;
      feedback->knockback_z = 0.0F;
    }
  }
}

} // namespace Game::Systems::Combat

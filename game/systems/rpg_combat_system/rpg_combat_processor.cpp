#include "rpg_combat_processor.h"
#include "../../core/component.h"
#include "../../core/world.h"

namespace Game::Systems::RpgCombat {

void tick_rpg_combat(Engine::Core::World *world,
                     Engine::Core::EntityID commander_id, float dt) {
  if (world == nullptr || commander_id == 0) {
    return;
  }

  for (auto *staggered :
       world->get_entities_with<Engine::Core::StaggerComponent>()) {
    auto *stagger = staggered->get_component<Engine::Core::StaggerComponent>();
    if (stagger == nullptr) {
      continue;
    }
    stagger->remaining -= dt;
    if (stagger->remaining <= 0.0F) {
      staggered->remove_component<Engine::Core::StaggerComponent>();
    } else {
      auto *fb = staggered->get_component<Engine::Core::HitFeedbackComponent>();
      if (fb == nullptr) {
        fb = staggered->add_component<Engine::Core::HitFeedbackComponent>();
      }
      if (fb != nullptr) {
        fb->is_reacting = true;
        fb->reaction_time = 0.0F;
        fb->reaction_intensity = 1.0F;
      }
    }
  }

  auto *entity = world->get_entity(commander_id);
  if (entity == nullptr) {
    return;
  }

  auto *rpg = entity->get_component<Engine::Core::RpgHealthComponent>();
  auto *unit = entity->get_component<Engine::Core::UnitComponent>();
  if (rpg != nullptr && rpg->active && rpg->rpg_hp > 0 && unit != nullptr &&
      unit->health <= 0) {
    unit->health = 1;
  }
}

} // namespace Game::Systems::RpgCombat

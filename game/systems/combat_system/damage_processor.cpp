#include "damage_processor.h"

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_hit_resolver.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

void deal_damage(Engine::Core::World* world,
                 Engine::Core::Entity* target,
                 int damage,
                 Engine::Core::EntityID attacker_id) {
  auto const application = apply_unit_damage(world, target, damage, attacker_id);
  if (world == nullptr || target == nullptr || attacker_id == 0 ||
      application.queued_soldier_casualties <= 0) {
    return;
  }

  auto* attacker = world->get_entity(attacker_id);
  auto const* attacker_unit =
      attacker != nullptr ? attacker->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto const* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (attacker == nullptr || attacker_unit == nullptr || target_unit == nullptr ||
      attacker_unit->spawn_type != Game::Units::SpawnType::Elephant) {
    return;
  }

  bool const infantry_target =
      !Game::Units::is_cavalry(target_unit->spawn_type) &&
      target_unit->spawn_type != Game::Units::SpawnType::Elephant &&
      target_unit->spawn_type != Game::Units::SpawnType::Catapult &&
      target_unit->spawn_type != Game::Units::SpawnType::Ballista;
  if (infantry_target) {
    launch_new_casualties(
        *target, *attacker, application.queued_soldier_casualties, 5.5F);
  }
}

} // namespace Game::Systems::Combat

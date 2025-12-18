#include "damage_processor.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"

#include <cmath>
#include <optional>

namespace Game::Systems::Combat {

void deal_damage(Engine::Core::World *world, Engine::Core::Entity *target,
                 int damage, Engine::Core::EntityID attacker_id) {
  auto *unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }

  bool const is_killing_blow = (unit->health > 0 && unit->health <= damage);
  unit->health = std::max(0, unit->health - damage);

  int attacker_owner_id = 0;
  std::optional<Game::Units::SpawnType> attacker_type_opt;
  if (attacker_id != 0 && (world != nullptr)) {
    auto *attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto *attacker_unit =
          attacker->get_component<Engine::Core::UnitComponent>();
      if (attacker_unit != nullptr) {
        attacker_owner_id = attacker_unit->owner_id;
        attacker_type_opt = attacker_unit->spawn_type;
      }
    }
  }

  Game::Units::SpawnType const attacker_type =
      attacker_type_opt.value_or(Game::Units::SpawnType::Knight);

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      attacker_id, target->get_id(), damage, attacker_type, is_killing_blow));

  if (unit->health > 0) {
    apply_hit_feedback(target, attacker_id, world);
  }

  if (target->has_component<Engine::Core::BuildingComponent>() &&
      unit->health > 0) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::BuildingAttackedEvent(target->get_id(), unit->owner_id,
                                            unit->spawn_type, attacker_id,
                                            attacker_owner_id, damage));
  }

  if (unit->health <= 0) {
    int const killer_owner_id = attacker_owner_id;

    Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
        target->get_id(), unit->owner_id, unit->spawn_type, attacker_id,
        killer_owner_id));

    auto *target_atk = target->get_component<Engine::Core::AttackComponent>();
    if ((target_atk != nullptr) && target_atk->in_melee_lock &&
        target_atk->melee_lock_target_id != 0) {
      if (world != nullptr) {
        auto *lock_partner =
            world->get_entity(target_atk->melee_lock_target_id);
        if ((lock_partner != nullptr) &&
            !lock_partner
                 ->has_component<Engine::Core::PendingRemovalComponent>()) {
          auto *partner_atk =
              lock_partner->get_component<Engine::Core::AttackComponent>();
          if ((partner_atk != nullptr) &&
              partner_atk->melee_lock_target_id == target->get_id()) {
            partner_atk->in_melee_lock = false;
            partner_atk->melee_lock_target_id = 0;
          }
        }
      }
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      BuildingCollisionRegistry::instance().unregister_building(
          target->get_id());
    }

    if (auto *r = target->get_component<Engine::Core::RenderableComponent>()) {
      r->visible = false;
    }

    if (auto *movement =
            target->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }

    target->add_component<Engine::Core::PendingRemovalComponent>();
  }
}

void apply_hit_feedback(Engine::Core::Entity *target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World *world) {
  if (target == nullptr) {
    return;
  }

  auto *feedback = target->get_component<Engine::Core::HitFeedbackComponent>();
  if (feedback == nullptr) {
    feedback = target->add_component<Engine::Core::HitFeedbackComponent>();
  }
  if (feedback == nullptr) {
    return;
  }

  feedback->is_reacting = true;
  feedback->reaction_time = 0.0F;
  feedback->reaction_intensity = 1.0F;

  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto *attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto *attacker_transform =
          attacker->get_component<Engine::Core::TransformComponent>();
      if (attacker_transform != nullptr) {
        float const dx =
            target_transform->position.x - attacker_transform->position.x;
        float const dz =
            target_transform->position.z - attacker_transform->position.z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.001F) {
          float const knockback =
              Engine::Core::HitFeedbackComponent::kMaxKnockback;
          feedback->knockback_x = (dx / dist) * knockback;
          feedback->knockback_z = (dz / dist) * knockback;
        }
      }
    }
  }

  auto *combat_state =
      target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining =
        Engine::Core::CombatStateComponent::kHitPauseDuration;
  }
}

} // namespace Game::Systems::Combat

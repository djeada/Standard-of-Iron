#include "rpg_commander_damage.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../combat_system/damage_application.h"
#include "rpg_damage_resolver.h"

namespace Game::Systems::RpgCombat {

namespace {

struct GuardResolution {
  int damage{0};
  bool blocked{false};
  bool perfect_guarded{false};
  bool guard_broken{false};
};

auto is_guardable_attack(Engine::Core::Entity* target,
                         Engine::Core::Entity* attacker,
                         float frontal_arc_dot) -> bool {
  auto* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto* attacker_transform =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return false;
  }

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  const float yaw = target_transform->rotation.y * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D to_attacker(attacker_transform->position.x - target_transform->position.x,
                        0.0F,
                        attacker_transform->position.z - target_transform->position.z);
  if (to_attacker.lengthSquared() <= 0.0001F) {
    return false;
  }
  to_attacker.normalize();
  return QVector3D::dotProduct(forward, to_attacker) >= frontal_arc_dot;
}

void apply_posture_pressure(Engine::Core::Entity* target, float pressure) {
  if (target == nullptr || pressure <= 0.0F) {
    return;
  }
  auto* commander = target->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr) {
    return;
  }
  commander->posture =
      std::clamp(commander->posture + pressure, 0.0F, commander->posture_max);
}

auto has_punish_opening(Engine::Core::Entity* target) -> bool {
  if (target == nullptr) {
    return false;
  }
  if (target->has_component<Engine::Core::StaggerComponent>()) {
    return true;
  }
  if (auto* combat_state =
          target->get_component<Engine::Core::CombatStateComponent>()) {
    if (combat_state->animation_state == Engine::Core::CombatAnimationState::WindUp) {
      return true;
    }
  }
  if (auto* commander = target->get_component<Engine::Core::CommanderComponent>()) {
    if (commander->punish_window_remaining > 0.0F) {
      return true;
    }
  }
  if (auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>()) {
    if (guard->guard_break_remaining > 0.0F) {
      return true;
    }
  }
  return false;
}

auto resolve_perfect_guard(Engine::Core::World* world,
                           Engine::Core::Entity* target,
                           Engine::Core::EntityID attacker_id) -> bool {
  if (world == nullptr || target == nullptr || attacker_id == 0) {
    return false;
  }

  auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>();
  auto* attacker = world->get_entity(attacker_id);
  if (guard == nullptr || attacker == nullptr || !guard->active ||
      guard->guard_break_remaining > 0.0F || guard->perfect_guard_remaining <= 0.0F ||
      !is_guardable_attack(target, attacker, guard->frontal_arc_dot)) {
    return false;
  }

  guard->perfect_guard_remaining = 0.0F;
  if (auto* commander = target->get_component<Engine::Core::CommanderComponent>()) {
    commander->punish_window_remaining =
        std::max(commander->punish_window_remaining, 1.0F);
    commander->posture = std::max(0.0F, commander->posture - 18.0F);
  }
  Game::Systems::Combat::add_or_extend_stagger(attacker, 0.75F);
  return true;
}

auto resolve_commander_guard(Engine::Core::World* world,
                             Engine::Core::Entity* target,
                             int damage,
                             Engine::Core::EntityID attacker_id) -> GuardResolution {
  GuardResolution result{damage, false, false, false};
  if (world == nullptr || target == nullptr || attacker_id == 0 || damage <= 0) {
    return result;
  }

  auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>();
  auto* attacker = world->get_entity(attacker_id);
  if (guard == nullptr || attacker == nullptr ||
      !is_guardable_attack(target, attacker, guard->frontal_arc_dot)) {
    return result;
  }

  auto* commander = target->get_component<Engine::Core::CommanderComponent>();
  if (!guard->active || guard->guard_break_remaining > 0.0F) {
    return result;
  }

  result.blocked = true;
  result.damage = std::max(
      1,
      static_cast<int>(std::round(static_cast<float>(damage) *
                                  std::clamp(guard->damage_multiplier, 0.0F, 1.0F))));
  apply_posture_pressure(target, std::max(10.0F, static_cast<float>(damage) * 0.85F));
  if (commander != nullptr && commander->posture >= commander->posture_max) {
    guard->guard_break_remaining = std::max(guard->guard_break_remaining, 1.0F);
    guard->rearm_requires_release = true;
    guard->active = false;
    guard->perfect_guard_remaining = 0.0F;
    commander->punish_window_remaining =
        std::max(commander->punish_window_remaining, 1.0F);
    commander->posture = commander->posture_max;
    result.damage = damage;
    result.blocked = false;
    result.guard_broken = true;
  }
  return result;
}

auto attacker_spawn_type(Engine::Core::World* world,
                         Engine::Core::EntityID attacker_id) -> Game::Units::SpawnType {
  if (world == nullptr || attacker_id == 0) {
    return Game::Units::SpawnType::Knight;
  }
  auto* attacker = world->get_entity(attacker_id);
  auto* unit = attacker != nullptr
                   ? attacker->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
  return unit != nullptr ? unit->spawn_type : Game::Units::SpawnType::Knight;
}

void mark_commander_hit(Engine::Core::CommanderComponent& commander,
                        int combo_step,
                        bool power_strike_hit) {
  commander.combo_step = (commander.combo_step + 1) % 4;
  commander.just_struck_enemy = true;
  commander.last_strike_combo_step = static_cast<std::uint8_t>(combo_step);
  if (power_strike_hit) {
    commander.power_strike_active = false;
  }
}

} // namespace

CommanderDamageResult
deal_commander_attack_damage(Engine::Core::World* world,
                             Engine::Core::Entity* target,
                             int raw_damage,
                             Engine::Core::EntityID commander_id) {
  CommanderDamageResult result;
  if (world == nullptr || target == nullptr || raw_damage <= 0 || commander_id == 0) {
    return result;
  }

  auto* commander_entity = world->get_entity(commander_id);
  auto* commander =
      commander_entity != nullptr
          ? commander_entity->get_component<Engine::Core::CommanderComponent>()
          : nullptr;

  int damage = raw_damage;
  int combo_step = 0;
  bool finisher_hit = false;
  bool power_strike_hit = false;
  if (commander != nullptr) {
    constexpr std::array<float, 4> k_combo_mult = {1.0F, 1.1F, 1.2F, 1.5F};
    combo_step = std::min(commander->combo_step, 3);
    finisher_hit = combo_step >= 3;
    damage = static_cast<int>(
        std::roundf(static_cast<float>(damage) * k_combo_mult[combo_step]));
    if (commander->power_strike_active) {
      damage = static_cast<int>(std::roundf(static_cast<float>(damage) * 1.8F));
      power_strike_hit = true;
    }
  }

  bool const punish_opening = has_punish_opening(target);
  if (punish_opening) {
    float const punish_multiplier = finisher_hit ? 1.35F : 1.22F;
    damage =
        static_cast<int>(std::roundf(static_cast<float>(damage) * punish_multiplier));
  }

  if (auto* target_rpg = target->get_component<Engine::Core::RpgHealthComponent>();
      target_rpg != nullptr && target_rpg->active) {
    result = deal_damage_to_rpg_commander(world, target, damage, commander_id);
    if (result.effective_damage > 0 && commander != nullptr) {
      mark_commander_hit(*commander, combo_step, power_strike_hit);
    }
    return result;
  }

  auto const application =
      Game::Systems::Combat::apply_unit_damage(world, target, damage, commander_id);
  result.effective_damage = application.applied_damage;
  result.killed = application.killed;

  if (application.applied_damage > 0) {
    if (commander != nullptr) {
      mark_commander_hit(*commander, combo_step, power_strike_hit);
    }
    if (punish_opening || finisher_hit) {
      auto tier = finisher_hit ? Engine::Core::StaggerTier::Knockback
                               : Engine::Core::StaggerTier::HeavyStagger;
      Game::Systems::Combat::add_or_extend_stagger(
          target, finisher_hit ? 0.75F : 0.45F, tier);
    } else if (application.applied_damage >= 20) {
      Game::Systems::Combat::add_or_extend_stagger(
          target, 0.15F, Engine::Core::StaggerTier::LightFlinch);
    }
  }

  return result;
}

CommanderDamageResult deal_damage_to_rpg_commander(Engine::Core::World* world,
                                                   Engine::Core::Entity* commander,
                                                   int raw_damage,
                                                   Engine::Core::EntityID attacker_id) {
  CommanderDamageResult result;
  if (world == nullptr || commander == nullptr || raw_damage <= 0) {
    return result;
  }

  if (resolve_perfect_guard(world, commander, attacker_id)) {
    result.perfect_guarded = true;
    result.blocked = true;
    return result;
  }

  GuardResolution const guard =
      resolve_commander_guard(world, commander, raw_damage, attacker_id);
  result.blocked = guard.blocked;
  result.guard_broken = guard.guard_broken;

  auto const rpg_result =
      resolve_rpg_damage(world, commander, guard.damage, attacker_id);
  result.effective_damage = rpg_result.effective_damage;
  result.killed = rpg_result.killed;

  if (rpg_result.effective_damage <= 0) {
    return result;
  }

  if (result.guard_broken || result.killed || has_punish_opening(commander)) {
    auto tier = result.guard_broken ? Engine::Core::StaggerTier::GuardBreak
                                    : Engine::Core::StaggerTier::HeavyStagger;
    Game::Systems::Combat::add_or_extend_stagger(
        commander, result.guard_broken ? 0.65F : 0.45F, tier);
  }
  if (!result.blocked) {
    apply_posture_pressure(
        commander,
        std::max(6.0F, static_cast<float>(rpg_result.effective_damage) * 0.25F));
  }
  Game::Systems::Combat::apply_hit_feedback(commander, attacker_id, world);

  Engine::Core::EventManager::instance().publish(
      Engine::Core::CombatHitEvent(attacker_id,
                                   commander->get_id(),
                                   rpg_result.effective_damage,
                                   attacker_spawn_type(world, attacker_id),
                                   false));
  return result;
}

} // namespace Game::Systems::RpgCombat

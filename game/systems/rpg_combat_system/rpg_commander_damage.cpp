#include "rpg_commander_damage.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../combat_actions/melee_intent_solver.h"
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

constexpr float k_degrees_to_radians = 0.017453292519943295F;

constexpr float k_guard_chest_height = 1.05F;
constexpr float k_guard_stand_off = 0.28F;
constexpr float k_guard_plate_offset = 0.30F;
constexpr float k_guard_plate_radius = 0.60F;

constexpr float k_guard_low_coverage = 0.25F;
constexpr float k_guard_high_coverage = 2.10F;
constexpr float k_guard_contact_side_dot = -0.20F;

auto attack_comes_from_the_front(const Engine::Core::TransformComponent& target,
                                 const Engine::Core::TransformComponent& attacker,
                                 float frontal_arc_dot) -> bool {
  const float yaw = target.rotation.y * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D to_attacker(attacker.position.x - target.position.x,
                        0.0F,
                        attacker.position.z - target.position.z);
  if (to_attacker.lengthSquared() <= 0.0001F) {
    return false;
  }
  to_attacker.normalize();
  return QVector3D::dotProduct(forward, to_attacker) >= frontal_arc_dot;
}

auto contact_enters_guard_arc(Engine::Core::Entity* target,
                              Engine::Core::Entity* attacker,
                              const Engine::Core::CommanderGuardComponent& guard,
                              std::optional<QVector3D> contact_point) -> bool {
  auto const* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto const* attacker_transform =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return false;
  }
  if (!attack_comes_from_the_front(
          *target_transform, *attacker_transform, guard.frontal_arc_dot)) {
    return false;
  }
  if (!contact_point.has_value()) {
    return true;
  }

  float const local_height = contact_point->y() - target_transform->position.y;
  if (local_height < k_guard_low_coverage || local_height > k_guard_high_coverage) {
    return false;
  }

  const float yaw = target_transform->rotation.y * k_degrees_to_radians;
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const to_contact(contact_point->x() - target_transform->position.x,
                             0.0F,
                             contact_point->z() - target_transform->position.z);
  if (to_contact.lengthSquared() <= 0.0001F) {
    return true;
  }
  return QVector3D::dotProduct(forward, to_contact.normalized()) >=
         k_guard_contact_side_dot;
}

auto contact_meets_guard_plate(Engine::Core::Entity* target,
                               const Engine::Core::CommanderGuardComponent& guard,
                               std::optional<QVector3D> contact_point) -> bool {
  auto const* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (target_transform == nullptr) {
    return false;
  }
  if (!contact_point.has_value()) {
    return true;
  }

  const float yaw = target_transform->rotation.y * k_degrees_to_radians;
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const right(forward.z(), 0.0F, -forward.x());
  QVector3D const chest(target_transform->position.x,
                        target_transform->position.y + k_guard_chest_height,
                        target_transform->position.z);
  QVector3D const plate_centre =
      chest + (forward * k_guard_stand_off) +
      (right * guard.guard_dir_x * k_guard_plate_offset) +
      (QVector3D(0.0F, 1.0F, 0.0F) * guard.guard_dir_y * k_guard_plate_offset);

  float const settled =
      std::clamp(1.0F - (guard.guard_turn_rate /
                         (2.0F * Game::Systems::CombatActions::k_guard_slew_rate)),
                 0.45F,
                 1.0F);
  float const radius = k_guard_plate_radius * settled;
  return (*contact_point - plate_centre).lengthSquared() <= radius * radius;
}

auto dodge_beats_contact(Engine::Core::Entity* target,
                         Engine::Core::Entity* attacker,
                         std::optional<QVector3D> contact_point) -> bool {
  auto const* rpg = target != nullptr
                        ? target->get_component<Engine::Core::RpgHealthComponent>()
                        : nullptr;
  if (rpg == nullptr || !rpg->active || rpg->dodge_grace_remaining <= 0.0F) {
    return false;
  }
  auto const* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  QVector3D from = contact_point.value_or(QVector3D(0.0F, 0.0F, 0.0F));
  if (!contact_point.has_value()) {
    auto const* attacker_transform =
        attacker != nullptr
            ? attacker->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (attacker_transform == nullptr) {
      return false;
    }
    from =
        QVector3D(attacker_transform->position.x, 0.0F, attacker_transform->position.z);
  }

  QVector3D away(target_transform->position.x - from.x(),
                 0.0F,
                 target_transform->position.z - from.z());
  if (away.lengthSquared() <= 0.0001F) {
    return false;
  }
  away.normalize();
  QVector3D const roll(rpg->dodge_dir_x, 0.0F, rpg->dodge_dir_z);
  if (roll.lengthSquared() <= 0.0001F) {
    return false;
  }
  return QVector3D::dotProduct(away, roll.normalized()) > 0.15F;
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

auto is_player_controlled_commander(Engine::Core::Entity* entity) -> bool {
  auto const* commander =
      entity != nullptr ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  return commander != nullptr && commander->fpv_controlled;
}

void play_guard_cue(Engine::Core::Entity* target, const char* cue_id) {
  if (!is_player_controlled_commander(target)) {
    return;
  }
  Engine::Core::EventManager::instance().publish(Engine::Core::AudioCueEvent(cue_id));
}

auto resolve_perfect_guard(Engine::Core::World* world,
                           Engine::Core::Entity* target,
                           Engine::Core::EntityID attacker_id,
                           std::optional<QVector3D> contact_point) -> bool {
  if (world == nullptr || target == nullptr || attacker_id == 0) {
    return false;
  }

  auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>();
  auto* attacker = world->get_entity(attacker_id);
  if (guard == nullptr || attacker == nullptr || !guard->active ||
      guard->guard_break_remaining > 0.0F || guard->perfect_guard_remaining <= 0.0F ||
      !contact_enters_guard_arc(target, attacker, *guard, contact_point) ||
      !contact_meets_guard_plate(target, *guard, contact_point)) {
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
                             Engine::Core::EntityID attacker_id,
                             CommanderDamageProfile profile,
                             std::optional<QVector3D> contact_point)
    -> GuardResolution {
  GuardResolution result{damage, false, false, false};
  if (world == nullptr || target == nullptr || attacker_id == 0 || damage <= 0) {
    return result;
  }

  auto* guard = target->get_component<Engine::Core::CommanderGuardComponent>();
  auto* attacker = world->get_entity(attacker_id);
  if (guard == nullptr || attacker == nullptr ||
      !contact_enters_guard_arc(target, attacker, *guard, contact_point)) {
    return result;
  }

  auto* commander = target->get_component<Engine::Core::CommanderComponent>();
  if (!guard->active || guard->guard_break_remaining > 0.0F) {
    return result;
  }

  if (profile.unblockable) {

    apply_posture_pressure(target, std::max(0.0F, profile.guard_pressure));
    if (commander != nullptr && commander->posture >= commander->posture_max) {
      guard->guard_break_remaining = std::max(guard->guard_break_remaining, 1.0F);
      guard->rearm_requires_release = true;
      guard->active = false;
      guard->perfect_guard_remaining = 0.0F;
      commander->punish_window_remaining =
          std::max(commander->punish_window_remaining, 1.0F);
      commander->posture = commander->posture_max;
      result.guard_broken = true;
    }
    return result;
  }

  result.blocked = true;
  result.damage = std::max(
      0,
      static_cast<int>(std::round(static_cast<float>(damage) *
                                  std::clamp(guard->damage_multiplier, 0.0F, 1.0F))));
  float const guard_pressure =
      std::max(std::max(10.0F, static_cast<float>(damage) * 0.85F),
               std::max(0.0F, profile.guard_pressure));
  apply_posture_pressure(target, guard_pressure);
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

void record_resolved_contact(Engine::Core::Entity* defender,
                             const CommanderDamageResult& result) {
  auto* rpg = defender != nullptr
                  ? defender->get_component<Engine::Core::RpgHealthComponent>()
                  : nullptr;
  if (rpg == nullptr) {
    return;
  }
  if (result.dodged) {
    ++rpg->dodged_contacts;
    rpg->last_contact_outcome = Engine::Core::RpgContactOutcome::Dodge;
    return;
  }
  if (result.perfect_guarded) {
    ++rpg->perfect_guard_contacts;
    rpg->last_contact_outcome = Engine::Core::RpgContactOutcome::PerfectGuard;
    return;
  }
  if (result.guard_broken) {
    ++rpg->guard_broken_contacts;
  }
  if (result.blocked) {
    ++rpg->blocked_contacts;
    rpg->last_contact_outcome = Engine::Core::RpgContactOutcome::Block;
    return;
  }
  if (result.effective_damage > 0) {
    ++rpg->damaging_contacts;
    rpg->last_contact_outcome = Engine::Core::RpgContactOutcome::Damage;
  }
}

void mark_commander_hit(Engine::Core::CommanderComponent& commander,
                        int combo_step,
                        bool power_strike_hit) {

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
                             Engine::Core::EntityID commander_id,
                             CommanderDamageProfile profile,
                             std::optional<std::uint16_t> target_soldier_slot,
                             std::optional<QVector3D> contact_point,
                             float impact_speed) {
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
    result = deal_damage_to_rpg_commander(
        world, target, damage, commander_id, profile, contact_point, impact_speed);
    if (result.effective_damage > 0 && commander != nullptr) {
      mark_commander_hit(*commander, combo_step, power_strike_hit);
    }
    return result;
  }

  auto const application = Game::Systems::Combat::apply_unit_damage(world,
                                                                    target,
                                                                    damage,
                                                                    commander_id,
                                                                    contact_point,
                                                                    target_soldier_slot,
                                                                    impact_speed);
  result.effective_damage = application.applied_damage;
  result.killed = application.killed;

  if (application.applied_damage > 0) {
    if (commander_entity != nullptr) {
      if (auto* targets = Engine::Core::get_or_add_component<
              Engine::Core::RpgCommanderTargetComponent>(commander_entity)) {
        targets->recent_hit_target_id = target->get_id();
        targets->recent_hit_soldier_slot = target_soldier_slot.value_or(
            Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
        targets->recent_hit_timer = 0.28F;
        targets->recent_hit_killed = result.killed;
        ++targets->hit_confirm_sequence;
      }
    }
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

CommanderDamageResult
deal_damage_to_rpg_commander(Engine::Core::World* world,
                             Engine::Core::Entity* commander,
                             int raw_damage,
                             Engine::Core::EntityID attacker_id,
                             CommanderDamageProfile profile,
                             std::optional<QVector3D> contact_point,
                             float impact_speed) {
  CommanderDamageResult result;
  if (world == nullptr || commander == nullptr || raw_damage <= 0) {
    return result;
  }

  auto* attacker = world->get_entity(attacker_id);
  if (dodge_beats_contact(commander, attacker, contact_point)) {
    result.dodged = true;
    record_resolved_contact(commander, result);
    return result;
  }

  if (!profile.unblockable &&
      resolve_perfect_guard(world, commander, attacker_id, contact_point)) {
    result.perfect_guarded = true;
    result.blocked = true;
    play_guard_cue(commander, "combat.perfect_guard");
    record_resolved_contact(commander, result);
    return result;
  }

  GuardResolution const guard = resolve_commander_guard(
      world, commander, raw_damage, attacker_id, profile, contact_point);
  result.blocked = guard.blocked;
  result.guard_broken = guard.guard_broken;
  if (result.guard_broken) {
    play_guard_cue(commander, "combat.guard_break");
  } else if (result.blocked) {
    play_guard_cue(commander, "combat.block");
  }

  auto const rpg_result = resolve_rpg_damage(
      world, commander, guard.damage, attacker_id, contact_point, impact_speed);
  result.effective_damage = rpg_result.effective_damage;
  result.killed = rpg_result.killed;
  record_resolved_contact(commander, result);

  if (rpg_result.effective_damage <= 0) {
    return result;
  }

  if (result.guard_broken) {
    Game::Systems::Combat::add_or_extend_stagger(
        commander, 0.65F, Engine::Core::StaggerTier::GuardBreak);
  } else if (has_punish_opening(commander) &&
             !commander->has_component<Engine::Core::StaggerComponent>()) {
    Game::Systems::Combat::add_or_extend_stagger(
        commander, 0.22F, Engine::Core::StaggerTier::LightFlinch);
  }
  if (!result.blocked) {
    float const posture_damage = std::max(
        std::max(6.0F, static_cast<float>(rpg_result.effective_damage) * 0.25F),
        std::max(0.0F, profile.posture_damage));
    apply_posture_pressure(commander, posture_damage);
  }
  return result;
}

} // namespace Game::Systems::RpgCombat

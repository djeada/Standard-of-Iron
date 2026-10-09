#include "attack_processor.h"

#include <qvectornd.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../units/squad.h"
#include "../combat_rules.h"
#include "../formation_combat_geometry.h"
#include "../movement/command_service.h"
#include "../movement/order_service.h"
#include "../projectile_system.h"
#include "../rpg_combat_system/rpg_commander_damage.h"
#include "arrow_volley.h"
#include "attack_chase.h"
#include "attack_control.h"
#include "attack_eligibility.h"
#include "attack_stat_modifiers.h"
#include "combat_mode_processor.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "damage_processor.h"
#include "elephant_lane_run.h"
#include "melee_lock.h"
#include "rts_commander_attack.h"
#include "structure_combat.h"
#include "target_assignment.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

bool release_rts_arrow_volley(Engine::Core::World& world,
                              Engine::Core::Entity& attacker,
                              Engine::Core::EntityID target_id,
                              int damage) {
  auto* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* target = world.get_entity(target_id);
  if (attacker_unit == nullptr || attacker_unit->health <= 0 ||
      !may_attack(attacker_unit,
                  target,
                  {.intent = EngagementIntent::Ordered, .allow_buildings = true})) {
    return false;
  }
  auto* projectile_system = world.get_system<ProjectileSystem>();
  if (projectile_system == nullptr) {
    return false;
  }
  spawn_rts_arrow_volley(&attacker, target, projectile_system, std::max(1, damage));
  return true;
}

namespace {

struct TickContext {
  Engine::Core::World* world = nullptr;
  const CombatQueryContext& query;
  ProjectileSystem* projectiles = nullptr;
  float delta_time = 0.0F;
  FacingLedger& facing;
  std::vector<CommandService::MoveIntent>& chase_move_intents;
  const FormationRanks& ranks;
};

struct Attacker {
  Engine::Core::Entity* entity = nullptr;
  Engine::Core::UnitComponent* unit = nullptr;
  Engine::Core::TransformComponent* transform = nullptr;
  Engine::Core::AttackComponent* attack = nullptr;
};

struct AttackStats {
  float range = 2.0F;
  int damage = 10;
  float cooldown = 1.0F;
};

struct TargetChoice {
  Engine::Core::Entity* target = nullptr;
  Engine::Core::UnitComponent* unit = nullptr;
  Engine::Core::TransformComponent* transform = nullptr;
  bool abandon_tick = false;
  bool had_ordered_target = false;
  bool suppress_opportunistic = false;

  [[nodiscard]] auto valid() const -> bool {
    return target != nullptr && unit != nullptr && transform != nullptr;
  }

  void set(Engine::Core::Entity* entity) {
    target = entity;
    unit = entity->get_component<Engine::Core::UnitComponent>();
    transform = entity->get_component<Engine::Core::TransformComponent>();
  }
};

auto is_charge_resolving(const Engine::Core::Entity* attacker) -> bool {
  auto const* charge = attacker->get_component<Engine::Core::MountedChargeComponent>();
  return charge != nullptr &&
         (charge->state == Engine::Core::MountedChargeState::Charging ||
          charge->state == Engine::Core::MountedChargeState::ImpactActive);
}

auto melee_strike_has_no_room(Engine::Core::Entity* attacker,
                              Engine::Core::Entity* target) -> bool {
  if (attacker == nullptr || target == nullptr) {
    return false;
  }
  auto const* commander = target->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return false;
  }
  auto const* engagement =
      target->get_component<Engine::Core::RpgEngagementComponent>();
  if (engagement == nullptr || engagement->engagement_slots.empty()) {
    return false;
  }
  return !engagement->is_pressing(attacker->get_id());
}

auto load_eligible_attacker(Engine::Core::Entity* entity,
                            const TickContext& ctx) -> std::optional<Attacker> {
  if (entity->has_component<Engine::Core::PendingRemovalComponent>() ||
      entity->has_component<Engine::Core::StaggerComponent>()) {
    return std::nullopt;
  }
  Attacker attacker{.entity = entity,
                    .unit = entity->get_component<Engine::Core::UnitComponent>(),
                    .transform =
                        entity->get_component<Engine::Core::TransformComponent>(),
                    .attack = entity->get_component<Engine::Core::AttackComponent>()};
  if (attacker.unit == nullptr || attacker.transform == nullptr ||
      attacker.unit->health <= 0) {
    return std::nullopt;
  }
  if (entity->has_component<Engine::Core::WildlifeComponent>() ||
      ctx.ranks.is_reserve(entity)) {
    Game::Systems::CombatRules::clear_rts_combat_tracking(entity);
    return std::nullopt;
  }
  return attacker;
}

void maintain_melee_lock(const Attacker& attacker, TickContext& ctx) {
  release_structure_lock_for_troop_target(attacker.entity, attacker.attack, ctx.world);
  if (!is_charge_resolving(attacker.entity)) {
    process_melee_lock(
        attacker.entity, attacker.attack, ctx.world, ctx.delta_time, ctx.facing);
  }
  sync_melee_lock_target(attacker.entity, attacker.attack);
  drop_target_left_by_a_finished_lock(ctx.world, attacker.entity, attacker.attack);
}

auto load_attack_stats(const Attacker& attacker,
                       const TickContext& ctx) -> AttackStats {
  AttackStats stats;
  if (attacker.attack == nullptr) {
    return stats;
  }
  update_combat_mode(attacker.entity, ctx.world, attacker.attack);
  stats.range = attacker.attack->get_current_range();
  stats.damage = attacker.attack->get_current_damage();
  stats.cooldown = attacker.attack->get_current_cooldown();
  apply_hold_mode_bonuses(attacker.entity, attacker.unit, stats.range, stats.damage);
  return stats;
}

auto reached_by_range_or_contact(const Attacker& attacker,
                                 Engine::Core::Entity* target,
                                 float range) -> bool {
  bool reached = is_in_range(attacker.entity, target, range);
  if (!reached || is_ranged_mode(attacker.attack)) {
    return reached;
  }
  auto const geometry = FormationCombat::contact_geometry(*attacker.entity, *target);
  auto const* movement =
      attacker.entity->get_component<Engine::Core::MovementComponent>();
  bool const settled_single_body = !geometry.uses_formation_slots &&
                                   (movement == nullptr || !movement->get_has_target());
  return settled_single_body ||
         melee_contact_reached(*attacker.entity, *target, geometry);
}

void select_ordered_target(const Attacker& attacker,
                           TickContext& ctx,
                           const AttackStats& stats,
                           bool in_melee_lock,
                           Engine::Core::AttackTargetComponent* order,
                           TargetChoice& choice) {
  auto* target = ctx.world->get_entity(order->target_id);
  auto* target_unit =
      may_attack(attacker.unit,
                 target,
                 {.intent = EngagementIntent::Ordered, .allow_buildings = true})
          ? target->get_component<Engine::Core::UnitComponent>()
          : nullptr;
  auto* target_transform =
      (target_unit != nullptr)
          ? target->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  if (target_unit == nullptr || target_transform == nullptr) {
    drop_attack_target(ctx.world, attacker.entity);
    return;
  }

  if (attacker.entity->has_component<Engine::Core::ElephantComponent>()) {
    if (auto const lane = elephant_lane_goal(*ctx.world, *attacker.entity, target)) {
      steer_down_lane(*attacker.entity, *lane, ctx.chase_move_intents);
      return;
    }
  }

  if (reached_by_range_or_contact(attacker, target, stats.range)) {
    choice.target = target;
    choice.unit = target_unit;
    choice.transform = target_transform;
    stop_unit_movement(attacker.entity, attacker.transform);
    if (!in_melee_lock) {
      face_target(attacker.transform, target_transform);
    }
    return;
  }
  if (!keeps_pursuing(attacker.entity, target)) {
    drop_attack_target(ctx.world, attacker.entity);
    choice.abandon_tick = true;
    return;
  }
  steer_toward_target({.attacker = attacker.entity,
                       .attacker_transform = attacker.transform,
                       .target = target,
                       .target_transform = target_transform,
                       .range = stats.range,
                       .ranged_unit = is_ranged_mode(attacker.attack),
                       .delta_time = ctx.delta_time},
                      ctx.chase_move_intents);
}

void acquire_nearby_target(const Attacker& attacker,
                           const TickContext& ctx,
                           const AttackStats& stats,
                           TargetChoice& choice) {
  auto* best = find_nearest_enemy(attacker.entity,
                                  ctx.query,
                                  stats.range,
                                  nullptr,
                                  {},
                                  nullptr,
                                  {.intent = EngagementIntent::AutoAcquired,
                                   .allow_buildings = false,
                                   .in_reach = true});
  bool const melee = !is_ranged_mode(attacker.attack);
  if (best != nullptr && melee &&
      structure_separates_combatants(attacker.entity, best)) {
    best = nullptr;
  }
  if (best != nullptr && melee && !is_building(best)) {
    auto const geometry = FormationCombat::contact_geometry(*attacker.entity, *best);
    auto const* hold =
        attacker.entity->get_component<Engine::Core::HoldModeComponent>();
    bool const striking_from_hold = hold != nullptr && hold->active;
    if (!geometry.uses_formation_slots && !striking_from_hold &&
        !melee_contact_reached(*attacker.entity, *best, geometry)) {
      best = nullptr;
    }
  }
  if (best != nullptr) {
    choice.set(best);
  }
}

auto select_target(const Attacker& attacker,
                   TickContext& ctx,
                   const AttackStats& stats,
                   bool in_melee_lock) -> TargetChoice {
  TargetChoice choice;
  auto* order =
      !Game::Systems::CombatRules::uses_rpg_combat_rules(attacker.entity)
          ? attacker.entity->get_component<Engine::Core::AttackTargetComponent>()
          : nullptr;
  choice.suppress_opportunistic = suppresses_opportunistic_combat(attacker.entity);
  if (choice.suppress_opportunistic && order != nullptr) {
    drop_attack_target(ctx.world, attacker.entity);
    order = nullptr;
  }
  choice.had_ordered_target = order != nullptr;

  if (in_melee_lock) {
    if (auto* locked =
            locked_target_for_attack(attacker.entity, attacker.attack, ctx.world)) {
      choice.set(locked);
    }
  }
  if (choice.target == nullptr && order != nullptr && order->target_id != 0) {
    select_ordered_target(attacker, ctx, stats, in_melee_lock, order, choice);
    if (choice.abandon_tick) {
      return choice;
    }
  }

  bool const has_attack_target =
      attacker.entity->has_component<Engine::Core::AttackTargetComponent>();
  if (choice.target == nullptr && !has_attack_target &&
      !choice.suppress_opportunistic && auto_acquires_targets(attacker.entity)) {
    acquire_nearby_target(attacker, ctx, stats, choice);
  }
  return choice;
}

void engage_target(const Attacker& attacker,
                   const TargetChoice& choice,
                   bool in_melee_lock) {
  if (attacker.entity->has_component<Engine::Core::MovementComponent>()) {
    OrderService::clear_player_order_intent(attacker.entity);
  }
  auto const* existing =
      attacker.entity->get_component<Engine::Core::AttackTargetComponent>();
  if (existing == nullptr || existing->target_id != choice.target->get_id()) {
    assign_attack_target(
        attacker.entity, choice.target->get_id(), TargetSource::InReach);
  }
  if (is_ranged_mode(attacker.attack)) {
    stop_unit_movement(attacker.entity, attacker.transform);
  }
  if (!in_melee_lock) {
    face_target(attacker.transform, choice.transform);
  }
}

void lock_on_body_contact(const Attacker& attacker,
                          const TargetChoice& choice,
                          TickContext& ctx,
                          bool in_melee_lock) {
  auto const& intents = ctx.chase_move_intents;
  bool const still_closing =
      std::any_of(intents.begin(), intents.end(), [&attacker](auto const& intent) {
        return intent.unit_id == attacker.entity->get_id();
      });
  bool const melee_contact =
      attacker.attack != nullptr &&
      attacker.attack->current_mode ==
          Engine::Core::AttackComponent::CombatMode::Melee &&
      !in_melee_lock && !still_closing &&
      bodies_have_met(
          *attacker.entity, *attacker.transform, *choice.target, *choice.transform);
  if (melee_contact) {
    (void)enter_melee_lock(attacker.entity,
                           choice.target,
                           attacker.attack,
                           ctx.world,
                           ctx.delta_time,
                           ctx.facing);
  }
}

auto scaled_attack_damage(const Attacker& attacker,
                          const TargetChoice& choice,
                          int base_damage) -> int {
  float const tactical = calculate_tactical_damage_multiplier(
      attacker.entity, choice.target, attacker.unit, choice.unit);
  int damage = static_cast<int>(static_cast<float>(base_damage) * tactical);
  damage = std::max(1,
                    static_cast<int>(static_cast<float>(damage) *
                                     Game::Units::squad_fraction(*attacker.unit)));
  apply_high_ground_defense_bonuses(
      attacker.entity, choice.target, choice.unit, damage);
  return damage;
}

auto attacker_uses_bow_action(const Attacker& attacker,
                              bool special_projectile) -> bool {
  return special_projectile ||
         (is_ranged_mode(attacker.attack) &&
          (Game::Units::is_skirmisher_spawn(attacker.unit->spawn_type) ||
           attacker.unit->spawn_type == Game::Units::SpawnType::Archer ||
           attacker.unit->spawn_type == Game::Units::SpawnType::HorseArcher));
}

void apply_attack_effect(const Attacker& attacker,
                         const TargetChoice& choice,
                         TickContext& ctx,
                         int damage,
                         float cooldown,
                         bool defer_melee_strike) {
  bool const ranged_unit = is_ranged_mode(attacker.attack);
  auto* special =
      attacker.entity->get_component<Engine::Core::SpecialAttackComponent>();
  bool const special_projectile = ranged_unit && special != nullptr &&
                                  special->use_projectile_system &&
                                  ctx.projectiles != nullptr;
  bool const show_arrow_vfx =
      attacker.unit->spawn_type != Game::Units::SpawnType::Catapult &&
      attacker.unit->spawn_type != Game::Units::SpawnType::Ballista;

  if (attacker_uses_bow_action(attacker, special_projectile)) {
    begin_rts_bow_action(*ctx.world, attacker.entity, choice.target, damage, cooldown);
  } else if (show_arrow_vfx && ranged_unit && ctx.projectiles != nullptr) {
    spawn_rts_arrow_volley(attacker.entity, choice.target, ctx.projectiles, damage);
  } else if (defer_melee_strike) {
    begin_rts_melee_action(*ctx.world, attacker.entity, choice.target, damage);
  } else if (Game::Systems::CombatRules::uses_rpg_combat_rules(choice.target)) {
    Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
        ctx.world, choice.target, damage, attacker.entity->get_id());
  } else {
    deal_damage(ctx.world, choice.target, damage, attacker.entity->get_id());
  }
}

void strike_target(const Attacker& attacker,
                   const TargetChoice& choice,
                   TickContext& ctx,
                   const AttackStats& stats,
                   float& attack_clock) {
  if (is_ranged_mode(attacker.attack)) {
    begin_attack_animation(attacker.entity);
  }
  bool const is_melee_attack =
      attacker.attack != nullptr &&
      attacker.attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee;
  if (is_melee_attack) {
    if (melee_strike_has_no_room(attacker.entity, choice.target)) {
      return;
    }
    initiate_melee_combat(attacker.entity,
                          choice.target,
                          attacker.attack,
                          ctx.world,
                          ctx.delta_time,
                          ctx.facing);
  }

  int const damage = scaled_attack_damage(attacker, choice, stats.damage);
  auto const* commander =
      attacker.entity->get_component<Engine::Core::CommanderComponent>();
  bool const fpv_commander = commander != nullptr && commander->fpv_controlled;
  bool const defer_melee_strike =
      is_melee_attack && !fpv_commander && stats.cooldown > 0.001F;
  apply_attack_effect(
      attacker, choice, ctx, damage, stats.cooldown, defer_melee_strike);

  attack_clock =
      defer_melee_strike
          ? resolve_melee_swing_cadence(attacker.entity, choice.target, stats.cooldown)
          : -deterministic_attack_delay(
                attacker.entity->get_id(), choice.target->get_id(), stats.cooldown);
}

void settle_without_target(const Attacker& attacker,
                           TickContext& ctx,
                           bool had_ordered_target) {
  Engine::Core::Entity* entity = attacker.entity;
  clear_orphaned_rts_attack_presentation(entity);
  if (Game::Systems::CombatRules::participates_in_rts_melee_lock(entity) &&
      !had_ordered_target &&
      entity->has_component<Engine::Core::AttackTargetComponent>()) {
    drop_attack_target(ctx.world, entity);
  }
  auto const* held = entity->get_component<Engine::Core::AttackTargetComponent>();
  if (is_unit_in_guard_mode(entity) && (held == nullptr || held->target_id == 0)) {
    send_guard_home(*ctx.world, entity);
  }
}

void process_attacker(Engine::Core::Entity* entity, TickContext& ctx) {
  auto attacker_opt = load_eligible_attacker(entity, ctx);
  if (!attacker_opt) {
    return;
  }
  Attacker const& attacker = *attacker_opt;
  maintain_melee_lock(attacker, ctx);

  AttackStats const stats = load_attack_stats(attacker, ctx);
  float scratch_clock = 0.0F;
  float& attack_clock =
      attacker.attack != nullptr ? attacker.attack->time_since_last : scratch_clock;
  attack_clock += ctx.delta_time;

  bool const attack_ready =
      attack_clock >= stats.cooldown && !commander_link_still_swinging(entity);
  if (attack_ready && should_prioritize_healing(entity, ctx.query)) {
    return;
  }

  bool const in_melee_lock =
      attacker.attack != nullptr && attacker.attack->in_melee_lock &&
      Game::Systems::CombatRules::participates_in_rts_melee_lock(entity);
  TargetChoice const choice = select_target(attacker, ctx, stats, in_melee_lock);
  if (choice.abandon_tick) {
    return;
  }
  if (!choice.valid()) {
    settle_without_target(attacker, ctx, choice.had_ordered_target);
    return;
  }

  engage_target(attacker, choice, in_melee_lock);
  if (is_charge_resolving(entity)) {
    return;
  }
  if (!attack_ready) {
    lock_on_body_contact(attacker, choice, ctx, in_melee_lock);
    return;
  }
  strike_target(attacker, choice, ctx, stats, attack_clock);
}

} // namespace

void process_attacks(Engine::Core::World* world,
                     const CombatQueryContext& query_context,
                     float delta_time) {
  std::vector<CommandService::MoveIntent> chase_move_intents;
  chase_move_intents.reserve(query_context.units.size());
  FacingLedger facing_ledger;
  const FormationRanks formation_ranks(*world);
  lock_touching_enemies(world, query_context, delta_time, facing_ledger);

  TickContext ctx{.world = world,
                  .query = query_context,
                  .projectiles = world->get_system<ProjectileSystem>(),
                  .delta_time = delta_time,
                  .facing = facing_ledger,
                  .chase_move_intents = chase_move_intents,
                  .ranks = formation_ranks};
  for (auto* attacker : query_context.units) {
    process_attacker(attacker, ctx);
  }

  if (!chase_move_intents.empty()) {
    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::AttackChase;
    CommandService::move_units(*world, chase_move_intents, options);
  }
}

} // namespace Game::Systems::Combat

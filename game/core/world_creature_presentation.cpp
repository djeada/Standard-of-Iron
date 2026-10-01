#include "world_creature_presentation.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "../../animation/action_manifest.h"
#include "../../animation/clip_manifest.h"
#include "../formation/unit_layout_state.h"
#include "../systems/builder_product_types.h"
#include "../systems/unit_activity.h"
#include "component.h"
#include "core/entity.h"
#include "death_sequence.h"
#include "world.h"

namespace Engine::Core {

namespace {

[[nodiscard]] constexpr auto
to_animation_phase(CombatAnimationState phase) noexcept -> Animation::CombatPhase {
  switch (phase) {
  case CombatAnimationState::Idle:
    return Animation::CombatPhase::Idle;
  case CombatAnimationState::Advance:
    return Animation::CombatPhase::Advance;
  case CombatAnimationState::WindUp:
    return Animation::CombatPhase::WindUp;
  case CombatAnimationState::Strike:
    return Animation::CombatPhase::Strike;
  case CombatAnimationState::Impact:
    return Animation::CombatPhase::Impact;
  case CombatAnimationState::Recover:
    return Animation::CombatPhase::Recover;
  case CombatAnimationState::Reposition:
    return Animation::CombatPhase::Reposition;
  }
  return Animation::CombatPhase::Idle;
}

[[nodiscard]] constexpr auto to_animation_family(CombatAttackFamily family) noexcept
    -> Animation::CombatAttackFamily {
  switch (family) {
  case CombatAttackFamily::Sword:
    return Animation::CombatAttackFamily::Sword;
  case CombatAttackFamily::Spear:
    return Animation::CombatAttackFamily::Spear;
  case CombatAttackFamily::Bow:
    return Animation::CombatAttackFamily::Bow;
  case CombatAttackFamily::None:
    return Animation::CombatAttackFamily::None;
  }
  return Animation::CombatAttackFamily::None;
}

auto builder_work_job(std::string_view product_type) -> std::uint8_t {
  using Animation::HumanoidWorkJob;
  if (product_type == Game::Systems::k_builder_product_cut_tree) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Chop);
  }
  if (product_type == Game::Systems::k_builder_product_collect_stone ||
      product_type == Game::Systems::k_builder_product_collect_iron_ore) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Quarry);
  }
  if (product_type == Game::Systems::k_builder_product_harvest_grain) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Reap);
  }
  if (product_type == Game::Systems::k_builder_product_slaughter_sheep) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Butcher);
  }
  return static_cast<std::uint8_t>(HumanoidWorkJob::Build);
}

void publish_target(CreaturePresentationComponent& next,
                    const Entity& entity,
                    World* world) {
  auto const* target_ref = entity.get_component<AttackTargetComponent>();
  next.target_id = target_ref != nullptr ? target_ref->target_id : 0U;
  if (next.target_id != 0U && world != nullptr) {
    auto* target = world->get_entity(next.target_id);
    auto const* target_unit =
        target != nullptr ? target->get_component<UnitComponent>() : nullptr;
    next.target_alive = target_unit != nullptr && target_unit->health > 0 &&
                        !target->has_component<PendingRemovalComponent>() &&
                        !target->has_component<DeathAnimationComponent>();
  }
}

[[nodiscard]] auto uses_rpg_rules(const Entity& entity) -> bool {
  return (entity.get_component<CommanderComponent>() != nullptr &&
          entity.get_component<CommanderComponent>()->fpv_controlled) ||
         (entity.get_component<RpgHealthComponent>() != nullptr &&
          entity.get_component<RpgHealthComponent>()->active);
}

void add_death_inputs(Animation::HumanoidActionSampleInputs& inputs,
                      const DeathAnimationComponent* death) {
  if (death == nullptr) {
    return;
  }
  inputs.death = {
      .active = true,
      .dying = death->state == DeathSequenceState::Dying,
      .state_time = death->state_time,
      .state_duration = death->state_duration,
      .variant = death->sequence_variant,
      .sink_progress = death_sink_progress(*death),
  };
}

[[nodiscard]] auto
add_construction_inputs(Animation::HumanoidActionSampleInputs& inputs,
                        const Entity& entity,
                        const DeathAnimationComponent* death) -> std::uint8_t {
  auto const* builder = entity.get_component<BuilderProductionComponent>();
  std::uint8_t construction_job = 0;
  if (builder != nullptr && builder->in_progress) {
    inputs.construction = {
        .active = true,
        .build_time = builder->build_time,
        .time_remaining = builder->time_remaining,
    };
    construction_job = builder_work_job(builder->product_type);
  } else if (auto const* push = entity.get_component<RockfallPushComponent>();
             death == nullptr && push != nullptr) {
    inputs.construction = {
        .active = true,
        .build_time = push->elapsed,
        .time_remaining = 0.0F,
        .cycles_per_second = 1.0F / Animation::k_humanoid_crew_push_cycle_time,
    };
    construction_job = static_cast<std::uint8_t>(Animation::HumanoidWorkJob::Push);
  } else if (auto const* resident = entity.get_component<SettlementResidentComponent>();

             death == nullptr && resident != nullptr && resident->is_labouring()) {

    inputs.construction = {
        .active = true,
        .build_time = resident->work_elapsed,
        .time_remaining = 0.0F,
        .cycles_per_second = k_settlement_labour_cycles_per_second,
    };
  }
  return construction_job;
}

void add_combat_inputs(Animation::HumanoidActionSampleInputs& inputs,
                       const CombatStateComponent* combat,
                       const AttackComponent* attack) {
  if (combat == nullptr) {
    return;
  }
  inputs.combat = {
      .has_state = true,
      .phase = to_animation_phase(combat->animation_state),
      .phase_time = combat->state_time,
      .phase_duration = combat->state_duration,
      .attack_family = to_animation_family(combat->attack_family),
      .attack_variant = combat->attack_variant,
      .finisher_attack = combat->finisher_attack,
      .attack_offset = combat->attack_offset,
      .fallback_mode_is_melee =
          attack != nullptr &&
          attack->current_mode == AttackComponent::CombatMode::Melee,
  };
}

void add_melee_lock_inputs(Animation::HumanoidActionSampleInputs& inputs,
                           const AttackComponent* attack,
                           const UnitComponent* unit,
                           bool rpg_rules) {
  if (attack == nullptr) {
    return;
  }
  inputs.melee_lock = {
      .in_lock = attack->in_melee_lock,
      .participates = !rpg_rules,
      .fallback_attack_family =
          unit != nullptr ? to_animation_family(resolve_combat_attack_family(
                                unit->spawn_type, AttackComponent::CombatMode::Melee))
                          : Animation::CombatAttackFamily::None,
  };
}

void add_hit_and_cast_inputs(Animation::HumanoidActionSampleInputs& inputs,
                             const HitFeedbackComponent* hit,
                             const SpecialAttackComponent* special) {
  if (hit != nullptr && hit->is_reacting) {
    inputs.hit_reaction = {
        .active = true,
        .reaction_time = hit->reaction_time,
        .reaction_duration = hit->reaction_duration > 0.0F
                                 ? hit->reaction_duration
                                 : HitFeedbackComponent::k_reaction_duration,
        .intensity = hit->reaction_intensity,
        .knockback_x = hit->knockback_x,
        .knockback_z = hit->knockback_z,
    };
  }
  if (special != nullptr && special->use_projectile_system &&
      Game::Systems::is_cast_projectile_kind(special->projectile_kind)) {
    inputs.cast = {
        .has_projectile_cast = true,
        .projectile_is_fireball =
            special->projectile_kind == Game::Systems::ProjectileKind::Fireball,
    };
  }
}

void apply_action_sample(CreaturePresentationComponent& next,
                         const Animation::HumanoidActionSample& action,
                         const Entity& entity,
                         const UnitComponent* unit,
                         std::uint8_t construction_job) {
  auto const* combat = entity.get_component<CombatStateComponent>();
  auto const* hit = entity.get_component<HitFeedbackComponent>();
  next.is_attacking = action.is_attacking;
  next.is_melee = action.is_melee;
  next.is_in_melee_lock = action.is_in_melee_lock;
  next.combat_phase =
      combat != nullptr ? combat->animation_state : CombatAnimationState::Idle;
  next.combat_phase_progress = action.combat_phase_progress;
  if (combat != nullptr) {
    next.melee_intent = combat->intent;
    next.melee_intent_valid = true;
  }
  if (auto const* body = entity.get_component<CommanderBodyControlComponent>()) {
    next.melee_rest_x = body->rest_dir_x;
    next.melee_rest_y = body->rest_dir_y;
    next.melee_rest_valid = body->rest_valid;
  }
  next.attack_family =
      combat != nullptr ? combat->attack_family : CombatAttackFamily::None;
  if (action.attack_family != Animation::CombatAttackFamily::None &&
      next.attack_family == CombatAttackFamily::None && unit != nullptr) {
    next.attack_family = resolve_combat_attack_family(
        unit->spawn_type, AttackComponent::CombatMode::Melee);
  }
  next.attack_variant = action.attack_variant;
  next.finisher_attack = action.finisher_attack;
  next.attack_offset = action.attack_offset;
  next.has_attack_offset = action.has_attack_offset;
  next.attack_from_combat_state = action.attack_from_combat_state;
  next.attack_from_melee_lock = action.attack_from_melee_lock;
  next.is_casting = action.is_casting;
  next.cast = action.cast_kind == Animation::CastVisualKind::Fireball
                  ? CreatureCastPresentation::Fireball
                  : CreatureCastPresentation::None;
  next.is_hit_reacting = action.is_hit_reacting;
  next.hit_reaction_intensity = action.hit_reaction_intensity;
  next.hit_reaction_progress = action.hit_reaction_progress;
  next.hit_reaction_kind =
      hit != nullptr ? hit->reaction_kind : HitReactionKind::Flinch;
  next.hit_recoil_x = action.hit_recoil_x;
  next.hit_recoil_z = action.hit_recoil_z;
  next.is_constructing = action.is_constructing;
  next.construction_progress = action.construction_progress;
  next.construction_job = construction_job;

  if (action.is_in_melee_lock) {
    next.is_constructing = false;
    next.construction_progress = 0.0F;
    next.construction_job = 0;
  }
  next.is_dying = action.is_dying;
  next.is_dead = action.is_dead;
  next.death_progress = action.death_progress;
  next.death_variant = action.death_variant;
  next.death_sink_progress = action.death_sink_progress;
}

void apply_formation_hit_gate(CreaturePresentationComponent& next,
                              const Entity& entity) {
  auto const* formation = entity.get_component<FormationPresentationComponent>();
  next.allow_full_body_hit_reaction =
      formation == nullptr || formation->allow_full_body_hit_reaction;
  if (!next.allow_full_body_hit_reaction) {
    next.is_hit_reacting = false;
    next.hit_reaction_intensity = 0.0F;
    next.hit_reaction_progress = 0.0F;
    next.hit_recoil_x = 0.0F;
    next.hit_recoil_z = 0.0F;
  }
}

void apply_healing(CreaturePresentationComponent& next,
                   const Entity& entity,
                   bool in_melee_lock) {
  auto const* transform = entity.get_component<TransformComponent>();
  auto const* healer = entity.get_component<HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr &&
      !in_melee_lock) {
    next.is_healing = true;
    next.healing_target_dx = healer->healing_target_x - transform->position.x;
    next.healing_target_dz = healer->healing_target_z - transform->position.z;
  }
}

void apply_commander(CreaturePresentationComponent& next, const Entity& entity) {
  auto const* commander = entity.get_component<CommanderComponent>();
  next.has_commander = commander != nullptr;
  next.fpv_controlled = commander != nullptr && commander->fpv_controlled;
  if (commander == nullptr) {
    return;
  }
  next.jump_active = commander->jump_active;
  next.jump_phase = commander->jump_phase;
  next.jump_height_offset = commander->jump_height_offset;
  next.dodge_active = commander->dodge_active;
  next.dodge_phase = commander->dodge_phase;
  next.flag_rally_planting = commander->is_flag_rally_planting();
  next.flag_rally_animation_timer = commander->flag_rally_animation_timer;
  next.flag_rally_cost = commander->flag_rally_cost;
  if (next.dodge_active && !next.is_dying && !next.is_dead) {
    next.is_attacking = false;
    next.is_hit_reacting = true;
    next.hit_reaction_intensity = 1.0F;
    next.hit_reaction_progress = std::clamp(next.dodge_phase, 0.0F, 1.0F);
    next.hit_reaction_kind = HitReactionKind::Evade;
    next.allow_full_body_hit_reaction = true;
  }
}

void apply_guard_and_layout(CreaturePresentationComponent& next,
                            const Entity& entity,
                            const UnitComponent* unit) {
  auto const* commander_guard = entity.get_component<CommanderGuardComponent>();
  auto const* formation_mode = entity.get_component<FormationModeComponent>();
  auto const* guard_mode = entity.get_component<GuardModeComponent>();
  auto const* brace = entity.get_component<SpearBraceComponent>();
  next.formation_guard_active = (formation_mode != nullptr && formation_mode->active) ||
                                (guard_mode != nullptr && guard_mode->active);

  auto const* unit_layout = entity.get_component<UnitLayoutStateComponent>();
  next.defensive_layout_locked =
      unit_layout != nullptr &&
      unit_layout->state ==
          static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Defensive) &&
      unit_layout->is_formed();
  next.guard_requested =
      (next.fpv_controlled && commander_guard != nullptr && commander_guard->active) ||
      (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Swordsman &&
       next.formation_guard_active) ||
      (brace != nullptr && (brace->requested || brace->active));
  next.activity = Game::Systems::classify_unit_activity(entity);
}

void apply_hold_showcase_authored(CreaturePresentationComponent& next,
                                  const Entity& entity) {
  auto const* hold = entity.get_component<HoldModeComponent>();
  if (hold != nullptr) {
    next.hold_requested = hold->active;
    next.hold_exit_requested = !hold->active && hold->exit_cooldown > 0.0F;
    next.hold_entry_progress = hold->kneel_entry_progress;
    next.hold_exit_progress =
        1.0F - hold->exit_cooldown / std::max(hold->stand_up_duration, 1.0e-4F);
    next.hold_enter_duration = hold->kneel_duration;
    next.hold_exit_duration = hold->stand_up_duration;
  }
  auto const* showcase = entity.get_component<ShowcaseRoutineComponent>();
  if (showcase != nullptr) {
    next.showcase_active = showcase->active;
    next.showcase_move = showcase->current_move;
    next.showcase_phase = showcase->phase;
  }
  auto const* authored = entity.get_component<RpgCommanderActionComponent>();
  if (authored != nullptr) {
    next.authored_action_id = authored->combat_action_id;
    next.authored_action_running = authored->action_running;
    next.authored_action_completed = authored->action_completed;
    next.authored_action_phase = authored->normalized_action_time;
    next.authored_action_exchange_outcome = authored->exchange_outcome;
  }
}

[[nodiscard]] auto
presentation_changed(const CreaturePresentationComponent& current,
                     const CreaturePresentationComponent& next) -> bool {
  return current.snapshot_valid != next.snapshot_valid ||
         current.target_id != next.target_id ||
         current.target_alive != next.target_alive ||
         current.combat_active != next.combat_active ||
         current.is_attacking != next.is_attacking ||
         current.is_melee != next.is_melee ||
         current.combat_phase != next.combat_phase ||
         current.is_hit_reacting != next.is_hit_reacting ||
         current.hit_reaction_kind != next.hit_reaction_kind ||
         current.construction_job != next.construction_job ||
         current.is_dying != next.is_dying || current.is_dead != next.is_dead ||
         current.guard_requested != next.guard_requested ||
         current.defensive_layout_locked != next.defensive_layout_locked ||
         current.hold_requested != next.hold_requested ||
         current.dodge_active != next.dodge_active ||
         current.dodge_phase != next.dodge_phase ||
         current.showcase_active != next.showcase_active ||
         current.showcase_move != next.showcase_move ||
         current.showcase_phase != next.showcase_phase;
}

auto publish_creature_presentation_entity(Entity* entity, World* world)
    -> CreaturePresentationComponent* {
  if (entity == nullptr) {
    return nullptr;
  }
  auto const* unit = entity->get_component<UnitComponent>();

  auto* presentation = get_or_add_component<CreaturePresentationComponent>(entity);
  if (presentation == nullptr) {
    return nullptr;
  }
  CreaturePresentationComponent next;
  next.snapshot_valid = true;
  publish_target(next, *entity, world);

  auto const* death = entity->get_component<DeathAnimationComponent>();
  auto const* combat = entity->get_component<CombatStateComponent>();
  auto const* attack = entity->get_component<AttackComponent>();

  Animation::HumanoidActionSampleInputs action_inputs{};
  add_death_inputs(action_inputs, death);
  std::uint8_t const construction_job =
      add_construction_inputs(action_inputs, *entity, death);
  add_combat_inputs(action_inputs, combat, attack);
  add_melee_lock_inputs(action_inputs, attack, unit, uses_rpg_rules(*entity));
  add_hit_and_cast_inputs(action_inputs,
                          entity->get_component<HitFeedbackComponent>(),
                          entity->get_component<SpecialAttackComponent>());
  auto const action = Animation::resolve_humanoid_action_sample(action_inputs);

  apply_action_sample(next, action, *entity, unit, construction_job);
  apply_formation_hit_gate(next, *entity);
  apply_healing(next, *entity, action.is_in_melee_lock);
  apply_commander(next, *entity);
  apply_guard_and_layout(next, *entity, unit);
  apply_hold_showcase_authored(next, *entity);
  next.combat_active = next.is_attacking || next.is_hit_reacting || next.is_dying ||
                       next.is_dead || next.target_id != 0U;

  next.revision = presentation->revision;
  if (presentation_changed(*presentation, next)) {
    ++next.revision;
  }
  *presentation = next;
  return presentation;
}

} // namespace

void publish_creature_presentation_frame(World& world) {
  auto const unit_ids = world.entities_with<UnitComponent>();
  for (EntityID const id : unit_ids) {
    publish_creature_presentation_entity(world.get_entity(id), &world);
  }
}

auto publish_creature_presentation(Entity* entity,
                                   World* world) -> CreaturePresentationComponent* {
  return publish_creature_presentation_entity(entity, world);
}

void publish_creature_presentations(World& world) {
  if (world.presentation_enabled()) {
    publish_creature_presentation_frame(world);
  }
}

} // namespace Engine::Core

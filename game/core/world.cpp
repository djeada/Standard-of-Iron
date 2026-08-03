#include "world.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <type_traits>
#include <utility>
#include <vector>

#include "../../animation/action_manifest.h"
#include "../systems/building_collision_registry.h"
#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
#include "../systems/unit_activity.h"
#include "component.h"
#include "core/entity.h"
#include "core/system.h"

namespace Engine::Core {

namespace {

constexpr float k_motion_displacement_epsilon_sq = 1.0e-6F;
constexpr float k_motion_velocity_epsilon_sq = 1.0e-4F;
[[nodiscard]] auto
forward_xz_from_yaw(float yaw_degrees) noexcept -> std::pair<float, float> {
  float const yaw_rad = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  return {std::sin(yaw_rad), std::cos(yaw_rad)};
}

void normalize_xz(float& x, float& z) noexcept {
  float const len_sq = x * x + z * z;
  if (len_sq <= 1.0e-8F) {
    x = 0.0F;
    z = 1.0F;
    return;
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  x *= inv_len;
  z *= inv_len;
}

[[nodiscard]] auto
attack_target_is_in_range(World& world,
                          const Entity* attacker_entity,
                          const AttackComponent* attack,
                          const AttackTargetComponent* attack_target,
                          const TransformComponent* transform) -> bool {
  if (attack == nullptr || attack_target == nullptr || attack_target->target_id == 0 ||
      transform == nullptr) {
    return false;
  }

  auto* target = world.get_entity(attack_target->target_id);
  if (target == nullptr) {
    return false;
  }
  if (attack->current_mode == AttackComponent::CombatMode::Melee &&
      attacker_entity != nullptr) {

    auto const* contact = attacker_entity->get_component<FormationContactComponent>();
    if (contact != nullptr) {
      return contact->target_id == attack_target->target_id && contact->in_contact;
    }
  }
  auto* target_transform = target->get_component<TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  float const dx = target_transform->position.x - transform->position.x;
  float const dz = target_transform->position.z - transform->position.z;
  float const dist_sq = dx * dx + dz * dz;
  float target_radius =
      std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
  if (auto* elephant = target->get_component<ElephantComponent>()) {
    target_radius = std::max(target_radius, elephant->trample_radius);
  }
  float const effective_range = attack->range + target_radius + 0.25F;
  return dist_sq <= effective_range * effective_range;
}

struct MotionPresentationSample {
  bool displaced{false};
  bool has_component_velocity{false};
  bool has_navigation_intent{false};
  bool direct_control_moving{false};
  bool builder_bypass{false};
  bool has_chase_intent{false};
  bool has_active_navigation_segment{false};
  bool is_running{false};
};

[[nodiscard]] auto resolve_motion_presentation_state(
    const MotionPresentationSample& sample) noexcept -> MotionPresentationState {
  bool const moving = sample.displaced || sample.direct_control_moving ||
                      sample.builder_bypass || sample.has_component_velocity ||
                      (sample.has_chase_intent && sample.has_active_navigation_segment);
  if (!moving) {
    return MotionPresentationState::Idle;
  }
  return sample.is_running ? MotionPresentationState::Run
                           : MotionPresentationState::Walk;
}

void begin_motion_presentation_frame(World& world, float delta_time) {
  auto const unit_ids = world.entities_with<UnitComponent>();
  for (EntityID const id : unit_ids) {
    Entity* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* transform = entity->get_component<TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    auto* motion = get_or_add_component<MotionPresentationComponent>(*entity);
    if (motion == nullptr) {
      continue;
    }
    motion->previous_x = transform->position.x;
    motion->previous_y = transform->position.y;
    motion->previous_z = transform->position.z;
    motion->tick_delta_time = std::max(0.0F, delta_time);
    motion->snapshot_valid = false;
    motion->initialized = true;
  }
}

void finalize_motion_presentation_frame(World& world, float delta_time) {
  const float safe_dt = std::max(delta_time, 1.0e-5F);
  world.each<MotionPresentationComponent, TransformComponent, UnitComponent>(
      [&world, delta_time, safe_dt](EntityID id,
                                    MotionPresentationComponent& motion_value,
                                    TransformComponent& transform_value,
                                    UnitComponent& unit_value) {
        Entity* entity_ptr = world.get_entity(id);
        if (entity_ptr == nullptr) {
          return;
        }
        Entity& entity = *entity_ptr;
        auto* motion = &motion_value;
        auto* transform = &transform_value;
        auto* unit = &unit_value;

        auto* movement = entity.get_component<MovementComponent>();
        auto* attack = entity.get_component<AttackComponent>();
        auto* attack_target = entity.get_component<AttackTargetComponent>();
        auto* commander = entity.get_component<CommanderComponent>();
        auto* builder_prod = entity.get_component<BuilderProductionComponent>();
        auto* stamina = entity.get_component<StaminaComponent>();

        float const displacement_x = transform->position.x - motion->previous_x;
        float const displacement_z = transform->position.z - motion->previous_z;
        float const displacement_sq =
            displacement_x * displacement_x + displacement_z * displacement_z;
        bool const displaced = displacement_sq > k_motion_displacement_epsilon_sq;

        float movement_speed_sq = 0.0F;
        if (movement != nullptr) {
          movement_speed_sq = movement->get_vx() * movement->get_vx() +
                              movement->get_vz() * movement->get_vz();
        }
        bool const has_component_velocity =
            movement_speed_sq > k_motion_velocity_epsilon_sq;

        bool const has_active_navigation_segment =
            movement != nullptr &&
            (movement->get_has_target() || movement->has_waypoints());
        bool const has_navigation_intent =
            has_active_navigation_segment || has_component_velocity;

        bool const direct_control_moving =
            commander != nullptr && commander->fpv_controlled &&
            ((commander->fpv_motion_vx * commander->fpv_motion_vx +
              commander->fpv_motion_vz * commander->fpv_motion_vz) >
                 k_motion_velocity_epsilon_sq ||
             has_component_velocity);
        bool const builder_bypass =
            builder_prod != nullptr && builder_prod->bypass_movement_active;

        motion->attack_target_in_range =
            attack_target_is_in_range(world, &entity, attack, attack_target, transform);
        motion->has_chase_intent =
            attack_target != nullptr && attack_target->target_id > 0 &&
            attack_target->should_chase && !motion->attack_target_in_range;

        MotionPresentationSample sample{};
        sample.displaced = displaced;
        sample.has_component_velocity = has_component_velocity;
        sample.has_navigation_intent = has_navigation_intent;
        sample.direct_control_moving = direct_control_moving;
        sample.builder_bypass = builder_bypass;
        sample.has_chase_intent = motion->has_chase_intent;
        sample.has_active_navigation_segment = has_active_navigation_segment;
        sample.is_running = stamina != nullptr && stamina->is_running;

        const MotionPresentationState next_state =
            resolve_motion_presentation_state(sample);
        motion->set_state(next_state);
        motion->state_time = motion->state_changed
                                 ? 0.0F
                                 : motion->state_time + std::max(0.0F, delta_time);
        motion->has_velocity = displaced || has_component_velocity;
        motion->has_navigation_intent = has_navigation_intent || builder_bypass;

        motion->displacement_x = displacement_x;
        motion->displacement_z = displacement_z;
        if (has_component_velocity && movement != nullptr) {
          motion->velocity_x = movement->get_vx();
          motion->velocity_z = movement->get_vz();
          motion->speed = std::sqrt(movement_speed_sq);
        } else if (displaced) {
          motion->velocity_x = displacement_x / safe_dt;
          motion->velocity_z = displacement_z / safe_dt;
          motion->speed = std::sqrt(displacement_sq) / safe_dt;
        } else {
          motion->velocity_x = 0.0F;
          motion->velocity_z = 0.0F;
          motion->speed = next_state != MotionPresentationState::Idle
                              ? std::max(0.1F, unit->speed)
                              : 0.0F;
          if (next_state == MotionPresentationState::Run && stamina != nullptr) {
            motion->speed *= StaminaComponent::k_run_speed_multiplier;
          }
        }

        motion->has_movement_target = false;
        if (builder_bypass) {
          motion->movement_target_x = builder_prod->bypass_target_x;
          motion->movement_target_z = builder_prod->bypass_target_z;
          motion->has_movement_target = true;
        } else if (movement != nullptr && movement->has_waypoints()) {
          auto const& waypoint = movement->current_waypoint();
          motion->movement_target_x = waypoint.first;
          motion->movement_target_z = waypoint.second;
          motion->has_movement_target = true;
        } else if (movement != nullptr && movement->get_has_target()) {
          motion->movement_target_x = movement->get_target_x();
          motion->movement_target_z = movement->get_target_y();
          motion->has_movement_target = true;
        } else if (motion->has_chase_intent && attack_target != nullptr) {
          if (auto* target = world.get_entity(attack_target->target_id)) {
            if (auto* target_transform = target->get_component<TransformComponent>()) {
              motion->movement_target_x = target_transform->position.x;
              motion->movement_target_z = target_transform->position.z;
              motion->has_movement_target = true;
            }
          }
        }

        if (has_component_velocity && movement != nullptr) {
          motion->direction_x = movement->get_vx();
          motion->direction_z = movement->get_vz();
        } else if (displaced) {
          motion->direction_x = displacement_x;
          motion->direction_z = displacement_z;
        } else if (motion->has_movement_target) {
          motion->direction_x = motion->movement_target_x - transform->position.x;
          motion->direction_z = motion->movement_target_z - transform->position.z;
        } else {
          auto [forward_x, forward_z] = forward_xz_from_yaw(transform->rotation.y);
          motion->direction_x = forward_x;
          motion->direction_z = forward_z;
        }
        normalize_xz(motion->direction_x, motion->direction_z);

        if (direct_control_moving) {
          motion->source = MotionPresentationSource::DirectControl;
        } else if (builder_bypass) {
          motion->source = MotionPresentationSource::BuilderBypass;
        } else if (motion->has_chase_intent) {
          motion->source = MotionPresentationSource::Chase;
        } else if (has_navigation_intent) {
          motion->source = MotionPresentationSource::Navigation;
        } else if (displaced) {
          motion->source = MotionPresentationSource::ForcedDisplacement;
        } else {
          motion->source = MotionPresentationSource::None;
        }

        motion->seconds_since_motion =
            motion->has_locomotion()
                ? 0.0F
                : motion->seconds_since_motion + std::max(0.0F, delta_time);
        motion->snapshot_valid = true;
      });
}

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

void publish_creature_presentation_entity(Entity* entity, World* world) {
  if (entity == nullptr) {
    return;
  }
  auto const* unit = entity->get_component<UnitComponent>();

  auto* presentation = get_or_add_component<CreaturePresentationComponent>(entity);
  if (presentation == nullptr) {
    return;
  }
  CreaturePresentationComponent next;
  next.snapshot_valid = true;

  auto const* target_ref = entity->get_component<AttackTargetComponent>();
  next.target_id = target_ref != nullptr ? target_ref->target_id : 0U;
  if (next.target_id != 0U && world != nullptr) {
    auto* target = world->get_entity(next.target_id);
    auto const* target_unit =
        target != nullptr ? target->get_component<UnitComponent>() : nullptr;
    next.target_alive = target_unit != nullptr && target_unit->health > 0 &&
                        !target->has_component<PendingRemovalComponent>() &&
                        !target->has_component<DeathAnimationComponent>();
  }

  auto const* death = entity->get_component<DeathAnimationComponent>();
  auto const* builder = entity->get_component<BuilderProductionComponent>();
  auto const* combat = entity->get_component<CombatStateComponent>();
  auto const* attack = entity->get_component<AttackComponent>();
  auto const* hit = entity->get_component<HitFeedbackComponent>();
  auto const* special = entity->get_component<SpecialAttackComponent>();
  bool const uses_rpg_rules =
      (entity->get_component<CommanderComponent>() != nullptr &&
       entity->get_component<CommanderComponent>()->fpv_controlled) ||
      (entity->get_component<RpgHealthComponent>() != nullptr &&
       entity->get_component<RpgHealthComponent>()->active);

  Animation::HumanoidActionSampleInputs action_inputs{};
  if (death != nullptr) {
    action_inputs.death = {
        .active = true,
        .dying = death->state == DeathSequenceState::Dying,
        .state_time = death->state_time,
        .state_duration = death->state_duration,
        .variant = death->sequence_variant,
    };
  }
  if (builder != nullptr && builder->in_progress) {
    action_inputs.construction = {
        .active = true,
        .build_time = builder->build_time,
        .time_remaining = builder->time_remaining,
    };
  }
  if (combat != nullptr) {
    action_inputs.combat = {
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
  if (attack != nullptr) {
    action_inputs.melee_lock = {
        .in_lock = attack->in_melee_lock,
        .participates = !uses_rpg_rules,
        .fallback_attack_family =
            unit != nullptr ? to_animation_family(resolve_combat_attack_family(
                                  unit->spawn_type, AttackComponent::CombatMode::Melee))
                            : Animation::CombatAttackFamily::None,
    };
  }
  if (hit != nullptr && hit->is_reacting) {
    action_inputs.hit_reaction = {
        .active = true,
        .reaction_time = hit->reaction_time,
        .reaction_duration = HitFeedbackComponent::k_reaction_duration,
        .intensity = hit->reaction_intensity,
        .knockback_x = hit->knockback_x,
        .knockback_z = hit->knockback_z,
    };
  }
  if (special != nullptr && special->use_projectile_system &&
      Game::Systems::is_cast_projectile_kind(special->projectile_kind)) {
    action_inputs.cast = {
        .has_projectile_cast = true,
        .projectile_is_fireball =
            special->projectile_kind == Game::Systems::ProjectileKind::Fireball,
    };
  }
  auto const action = Animation::resolve_humanoid_action_sample(action_inputs);
  next.is_attacking = action.is_attacking;
  next.is_melee = action.is_melee;
  next.is_in_melee_lock = action.is_in_melee_lock;
  next.combat_phase =
      combat != nullptr ? combat->animation_state : CombatAnimationState::Idle;
  next.combat_phase_progress = action.combat_phase_progress;
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
  next.hit_recoil_x = action.hit_recoil_x;
  next.hit_recoil_z = action.hit_recoil_z;
  next.is_constructing = action.is_constructing;
  next.construction_progress = action.construction_progress;

  if (action.is_in_melee_lock) {
    next.is_constructing = false;
    next.construction_progress = 0.0F;
  }
  next.is_dying = action.is_dying;
  next.is_dead = action.is_dead;
  next.death_progress = action.death_progress;
  next.death_variant = action.death_variant;

  auto const* formation = entity->get_component<FormationPresentationComponent>();
  next.allow_full_body_hit_reaction =
      formation == nullptr || formation->allow_full_body_hit_reaction;
  if (!next.allow_full_body_hit_reaction) {
    next.is_hit_reacting = false;
    next.hit_reaction_intensity = 0.0F;
    next.hit_recoil_x = 0.0F;
    next.hit_recoil_z = 0.0F;
  }

  auto const* transform = entity->get_component<TransformComponent>();
  auto const* healer = entity->get_component<HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr &&
      !action.is_in_melee_lock) {
    next.is_healing = true;
    next.healing_target_dx = healer->healing_target_x - transform->position.x;
    next.healing_target_dz = healer->healing_target_z - transform->position.z;
  }
  auto const* commander = entity->get_component<CommanderComponent>();
  next.has_commander = commander != nullptr;
  next.fpv_controlled = commander != nullptr && commander->fpv_controlled;
  if (commander != nullptr) {
    next.jump_active = commander->jump_active;
    next.jump_phase = commander->jump_phase;
    next.jump_height_offset = commander->jump_height_offset;
    next.flag_rally_planting = commander->is_flag_rally_planting();
    next.flag_rally_animation_timer = commander->flag_rally_animation_timer;
    next.flag_rally_cost = commander->flag_rally_cost;
  }
  auto const* commander_guard = entity->get_component<CommanderGuardComponent>();
  auto const* formation_mode = entity->get_component<FormationModeComponent>();
  auto const* guard_mode = entity->get_component<GuardModeComponent>();
  auto const* brace = entity->get_component<SpearBraceComponent>();
  next.formation_guard_active = (formation_mode != nullptr && formation_mode->active) ||
                                (guard_mode != nullptr && guard_mode->active);
  next.guard_requested =
      (next.fpv_controlled && commander_guard != nullptr && commander_guard->active) ||
      (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Knight &&
       next.formation_guard_active) ||
      (brace != nullptr && (brace->requested || brace->active));
  next.activity = Game::Systems::classify_unit_activity(*entity);

  auto const* hold = entity->get_component<HoldModeComponent>();
  if (hold != nullptr) {
    next.hold_requested = hold->active;
    next.hold_exit_requested = !hold->active && hold->exit_cooldown > 0.0F;
    next.hold_entry_progress = hold->kneel_entry_progress;
    next.hold_exit_progress =
        1.0F - hold->exit_cooldown / std::max(hold->stand_up_duration, 1.0e-4F);
    next.hold_enter_duration = hold->kneel_duration;
    next.hold_exit_duration = hold->stand_up_duration;
  }
  auto const* showcase = entity->get_component<ShowcaseRoutineComponent>();
  if (showcase != nullptr) {
    next.showcase_active = showcase->active;
    next.showcase_move = showcase->current_move;
    next.showcase_phase = showcase->phase;
  }
  auto const* authored = entity->get_component<RpgCommanderActionComponent>();
  if (authored != nullptr) {
    next.authored_action_id = authored->combat_action_id;
    next.authored_action_running = authored->action_running;
    next.authored_action_completed = authored->action_completed;
    next.authored_action_phase = authored->normalized_action_time;
  }
  next.combat_active = next.is_attacking || next.is_hit_reacting || next.is_dying ||
                       next.is_dead || next.target_id != 0U;

  next.revision = presentation->revision;
  bool const changed = presentation->snapshot_valid != next.snapshot_valid ||
                       presentation->target_id != next.target_id ||
                       presentation->target_alive != next.target_alive ||
                       presentation->combat_active != next.combat_active ||
                       presentation->is_attacking != next.is_attacking ||
                       presentation->is_melee != next.is_melee ||
                       presentation->combat_phase != next.combat_phase ||
                       presentation->is_hit_reacting != next.is_hit_reacting ||
                       presentation->is_dying != next.is_dying ||
                       presentation->is_dead != next.is_dead ||
                       presentation->guard_requested != next.guard_requested ||
                       presentation->hold_requested != next.hold_requested ||
                       presentation->showcase_active != next.showcase_active ||
                       presentation->showcase_move != next.showcase_move ||
                       presentation->showcase_phase != next.showcase_phase;
  if (changed) {
    ++next.revision;
  }
  *presentation = next;
}

void publish_creature_presentation_frame(World& world) {
  auto const unit_ids = world.entities_with<UnitComponent>();
  for (EntityID const id : unit_ids) {
    publish_creature_presentation_entity(world.get_entity(id), &world);
  }
}

template <typename ComponentType>
void copy_snapshot_component(const Entity& source, Entity& destination) {
  if (auto const* component = source.get_component<ComponentType>()) {
    static_assert(std::is_copy_constructible_v<ComponentType>);
    destination.add_component<ComponentType>(*component);
  }
}

void copy_render_components(const Entity& source, Entity& destination) {
  copy_snapshot_component<TransformComponent>(source, destination);
  copy_snapshot_component<UnitComponent>(source, destination);
  copy_snapshot_component<RenderableComponent>(source, destination);
  copy_snapshot_component<MovementComponent>(source, destination);
  copy_snapshot_component<MotionPresentationComponent>(source, destination);
  copy_snapshot_component<CreaturePresentationComponent>(source, destination);
  copy_snapshot_component<BuildingComponent>(source, destination);
  copy_snapshot_component<PendingRemovalComponent>(source, destination);
  copy_snapshot_component<AttackComponent>(source, destination);
  copy_snapshot_component<AttackTargetComponent>(source, destination);
  copy_snapshot_component<CombatStateComponent>(source, destination);
  copy_snapshot_component<FormationContactComponent>(source, destination);
  copy_snapshot_component<FormationRosterPresentationComponent>(source, destination);
  copy_snapshot_component<FormationPresentationComponent>(source, destination);
  copy_snapshot_component<FormationHitPresentationComponent>(source, destination);
  copy_snapshot_component<SoldierCasualtyAnimationComponent>(source, destination);
  copy_snapshot_component<DeathAnimationComponent>(source, destination);
  copy_snapshot_component<BuilderProductionComponent>(source, destination);
  copy_snapshot_component<ProductionComponent>(source, destination);
  copy_snapshot_component<CaptureComponent>(source, destination);
  copy_snapshot_component<CommanderComponent>(source, destination);
  copy_snapshot_component<CommanderAuraBuffComponent>(source, destination);
  copy_snapshot_component<RpgCommanderActionComponent>(source, destination);
  copy_snapshot_component<RpgCommanderTargetComponent>(source, destination);
  copy_snapshot_component<HealerComponent>(source, destination);
  copy_snapshot_component<PatrolComponent>(source, destination);
  copy_snapshot_component<GuardModeComponent>(source, destination);
  copy_snapshot_component<HoldModeComponent>(source, destination);
  copy_snapshot_component<FormationModeComponent>(source, destination);
  copy_snapshot_component<SpearBraceComponent>(source, destination);
  copy_snapshot_component<StaminaComponent>(source, destination);
  copy_snapshot_component<MoraleComponent>(source, destination);
  copy_snapshot_component<BurningStatusComponent>(source, destination);
  copy_snapshot_component<StaggerComponent>(source, destination);
  copy_snapshot_component<HitFeedbackComponent>(source, destination);
  copy_snapshot_component<ConstructionPreviewComponent>(source, destination);
  copy_snapshot_component<WallConstructionSiteComponent>(source, destination);
  copy_snapshot_component<StructureDamagePresentationComponent>(source, destination);
  copy_snapshot_component<RpgContactPresentationComponent>(source, destination);
  copy_snapshot_component<BloodStainComponent>(source, destination);
  copy_snapshot_component<FirePatchComponent>(source, destination);
  copy_snapshot_component<StructureFireComponent>(source, destination);
  copy_snapshot_component<ElephantComponent>(source, destination);
  copy_snapshot_component<ElephantStompImpactComponent>(source, destination);
  copy_snapshot_component<CatapultLoadingComponent>(source, destination);
  copy_snapshot_component<GateComponent>(source, destination);
}

void render_hash_combine(std::uint64_t& seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void render_hash_float(std::uint64_t& seed, float value) {
  render_hash_combine(seed, std::bit_cast<std::uint32_t>(value));
}

auto render_entity_is_stable(const Entity& entity) -> bool {
  auto const* movement = entity.get_component<MovementComponent>();
  auto const* motion = entity.get_component<MotionPresentationComponent>();
  auto const* creature = entity.get_component<CreaturePresentationComponent>();
  auto const* target = entity.get_component<AttackTargetComponent>();
  auto const* combat = entity.get_component<CombatStateComponent>();
  auto const* contact = entity.get_component<FormationContactComponent>();
  auto const* casualties = entity.get_component<SoldierCasualtyAnimationComponent>();
  bool const moving = (movement != nullptr &&
                       (movement->get_has_target() || movement->has_waypoints() ||
                        std::hypot(movement->get_vx(), movement->get_vz()) > 0.001F)) ||
                      (motion != nullptr && motion->has_locomotion());
  bool const active_creature =
      creature != nullptr &&
      (creature->combat_active || creature->is_constructing || creature->is_healing ||
       creature->is_dying || creature->is_dead || creature->jump_active ||
       creature->flag_rally_planting || creature->authored_action_running);
  bool const active_combat =
      (target != nullptr && target->target_id != 0) ||
      (combat != nullptr && combat->animation_state != CombatAnimationState::Idle) ||
      (contact != nullptr && (contact->in_contact || !contact->fronts.empty())) ||
      (casualties != nullptr && !casualties->entries.empty());
  bool const transient = entity.has_component<PendingRemovalComponent>() ||
                         entity.has_component<DeathAnimationComponent>() ||
                         entity.has_component<BuilderProductionComponent>() ||
                         entity.has_component<ProductionComponent>() ||
                         entity.has_component<CaptureComponent>() ||
                         entity.has_component<CommanderComponent>() ||
                         entity.has_component<CommanderAuraBuffComponent>() ||
                         entity.has_component<RpgCommanderActionComponent>() ||
                         entity.has_component<RpgCommanderTargetComponent>() ||
                         entity.has_component<HealerComponent>() ||
                         entity.has_component<BurningStatusComponent>() ||
                         entity.has_component<StaggerComponent>() ||
                         entity.has_component<HitFeedbackComponent>() ||
                         entity.has_component<FormationHitPresentationComponent>() ||
                         entity.has_component<ConstructionPreviewComponent>() ||
                         entity.has_component<WallConstructionSiteComponent>() ||
                         entity.has_component<StructureDamagePresentationComponent>() ||
                         entity.has_component<RpgContactPresentationComponent>() ||
                         entity.has_component<BloodStainComponent>() ||
                         entity.has_component<FirePatchComponent>() ||
                         entity.has_component<StructureFireComponent>() ||
                         entity.has_component<ElephantStompImpactComponent>() ||
                         entity.has_component<CatapultLoadingComponent>();
  return !moving && !active_creature && !active_combat && !transient;
}

auto render_entity_signature(const Entity& entity) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  if (auto const* transform = entity.get_component<TransformComponent>()) {
    render_hash_float(signature, transform->position.x);
    render_hash_float(signature, transform->position.y);
    render_hash_float(signature, transform->position.z);
    render_hash_float(signature, transform->rotation.x);
    render_hash_float(signature, transform->rotation.y);
    render_hash_float(signature, transform->rotation.z);
    render_hash_float(signature, transform->scale.x);
    render_hash_float(signature, transform->scale.y);
    render_hash_float(signature, transform->scale.z);
  }
  if (auto const* unit = entity.get_component<UnitComponent>()) {
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->health));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->max_health));
    render_hash_float(signature, unit->speed);
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->owner_id));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
    render_hash_combine(
        signature,
        static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
  }
  if (auto const* gate = entity.get_component<GateComponent>()) {

    render_hash_float(signature, gate->open_amount);
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->state));
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->manual_mode));
  }
  if (auto const* renderable = entity.get_component<RenderableComponent>()) {
    render_hash_combine(signature, std::hash<std::string>{}(renderable->mesh_path));
    render_hash_combine(signature, std::hash<std::string>{}(renderable->texture_path));
    render_hash_combine(signature, std::hash<std::string>{}(renderable->renderer_id));
    render_hash_combine(signature, renderable->visible ? 1U : 0U);
    render_hash_combine(signature, static_cast<std::uint64_t>(renderable->mesh));
    for (float const channel : renderable->color) {
      render_hash_float(signature, channel);
    }
  }
  if (auto const* morale = entity.get_component<MoraleComponent>()) {
    render_hash_float(signature, morale->morale);
    render_hash_float(signature, morale->commander_aura_bonus);
    render_hash_combine(signature, morale->wavering ? 1U : 0U);
    render_hash_combine(signature, morale->routing ? 1U : 0U);
  }
  if (auto const* creature = entity.get_component<CreaturePresentationComponent>()) {
    render_hash_combine(signature, creature->revision);
  }
  if (auto const* formation = entity.get_component<FormationPresentationComponent>()) {
    render_hash_combine(signature, formation->revision);
  }
  if (auto const* roster =
          entity.get_component<FormationRosterPresentationComponent>()) {
    render_hash_combine(signature, roster->revision);
  }
  render_hash_combine(signature, entity.has_component<UnitComponent>() ? 1U : 0U);
  render_hash_combine(signature, entity.has_component<BuildingComponent>() ? 1U : 0U);
  return signature;
}

} // namespace

void publish_creature_presentation(Entity* entity, World* world) {
  publish_creature_presentation_entity(entity, world);
}

void World::ComponentSet::insert(EntityID id) {
  const std::uint32_t index = Handle::index_of(id);
  if (sparse.size() <= index) {
    sparse.resize(static_cast<std::size_t>(index) + 1, k_absent);
  }
  if (sparse[index] != k_absent) {

    dense[sparse[index]] = id;
    return;
  }
  sparse[index] = static_cast<std::uint32_t>(dense.size());
  dense.push_back(id);
}

void World::ComponentSet::erase(EntityID id) {
  const std::uint32_t index = Handle::index_of(id);
  if (index >= sparse.size()) {
    return;
  }
  const std::uint32_t position = sparse[index];
  if (position == k_absent) {
    return;
  }

  const EntityID moved = dense.back();
  dense[position] = moved;
  sparse[Handle::index_of(moved)] = position;
  dense.pop_back();
  sparse[index] = k_absent;
}

auto World::ComponentSet::contains(EntityID id) const -> bool {
  const std::uint32_t index = Handle::index_of(id);
  return index < sparse.size() && sparse[index] != k_absent &&
         dense[sparse[index]] == id;
}

void World::ComponentSet::clear() {
  dense.clear();
  sparse.clear();
}

World::World()
    : World(true, false) {
}

World::World(bool presentation_enabled, bool render_snapshot)
    : m_presentation_enabled(presentation_enabled)
    , m_is_render_snapshot(render_snapshot) {
  m_slots.emplace_back();
}

World::~World() = default;

auto World::resolve(EntityID entity_id) const -> Entity* {
  const std::uint32_t index = Handle::index_of(entity_id);
  if (index == 0 || index >= m_slots.size()) {
    return nullptr;
  }
  const auto& slot = m_slots[index];
  if (slot.entity == nullptr || slot.generation != Handle::generation_of(entity_id)) {
    return nullptr;
  }
  return slot.entity.get();
}

void World::detach_from_all_component_sets(EntityID entity_id) {
  for (auto& set : m_component_sets) {
    set.erase(entity_id);
  }
}

void World::on_component_changed(EntityID entity_id,
                                 ComponentTypeId type_id,
                                 std::type_index component_type,
                                 bool added) {

  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  if (m_component_sets.size() <= type_id) {
    m_component_sets.resize(static_cast<std::size_t>(type_id) + 1);
  }
  if (added) {
    m_component_sets[type_id].insert(entity_id);
  } else {
    m_component_sets[type_id].erase(entity_id);
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

void World::setup_entity_callback(Entity* entity) {
  entity->set_component_change_callback([this](EntityID entity_id,
                                               ComponentTypeId type_id,
                                               std::type_index component_type,
                                               bool added) {
    this->on_component_changed(entity_id, type_id, component_type, added);
  });
}

auto World::collect_entities_with(ComponentTypeId type_id) -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (type_id >= m_component_sets.size()) {
    return {};
  }

  const auto& dense = m_component_sets[type_id].dense;
  std::vector<Entity*> result;
  result.reserve(dense.size());
  for (const EntityID id : dense) {
    if (auto* entity = resolve(id)) {
      result.push_back(entity);
    }
  }
  return result;
}

auto World::entities_with(ComponentTypeId type_id) const -> std::span<const EntityID> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (type_id >= m_component_sets.size()) {
    return {};
  }
  return m_component_sets[type_id].dense;
}

void World::resolve_entities_into(std::span<const EntityID> ids,
                                  std::vector<Entity*>& output) const {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  output.clear();
  if (output.capacity() < ids.size()) {
    output.reserve(ids.size());
  }
  for (const EntityID id : ids) {
    if (Entity* entity = resolve(id)) {
      output.push_back(entity);
    }
  }
}

auto World::create_entity() -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  std::uint32_t index = 0;
  if (!m_free_slots.empty()) {
    index = m_free_slots.back();
    m_free_slots.pop_back();
  } else {
    index = static_cast<std::uint32_t>(m_slots.size());
    m_slots.emplace_back();
  }

  auto& slot = m_slots[index];
  const EntityID id = Handle::make(index, slot.generation);
  slot.entity = std::make_unique<Entity>(id);
  setup_entity_callback(slot.entity.get());
  ++m_live_count;
  return slot.entity.get();
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (entity_id == NULL_ENTITY) {
    return nullptr;
  }

  const std::uint32_t index = Handle::index_of(entity_id);
  if (index == 0) {
    return nullptr;
  }

  if (m_slots.size() <= index) {
    const auto previous_size = m_slots.size();
    m_slots.resize(static_cast<std::size_t>(index) + 1);

    for (std::size_t i = previous_size; i < index; ++i) {
      m_free_slots.push_back(static_cast<std::uint32_t>(i));
    }
  } else {
    std::erase(m_free_slots, index);
  }

  auto& slot = m_slots[index];
  if (slot.entity != nullptr) {
    detach_from_all_component_sets(slot.entity->get_id());
    slot.entity.reset();
    --m_live_count;
  }

  slot.generation = Handle::generation_of(entity_id);
  slot.entity = std::make_unique<Entity>(entity_id);
  setup_entity_callback(slot.entity.get());
  ++m_live_count;
  return slot.entity.get();
}

void World::destroy_entity(EntityID entity_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  const std::uint32_t index = Handle::index_of(entity_id);
  if (index != 0 && index < m_slots.size()) {
    auto& slot = m_slots[index];
    if (slot.entity != nullptr && slot.generation == Handle::generation_of(entity_id)) {
      if (!m_is_render_snapshot && slot.entity->has_component<BuildingComponent>()) {

        Game::Systems::BuildingCollisionRegistry::instance().unregister_building(
            entity_id);
      }

      detach_from_all_component_sets(entity_id);
      slot.entity.reset();

      ++slot.generation;
      m_free_slots.push_back(index);
      --m_live_count;
    }
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  for (std::size_t i = 1; i < m_slots.size(); ++i) {
    if (m_slots[i].entity != nullptr) {
      m_slots[i].entity.reset();
      ++m_slots[i].generation;
    }
  }
  m_free_slots.clear();
  for (std::size_t i = m_slots.size(); i-- > 1;) {
    m_free_slots.push_back(static_cast<std::uint32_t>(i));
  }
  m_live_count = 0;

  for (auto& set : m_component_sets) {
    set.clear();
  }

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return resolve(entity_id);
}

auto World::is_alive(EntityID entity_id) const -> bool {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return resolve(entity_id) != nullptr;
}

auto World::entity_count() const -> std::size_t {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return m_live_count;
}

void World::add_system(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float delta_time) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (m_presentation_enabled) {
    begin_motion_presentation_frame(*this, delta_time);
  }
  for (auto& system : m_systems) {
    system->update(this, delta_time);
  }
  if (m_presentation_enabled) {
    finalize_motion_presentation_frame(*this, delta_time);
    publish_creature_presentation_frame(*this);
  }
  if (!m_is_render_snapshot &&
      m_render_snapshots_requested.load(std::memory_order_acquire)) {
    publish_render_snapshot();
  }
}

auto World::acquire_render_snapshot() const -> std::shared_ptr<World> {
  return std::atomic_load_explicit(&m_render_snapshot, std::memory_order_acquire);
}

void World::publish_render_snapshot() {
  std::size_t const buffer_index = m_next_render_snapshot_buffer;
  m_next_render_snapshot_buffer =
      (m_next_render_snapshot_buffer + 1U) % m_render_snapshot_buffers.size();
  auto& buffer = m_render_snapshot_buffers[buffer_index];
  auto const published = acquire_render_snapshot();
  if (buffer == nullptr || buffer == published || buffer.use_count() > 1) {
    buffer = std::shared_ptr<World>(new World(false, true));
  }
  auto snapshot = buffer;
  snapshot->m_render_unit_ids.clear();
  snapshot->m_render_building_ids.clear();
  snapshot->m_render_other_ids.clear();
  if (snapshot->m_render_entity_signatures.size() < m_slots.size()) {
    snapshot->m_render_entity_signatures.resize(m_slots.size(), 0U);
  }
  ++m_render_publish_revision;
  snapshot->m_render_unit_ids.reserve(entities_with<UnitComponent>().size());
  snapshot->m_render_building_ids.reserve(entities_with<BuildingComponent>().size());
  snapshot->m_render_other_ids.reserve(entities_with<RenderableComponent>().size());

  for (std::size_t index = 1; index < m_slots.size(); ++index) {
    auto const& slot = m_slots[index];
    auto& snapshot_slot = index < snapshot->m_slots.size()
                              ? snapshot->m_slots[index]
                              : snapshot->m_slots.emplace_back();
    if (slot.entity == nullptr) {
      if (snapshot_slot.entity != nullptr) {
        snapshot->destroy_entity(snapshot_slot.entity->get_id());
      }
      snapshot->m_render_entity_signatures[index] = 0U;
      continue;
    }
    Entity const& source = *slot.entity;
    std::uint64_t signature = render_entity_signature(source);
    if (!render_entity_is_stable(source)) {
      render_hash_combine(signature, m_render_publish_revision);
    }
    Entity* destination = snapshot->resolve(source.get_id());
    bool const reusable = destination != nullptr &&
                          snapshot->m_render_entity_signatures[index] == signature;
    if (!reusable) {
      if (snapshot_slot.entity != nullptr) {
        snapshot->destroy_entity(snapshot_slot.entity->get_id());
      }
      destination = snapshot->create_entity_with_id(source.get_id());
      if (destination == nullptr) {
        continue;
      }
      copy_render_components(source, *destination);
      snapshot->m_render_entity_signatures[index] = signature;
    }

    if (!destination->has_component<RenderableComponent>() ||
        destination->has_component<PendingRemovalComponent>()) {
      continue;
    }
    if (destination->has_component<UnitComponent>()) {
      snapshot->m_render_unit_ids.push_back(source.get_id());
    } else if (destination->has_component<BuildingComponent>()) {
      snapshot->m_render_building_ids.push_back(source.get_id());
    } else {
      snapshot->m_render_other_ids.push_back(source.get_id());
    }
  }
  std::atomic_store_explicit(
      &m_render_snapshot, std::move(snapshot), std::memory_order_release);
}

namespace {

template <typename Predicate>
auto collect_units(const World& world, Predicate&& predicate) -> std::vector<Entity*> {
  std::vector<Entity*> result;
  result.reserve(world.entity_count());
  world.for_each_entity([&](Entity& entity) {
    auto* unit = entity.get_component<UnitComponent>();
    if (unit != nullptr && predicate(*unit)) {
      result.push_back(&entity);
    }
  });
  return result;
}

} // namespace

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units(*this, [owner_id](const UnitComponent& unit) {
    return unit.owner_id == owner_id;
  });
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units(*this, [owner_id](const UnitComponent& unit) {
    return unit.owner_id != owner_id;
  });
}

auto World::get_allied_units(int owner_id) const -> std::vector<Entity*> {
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  return collect_units(*this, [owner_id, &owner_registry](const UnitComponent& unit) {
    return unit.owner_id == owner_id ||
           owner_registry.are_allies(owner_id, unit.owner_id);
  });
}

auto World::get_enemy_units(int owner_id) const -> std::vector<Entity*> {
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  return collect_units(*this, [owner_id, &owner_registry](const UnitComponent& unit) {
    return owner_registry.are_enemies(owner_id, unit.owner_id);
  });
}

auto World::count_troops_for_player(int owner_id) -> int {
  return Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
}

auto World::get_next_entity_id() const -> EntityID {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return Handle::make(static_cast<std::uint32_t>(m_slots.size()), 0);
}

void World::set_next_entity_id(EntityID next_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const std::uint32_t index = Handle::index_of(next_id);
  if (m_slots.size() < index) {
    const auto previous_size = m_slots.size();
    m_slots.resize(index);
    for (std::size_t i = previous_size; i < index; ++i) {
      m_free_slots.push_back(static_cast<std::uint32_t>(i));
    }
  }
}

auto World::add_component_observer(ComponentObserverCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_component_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_entity_destroyed_observer(EntityDestroyedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_entity_destroyed_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_world_cleared_observer(WorldClearedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_world_cleared_observers.push_back({handle, std::move(callback)});
  return handle;
}

void World::remove_component_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_component_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_entity_destroyed_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_entity_destroyed_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_world_cleared_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_world_cleared_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

} // namespace Engine::Core

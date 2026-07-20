#include "world.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <utility>
#include <vector>

#include "../../animation/action_manifest.h"
#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
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
  const std::lock_guard<std::recursive_mutex> lock(world.get_entity_mutex());
  for (const auto& [entity_id, entity] : world.get_entities()) {
    (void)entity_id;
    auto* transform = entity->get_component<TransformComponent>();
    auto* unit = entity->get_component<UnitComponent>();
    if (transform == nullptr || unit == nullptr) {
      continue;
    }
    auto* motion = get_or_add_component<MotionPresentationComponent>(entity.get());
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
  const std::lock_guard<std::recursive_mutex> lock(world.get_entity_mutex());
  for (const auto& [entity_id, entity] : world.get_entities()) {
    (void)entity_id;
    auto* motion = entity->get_component<MotionPresentationComponent>();
    auto* transform = entity->get_component<TransformComponent>();
    auto* unit = entity->get_component<UnitComponent>();
    if (motion == nullptr || transform == nullptr || unit == nullptr) {
      continue;
    }

    auto* movement = entity->get_component<MovementComponent>();
    auto* attack = entity->get_component<AttackComponent>();
    auto* attack_target = entity->get_component<AttackTargetComponent>();
    auto* commander = entity->get_component<CommanderComponent>();
    auto* builder_prod = entity->get_component<BuilderProductionComponent>();
    auto* stamina = entity->get_component<StaminaComponent>();

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

    motion->attack_target_in_range = attack_target_is_in_range(
        world, entity.get(), attack, attack_target, transform);
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
    motion->state_time =
        motion->state_changed ? 0.0F : motion->state_time + std::max(0.0F, delta_time);
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
  }
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
  if (healer != nullptr && healer->is_healing_active && transform != nullptr) {
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
  auto const* patrol = entity->get_component<PatrolComponent>();
  auto const* brace = entity->get_component<SpearBraceComponent>();
  next.formation_guard_active = (formation_mode != nullptr && formation_mode->active) ||
                                (guard_mode != nullptr && guard_mode->active);
  next.guard_requested =
      (next.fpv_controlled && commander_guard != nullptr && commander_guard->active) ||
      (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Knight &&
       next.formation_guard_active) ||
      (brace != nullptr && (brace->requested || brace->active));
  next.guard_mode_indicator = guard_mode != nullptr && guard_mode->active;
  next.patrol_mode_indicator = patrol != nullptr && patrol->patrolling;

  auto const* hold = entity->get_component<HoldModeComponent>();
  if (hold != nullptr) {
    next.hold_mode_indicator = hold->active;
    next.hold_requested = hold->active;
    next.hold_exit_requested = !hold->active && hold->exit_cooldown > 0.0F;
    next.hold_entry_progress = hold->kneel_entry_progress;
    next.hold_exit_progress =
        1.0F - hold->exit_cooldown / std::max(hold->stand_up_duration, 1.0e-4F);
    next.hold_enter_duration = hold->kneel_duration;
    next.hold_exit_duration = hold->stand_up_duration;
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
                       presentation->hold_requested != next.hold_requested;
  if (changed) {
    ++next.revision;
  }
  *presentation = next;
}

void publish_creature_presentation_frame(World& world) {
  const std::lock_guard<std::recursive_mutex> lock(world.get_entity_mutex());
  for (const auto& [entity_id, owned_entity] : world.get_entities()) {
    (void)entity_id;
    publish_creature_presentation_entity(owned_entity.get(), &world);
  }
}

} // namespace

void publish_creature_presentation(Entity* entity, World* world) {
  publish_creature_presentation_entity(entity, world);
}

World::World() = default;
World::~World() = default;

void World::on_component_changed(EntityID entity_id,
                                 std::type_index component_type,
                                 bool added) {

  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  if (added) {
    m_component_index[component_type].insert(entity_id);
  } else {
    auto it = m_component_index.find(component_type);
    if (it != m_component_index.end()) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        m_component_index.erase(it);
      }
    }
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

void World::setup_entity_callback(Entity* entity) {
  entity->set_component_change_callback(
      [this](EntityID entity_id, std::type_index component_type, bool added) {
        this->on_component_changed(entity_id, component_type, added);
      });
}

auto World::create_entity() -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  EntityID const id = m_next_entity_id++;
  auto entity = std::make_unique<Entity>(id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[id] = std::move(entity);
  return ptr;
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (entity_id == NULL_ENTITY) {
    return nullptr;
  }

  if (m_entities.contains(entity_id)) {
    for (auto it = m_component_index.begin(); it != m_component_index.end();) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        it = m_component_index.erase(it);
      } else {
        ++it;
      }
    }
  }

  auto entity = std::make_unique<Entity>(entity_id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[entity_id] = std::move(entity);

  if (entity_id >= m_next_entity_id) {
    m_next_entity_id = entity_id + 1;
  }

  return ptr;
}

void World::destroy_entity(EntityID entity_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  auto it = m_entities.find(entity_id);

  for (auto& [type, entity_set] : m_component_index) {
    entity_set.erase(entity_id);
  }

  if (it != m_entities.end()) {
    m_entities.erase(it);
  } else {
    m_entities.erase(entity_id);
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_entities.clear();
  m_component_index.clear();
  m_next_entity_id = 1;

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  auto it = m_entities.find(entity_id);
  return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::add_system(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float delta_time) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  begin_motion_presentation_frame(*this, delta_time);
  for (auto& system : m_systems) {
    system->update(this, delta_time);
  }
  finalize_motion_presentation_frame(*this, delta_time);
  publish_creature_presentation_frame(*this);
}

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id != owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_allied_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->owner_id == owner_id ||
        owner_registry.are_allies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_enemy_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (owner_registry.are_enemies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::count_troops_for_player(int owner_id) -> int {
  return Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
}

auto World::get_next_entity_id() const -> EntityID {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return m_next_entity_id;
}

void World::set_next_entity_id(EntityID next_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_next_entity_id = std::max(next_id, m_next_entity_id);
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

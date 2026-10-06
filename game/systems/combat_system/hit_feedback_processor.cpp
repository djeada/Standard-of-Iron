#include "hit_feedback_processor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/component_gameplay.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../combat_rules.h"
#include "../formation_combat_geometry.h"
#include "../navigation/nav_grid.h"
#include "../navigation/pathfinding.h"
#include "damage_application.h"
#include "formation_casualties.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

[[nodiscard]] auto locked_commander_holds_ground(
    const Engine::Core::Entity& unit,
    const Engine::Core::HitFeedbackComponent& feedback) -> bool {
  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || commander->fpv_controlled ||
      !commander->advanced_combat_enabled) {
    return false;
  }
  auto const* attack = unit.get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr || !attack->in_melee_lock) {
    return false;
  }
  if (feedback.stagger_tier != Engine::Core::StaggerTier::LightFlinch) {
    return false;
  }
  auto const* stagger = unit.get_component<Engine::Core::StaggerComponent>();
  return stagger == nullptr || stagger->tier == Engine::Core::StaggerTier::LightFlinch;
}

[[nodiscard]] auto
knockback_moves_body(const Engine::Core::Entity& unit,
                     const Engine::Core::HitFeedbackComponent& feedback) -> bool {
  const auto* registry = unit.registry();
  if (is_building(&unit) || unit.has_component<Engine::Core::ElephantComponent>() ||
      (registry != nullptr &&
       registry->has<Engine::Core::WildlifeComponent>(unit.get_id()))) {

    return false;
  }
  if (Game::Systems::CombatRules::uses_rpg_combat_rules(&unit)) {
    return false;
  }
  if (const auto* unit_component = unit.get_component<Engine::Core::UnitComponent>();
      unit_component != nullptr &&
      Game::Units::is_siege_engine_spawn(unit_component->spawn_type)) {
    return false;
  }
  if (FormationCombat::has_formation_slots(unit)) {
    return false;
  }
  if (locked_commander_holds_ground(unit, feedback)) {
    return false;
  }
  auto const* movement = unit.get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->get_has_target()) {
    return false;
  }
  return true;
}

[[nodiscard]] auto
knockback_travel_scale(Engine::Core::HitReactionKind kind) noexcept -> float {
  switch (kind) {
  case Engine::Core::HitReactionKind::Flinch:
    return 1.0F;
  case Engine::Core::HitReactionKind::Block:
    return 0.8F;
  case Engine::Core::HitReactionKind::Evade:
    return 1.15F;
  case Engine::Core::HitReactionKind::Stagger:
    return 1.2F;
  case Engine::Core::HitReactionKind::Recoil:
    return 0.9F;
  }
  return 1.0F;
}

void apply_knockback_step(Engine::Core::Entity& unit,
                          Engine::Core::HitFeedbackComponent& feedback,
                          float progress) {
  float const total_x =
      feedback.knockback_x * knockback_travel_scale(feedback.reaction_kind);
  float const total_z =
      feedback.knockback_z * knockback_travel_scale(feedback.reaction_kind);
  float const total = std::hypot(total_x, total_z);
  if (total <= 0.0005F) {
    return;
  }
  float const clamped = std::clamp(progress, 0.0F, 1.0F);
  float const remaining = 1.0F - clamped;
  float const eased = 1.0F - remaining * remaining * remaining;
  float const desired = eased * total;
  auto* transform = unit.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr || !knockback_moves_body(unit, feedback)) {

    feedback.knockback_applied = std::max(feedback.knockback_applied, desired);
    return;
  }
  float const step = desired - feedback.knockback_applied;
  if (step <= 0.0F) {
    return;
  }
  float const next_x = transform->position.x + total_x / total * step;
  float const next_z = transform->position.z + total_z / total * step;
  if (auto const* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {

    auto const here = Game::Systems::NavGrid::world_to_grid(transform->position.x,
                                                            transform->position.z);
    auto const there = Game::Systems::NavGrid::world_to_grid(next_x, next_z);
    if (pathfinder->is_terrain_walkable(here.x, here.y) &&
        !pathfinder->is_terrain_walkable(there.x, there.y)) {
      feedback.knockback_applied = std::max(feedback.knockback_applied, desired);
      return;
    }
  }
  transform->position.x = next_x;
  transform->position.z = next_z;
  feedback.knockback_applied = desired;
}

} // namespace

void process_hit_feedback(Engine::Core::World* world, float delta_time) {
  for (auto [unit, feedback] :
       world->entity_view<Engine::Core::HitFeedbackComponent>()) {
    if (unit.has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    feedback.recent_damage_remaining =
        std::max(0.0F, feedback.recent_damage_remaining - delta_time);

    if (!feedback.is_reacting) {
      continue;
    }

    feedback.reaction_time += delta_time;
    float const duration =
        feedback.reaction_duration > 0.0F
            ? feedback.reaction_duration
            : Engine::Core::HitFeedbackComponent::k_reaction_duration;
    float const progress = feedback.reaction_time / duration;

    apply_knockback_step(unit, feedback, progress);

    if (progress >= 1.0F) {
      feedback.is_reacting = false;
      feedback.reaction_time = 0.0F;
      feedback.reaction_intensity = 0.0F;
      feedback.knockback_x = 0.0F;
      feedback.knockback_z = 0.0F;
      feedback.knockback_applied = 0.0F;
    }
  }
}

namespace {

[[nodiscard]] auto
reaction_knockback_scale(Engine::Core::HitReactionKind kind) noexcept -> float {
  switch (kind) {
  case Engine::Core::HitReactionKind::Flinch:
    return 1.0F;
  case Engine::Core::HitReactionKind::Block:
    return 0.55F;
  case Engine::Core::HitReactionKind::Evade:
    return 1.7F;
  case Engine::Core::HitReactionKind::Stagger:
    return 2.4F;
  case Engine::Core::HitReactionKind::Recoil:
    return 0.7F;
  }
  return 1.0F;
}

[[nodiscard]] auto
reaction_pauses_swing(Engine::Core::HitReactionKind kind) noexcept -> bool {
  return kind == Engine::Core::HitReactionKind::Flinch ||
         kind == Engine::Core::HitReactionKind::Stagger;
}

void begin_reaction(Engine::Core::HitFeedbackComponent& feedback,
                    Engine::Core::EntityID attacker_id,
                    Engine::Core::HitReactionKind kind) {
  feedback.is_reacting = true;
  feedback.recent_damage_remaining =
      Engine::Core::HitFeedbackComponent::k_recent_damage_window;
  feedback.source_attacker_id = attacker_id;
  feedback.reaction_time = 0.0F;
  feedback.reaction_duration = Engine::Core::hit_reaction_duration(kind);
  feedback.reaction_kind = kind;
  feedback.knockback_applied = 0.0F;
  feedback.knockback_x = 0.0F;
  feedback.knockback_z = 0.0F;
  feedback.reaction_intensity = 0.85F;
}

struct ImpactStrength {
  float knockback_scale{1.0F};
  float intensity{0.85F};
};

[[nodiscard]] auto impact_strength(const Engine::Core::Entity& attacker,
                                   Engine::Core::HitReactionKind kind,
                                   float weapon_weight) -> ImpactStrength {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* unit = attacker.get_component<Engine::Core::UnitComponent>();
  ImpactStrength strength;
  if (attack != nullptr &&
      attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee) {
    strength = {1.25F, 1.0F};
  } else {
    strength = {0.8F, 0.70F};
  }
  if (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Elephant) {
    strength = {2.2F, 1.35F};
  }
  strength.knockback_scale *= reaction_knockback_scale(kind) * weapon_weight;
  strength.intensity *= weapon_weight;
  if (kind == Engine::Core::HitReactionKind::Stagger) {
    strength.intensity = std::max(strength.intensity, 1.25F);
  } else if (kind == Engine::Core::HitReactionKind::Recoil) {
    strength.intensity = 0.6F;
  }
  return strength;
}

void turn_toward_attacker(Engine::Core::Entity& target,
                          Engine::Core::TransformComponent& target_transform,
                          const Engine::Core::TransformComponent& attacker_transform) {
  bool const hit_controls_root_facing =
      !target.registry()->has<Engine::Core::WildlifeComponent>(target.get_id()) &&
      (Game::Systems::CombatRules::uses_rpg_combat_rules(&target) ||
       !Game::Systems::FormationCombat::has_formation_slots(target));
  if (!hit_controls_root_facing) {
    return;
  }
  float const face_dx = attacker_transform.position.x - target_transform.position.x;
  float const face_dz = attacker_transform.position.z - target_transform.position.z;
  float const face_dist = std::sqrt(face_dx * face_dx + face_dz * face_dz);
  if (face_dist > 0.001F) {
    target_transform.desired_yaw =
        std::atan2(face_dx, face_dz) * 180.0F / std::numbers::pi_v<float>;
    target_transform.has_desired_yaw = true;
  }
}

void apply_attacker_impulse(Engine::Core::Entity& target,
                            Engine::Core::TransformComponent& target_transform,
                            const Engine::Core::Entity& attacker,
                            const Engine::Core::TransformComponent& attacker_transform,
                            Engine::Core::HitFeedbackComponent& feedback,
                            Engine::Core::HitReactionKind kind,
                            const HitImpulse& impulse,
                            float weapon_weight) {
  auto const strength = impact_strength(attacker, kind, weapon_weight);
  feedback.reaction_intensity = strength.intensity;

  bool const from_weapon_contact =
      impulse.contact_point.has_value() && impulse.weapon_speed > 0.0F;
  float const from_x =
      from_weapon_contact ? impulse.contact_point->x() : attacker_transform.position.x;
  float const from_z =
      from_weapon_contact ? impulse.contact_point->z() : attacker_transform.position.z;
  float const dx = target_transform.position.x - from_x;
  float const dz = target_transform.position.z - from_z;
  float const dist = std::sqrt(dx * dx + dz * dz);
  if (dist <= 0.001F) {
    return;
  }
  feedback.hit_direction_x = dx / dist;
  feedback.hit_direction_z = dz / dist;
  float const knockback = std::clamp(
      Engine::Core::HitFeedbackComponent::k_max_knockback * strength.knockback_scale,
      0.0F,
      Engine::Core::HitFeedbackComponent::k_max_knockback * 2.8F);
  feedback.knockback_x = (dx / dist) * knockback;
  feedback.knockback_z = (dz / dist) * knockback;
  turn_toward_attacker(target, target_transform, attacker_transform);
}

void announce_new_stagger(const Engine::Core::Entity* entity, bool was_staggered) {
  if (entity == nullptr || was_staggered) {
    return;
  }
  Engine::Core::AudioCueEvent cue("combat.stagger");
  if (const auto* transform =
          entity->get_component<Engine::Core::TransformComponent>()) {
    cue.at(transform->position.x, transform->position.y, transform->position.z);
  }
  Engine::Core::EventManager::instance().publish(cue);
}

} // namespace

void add_or_extend_stagger(Engine::Core::Entity* entity, float duration) {
  if (entity == nullptr || duration <= 0.0F) {
    return;
  }
  bool const was_staggered = entity->has_component<Engine::Core::StaggerComponent>();
  auto* stagger = Engine::Core::get_or_add_component<Engine::Core::StaggerComponent>(
      entity, duration);
  if (stagger != nullptr) {
    stagger->remaining = std::max(stagger->remaining, duration);
    announce_new_stagger(entity, was_staggered);
  }
}

void add_or_extend_stagger(Engine::Core::Entity* entity,
                           float duration,
                           Engine::Core::StaggerTier tier) {
  if (entity == nullptr || duration <= 0.0F) {
    return;
  }
  bool const was_staggered = entity->has_component<Engine::Core::StaggerComponent>();
  auto* stagger = Engine::Core::get_or_add_component<Engine::Core::StaggerComponent>(
      entity, duration);
  if (stagger != nullptr) {
    stagger->remaining = std::max(stagger->remaining, duration);
    if (static_cast<std::uint8_t>(tier) > static_cast<std::uint8_t>(stagger->tier)) {
      stagger->tier = tier;
    }
    announce_new_stagger(entity, was_staggered);
  }
}

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world) {
  apply_hit_feedback(target, attacker_id, world, Engine::Core::HitReactionKind::Flinch);
}

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world,
                        Engine::Core::HitReactionKind kind,
                        const HitImpulse& impulse) {
  if (target == nullptr) {
    return;
  }
  float const weapon_weight =
      impulse.weapon_speed > 0.0F
          ? std::clamp(impulse.weapon_speed / k_reference_weapon_speed, 0.55F, 2.1F)
          : 1.0F;
  auto* feedback =
      Engine::Core::get_or_add_component<Engine::Core::HitFeedbackComponent>(target);
  if (feedback == nullptr) {
    return;
  }
  begin_reaction(*feedback, attacker_id, kind);

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto* attacker = world->get_entity(attacker_id);
    auto* attacker_transform =
        attacker != nullptr
            ? attacker->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (attacker_transform != nullptr) {
      apply_attacker_impulse(*target,
                             *target_transform,
                             *attacker,
                             *attacker_transform,
                             *feedback,
                             kind,
                             impulse,
                             weapon_weight);
    }
  }

  auto* combat_state = target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr && reaction_pauses_swing(kind)) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining =
        Engine::Core::CombatStateComponent::k_combat_animation_hit_pause_duration;
  }
}

void apply_melee_reaction_feedback(Engine::Core::World* world,
                                   Engine::Core::Entity* target,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::HitReactionKind kind) {
  if (target == nullptr) {
    return;
  }
  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0) {
    return;
  }
  apply_hit_feedback(target, attacker_id, world, kind);
  Engine::Core::Entity* attacker =
      (world != nullptr && attacker_id != 0) ? world->get_entity(attacker_id) : nullptr;
  publish_formation_hit(*target,
                        attacker_id,
                        preferred_formation_hit_slot(target, attacker),
                        kind,
                        world);
}

} // namespace Game::Systems::Combat

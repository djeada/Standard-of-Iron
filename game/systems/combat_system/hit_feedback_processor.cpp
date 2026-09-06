#include "hit_feedback_processor.h"

#include <algorithm>
#include <cmath>

#include "../../core/component_gameplay.h"
#include "../../core/world.h"
#include "../combat_rules.h"
#include "../formation_combat_geometry.h"

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
  if (unit.has_component<Engine::Core::BuildingComponent>() ||
      unit.has_component<Engine::Core::ElephantComponent>() ||
      (registry != nullptr &&
       registry->has<Engine::Core::WildlifeComponent>(unit.get_id()))) {

    return false;
  }
  if (Game::Systems::CombatRules::uses_rpg_combat_rules(&unit)) {
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
  if (total <= 0.0005F || !knockback_moves_body(unit, feedback)) {
    return;
  }
  auto* transform = unit.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }
  float const clamped = std::clamp(progress, 0.0F, 1.0F);
  float const remaining = 1.0F - clamped;
  float const eased = 1.0F - remaining * remaining * remaining;
  float const desired = eased * total;
  float const step = desired - feedback.knockback_applied;
  if (step <= 0.0F) {
    return;
  }
  transform->position.x += total_x / total * step;
  transform->position.z += total_z / total * step;
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

} // namespace Game::Systems::Combat

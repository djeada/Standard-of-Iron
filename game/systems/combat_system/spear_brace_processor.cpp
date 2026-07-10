#include "spear_brace_processor.h"

#include <algorithm>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_brace_ready_progress = 0.85F;

[[nodiscard]] auto is_spear_user(const Engine::Core::Entity& entity) -> bool {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr &&
         Engine::Core::resolve_combat_attack_family(
             unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee) ==
             Engine::Core::CombatAttackFamily::Spear;
}

struct BraceIntent {
  bool requested{false};
  float progress_hint{-1.0F};
  Engine::Core::SpearBraceSource source{Engine::Core::SpearBraceSource::Explicit};
};

[[nodiscard]] auto
resolve_brace_intent(const Engine::Core::Entity& entity) -> BraceIntent {
  if (auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && commander->fpv_controlled) {
    auto const* guard = entity.get_component<Engine::Core::CommanderGuardComponent>();
    if (guard != nullptr && guard->active) {
      return {.requested = true,
              .source = Engine::Core::SpearBraceSource::CommanderGuard};
    }
  }

  auto const* hold = entity.get_component<Engine::Core::HoldModeComponent>();
  if (hold != nullptr && hold->active) {
    return {.requested = true,
            .progress_hint = std::clamp(hold->kneel_entry_progress, 0.0F, 1.0F),
            .source = Engine::Core::SpearBraceSource::HoldMode};
  }

  auto const* guard = entity.get_component<Engine::Core::GuardModeComponent>();
  if (guard != nullptr && guard->active) {
    return {.requested = true, .source = Engine::Core::SpearBraceSource::GuardMode};
  }

  return {};
}

void advance_brace_pose(Engine::Core::SpearBraceComponent& brace,
                        float delta_time,
                        float progress_hint) {
  if (brace.requested) {
    if (progress_hint >= 0.0F) {
      brace.pose_progress = progress_hint;
    } else {
      brace.pose_progress = std::min(
          1.0F,
          brace.pose_progress + delta_time / std::max(0.001F, brace.enter_duration));
    }
  } else {
    brace.pose_progress = std::max(
        0.0F, brace.pose_progress - delta_time / std::max(0.001F, brace.exit_duration));
  }
  brace.active = brace.requested && brace.pose_progress >= k_brace_ready_progress;
}

} // namespace

void process_spear_brace_state(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>() ||
        !is_spear_user(*entity)) {
      continue;
    }

    BraceIntent const intent = resolve_brace_intent(*entity);
    auto* brace = entity->get_component<Engine::Core::SpearBraceComponent>();
    if (brace == nullptr && !intent.requested) {
      continue;
    }
    if (brace == nullptr) {
      brace = entity->add_component<Engine::Core::SpearBraceComponent>();
    }
    if (brace == nullptr) {
      continue;
    }

    if (brace->source == Engine::Core::SpearBraceSource::Explicit &&
        (brace->requested || brace->active)) {
      brace->requested = brace->requested || brace->active;
      if (brace->active) {
        brace->pose_progress = std::max(brace->pose_progress, 1.0F);
      }
      continue;
    }

    brace->requested = intent.requested;
    brace->source = intent.source;
    advance_brace_pose(*brace, std::max(0.0F, delta_time), intent.progress_hint);
  }
}

} // namespace Game::Systems::Combat

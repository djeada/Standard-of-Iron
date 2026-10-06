#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

#include "arena_viewport.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"
#include "game/units/unit.h"
#include "render/profiling/combat_animation_diagnostics.h"

namespace {

auto combat_phase_duration(Render::GL::CombatAnimPhase phase) -> float {
  switch (phase) {
  case Render::GL::CombatAnimPhase::Advance:
    return Engine::Core::CombatStateComponent::k_advance_duration;
  case Render::GL::CombatAnimPhase::WindUp:
    return Engine::Core::CombatStateComponent::k_wind_up_duration;
  case Render::GL::CombatAnimPhase::Strike:
    return Engine::Core::CombatStateComponent::k_strike_duration;
  case Render::GL::CombatAnimPhase::Impact:
    return Engine::Core::CombatStateComponent::k_impact_duration;
  case Render::GL::CombatAnimPhase::Recover:
    return Engine::Core::CombatStateComponent::k_recover_duration;
  case Render::GL::CombatAnimPhase::Reposition:
    return Engine::Core::CombatStateComponent::k_reposition_duration;
  case Render::GL::CombatAnimPhase::Idle:
  default:
    return 0.0F;
  }
}

} // namespace

void ArenaViewport::set_animation_name(const QString& animation_name) {
  m_animation_name = animation_name.trimmed();
  if (m_animation_name.isEmpty()) {
    m_animation_name = QStringLiteral("Idle");
  }
}

void ArenaViewport::play_selected_animation() {
  if (m_animation_name.compare(QStringLiteral("Walk"), Qt::CaseInsensitive) == 0) {
    play_walk_animation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Attack"), Qt::CaseInsensitive) == 0) {
    play_attack_animation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Death"), Qt::CaseInsensitive) == 0) {
    play_death_animation();
    return;
  }
  play_idle_animation();
}

auto ArenaViewport::selected_unit_ids_or_fallback()
    -> std::vector<Engine::Core::EntityID> {
  sanitize_selection();
  auto* selection = selection_system();
  if (selection == nullptr) {
    return {};
  }

  if (!selection->get_selected_units().empty()) {
    return selection->get_selected_units();
  }

  while (!m_units.empty() && (m_units.back() == nullptr ||
                              m_world->get_entity(m_units.back()->id()) == nullptr)) {
    m_units.pop_back();
  }

  if (!m_units.empty()) {
    Engine::Core::EntityID const fallback_id = m_units.back()->id();
    select_entity(fallback_id);
    return {fallback_id};
  }

  return {};
}

void ArenaViewport::clear_forced_animation_state(
    const std::vector<Engine::Core::EntityID>& ids) {
  if (m_world == nullptr) {
    return;
  }

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }
    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->stop();
    }
    entity->remove_component<Engine::Core::AttackTargetComponent>();
    entity->remove_component<Engine::Core::CombatStateComponent>();
    entity->remove_component<Engine::Core::HitFeedbackComponent>();
  }
}

void ArenaViewport::play_idle_animation() {
  auto ids = selected_unit_ids_or_fallback();
  clear_forced_animation_state(ids);
  update();
}

void ArenaViewport::play_walk_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto* movement = entity != nullptr
                         ? entity->get_component<Engine::Core::MovementComponent>()
                         : nullptr;
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const direction_x = std::sin(yaw_rad);
    float const direction_z = std::cos(yaw_rad);
    float const distance = std::max(2.0F, m_default_unit_speed * 1.8F);
    float const target_x = transform->position.x + direction_x * distance;
    float const target_z = transform->position.z + direction_z * distance;

    movement->clear_path();
    movement->engage_manual_move(target_x, target_z);
    movement->set_manual_velocity(0.0F, 0.0F);
  }

  update();
}

void ArenaViewport::play_attack_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto* combat_state = entity->get_component<Engine::Core::CombatStateComponent>();
    if (combat_state == nullptr) {
      combat_state = entity->add_component<Engine::Core::CombatStateComponent>();
    }
    if (combat_state == nullptr) {
      continue;
    }

    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration;
    combat_state->attack_variant = 0;
  }

  update();
}

void ArenaViewport::play_death_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->stop();
    }
    if (auto* renderable = entity->get_component<Engine::Core::RenderableComponent>()) {
      renderable->visible = false;
    }
    if (auto* unit = entity->get_component<Engine::Core::UnitComponent>()) {
      unit->health = 0;
    }
    if (!entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
    }
  }

  auto* selection = selection_system();
  if (selection != nullptr) {
    selection->clear_selection();
  }
  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::move_selected_unit_forward() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto* entity = m_world->get_entity(entity_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto* movement = entity != nullptr
                         ? entity->get_component<Engine::Core::MovementComponent>()
                         : nullptr;
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const target_x = transform->position.x + std::sin(yaw_rad) * 5.0F;
    float const target_z = transform->position.z + std::cos(yaw_rad) * 5.0F;

    movement->clear_path();
    movement->engage_manual_move(target_x, target_z);
    movement->set_manual_velocity(0.0F, 0.0F);
  }

  update();
}

void ArenaViewport::set_movement_speed(float speed) {
  m_default_unit_speed = std::max(0.1F, speed);
  if (m_world == nullptr) {
    return;
  }

  for (const auto& unit : m_units) {
    if (unit == nullptr) {
      continue;
    }
    auto* entity = m_world->get_entity(unit->id());
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if (unit_component != nullptr) {
      unit_component->speed = m_default_unit_speed;
    }
  }
}

void ArenaViewport::set_skeleton_debug_enabled(bool enabled) {
  m_pose_overlay_enabled = enabled;
  update();
}

void ArenaViewport::set_combat_debug_enabled(bool enabled) {
  m_combat_debug_overlay_enabled = enabled;
  Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(enabled);
  Render::Profiling::CombatAnimationDiagnostics::instance().set_logging_enabled(
      enabled);
  update();
}

void ArenaViewport::set_attack_scrub_enabled(bool enabled) {
  m_attack_scrub_enabled = enabled;
  if (m_attack_scrub_enabled) {
    capture_attack_scrub_anchor();
  } else {
    m_attack_scrub_entity_id = 0;
  }
  update();
}

void ArenaViewport::set_attack_scrub_phase(float phase) {
  m_attack_scrub_phase = std::clamp(phase, 0.0F, 1.0F);
  update();
}

void ArenaViewport::capture_attack_scrub_anchor() {
  if (m_world == nullptr) {
    m_attack_scrub_entity_id = 0;
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    m_attack_scrub_entity_id = 0;
    return;
  }

  auto* entity = m_world->get_entity(ids.front());
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (entity == nullptr || transform == nullptr) {
    m_attack_scrub_entity_id = 0;
    return;
  }

  m_attack_scrub_entity_id = entity->get_id();
  m_attack_scrub_position =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  m_attack_scrub_rotation =
      QVector3D(transform->rotation.x, transform->rotation.y, transform->rotation.z);
  m_attack_scrub_scale =
      QVector3D(transform->scale.x, transform->scale.y, transform->scale.z);
  m_attack_scrub_family = Engine::Core::CombatAttackFamily::Sword;
  m_attack_scrub_variant = 0U;
  m_attack_scrub_offset = 0.0F;
  m_attack_scrub_finisher = false;

  if (auto* combat_state = entity->get_component<Engine::Core::CombatStateComponent>();
      combat_state != nullptr) {
    m_attack_scrub_family = combat_state->attack_family;
    m_attack_scrub_variant = combat_state->attack_variant;
    m_attack_scrub_offset = combat_state->attack_offset;
    m_attack_scrub_finisher = combat_state->finisher_attack;
  } else if (auto* unit = entity->get_component<Engine::Core::UnitComponent>();
             unit != nullptr) {
    auto const mode =
        (entity->get_component<Engine::Core::AttackComponent>() != nullptr &&
         entity->get_component<Engine::Core::AttackComponent>()->current_mode ==
             Engine::Core::AttackComponent::CombatMode::Ranged)
            ? Engine::Core::AttackComponent::CombatMode::Ranged
            : Engine::Core::AttackComponent::CombatMode::Melee;
    m_attack_scrub_family =
        Engine::Core::resolve_combat_attack_family(unit->spawn_type, mode);
  }
}

void ArenaViewport::apply_attack_scrub_override() {
  if (!m_attack_scrub_enabled || m_world == nullptr) {
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    m_attack_scrub_entity_id = 0;
    return;
  }
  if (m_attack_scrub_entity_id != ids.front()) {
    capture_attack_scrub_anchor();
  }

  auto* entity = m_world->get_entity(m_attack_scrub_entity_id);
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (entity == nullptr || transform == nullptr) {
    m_attack_scrub_entity_id = 0;
    return;
  }

  transform->position.x = m_attack_scrub_position.x();
  transform->position.y = m_attack_scrub_position.y();
  transform->position.z = m_attack_scrub_position.z();
  transform->rotation.x = m_attack_scrub_rotation.x();
  transform->rotation.y = m_attack_scrub_rotation.y();
  transform->rotation.z = m_attack_scrub_rotation.z();
  transform->scale.x = m_attack_scrub_scale.x();
  transform->scale.y = m_attack_scrub_scale.y();
  transform->scale.z = m_attack_scrub_scale.z();

  if (auto* movement = entity->get_component<Engine::Core::MovementComponent>();
      movement != nullptr) {
    movement->stop();
  }

  auto* combat_state = entity->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state == nullptr) {
    combat_state = entity->add_component<Engine::Core::CombatStateComponent>();
  }
  if (combat_state == nullptr) {
    return;
  }

  auto const scrubbed = Render::Profiling::scrubbed_combat_phase_from_attack_phase(
      m_attack_scrub_phase, false, m_attack_scrub_finisher);
  combat_state->animation_state = scrubbed.phase;
  combat_state->state_duration = combat_phase_duration(scrubbed.phase);
  combat_state->state_time = scrubbed.progress * combat_state->state_duration;
  combat_state->attack_family = m_attack_scrub_family;
  combat_state->attack_variant = m_attack_scrub_variant;
  combat_state->attack_offset = m_attack_scrub_offset;
  combat_state->finisher_attack = m_attack_scrub_finisher;
  combat_state->is_hit_paused = false;
  combat_state->hit_pause_remaining = 0.0F;
}

#include "app/commander/commander_defence.h"

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <utility>

#include "app/commander/commander_entity_access.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/systems/combat_actions/commander_defense_timeline.h"

namespace App::Core {

void CommanderDefence::advance_upkeep(Engine::Core::Entity& commander,
                                      Engine::Core::CommanderComponent* cmd_comp,
                                      bool guard_held,
                                      float dt) const {
  if (cmd_comp != nullptr) {
    cmd_comp->punish_window_remaining =
        std::max(0.0F, cmd_comp->punish_window_remaining - dt);
    cmd_comp->posture = std::max(
        0.0F,
        cmd_comp->posture - ((m_guard_was_active || guard_held) ? 8.0F : 18.0F) * dt);
  }

  if (auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>()) {
    guard->perfect_guard_remaining =
        std::max(0.0F, guard->perfect_guard_remaining - dt);
    guard->guard_break_remaining = std::max(0.0F, guard->guard_break_remaining - dt);
    if (!guard_held) {
      guard->rearm_requires_release = false;
    }
  }
}

void CommanderDefence::hold_for_rally(Engine::Core::Entity& commander, float dt) {
  m_guard_was_active = false;
  if (auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>()) {
    guard->active = false;
    guard->perfect_guard_remaining =
        std::max(0.0F, guard->perfect_guard_remaining - dt);
    guard->guard_break_remaining = std::max(0.0F, guard->guard_break_remaining - dt);
    guard->rearm_requires_release = false;
  }
  if (auto* rpg = commander.get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->dodge_grace_remaining = 0.0F;
  }
}

void CommanderDefence::apply_guard(Engine::Core::Entity& commander,
                                   const GuardTickInput& input,
                                   CommanderLatencyProbe* probe) {
  auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>();
  if (input.guard_held) {
    if (guard == nullptr) {
      guard = commander.add_component<Engine::Core::CommanderGuardComponent>();
    }
    if (guard != nullptr && guard->guard_break_remaining <= 0.0F &&
        !guard->rearm_requires_release && !input.dodging && !input.airborne &&
        body_allows_now(commander).accepts_guard) {
      guard->active = true;
      if (!m_guard_was_active) {
        cancel_current_attack(commander);
        guard->perfect_guard_remaining =
            Game::Systems::CombatActions::k_commander_guard_timeline
                .perfect_window_seconds;
        Game::Audio::play_cue(Game::Audio::Cue::k_combat_guard_raise);
        if (probe != nullptr) {
          probe->note_guard_start();
          probe->note_pose_response();
        }
      }
    } else if (guard != nullptr) {
      guard->active = false;
    }
  } else if (guard != nullptr) {
    guard->active = false;
  }
  if (guard != nullptr && guard->guard_break_remaining > 0.0F) {
    guard->active = false;
  }
  m_guard_was_active = (guard != nullptr) && guard->active;

  if (guard != nullptr && guard->active) {
    if (auto* stamina = commander.get_component<Engine::Core::StaminaComponent>()) {
      stamina->spend(
          Engine::Core::CombatStateComponent::k_stamina_cost_guard_per_second *
          input.dt);
    }
  }
}

auto CommanderDefence::advance_dodge_grace(Engine::Core::Entity& commander,
                                           float dt) -> float {
  if (auto* rpg = commander.get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->dodge_grace_remaining = std::max(0.0F, rpg->dodge_grace_remaining - dt);
    return rpg->dodge_grace_remaining;
  }
  return 0.0F;
}

void CommanderDefence::publish_resolved_feedback(
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    const Engine::Core::TransformComponent& transform,
    PlayerFeedbackBus* bus) {
  auto const* rpg = commander.get_component<Engine::Core::RpgHealthComponent>();
  if (rpg == nullptr) {
    return;
  }

  QVector3D const at(transform.position.x, transform.position.y, transform.position.z);
  auto publish = [&](PlayerFeedbackType type, const char* reason) {
    if (bus == nullptr) {
      return;
    }
    PlayerFeedbackEvent event;
    event.type = type;
    event.entity = commander_id;
    event.has_world_position = true;
    event.world_position = at;
    event.reason = QString::fromLatin1(reason);
    bus->publish(std::move(event));
  };

  if (rpg->perfect_guard_contacts != m_observed_perfect_guard_contacts) {
    m_observed_perfect_guard_contacts = rpg->perfect_guard_contacts;
    publish(PlayerFeedbackType::PerfectGuard, "perfect_guard");
  }
  if (rpg->dodged_contacts != m_observed_dodged_contacts) {
    m_observed_dodged_contacts = rpg->dodged_contacts;
    publish(PlayerFeedbackType::DodgeSuccess, "iframe_reject");
  }
  if (rpg->blocked_contacts != m_observed_blocked_contacts) {
    m_observed_blocked_contacts = rpg->blocked_contacts;
    publish(PlayerFeedbackType::WeaponContact, "guard_block");
  }
  if (rpg->guard_broken_contacts != m_observed_guard_broken_contacts) {
    m_observed_guard_broken_contacts = rpg->guard_broken_contacts;
    publish(PlayerFeedbackType::GuardBroken, "guard_break");
  }
}

} // namespace App::Core

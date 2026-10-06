#include <QDebug>
#include <QKeyEvent>

#include <algorithm>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_status_builder.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/world.h"
#include "game/systems/combat_rules.h"
#include "game/systems/run_stamina.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/scene_renderer.h"

using namespace arena_viewport_internal;

namespace {

constexpr int k_scripted_attack_hold_ticks = 3;

} // namespace

void ArenaViewport::configure_rpg_scenario_commander(Engine::Core::EntityID entity_id) {
  if (m_world == nullptr || m_rpg_commander_controller == nullptr) {
    return;
  }
  auto* entity = m_world->get_entity(entity_id);
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
  auto* commander = entity != nullptr
                        ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (entity == nullptr || unit == nullptr || commander == nullptr ||
      transform == nullptr) {
    return;
  }

  m_rpg_commander_id = entity_id;
  m_rpg_commander_controller->reset();
  m_rpg_commander_controller->set_presentation_trace_enabled(true);
  m_rpg_commander_controller->set_view_yaw(transform->rotation.y);
  m_rpg_commander_controller->set_view_pitch(k_commander_rest_view_pitch_degrees);
  commander->fpv_controlled = true;
  commander->posture = 0.0F;
  commander->punish_window_remaining = 0.0F;
  Game::Systems::CombatRules::clear_rts_combat_tracking(entity);

  if (auto* stamina = Game::Systems::ensure_run_stamina(*entity)) {
    stamina->max_stamina = std::max(stamina->max_stamina, 180.0F);
    stamina->regen_rate = std::max(stamina->regen_rate, 24.0F);
    stamina->stamina = stamina->max_stamina;
    stamina->regen_delay_remaining = 0.0F;
    stamina->is_running = false;
    stamina->run_requested = false;
  }

  auto* rpg =
      Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(entity);
  if (rpg != nullptr) {
    rpg->active = true;
    rpg->crit_chance = 0.0F;
    rpg->dodge_grace_remaining = 0.0F;
  }
  if (auto* guard =
          Engine::Core::get_or_add_component<Engine::Core::CommanderGuardComponent>(
              entity)) {
    guard->active = false;
    guard->damage_multiplier = 0.0F;
    guard->perfect_guard_remaining = 0.0F;
    guard->guard_break_remaining = 0.0F;
    guard->rearm_requires_release = false;
  }
  if (auto* targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              entity)) {
    *targets = Engine::Core::RpgCommanderTargetComponent{};
  }
  if (m_renderer != nullptr) {
    m_renderer->set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rpg);
    m_renderer->set_rpg_camera_focus(entity_id);
  }
  if (m_rpg_telegraphs != nullptr) {
    m_rpg_telegraphs->clear();
  }
}

auto ArenaViewport::rpg_commander_status() const -> QVariantMap {
  if (m_world == nullptr || m_rpg_commander_id == 0 ||
      m_rpg_commander_controller == nullptr) {
    return {};
  }
  App::Core::CommanderStatusInput input;
  input.world = m_world.get();
  input.controlled_commander_id = m_rpg_commander_id;
  input.dodge_active = m_rpg_commander_controller->is_dodge_rolling();
  input.locked_target_id = m_rpg_commander_controller->locked_target_id();
  return App::Core::build_controlled_commander_status(input);
}

auto ArenaViewport::take_due_presentation_hitch() -> float {
  if (m_scenario_runner == nullptr) {
    return 0.0F;
  }
  auto const& hitches = m_scenario_runner->definition().presentation_hitches;
  if (hitches.empty()) {
    return 0.0F;
  }
  if (m_presentation_hitches_fired.size() != hitches.size()) {
    m_presentation_hitches_fired.assign(hitches.size(), false);
  }
  float const elapsed = m_scenario_runner->elapsed_seconds();
  for (std::size_t index = 0; index < hitches.size(); ++index) {
    if (m_presentation_hitches_fired[index] || elapsed < hitches[index].at_seconds) {
      continue;
    }
    m_presentation_hitches_fired[index] = true;
    return std::max(0.0F, hitches[index].frame_ms);
  }
  return 0.0F;
}

void ArenaViewport::publish_commander_presentation_trace() {
  if (m_scenario_runner == nullptr || m_rpg_commander_controller == nullptr ||
      !m_rpg_commander_controller->presentation_trace_enabled()) {
    return;
  }
  m_scenario_runner->observe_commander_presentation(
      m_rpg_commander_controller->presentation_trace());
}

void ArenaViewport::publish_animation_clock() {
  if (m_scenario_runner != nullptr && m_renderer != nullptr) {
    m_scenario_runner->set_animation_time(m_renderer->get_animation_time());
  }
}

void ArenaViewport::report_rpg_interactive_state(float simulation_dt) {
  static bool const enabled = qEnvironmentVariableIntValue("SOI_ARENA_RPG_TRACE") > 0;
  if (!enabled || !m_rpg_interactive || m_rpg_commander_controller == nullptr ||
      m_world == nullptr) {
    return;
  }
  m_rpg_trace_accumulator += std::max(0.0F, simulation_dt);
  if (m_rpg_trace_accumulator < 1.0F) {
    return;
  }
  m_rpg_trace_accumulator = 0.0F;

  auto const* transform =
      m_world->try_get<Engine::Core::TransformComponent>(m_rpg_commander_id);
  if (transform == nullptr) {
    return;
  }
  auto const& edges = m_rpg_commander_controller->input_edges();
  qInfo().noquote() << QStringLiteral(
                           "SOI_RPG_INTERACTIVE pos=%1,%2 yaw=%3 pitch=%4 "
                           "attack_press=%5 attack_consumed=%6 dodge=%7 jump=%8")
                           .arg(transform->position.x, 0, 'f', 3)
                           .arg(transform->position.z, 0, 'f', 3)
                           .arg(m_rpg_commander_controller->view_yaw(), 0, 'f', 2)
                           .arg(m_rpg_commander_controller->view_pitch(), 0, 'f', 2)
                           .arg(edges.primary_press_sequence)
                           .arg(edges.primary_consumed_sequence)
                           .arg(edges.dodge_consumed_sequence)
                           .arg(edges.jump_consumed_sequence);
}

void ArenaViewport::update_rpg_scenario_controller(float simulation_dt) {
  report_rpg_interactive_state(simulation_dt);
  if (simulation_dt <= 0.0F || m_rpg_commander_id == 0 ||
      m_rpg_commander_controller == nullptr || m_world == nullptr ||
      m_camera == nullptr) {
    return;
  }
  if (!m_rpg_commander_controller->update(
          *m_world, m_rpg_commander_id, k_local_owner_id, *m_camera, simulation_dt)) {
    if (m_rpg_interactive) {
      exit_rpg_interactive_control();
    } else {
      clear_rpg_scenario_state();
    }
    return;
  }
  if (m_rpg_scripted_attack_ticks > 0 && --m_rpg_scripted_attack_ticks == 0) {
    m_rpg_commander_controller->primary_action_up();
  }
}

void ArenaViewport::clear_rpg_scenario_state() {
  if (m_world != nullptr && m_rpg_commander_id != 0) {
    if (auto* entity = m_world->get_entity(m_rpg_commander_id)) {
      if (auto* commander = entity->get_component<Engine::Core::CommanderComponent>()) {
        commander->fpv_controlled = false;
      }
      if (auto* rpg = entity->get_component<Engine::Core::RpgHealthComponent>()) {
        rpg->active = false;
        rpg->dodge_grace_remaining = 0.0F;
      }
      if (auto* guard =
              entity->get_component<Engine::Core::CommanderGuardComponent>()) {
        guard->active = false;
      }
    }
  }
  m_rpg_commander_id = 0;
  if (m_rpg_commander_controller != nullptr) {
    m_rpg_commander_controller->reset();
  }
  if (m_rpg_telegraphs != nullptr) {
    m_rpg_telegraphs->clear();
  }
  if (m_renderer != nullptr) {
    m_renderer->set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rts);
    m_renderer->set_rpg_camera_focus(0);
  }
}

auto ArenaViewport::enter_rpg_interactive_control(Engine::Core::EntityID entity_id)
    -> bool {
  if (m_world == nullptr || m_rpg_commander_controller == nullptr) {
    return false;
  }

  Engine::Core::EntityID target_id = entity_id;
  if (target_id == 0) {
    for (auto candidate_id : selected_unit_ids_or_fallback()) {
      auto* candidate = m_world->get_entity(candidate_id);
      if (candidate != nullptr &&
          candidate->has_component<Engine::Core::CommanderComponent>()) {
        target_id = candidate_id;
        break;
      }
    }
  }
  if (target_id == 0) {
    for (auto* candidate :
         m_world->collect_entities_with<Engine::Core::CommanderComponent>()) {
      auto const* unit = candidate != nullptr
                             ? candidate->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (unit != nullptr && unit->owner_id == k_local_owner_id && unit->health > 0) {
        target_id = candidate->get_id();
        break;
      }
    }
  }
  if (target_id == 0) {
    return false;
  }

  configure_rpg_scenario_commander(target_id);
  if (m_rpg_commander_id == 0) {
    return false;
  }

  m_rpg_interactive = true;
  select_entity(0);
  setMouseTracking(true);
  setCursor(Qt::BlankCursor);
  recenter_rpg_mouse();
  setFocus(Qt::OtherFocusReason);
  update();
  return true;
}

void ArenaViewport::exit_rpg_interactive_control() {
  if (!m_rpg_interactive) {
    return;
  }
  m_rpg_interactive = false;
  m_rpg_mouse_captured = false;
  unsetCursor();
  if (m_rpg_commander_controller != nullptr) {
    m_rpg_commander_controller->release_all_input();
  }
  m_rpg_scripted_attack_ticks = 0;
  clear_rpg_scenario_state();
  reset_camera();
  update();
}

void ArenaViewport::recenter_rpg_mouse() {
  if (!m_rpg_interactive || width() <= 0 || height() <= 0) {
    return;
  }
  QPoint const local_center(width() / 2, height() / 2);
  m_rpg_mouse_center = mapToGlobal(local_center);
  m_rpg_mouse_captured = true;
  QCursor::setPos(m_rpg_mouse_center);
}

auto ArenaViewport::rpg_interactive_key_press(QKeyEvent* event) -> bool {
  if (!m_rpg_interactive || m_rpg_commander_controller == nullptr) {
    return false;
  }
  if (event->isAutoRepeat()) {
    return true;
  }

  switch (event->key()) {
  case Qt::Key_Escape:
  case Qt::Key_Tab:
    exit_rpg_interactive_control();
    return true;
  case Qt::Key_Space:
    m_rpg_commander_controller->request_dodge();
    return true;
  case Qt::Key_Control:
    m_rpg_commander_controller->request_jump();
    return true;
  case Qt::Key_R:
    if (m_world != nullptr) {
      m_rpg_commander_controller->cycle_lock_on_target(
          *m_world, m_rpg_commander_id, k_local_owner_id);
    }
    return true;
  case Qt::Key_C:
    if (m_world != nullptr) {
      m_rpg_commander_controller->toggle_close_camera_mode(
          *m_world, m_rpg_commander_id, k_local_owner_id);
    }
    return true;
  case Qt::Key_F:
    m_rpg_commander_controller->special_action();
    return true;
  case Qt::Key_X:
    if (m_world != nullptr) {
      m_rpg_commander_controller->toggle_weapon_stance(
          *m_world, m_rpg_commander_id, k_local_owner_id);
    }
    return true;
  case Qt::Key_V:
    m_rpg_commander_controller->request_vanguard_rush();
    return true;
  case Qt::Key_G:
    m_rpg_commander_controller->request_second_wind();
    return true;
  case Qt::Key_F1:
  case Qt::Key_Question:
    m_controls_overlay_visible = !m_controls_overlay_visible;
    update();
    return true;
  default:
    break;
  }

  m_rpg_commander_controller->key_down(event->key());
  return true;
}

auto ArenaViewport::rpg_interactive_key_release(QKeyEvent* event) -> bool {
  if (!m_rpg_interactive || m_rpg_commander_controller == nullptr) {
    return false;
  }
  if (!event->isAutoRepeat()) {
    m_rpg_commander_controller->key_up(event->key());
  }
  return true;
}

void ArenaViewport::bind_rpg_scenario_controls(Arena::ArenaScenarioHost& host) {
  host.configure_rpg_commander = [this](Engine::Core::EntityID entity_id) {
    configure_rpg_scenario_commander(entity_id);
  };
  host.rpg_primary_attack = [this](Engine::Core::EntityID entity_id) {
    if (m_world == nullptr || m_rpg_commander_controller == nullptr ||
        entity_id != m_rpg_commander_id) {
      return false;
    }
    if (m_rpg_commander_controller->input().primary_action) {
      return true;
    }
    m_rpg_commander_controller->primary_action_down();
    m_rpg_scripted_attack_ticks = k_scripted_attack_hold_ticks;
    return true;
  };
  host.rpg_heavy_attack = [this](Engine::Core::EntityID entity_id) {
    if (m_world == nullptr || m_rpg_commander_controller == nullptr ||
        entity_id != m_rpg_commander_id) {
      return false;
    }
    m_rpg_commander_controller->request_heavy_action();
    return true;
  };
  host.set_rpg_attack_held = [this](Engine::Core::EntityID entity_id, bool held) {
    if (m_rpg_commander_controller == nullptr || entity_id != m_rpg_commander_id) {
      return;
    }
    m_rpg_scripted_attack_ticks = 0;
    if (held) {
      m_rpg_commander_controller->primary_action_down();
    } else {
      m_rpg_commander_controller->primary_action_up();
    }
  };
  host.set_rpg_guard = [this](Engine::Core::EntityID entity_id, bool enabled) {
    if (m_world == nullptr || m_rpg_commander_controller == nullptr ||
        entity_id != m_rpg_commander_id) {
      return;
    }
    if (enabled) {
      m_rpg_commander_controller->secondary_action_down();
    } else {
      m_rpg_commander_controller->secondary_action_up();
    }
  };
  host.request_rpg_jump = [this](Engine::Core::EntityID entity_id) {
    if (m_rpg_commander_controller != nullptr && entity_id == m_rpg_commander_id) {
      m_rpg_commander_controller->request_jump();
    }
  };
  host.request_rpg_special = [this](Engine::Core::EntityID entity_id) {
    if (m_rpg_commander_controller != nullptr && entity_id == m_rpg_commander_id) {
      m_rpg_commander_controller->special_action();
    }
  };
  host.request_rpg_weapon_switch = [this](Engine::Core::EntityID entity_id) {
    if (m_world != nullptr && m_rpg_commander_controller != nullptr &&
        entity_id == m_rpg_commander_id) {
      m_rpg_commander_controller->toggle_weapon_stance(
          *m_world, entity_id, k_local_owner_id);
    }
  };
  host.set_rpg_move_input =
      [this](Engine::Core::EntityID entity_id, const QVector3D& axes, bool run) {
        if (m_rpg_commander_controller == nullptr || entity_id != m_rpg_commander_id) {
          return;
        }
        auto& controller = *m_rpg_commander_controller;
        auto const hold = [&controller](int key, bool held) {
          if (held) {
            controller.key_down(key);
          } else {
            controller.key_up(key);
          }
        };
        hold(Qt::Key_D, axes.x() > 0.5F);
        hold(Qt::Key_A, axes.x() < -0.5F);
        hold(Qt::Key_W, axes.z() > 0.5F);
        hold(Qt::Key_S, axes.z() < -0.5F);
        hold(Qt::Key_Shift, run);
      };
  host.set_rpg_view_yaw = [this](Engine::Core::EntityID entity_id, float yaw_degrees) {
    if (m_rpg_commander_controller != nullptr && entity_id == m_rpg_commander_id) {
      m_rpg_commander_controller->set_view_yaw(yaw_degrees);
    }
  };
  host.set_rpg_view_pitch = [this](Engine::Core::EntityID entity_id,
                                   float pitch_degrees) {
    if (m_rpg_commander_controller != nullptr && entity_id == m_rpg_commander_id) {
      m_rpg_commander_controller->set_view_pitch(pitch_degrees);
    }
  };
  host.aim_rpg_view_at = [this](Engine::Core::EntityID entity_id,
                                const QVector3D& world_point) {
    if (m_rpg_commander_controller == nullptr || entity_id != m_rpg_commander_id ||
        m_camera == nullptr) {
      return;
    }

    QVector3D direction = world_point - m_camera->get_position();
    if (direction.lengthSquared() <= 1.0e-6F) {
      return;
    }
    direction.normalize();
    constexpr float k_radians_to_degrees = 180.0F / std::numbers::pi_v<float>;
    m_rpg_commander_controller->set_view_yaw(std::atan2(direction.x(), direction.z()) *
                                             k_radians_to_degrees);
    m_rpg_commander_controller->set_view_pitch(
        std::asin(std::clamp(direction.y(), -1.0F, 1.0F)) * k_radians_to_degrees);
  };
  host.cycle_rpg_lock_on = [this](Engine::Core::EntityID entity_id) {
    if (m_world == nullptr || m_rpg_commander_controller == nullptr ||
        entity_id != m_rpg_commander_id) {
      return;
    }
    m_rpg_commander_controller->cycle_lock_on_target(
        *m_world, m_rpg_commander_id, k_local_owner_id);
  };
  host.rpg_locked_target =
      [this](Engine::Core::EntityID entity_id) -> Engine::Core::EntityID {
    if (m_rpg_commander_controller == nullptr || entity_id != m_rpg_commander_id) {
      return 0;
    }
    return m_rpg_commander_controller->locked_target_id();
  };
  host.request_rpg_dodge = [this](Engine::Core::EntityID entity_id,
                                  const QVector3D& world_direction) {
    if (m_rpg_commander_controller != nullptr && entity_id == m_rpg_commander_id) {
      if (world_direction.lengthSquared() > 0.0001F) {
        m_rpg_commander_controller->request_dodge(world_direction);
      } else {
        m_rpg_commander_controller->request_dodge();
      }
    }
  };
}

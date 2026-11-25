#include "command_system.h"

#include <QJsonArray>

#include "../commander/commander_handoff.h"
#include "../commander/commander_heading.h"
#include "../commander/commander_runtime.h"
#include "../core/component_commander.h"
#include "../core/world.h"
#include "../session/session_context.h"
#include "../session/simulation_clock.h"
#include "../systems/owner_registry.h"
#include "command_queue.h"
#include "json_u64.h"

namespace Game::Command {

auto CommandSystem::capture_checkpoint() const -> QJsonArray {
  QJsonArray state;
  for (const auto& [owner, control] : m_commanders) {
    if (control.entity != 0) {
      state.append(QJsonObject{{"owner", owner},
                               {"entity", u64_to_json(control.entity)},
                               {"runtime", control.runtime->capture_checkpoint()}});
    }
  }
  return state;
}

auto CommandSystem::restore_checkpoint(Engine::Core::World& world,
                                       const QJsonArray& state) -> bool {
  decltype(m_commanders) restored;
  for (const auto value : state) {
    const auto row = value.toObject();
    const int owner = row.value("owner").toInt();
    const auto id = u64_from_json(row.value("entity"));
    if (owner <= 0 || !id || restored.contains(owner) ||
        Game::Commander::CommanderRuntime::controlled_commander(world, *id, owner) ==
            nullptr) {
      return false;
    }
    auto runtime = std::make_shared<Game::Commander::CommanderRuntime>();
    if (!runtime->restore_checkpoint(row.value("runtime").toObject())) {
      return false;
    }
    restored.emplace(owner, CommanderControl{std::move(runtime), *id, false});
  }
  m_commanders.swap(restored);
  return true;
}

auto CommandSystem::commander_runtime(int owner)
    -> std::shared_ptr<Game::Commander::CommanderRuntime> {
  auto& control = m_commanders[owner];
  if (!control.runtime) {
    control.runtime = std::make_shared<Game::Commander::CommanderRuntime>();
  }
  return control.runtime;
}

void CommandSystem::apply_action(Engine::Core::World& world,
                                 int owner,
                                 const CommanderActionRecord& record) {
  auto runtime = commander_runtime(owner);
  auto& control = m_commanders[owner];
  switch (record.action) {
  case CommanderAction::EnterControl:
  case CommanderAction::ToggleControl:
    if (control.entity == 0) {
      if (auto* commander = Game::Commander::find_commander(world, owner)) {
        Game::Commander::begin_control(*commander);
        control.entity = commander->get_id();
        runtime->reset();
        const auto* transform =
            world.try_get<const Engine::Core::TransformComponent>(control.entity);
        runtime->set_view(transform->rotation.y,
                          Game::Commander::k_commander_rest_view_pitch_degrees);
      }
      return;
    }
    if (record.action == CommanderAction::EnterControl) {
      return;
    }
    [[fallthrough]];
  case CommanderAction::ExitControl:
    Game::Commander::end_control(&world, control.entity);
    control.entity = 0;
    runtime->reset();
    return;
  default:
    break;
  }
  if (control.entity == 0) {
    return;
  }
  switch (record.action) {
  case CommanderAction::CycleLockOn:
    runtime->set_view(record.view_yaw, record.view_pitch);
    runtime->cycle_lock_on_target(world, control.entity, owner);
    break;
  case CommanderAction::ToggleLockOn:
    runtime->set_view(record.view_yaw, record.view_pitch);
    runtime->toggle_lock_on_target(world, control.entity, owner);
    break;
  case CommanderAction::SwitchLockLeft:
  case CommanderAction::SwitchLockRight:
    runtime->switch_lock_on_target(
        world,
        control.entity,
        owner,
        record.action == CommanderAction::SwitchLockLeft ? -1 : 1);
    break;
  case CommanderAction::ToggleCameraMode:
    runtime->toggle_close_camera_mode(world, control.entity, owner);
    break;
  case CommanderAction::ToggleWeaponStance:
    runtime->toggle_weapon_stance(world, control.entity, owner);
    break;
  case CommanderAction::ReleaseForRally:
    runtime->release_for_rally();
    break;
  case CommanderAction::ReleaseInput:
    runtime->release_all_input();
    break;
  default:
    break;
  }
}

void CommandSystem::apply_input(Engine::Core::World& world,
                                int owner,
                                const CommanderInputFrame& frame,
                                float dt) {
  const auto found = m_commanders.find(owner);
  if (found == m_commanders.end() || frame.commander == 0 ||
      frame.commander != found->second.entity || found->second.applied_input) {
    return;
  }
  auto& control = found->second;
  control.applied_input = true;
  if (auto* commander = Game::Commander::CommanderRuntime::controlled_commander(
          world, control.entity, owner)) {
    Game::Commander::resume_control(*commander);
  }
  if (!control.runtime->advance(world, control.entity, owner, frame, dt)) {
    Game::Commander::end_control(&world, control.entity);
    control.entity = 0;
    control.runtime->reset();
  }
}

void CommandSystem::update(Engine::Core::World* world, float dt) {
  if (world == nullptr) {
    return;
  }

  auto* session = Game::Session::SessionContext::for_world(*world);
  if (session == nullptr) {
    return;
  }

  for (auto& [owner, control] : m_commanders) {
    control.applied_input = false;
  }
  session->commands().drain(
      *world, session->clock().tick(), [&](const LogEntry& entry) {
        if (const auto* frame = std::get_if<CommanderInputFrame>(&entry.payload)) {
          apply_input(*world, entry.owner, *frame, dt);
        } else if (const auto* action =
                       std::get_if<CommanderActionRecord>(&entry.payload);
                   action != nullptr && session->owners().is_player(entry.owner)) {
          apply_action(*world, entry.owner, *action);
        }
      });
  for (auto& [owner, control] : m_commanders) {
    if (control.entity != 0 && !control.applied_input) {
      apply_input(*world,
                  owner,
                  {.commander = control.entity,
                   .view_yaw = control.runtime->view_yaw(),
                   .view_pitch = control.runtime->view_pitch()},
                  dt);
    }
  }
}

} // namespace Game::Command

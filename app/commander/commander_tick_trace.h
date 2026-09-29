#pragma once

#include <QVector3D>

#include <cstdint>

#include "app/commander/commander_locomotion.h"
#include "app/commander/commander_presentation_trace.h"
#include "game/core/component.h"

namespace Engine::Core {
class Entity;
}

namespace App::Core {

struct TickTraceInputs {
  const CommanderInputTrace& edges;
  std::uint64_t frame_index{0};
  int forward_axis{0};
  int right_axis{0};
  bool run_held{false};
  bool primary_held{false};
  bool guard_held{false};
  float primary_held_duration{0.0F};
  float view_yaw{0.0F};
  float view_pitch{0.0F};
  float previous_view_yaw{0.0F};
  float previous_view_pitch{0.0F};
};

struct TickTraceMotor {
  const Engine::Core::TransformComponent& transform;
  bool direct_control{false};
  const MotorReport& report;
  QVector3D previous_position;
  float smoothed_speed{0.0F};
  bool grounded{true};
  float dt{0.0F};
  const Engine::Core::PresentationPose& pose;
  const CommanderCameraTrace& camera;
};

struct TickTraceCombat {
  const Engine::Core::RpgCommanderActionComponent* action{nullptr};
  const Engine::Core::CombatIntentQueueComponent* intents{nullptr};
  DodgeState dodge_state{DodgeState::None};
  float dodge_timer{0.0F};
  float dodge_grace_remaining{0.0F};
  Engine::Core::EntityID locked_target_id{0};
  std::uint16_t locked_target_slot{0};
  Engine::Core::EntityID soft_target_id{0};
  std::uint16_t soft_target_slot{0};
  std::uint32_t hit_confirm_sequence{0};
  float stamina{-1.0F};
  int health{-1};
};

void record_tick_trace(CommanderPresentationTrace& trace,
                       const Engine::Core::Entity& commander,
                       float dt,
                       const TickTraceInputs& input,
                       const TickTraceMotor& motor,
                       const TickTraceCombat& combat);

} // namespace App::Core

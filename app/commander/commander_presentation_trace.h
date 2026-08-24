#pragma once

#include <QVector3D>

#include <cstdint>

#include "app/commander/commander_camera_rig.h"

namespace App::Core {

enum class CommanderDisplacementSource : std::uint8_t {
  None,
  Walk,
  DodgeRoll,
  DodgeRecover,
  StrikeLunge,
  BodySeparation,
  JumpRecovery,
  Airborne
};

[[nodiscard]] inline auto
displacement_source_name(CommanderDisplacementSource source) -> const char* {
  switch (source) {
  case CommanderDisplacementSource::None:
    return "none";
  case CommanderDisplacementSource::Walk:
    return "walk";
  case CommanderDisplacementSource::DodgeRoll:
    return "dodge_roll";
  case CommanderDisplacementSource::DodgeRecover:
    return "dodge_recover";
  case CommanderDisplacementSource::StrikeLunge:
    return "strike_lunge";
  case CommanderDisplacementSource::BodySeparation:
    return "body_separation";
  case CommanderDisplacementSource::JumpRecovery:
    return "jump_recovery";
  case CommanderDisplacementSource::Airborne:
    return "airborne";
  }
  return "unknown";
}

struct CommanderInputTrace {
  std::uint64_t frame_index{0};

  std::uint64_t primary_press_sequence{0};
  std::uint64_t primary_release_sequence{0};
  std::uint64_t primary_consumed_sequence{0};
  std::uint64_t primary_dropped_sequence{0};
  std::uint64_t guard_press_sequence{0};
  std::uint64_t guard_release_sequence{0};
  std::uint64_t dodge_request_sequence{0};
  std::uint64_t dodge_consumed_sequence{0};
  std::uint64_t dodge_refused_sequence{0};
  std::uint64_t jump_request_sequence{0};
  std::uint64_t jump_consumed_sequence{0};
  std::uint64_t jump_refused_sequence{0};

  int move_forward_axis{0};
  int move_right_axis{0};
  bool run_held{false};
  bool primary_held{false};
  bool guard_held{false};
  float primary_held_duration{0.0F};

  float look_delta_yaw{0.0F};
  float look_delta_pitch{0.0F};
  float view_yaw{0.0F};
  float view_pitch{0.0F};
};

struct CommanderMotorTrace {
  QVector3D previous_position{};
  QVector3D position{};
  QVector3D desired_velocity{};
  QVector3D actual_velocity{};
  float requested_speed{0.0F};
  float smoothed_speed{0.0F};
  float speed_error{0.0F};
  bool grounded{true};
  bool blocked{false};
  bool slid{false};
  float separation_push{0.0F};
  float lunge_distance{0.0F};
  float snap_back_distance{0.0F};
  CommanderDisplacementSource displacement_source{CommanderDisplacementSource::None};
  float dt{0.0F};

  QVector3D presented_position{};
  float presented_yaw{0.0F};
  float presentation_alpha{1.0F};
  bool presentation_extrapolated{false};
};

struct CommanderCombatTrace {
  int action_phase{0};
  float action_normalized_time{0.0F};
  int queued_intents{0};
  bool action_running{false};
  bool guard_active{false};
  float perfect_guard_remaining{0.0F};
  int dodge_state{0};
  float dodge_timer{0.0F};
  float dodge_grace_remaining{0.0F};
  std::uint64_t locked_target_id{0};
  int locked_target_slot{-1};
  std::uint64_t soft_target_id{0};
  int soft_target_slot{-1};
  std::uint32_t hit_confirm_sequence{0};
  int action_hit_count{0};
  int health{-1};
  float stamina{-1.0F};
};

struct CommanderPresentationTrace {
  bool valid{false};
  std::uint64_t sequence{0};
  float time_seconds{0.0F};
  CommanderInputTrace input;
  CommanderMotorTrace motor;
  CommanderCameraTrace camera;
  CommanderCombatTrace combat;
};

} // namespace App::Core

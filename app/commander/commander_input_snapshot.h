#pragma once

#include <QVector3D>

#include <cstdint>

#include "game/command/commander_input.h"
#include "game/core/entity_id.h"

struct CommanderInputSnapshot {

  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool turn_left = false;
  bool turn_right = false;
  bool run = false;
  bool primary_held = false;
  bool guard_held = false;

  bool primary_pressed = false;
  bool heavy_pressed = false;
  bool dodge_pressed = false;
  bool jump_pressed = false;
  bool special_pressed = false;
  bool shield_bash_pressed = false;
  bool vanguard_rush_pressed = false;
  bool second_wind_pressed = false;

  bool has_dodge_direction = false;
  QVector3D dodge_direction{0.0F, 0.0F, 0.0F};

  std::uint64_t sequence = 0;

  [[nodiscard]] auto has_pending_edge() const -> bool {
    return primary_pressed || heavy_pressed || dodge_pressed || jump_pressed ||
           special_pressed || shield_bash_pressed || vanguard_rush_pressed ||
           second_wind_pressed;
  }

  [[nodiscard]] auto forward_axis() const -> int {
    return (forward ? 1 : 0) - (backward ? 1 : 0);
  }

  [[nodiscard]] auto right_axis() const -> int {
    return (right ? 1 : 0) - (left ? 1 : 0);
  }

  [[nodiscard]] auto to_record(Engine::Core::EntityID commander, float view_yaw) const
      -> Game::Command::CommanderInputFrame {
    using Frame = Game::Command::CommanderInputFrame;
    Frame frame;
    frame.commander = commander;
    frame.view_yaw = view_yaw;
    frame.sequence = sequence;
    frame.dodge_direction = dodge_direction;
    frame.set(Frame::Forward, forward);
    frame.set(Frame::Backward, backward);
    frame.set(Frame::Left, left);
    frame.set(Frame::Right, right);
    frame.set(Frame::TurnLeft, turn_left);
    frame.set(Frame::TurnRight, turn_right);
    frame.set(Frame::Run, run);
    frame.set(Frame::PrimaryHeld, primary_held);
    frame.set(Frame::GuardHeld, guard_held);
    frame.set(Frame::PrimaryPressed, primary_pressed);
    frame.set(Frame::HeavyPressed, heavy_pressed);
    frame.set(Frame::DodgePressed, dodge_pressed);
    frame.set(Frame::JumpPressed, jump_pressed);
    frame.set(Frame::SpecialPressed, special_pressed);
    frame.set(Frame::ShieldBashPressed, shield_bash_pressed);
    frame.set(Frame::VanguardRushPressed, vanguard_rush_pressed);
    frame.set(Frame::SecondWindPressed, second_wind_pressed);
    frame.set(Frame::HasDodgeDirection, has_dodge_direction);
    return frame;
  }

  [[nodiscard]] static auto from_record(const Game::Command::CommanderInputFrame& frame)
      -> CommanderInputSnapshot {
    using Frame = Game::Command::CommanderInputFrame;
    CommanderInputSnapshot snapshot;
    snapshot.forward = frame.held(Frame::Forward);
    snapshot.backward = frame.held(Frame::Backward);
    snapshot.left = frame.held(Frame::Left);
    snapshot.right = frame.held(Frame::Right);
    snapshot.turn_left = frame.held(Frame::TurnLeft);
    snapshot.turn_right = frame.held(Frame::TurnRight);
    snapshot.run = frame.held(Frame::Run);
    snapshot.primary_held = frame.held(Frame::PrimaryHeld);
    snapshot.guard_held = frame.held(Frame::GuardHeld);
    snapshot.primary_pressed = frame.held(Frame::PrimaryPressed);
    snapshot.heavy_pressed = frame.held(Frame::HeavyPressed);
    snapshot.dodge_pressed = frame.held(Frame::DodgePressed);
    snapshot.jump_pressed = frame.held(Frame::JumpPressed);
    snapshot.special_pressed = frame.held(Frame::SpecialPressed);
    snapshot.shield_bash_pressed = frame.held(Frame::ShieldBashPressed);
    snapshot.vanguard_rush_pressed = frame.held(Frame::VanguardRushPressed);
    snapshot.second_wind_pressed = frame.held(Frame::SecondWindPressed);
    snapshot.has_dodge_direction = frame.held(Frame::HasDodgeDirection);
    snapshot.dodge_direction = frame.dodge_direction;
    snapshot.sequence = frame.sequence;
    return snapshot;
  }
};

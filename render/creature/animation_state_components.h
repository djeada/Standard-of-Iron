#pragma once

#include <QVector3D>

#include <cstdint>

#include "animation/ambient_pose_manifest.h"
#include "animation/rig/horse_attachment_frames.h"
#include "animation/rig/horse_gait.h"
#include "combat_visual_state.h"
#include "game/core/entity.h"
#include "render/elephant/attachment_frames.h"
#include "render/elephant/dimensions.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render_request.h"

namespace Render::Creature {

struct HumanoidAnimationStateComponent {
  float idle_duration{0.0F};
  float last_sample_time{0.0F};
  bool initialized{false};
  float locomotion_last_sample_time{0.0F};
  float locomotion_phase_bias{0.0F};
  float locomotion_cycle_time{0.0F};
  float locomotion_phase{0.0F};
  float filtered_speed{0.0F};
  float filtered_acceleration{0.0F};
  float filtered_turn{0.0F};
  float filtered_travel_alignment{1.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float locomotion_presence{0.0F};
  float run_presence{0.0F};
  bool reverse_gait{false};
  Render::GL::HumanoidMotionState locomotion_state{
      Render::GL::HumanoidMotionState::Idle};
  bool locomotion_initialized{false};
  float guard_pose_progress{0.0F};
  float hold_pose_progress{0.0F};
  Animation::HumanoidAmbientRuntimeState ambient_idle{};
  CombatVisualPersistentState combat_visual{};
};

struct HorseAnimationStateComponent {
  Render::GL::GaitType current_gait{Render::GL::GaitType::IDLE};
  Render::GL::GaitType target_gait{Render::GL::GaitType::IDLE};
  float gait_transition_progress{1.0F};
  float transition_start_time{0.0F};
  float idle_bob_intensity{1.0F};
  float locomotion_phase{0.0F};
  float locomotion_phase_time{0.0F};
  bool locomotion_phase_valid{false};
};

struct ElephantAnimationStateComponent {
  Render::GL::ElephantGaitState gait_state{};
  float locomotion_phase{0.0F};
  float locomotion_phase_time{0.0F};
  bool locomotion_phase_valid{false};
};

struct HorseAnatomyComponent {
  Render::GL::HorseProfile profile{};
  Render::GL::MountedAttachmentFrame mount_frame{};
  std::uint32_t seed{0};
  QVector3D leather_base{};
  QVector3D cloth_base{};
  bool baked{false};
};

struct ElephantAnatomyComponent {
  Render::GL::ElephantProfile profile{};
  Render::GL::HowdahAttachmentFrame howdah_frame{};
  std::uint32_t seed{0};
  bool baked{false};
};

} // namespace Render::Creature

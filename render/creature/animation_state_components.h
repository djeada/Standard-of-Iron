#pragma once

#include <QVector3D>

#include <cstdint>

#include "../../game/core/entity.h"
#include "../elephant/attachment_frames.h"
#include "../elephant/dimensions.h"
#include "../gl/humanoid/humanoid_types.h"
#include "../horse/attachment_frames.h"
#include "../horse/dimensions.h"
#include "../horse/horse_gait.h"
#include "render_request.h"

namespace Render::Creature {

struct HumanoidAnimationStateComponent : public Engine::Core::Component {
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
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  Render::GL::HumanoidMotionState locomotion_state{
      Render::GL::HumanoidMotionState::Idle};
  bool locomotion_initialized{false};
  bool transient_attack_active{false};
  bool transient_attack_is_melee{false};
  bool transient_attack_is_mounted{false};
  bool transient_attack_finisher{false};
  bool transient_attack_amplified{false};
  float transient_attack_phase{0.0F};
  float transient_attack_last_sample_time{0.0F};
  float transient_attack_emphasis{1.0F};
  std::uint8_t transient_attack_variant{0U};
  Engine::Core::CombatAttackFamily transient_attack_family{
      Engine::Core::CombatAttackFamily::None};
  Render::Creature::AnimationStateId transient_attack_state{
      Render::Creature::AnimationStateId::Idle};
};

struct HorseAnimationStateComponent : public Engine::Core::Component {
  Render::GL::GaitType current_gait{Render::GL::GaitType::IDLE};
  Render::GL::GaitType target_gait{Render::GL::GaitType::IDLE};
  float gait_transition_progress{1.0F};
  float transition_start_time{0.0F};
  float idle_bob_intensity{1.0F};
  float locomotion_phase{0.0F};
  float locomotion_phase_time{0.0F};
  bool locomotion_phase_valid{false};
};

struct ElephantAnimationStateComponent : public Engine::Core::Component {
  Render::GL::ElephantGaitState gait_state{};
};

struct HorseAnatomyComponent : public Engine::Core::Component {
  Render::GL::HorseProfile profile{};
  Render::GL::MountedAttachmentFrame mount_frame{};
  std::uint32_t seed{0};
  QVector3D leather_base{};
  QVector3D cloth_base{};
  bool baked{false};
};

struct ElephantAnatomyComponent : public Engine::Core::Component {
  Render::GL::ElephantProfile profile{};
  Render::GL::HowdahAttachmentFrame howdah_frame{};
  std::uint32_t seed{0};
  bool baked{false};
};

} // namespace Render::Creature

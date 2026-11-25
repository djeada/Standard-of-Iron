#pragma once

#include <QVector3D>

#include <cstdint>

#include "animation/rig/horse_attachment_frames.h"
#include "animation/rig/horse_gait.h"
#include "render/creature/horse/dimensions.h"

namespace Render::Creature {

struct HorseAnimationStateComponent {
  Render::GL::GaitType current_gait{Render::GL::GaitType::IDLE};
  Render::GL::GaitType target_gait{Render::GL::GaitType::IDLE};
  float gait_transition_progress{1.0F};
  float transition_start_time{0.0F};
  float idle_bob_intensity{1.0F};
  Render::GL::HorseGait transition_source{};
  bool transition_source_valid{false};
  bool transition_has_motion{false};
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

} // namespace Render::Creature

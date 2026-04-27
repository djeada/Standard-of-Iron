#pragma once

#include "../../game/core/entity.h"
#include "../elephant/attachment_frames.h"
#include "../elephant/dimensions.h"
#include "../horse/attachment_frames.h"
#include "../horse/dimensions.h"
#include "../horse/horse_gait.h"

#include <QVector3D>
#include <cstdint>

namespace Render::Creature {

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

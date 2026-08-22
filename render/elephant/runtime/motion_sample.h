#pragma once

#include <QVector3D>

#include "render/creature/movement_state.h"
#include "render/elephant/elephant_gait.h"
#include "render/elephant/schema/attachment_schema.h"

namespace Render::GL {

struct ElephantMotionSample {
  ElephantGait gait{};
  HowdahAttachmentFrame howdah{};
  QVector3D barrel_center{0.0F, 0.0F, 0.0F};
  QVector3D chest_center{0.0F, 0.0F, 0.0F};
  QVector3D rump_center{0.0F, 0.0F, 0.0F};
  QVector3D neck_base{0.0F, 0.0F, 0.0F};
  QVector3D neck_top{0.0F, 0.0F, 0.0F};
  QVector3D head_center{0.0F, 0.0F, 0.0F};
  float phase = 0.0F;
  float bob = 0.0F;
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  bool is_moving = false;
  bool is_fighting = false;
  float body_sway = 0.0F;
  float trunk_swing = 0.0F;
  float ear_flap = 0.0F;
};

} // namespace Render::GL

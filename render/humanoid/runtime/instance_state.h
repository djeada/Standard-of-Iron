#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <vector>

#include "game/core/entity.h"
#include "game/formation/unit_layout.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/entity/formation_instance_layout.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/runtime/soldier_turn_smoothing.h"

namespace Render::Humanoid {

struct SoldierGroundSample {
  float x{0.0F};
  float z{0.0F};
  float model_y{0.0F};
  float surface_y{0.0F};
  bool valid{false};
};

struct SoldierSelectionCache {
  bool valid{false};
  std::uint8_t movement_state{0U};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  Render::Creature::Pipeline::HumanoidAnimationSelection selection{};
};

struct SoldierOffsetState {
  float x{0.0F};
  float z{0.0F};
};

struct CasualtyAnchor {
  std::uint16_t slot_index{0U};
  QMatrix4x4 frame{};
  float root_yaw{0.0F};
  float rest_x{0.0F};
  float rest_z{0.0F};
  float rest_yaw{0.0F};
};

struct HumanoidInstanceStateComponent {

  Render::Entity::FormationLayoutCache layout;
  std::vector<CasualtyAnchor> casualty_anchors;

  std::vector<Render::Creature::HumanoidAnimationStateComponent> animation_states;
  std::vector<Render::Creature::SoldierCombatLaneState> combat_lanes;
  std::vector<std::uint32_t> visibility_frames;
  std::vector<SoldierGroundSample> ground_samples;
  std::vector<SoldierSelectionCache> selection_cache;
  std::vector<SoldierTurnSmoothingState> turn_states;
  float turn_time{0.0F};
  bool turn_time_valid{false};
  std::vector<SoldierOffsetState> displayed_offsets;
  float offset_time{0.0F};
  bool offset_time_valid{false};
  bool offset_about_faced{false};
};

} // namespace Render::Humanoid

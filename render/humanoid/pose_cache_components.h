#pragma once

#include <cstdint>
#include <vector>

#include "game/core/entity.h"
#include "game/formation/unit_layout.h"
#include "prepare.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/gl/humanoid/humanoid_types.h"

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

struct HumanoidLayoutCacheComponent : public Engine::Core::Component {
  std::vector<SoldierLayout> soldiers;
  std::vector<Render::Creature::HumanoidAnimationStateComponent> animation_states;
  std::vector<Render::Creature::SoldierCombatLaneState> combat_lanes;
  std::vector<std::uint32_t> visibility_frames;
  std::vector<SoldierGroundSample> ground_samples;
  std::vector<SoldierSelectionCache> selection_cache;
  Render::GL::FormationParams formation{};
  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  int rows{0};
  int cols{0};
  std::uint32_t layout_version{0};
  std::uint32_t seed{0};
  std::uint32_t frame_number{0};
  bool valid{false};
};

} // namespace Render::Humanoid

#pragma once

#include <cstdint>

#include "combat_manifest.h"

namespace Animation {

enum class HitReactionForm : std::uint8_t {
  Flinch = 0,
  Block = 1,
  Evade = 2,
  Stagger = 3,
  Recoil = 4,
};

enum class MeleeSwingOutcome : std::uint8_t {
  Clean = 0,
  Blocked = 1,
  Evaded = 2,
  Heavy = 3,
  Plain = 4,
};

struct CombatRootMotionInputs {
  bool attacking{false};
  bool melee{false};
  bool mounted{false};
  bool formation_member{false};
  CombatTransactionPhase phase{CombatTransactionPhase::None};
  float attack_phase{0.0F};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
  MeleeSwingOutcome swing_outcome{MeleeSwingOutcome::Clean};

  bool hit_reacting{false};
  HitReactionForm reaction{HitReactionForm::Flinch};
  float reaction_progress{0.0F};
  float reaction_intensity{1.0F};

  float recoil_dir_x{0.0F};
  float recoil_dir_z{0.0F};

  bool body_displaced_by_simulation{false};
  std::uint32_t seed{0U};
};

struct CombatRootMotionSample {
  bool active{false};

  float forward_offset{0.0F};

  float world_offset_x{0.0F};
  float world_offset_z{0.0F};

  float pitch_degrees{0.0F};

  float roll_degrees{0.0F};

  float squash{0.0F};
};

inline constexpr float k_combat_root_pivot_height = 0.0F;

[[nodiscard]] auto melee_lunge_offset(CombatAttackFamily family,
                                      float attack_phase,
                                      MeleeSwingOutcome outcome,
                                      bool formation_member) noexcept -> float;

[[nodiscard]] auto resolve_combat_root_motion(
    const CombatRootMotionInputs& inputs) noexcept -> CombatRootMotionSample;

[[nodiscard]] auto
hit_reaction_form_from_kind(std::uint8_t kind) noexcept -> HitReactionForm;

} // namespace Animation
